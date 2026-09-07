/**
 * @file bsp_ntc.c
 * @brief NTC 采样、电阻/温度换算、滤波、标定和故障诊断。
 *
 * 硬件使用 10 kΩ 上拉电阻，NTC 接地，分压节点连接 ADC1_IN6。应用层每
 * 100 ms 调用一次采样：先用 5 点中值滤波抑制尖峰，再用 30 点移动平均
 * 平滑温度，最后通过实验标定点进行分段线性修正。
 */

#include "bsp_ntc.h"

#include "adc.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* ADC 与滤波参数。ADC 采用单次轮询，2 ms 超时远大于实际转换时间。 */
#define NTC_ADC_MAX_COUNT            4095U
#define NTC_ADC_FAULT_MARGIN           10U
#define NTC_ADC_POLL_TIMEOUT_MS         2U
#define NTC_MEDIAN_FILTER_SIZE          5U
#define NTC_AVERAGE_FILTER_SIZE        30U

/* 10 kΩ NTC 的 Steinhart-Hart 系数，实际器件更换后必须重新核对。 */
#define NTC_SERIES_RESISTANCE_OHM   10000.0f
#define NTC_COEFFICIENT_A               1.12924e-3f
#define NTC_COEFFICIENT_B               2.34108e-4f
#define NTC_COEFFICIENT_C               8.77547e-8f
#define NTC_KELVIN_OFFSET             273.15f

typedef struct {
    float measured; /**< 公式计算得到的温度。 */
    float actual;   /**< 独立温度计测得的参考温度。 */
} NTC_CalibrationPoint_t;

/* 标定点必须按 measured 从小到大排列，区间外使用端点相邻线段外推。 */
static const NTC_CalibrationPoint_t calibration_points[] = {
    {15.0f, 13.0f},
    {23.0f, 21.0f},
    {31.0f, 32.0f},
    {42.0f, 45.0f},
    {45.0f, 49.0f},
    {55.0f, 60.0f}
};

#define NTC_CALIBRATION_POINT_COUNT \
    (sizeof(calibration_points) / sizeof(calibration_points[0]))

/*
 * 全部状态均为模块私有。采样由主循环串行调用，不在 ADC 中断中修改，
 * 因此读取诊断快照时不需要临界区或 volatile。
 */
static float median_samples[NTC_MEDIAN_FILTER_SIZE];
static float average_samples[NTC_AVERAGE_FILTER_SIZE];
static uint8_t median_index;
static uint8_t median_count;
static uint8_t average_index;
static uint8_t average_count;
static float average_sum;
static float filtered_temperature = 25.0f;
static float latest_resistance_ohm;
static float latest_raw_temperature_c = 25.0f;
static uint16_t latest_adc_value;
static NTC_Status_t latest_status = NTC_STATUS_ADC_ERROR;

/**
 * @brief 完成一次 ADC 单次转换。
 *
 * 无论轮询成功与否都调用 HAL_ADC_Stop()，使 HAL 状态机回到可预测状态，
 * 便于下一周期重试。
 */
