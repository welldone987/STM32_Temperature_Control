#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <stdint.h>

typedef enum {
    THERMOSTAT_STATE_NORMAL = 0,   /**< 温度位于用户设定区间内。 */
    THERMOSTAT_STATE_TOO_COLD,     /**< 温度低于下限。 */
    THERMOSTAT_STATE_TOO_HOT,      /**< 温度高于上限。 */
    THERMOSTAT_STATE_SENSOR_FAULT  /**< 传感器数据不可信。 */
} ThermostatState_t;

/** 风扇线性调速参数。 */
typedef struct {
    float fan_start_temperature;  /**< 最小风速对应温度，单位 ℃。 */
    float fan_full_temperature;   /**< 最大风速对应温度，单位 ℃。 */
    uint8_t fan_min_percent;      /**< 自动模式最低占空比，范围 0–100。 */
    uint8_t fan_max_percent;      /**< 自动模式最高占空比，范围 0–100。 */
} ThermostatConfig_t;

/** 一次温控决策的输出，不直接包含任何硬件操作。 */
typedef struct {
    ThermostatState_t state;  /**< 当前温度/故障状态。 */
    uint8_t alarm_enabled;    /**< 1 表示需要报警，0 表示不报警。 */
    uint8_t fan_percent;      /**< 期望风扇占空比，范围 0–100。 */
} ThermostatOutput_t;

/**
 * @brief 根据温度、阈值和模式计算报警状态及风扇需求。
 * @param config 风扇调速配置。
 * @param temperature 当前滤波温度，单位 ℃。
 * @param low_limit 用户设定的低温报警阈值。
 * @param high_limit 用户设定的高温报警阈值。
 * @param fan_enabled 自动风扇模式是否开启。
 * @param sensor_ok 温度传感器数据是否有效。
 * @param output 输出决策结果。
 *
 * 本函数不依赖 HAL，也不直接操作 GPIO/TIM，因此控制策略可以脱离硬件测试。
 */
void Thermostat_Evaluate(const ThermostatConfig_t *config,
                         float temperature,
                         float low_limit,
                         float high_limit,
                         uint8_t fan_enabled,
                         uint8_t sensor_ok,
                         ThermostatOutput_t *output);

#endif /* THERMOSTAT_H */
