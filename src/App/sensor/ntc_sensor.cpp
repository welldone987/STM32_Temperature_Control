#include "sensor/ntc_sensor.hpp"

#include <algorithm>
#include <cmath>

#include "adc.h"
#include "main.h"

namespace app {

namespace {

/*
 * 分段线性校准表：(measured=滤波后示值, actual=真实温度)，单位摄氏度。
 *
 * 2026-08-29 用参考温度计采集 26 组数据（覆盖 27-90°C）拟合：
 * 先用旧表反解出滑动平均值 M，再最小二乘拟合得直线
 *   R = -4.689 + 1.1981 * M   （RMS 0.50°C，最大偏差 1.25°C）
 * 因此两个标定点即可精确表达该直线，表外区域按同一斜率外推。
 *
 * 重要约束：
 *  - 该校准与 ADC 采样时间(1.5 周期)绑定，改采样时间必须重新标定；
 *  - 旧的 6 点表最高只标到 55→60，更高温全靠外推导致失真，已被替换。
 */
constexpr NtcSensor::CalibrationPoint kCalibrationPoints[] = {
    {27.51f, 28.27f},
    {78.82f, 89.74f},
};

constexpr std::size_t kCalibrationPointCount =
    sizeof(kCalibrationPoints) / sizeof(kCalibrationPoints[0]);

}  // namespace

// DMA 缓冲区 50 点求均值，得到当前的 ADC 码值。
std::uint16_t NtcSensor::average_adc_counts() const {
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < kAdcBufferSize; ++i) {
        sum += dma_buffer_[i];
    }
    return static_cast<std::uint16_t>(sum / kAdcBufferSize);
}

// ADC 码值 -> NTC 阻值 -> Steinhart-Hart 方程 -> 原始温度（摄氏度）。
float NtcSensor::raw_temperature_c() const {
    float adc_average = static_cast<float>(average_adc_counts());
    // 钳位防止分母为 0 或对数无定义（对应 NTC 开路/短路的极端情况）。
    if (adc_average <= 1.0f) {
        adc_average = 1.0f;
    }
    if (adc_average >= kAdcMaxValue) {
        adc_average = kAdcMaxValue - 1.0f;
    }

    // 分压电路：NTC 接在 ADC 与地之间，串联电阻接在 3.3V 与 ADC 之间。
    const float resistance =
        kSeriesResistorOhm * (adc_average / (kAdcMaxValue - adc_average));

    const float ln_r = std::log(resistance);
    const float inv_t =
        kCoefficientA + kCoefficientB * ln_r + kCoefficientC * ln_r * ln_r * ln_r;

    return 1.0f / inv_t - 273.15f;  // 开尔文 -> 摄氏度
}

// 分段线性插值：落在表内用相邻两点插值，落在两端用端点斜率外推。
float NtcSensor::piecewise_calibrate(float measured) {
    if (measured <= kCalibrationPoints[0].measured) {
        const CalibrationPoint& lo = kCalibrationPoints[0];
        const CalibrationPoint& hi = kCalibrationPoints[1];
        const float slope = (hi.actual - lo.actual) / (hi.measured - lo.measured);
        return lo.actual + slope * (measured - lo.measured);
    }

    if (measured >= kCalibrationPoints[kCalibrationPointCount - 1].measured) {
        const CalibrationPoint& lo = kCalibrationPoints[kCalibrationPointCount - 2];
        const CalibrationPoint& hi = kCalibrationPoints[kCalibrationPointCount - 1];
        const float slope = (hi.actual - lo.actual) / (hi.measured - lo.measured);
        return hi.actual + slope * (measured - hi.measured);
    }

    for (std::size_t i = 0; i + 1 < kCalibrationPointCount; ++i) {
        const CalibrationPoint& lo = kCalibrationPoints[i];
        const CalibrationPoint& hi = kCalibrationPoints[i + 1];
        if (measured >= lo.measured && measured <= hi.measured) {
            const float slope = (hi.actual - lo.actual) / (hi.measured - lo.measured);
            return lo.actual + slope * (measured - lo.measured);
        }
    }

    return measured;
}