static HAL_StatusTypeDef NTC_ReadAdc(uint16_t *value)
{
    HAL_StatusTypeDef status;

    status = HAL_ADC_Start(&hadc1);
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_ADC_PollForConversion(&hadc1, NTC_ADC_POLL_TIMEOUT_MS);
    if (status == HAL_OK) {
        *value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    (void)HAL_ADC_Stop(&hadc1);
    return status;
}

/**
 * @brief 根据分压关系把 ADC 值换算为 NTC 电阻。
 *
 * NTC 接地时 Vadc/Vcc = Rntc/(Rfixed+Rntc)，整理后得到
 * Rntc = Rfixed × ADC/(4095-ADC)。调用前已排除接近 0 和满量程的值。
 */
static float NTC_ConvertAdcToResistance(uint16_t adc_value)
{
    float adc = (float)adc_value;

    return NTC_SERIES_RESISTANCE_OHM *
           adc / ((float)NTC_ADC_MAX_COUNT - adc);
}

/** 使用 Steinhart-Hart 方程把电阻换算为摄氏温度。 */
static float NTC_ConvertResistanceToTemperature(float resistance)
{
    float logarithm;
    float inverse_temperature;

    logarithm = logf(resistance);
    inverse_temperature = NTC_COEFFICIENT_A +
                          NTC_COEFFICIENT_B * logarithm +
                          NTC_COEFFICIENT_C * logarithm * logarithm * logarithm;

    return (1.0f / inverse_temperature) - NTC_KELVIN_OFFSET;
}

/**
 * @brief 按相邻标定点进行分段线性插值。
 *
 * 量程内选择包围当前测量值的线段，量程外沿首段或末段外推，使输出连续，
 * 避免简单查表在标定点处产生跳变。
 */
static float NTC_ApplyCalibration(float measured_temperature)
{
    uint32_t index;
    float slope;

    if (measured_temperature <= calibration_points[0].measured) {
        index = 0U;
    } else if (measured_temperature >=
               calibration_points[NTC_CALIBRATION_POINT_COUNT - 1U].measured) {
        index = NTC_CALIBRATION_POINT_COUNT - 2U;
    } else {
        index = 0U;
        while ((index + 1U) < NTC_CALIBRATION_POINT_COUNT) {
            if (measured_temperature <= calibration_points[index + 1U].measured) {
                break;
            }
            index++;
        }
    }

    slope = (calibration_points[index + 1U].actual -
             calibration_points[index].actual) /
            (calibration_points[index + 1U].measured -
             calibration_points[index].measured);

    return calibration_points[index].actual +
           slope * (measured_temperature - calibration_points[index].measured);
}

/**
 * @brief 五点中值滤波。
 *
 * 每次仅复制最多 5 个样本并执行插入排序，计算量固定且很小；启动阶段使用
 * 实际已有样本数量，不会让清零缓冲区参与中值计算。
 */
static float NTC_MedianFilter(float sample)
{
    float sorted[NTC_MEDIAN_FILTER_SIZE];
    float value;
    uint8_t index;
    uint8_t position;

    median_samples[median_index] = sample;
    median_index = (uint8_t)((median_index + 1U) % NTC_MEDIAN_FILTER_SIZE);
    if (median_count < NTC_MEDIAN_FILTER_SIZE) {
        median_count++;
    }

    memcpy(sorted, median_samples, median_count * sizeof(sorted[0]));
    for (index = 1U; index < median_count; index++) {
        value = sorted[index];
        position = index;
        while ((position > 0U) && (sorted[position - 1U] > value)) {
            sorted[position] = sorted[position - 1U];
            position--;
        }
        sorted[position] = value;
    }

    if ((median_count & 1U) != 0U) {
        return sorted[median_count / 2U];
    }

    return (sorted[(median_count / 2U) - 1U] +
            sorted[median_count / 2U]) * 0.5f;
}

/**
 * @brief 使用运行和维护 30 点滑动平均。
 *
 * 缓冲区未满时逐步增加样本数；填满后先减去即将被覆盖的旧样本再加入新值，
 * 每次更新时间复杂度为 O(1)，最后对平均温度应用分段标定。
 */
static void NTC_MovingAverageFilter(float sample)
{
    if (average_count < NTC_AVERAGE_FILTER_SIZE) {
        average_count++;
    } else {
        average_sum -= average_samples[average_index];
    }

    average_samples[average_index] = sample;
    average_sum += sample;
    average_index = (uint8_t)((average_index + 1U) % NTC_AVERAGE_FILTER_SIZE);

    filtered_temperature =
        NTC_ApplyCalibration(average_sum / (float)average_count);
}

/** @brief 复位 NTC 软件状态、校准 ADC 并完成首次采样。 */
HAL_StatusTypeDef NTC_Start(void)
{
    HAL_StatusTypeDef status;

    /* 清除历史状态，保证软件复位或重新初始化后的滤波结果可预测。 */
    memset(median_samples, 0, sizeof(median_samples));
    memset(average_samples, 0, sizeof(average_samples));
    median_index = 0U;
    median_count = 0U;
    average_index = 0U;
    average_count = 0U;
    average_sum = 0.0f;
    filtered_temperature = 25.0f;
    latest_resistance_ohm = 0.0f;
    latest_raw_temperature_c = 25.0f;
    latest_adc_value = 0U;
    latest_status = NTC_STATUS_ADC_ERROR;

    /* STM32F1 ADC 上电后执行自校准，以减小内部偏置误差。 */
    status = HAL_ADCEx_Calibration_Start(&hadc1);
    if (status != HAL_OK) {
        return status;
    }

    /* 启动阶段立即采集一次，避免业务层在首个周期读取空滤波器。 */
    NTC_UpdateMovingAverage();
    return (latest_status == NTC_STATUS_ADC_ERROR) ? HAL_ERROR : HAL_OK;
}

/** @brief 完成一次“采样→诊断→换算→滤波”处理链。 */
void NTC_UpdateMovingAverage(void)
{
    float raw_temperature;
    float median_temperature;

    /* HAL 转换失败时保留滤波温度，但把本次原始物理量标记为不可用。 */
    if (NTC_ReadAdc(&latest_adc_value) != HAL_OK) {
        latest_resistance_ohm = 0.0f;
        latest_raw_temperature_c = 0.0f;
        latest_status = NTC_STATUS_ADC_ERROR;
        return;
    }

    /* 两端各保留 10 个 ADC 码作为开路/短路诊断裕量，避免除零和对数异常。 */
    if (latest_adc_value < NTC_ADC_FAULT_MARGIN) {
        latest_resistance_ohm = 0.0f;
        latest_raw_temperature_c = 0.0f;
        latest_status = NTC_STATUS_ADC_LOW;
        return;
    }

    if (latest_adc_value > (NTC_ADC_MAX_COUNT - NTC_ADC_FAULT_MARGIN)) {
        /* 串口中以 0 表示本次电阻和原始温度不可用。 */
        latest_resistance_ohm = 0.0f;
        latest_raw_temperature_c = 0.0f;
        latest_status = NTC_STATUS_ADC_HIGH;
        return;
    }

    latest_resistance_ohm = NTC_ConvertAdcToResistance(latest_adc_value);
    raw_temperature = NTC_ConvertResistanceToTemperature(latest_resistance_ohm);
    latest_raw_temperature_c = raw_temperature;
    if ((raw_temperature < -45.0f) || (raw_temperature > 130.0f)) {
        latest_status = NTC_STATUS_TEMPERATURE_RANGE;
        return;
    }

    /* 只有通过全部合理性检查的样本才允许进入滤波器。 */
    median_temperature = NTC_MedianFilter(raw_temperature);
    NTC_MovingAverageFilter(median_temperature);
    latest_status = NTC_STATUS_OK;
}

float NTC_GetStableTemperature(void)
{
    return filtered_temperature;
}

NTC_Status_t NTC_CheckStatus(void)
{
    return latest_status;
}

uint16_t NTC_GetADCValue(void)
{
    return latest_adc_value;
}

/** @brief 向调用者复制最新诊断字段，不改变 NTC 模块状态。 */
void NTC_GetDiagnostics(NTC_Diagnostics_t *diagnostics)
{
    if (diagnostics == NULL) {
        return;
    }

    /* 主循环串行访问这些字段，因此逐字段拷贝即可得到一致快照。 */
    diagnostics->adc_raw = latest_adc_value;
    diagnostics->resistance_ohm = latest_resistance_ohm;
    diagnostics->raw_temperature_c = latest_raw_temperature_c;
    diagnostics->filtered_temperature_c = filtered_temperature;
    diagnostics->status = latest_status;
}
