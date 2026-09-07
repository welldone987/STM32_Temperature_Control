/**
 * @file thermostat.c
 * @brief 与硬件无关的温度报警和风扇调速策略。
 *
 * 模块只接收输入并返回决策结果，不访问 GPIO、TIM 或 HAL。这样可以把业务
 * 策略和硬件执行解耦，也便于后续为边界温度补充主机单元测试。
 */

#include "thermostat.h"

#include <stddef.h>

/**
 * @brief 把温度映射为限定范围内的线性风扇占空比。
 *
 * 温度低于起调点时返回最小占空比，高于满速点时返回最大占空比；中间区间
 * 使用一次线性插值，并通过 +0.5f 四舍五入到整数百分比。
 */
static uint8_t Thermostat_CalculateFan(const ThermostatConfig_t *config, float temperature)
{
    float ratio;
    float percent;

    if (temperature <= config->fan_start_temperature) {
        return config->fan_min_percent;
    }

    if (temperature >= config->fan_full_temperature) {
        return config->fan_max_percent;
    }

    ratio = (temperature - config->fan_start_temperature) /
            (config->fan_full_temperature - config->fan_start_temperature);
    percent = (float)config->fan_min_percent +
              ratio * (float)(config->fan_max_percent - config->fan_min_percent);

    return (uint8_t)(percent + 0.5f);
}

void Thermostat_Evaluate(const ThermostatConfig_t *config,
                         float temperature,
                         float low_limit,
                         float high_limit,
                         uint8_t fan_enabled,
                         uint8_t sensor_ok,
                         ThermostatOutput_t *output)
{
    if ((config == NULL) || (output == NULL)) {
        return;
    }

    /* 故障优先级最高：输入不可信时报警并停止自动执行器。 */
    if (!sensor_ok) {
        output->state = THERMOSTAT_STATE_SENSOR_FAULT;
        output->alarm_enabled = 1U;
        output->fan_percent = 0U;
        return;
    }

    /* 高、低阈值只决定报警状态；风扇曲线由独立配置决定。 */
    if (temperature > high_limit) {
        output->state = THERMOSTAT_STATE_TOO_HOT;
        output->alarm_enabled = 1U;
    } else if (temperature < low_limit) {
        output->state = THERMOSTAT_STATE_TOO_COLD;
        output->alarm_enabled = 1U;
    } else {
        output->state = THERMOSTAT_STATE_NORMAL;
        output->alarm_enabled = 0U;
    }

    /* 手动关闭自动风扇模式时，明确输出 0%，不保留上一次指令。 */
    output->fan_percent = fan_enabled ? Thermostat_CalculateFan(config, temperature) : 0U;
}