// 5 点中值滤波：写入环形缓冲后排序取中值，抑制偶发尖峰。
float NtcSensor::median_filter(float new_value) {
    median_buffer_[median_index_] = new_value;
    median_index_ = static_cast<std::uint8_t>((median_index_ + 1) % kMedianSize);

    std::array<float, kMedianSize> sorted = median_buffer_;
    std::sort(sorted.begin(), sorted.end());
    return sorted[kMedianSize / 2];
}

// 采样任务每 100ms 调一次：取一帧原始温度 -> 中值滤波 -> 压入滑动平均窗口。
void NtcSensor::update() {
    const std::uint32_t now = HAL_GetTick();
    if (now - last_sample_time_ < kSampleIntervalMs) {
        return;  // 未到采样间隔，节流（调用方更频繁时这里是空操作）
    }
    last_sample_time_ = now;

    const float filtered = median_filter(raw_temperature_c());

    moving_average_buffer_[moving_average_index_] = filtered;
    moving_average_index_ =
        static_cast<std::uint8_t>((moving_average_index_ + 1) % kMovingAverageSize);
    if (moving_average_count_ < kMovingAverageSize) {
        ++moving_average_count_;  // 窗口未填满前只对已有样本求均值
    }
}

// 滑动平均 + 分段校准 + 偏移 = 对外温度。窗口为空时返回安全默认值 25°C。
float NtcSensor::stable_temperature_c() {
    if (moving_average_count_ == 0) {
        return 25.0f;
    }

    float sum = 0.0f;
    for (std::uint8_t i = 0; i < moving_average_count_; ++i) {
        sum += moving_average_buffer_[i];
    }
    const float average = sum / static_cast<float>(moving_average_count_);

    return piecewise_calibrate(average) + calibration_offset_;
}

void NtcSensor::set_calibration_offset(float offset) { calibration_offset_ = offset; }

// 复位滤波器并启动循环 DMA 采样（调度器启动前调用一次）。
void NtcSensor::start() {
    median_buffer_.fill(0.0f);
    moving_average_buffer_.fill(0.0f);
    median_index_ = 0;
    moving_average_index_ = 0;
    moving_average_count_ = 0;

    last_sample_time_ = HAL_GetTick();
    calibration_offset_ = 0.0f;

    HAL_ADC_Stop_DMA(&hadc1);
    // HAL 接口要求 (uint32_t*)；缓冲实际是 16 位，HAL 按半字对齐搬运。
    if (HAL_ADC_Start_DMA(&hadc1,
                          reinterpret_cast<std::uint32_t*>(
                              const_cast<std::uint16_t*>(dma_buffer_)),
                          kAdcBufferSize) != HAL_OK) {
        Error_Handler();
    }
}

void NtcSensor::stop() { HAL_ADC_Stop_DMA(&hadc1); }

// ISR 上下文：出错后直接重启循环 DMA。
// 旧实现这里 HAL_Delay(5)——优先级 0 中断不会被 SysTick 抢占，
// 那样做会死等（HardFault 隐患），因此本版本改为立即重启。
void NtcSensor::recover_from_dma_error() {
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Start_DMA(&hadc1,
                      reinterpret_cast<std::uint32_t*>(
                          const_cast<std::uint16_t*>(dma_buffer_)),
                      kAdcBufferSize);
}

// 唯一实例。用函数内 static 保证初始化顺序安全，且只初始化一次。
NtcSensor& ntc_sensor() {
    static NtcSensor instance;
    return instance;
}

}  // namespace app

// HAL 弱符号回调的 C 语言桥接：ADC/DMA 出错时由 HAL 在 ISR 中调用。
// 必须是 extern "C"，否则链接器找不到与 HAL 声明匹配的符号。
extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        app::ntc_sensor().recover_from_dma_error();
    }
}
