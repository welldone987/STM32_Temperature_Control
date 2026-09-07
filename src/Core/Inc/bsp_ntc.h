#ifndef BSP_NTC_H
#define BSP_NTC_H

#include "stm32f1xx_hal.h"

typedef enum {
    NTC_STATUS_OK = 0,                 /**< 测量有效。 */
    NTC_STATUS_ADC_LOW = 1,            /**< ADC 接近 0，疑似采样节点短路。 */
    NTC_STATUS_ADC_HIGH = 2,           /**< ADC 接近满量程，疑似 NTC 开路。 */
    NTC_STATUS_TEMPERATURE_RANGE = 3,  /**< 换算温度超出允许物理范围。 */
    NTC_STATUS_ADC_ERROR = 4           /**< HAL ADC 启动或转换失败。 */
} NTC_Status_t;

/** NTC 只读诊断快照，用于 USART 遥测和板级调试。 */
typedef struct {
    uint16_t adc_raw;             /**< 最新 12 位 ADC 原始值。 */
    float resistance_ohm;         /**< 由分压关系换算的 NTC 电阻，单位 Ω。 */
    float raw_temperature_c;      /**< Steinhart-Hart 换算温度，尚未滤波，单位 ℃。 */
    float filtered_temperature_c; /**< 中值、移动平均及标定后的温度，单位 ℃。 */
    NTC_Status_t status;          /**< 最新一次采样的状态码。 */
} NTC_Diagnostics_t;

/**
 * @brief 校准 ADC1、清空滤波器并获取第一个 NTC 样本。
 * @return ADC 校准或首次转换失败时返回 HAL_ERROR。
 */
HAL_StatusTypeDef NTC_Start(void);

/** @brief 采集一个 ADC 样本，并在样本有效时更新两级温度滤波器。 */
void NTC_UpdateMovingAverage(void);

/** @return 最新滤波并标定后的温度，单位 ℃。 */
float NTC_GetStableTemperature(void);

/** @return 最新一次 ADC 采样及温度换算状态。 */
NTC_Status_t NTC_CheckStatus(void);

/** @return 最新 12 位 ADC 原始值，仅用于诊断。 */
uint16_t NTC_GetADCValue(void);

/**
 * @brief 拷贝当前诊断快照，不会触发新的 ADC 转换。
 * @param diagnostics 接收快照的结构体；传入 NULL 时直接返回。
 */
void NTC_GetDiagnostics(NTC_Diagnostics_t *diagnostics);

#endif /* BSP_NTC_H */
