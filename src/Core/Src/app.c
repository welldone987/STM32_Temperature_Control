/**
 * @file app.c
 * @brief 温控系统业务层：负责状态机、周期调度、控制执行、显示与遥测。
 *
 * 本文件只组织“何时做什么”，具体 ADC、PWM、Flash 等硬件操作由 BSP 完成。
 * 所有运行期任务使用 HAL_GetTick() 进行协作式调度，避免在主循环中使用长延时。
 */

#include "app.h"

#include "app_button.h"
#include "bsp_eeprom.h"
#include "bsp_motor.h"
#include "bsp_ntc.h"
#include "bsp_oled_debug.h"
#include "main.h"
#include "thermostat.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

/* 周期参数统一使用毫秒，便于评估业务响应时间和 CPU 负载。 */
#define APP_BUTTON_DEBOUNCE_MS       20U
#define APP_BUTTON_SCAN_PERIOD_MS     5U
#define APP_TEMPERATURE_PERIOD_MS   100U
#define APP_CONTROL_PERIOD_MS        50U
#define APP_DISPLAY_PERIOD_MS       500U
#define APP_TELEMETRY_PERIOD_MS    1000U
#define APP_SETTINGS_SAVE_DELAY_MS 3000U
#define APP_STATUS_MESSAGE_MS       1500U
#define APP_TELEMETRY_BUFFER_SIZE    160U

/* 用户温度阈值的默认值、合法范围和单次按键调整步长。 */
#define APP_DEFAULT_HIGH_TEMP       35.0f
#define APP_DEFAULT_LOW_TEMP        20.0f
#define APP_MIN_TEMP                 0.0f
#define APP_MAX_TEMP                80.0f
#define APP_TEMP_STEP                0.1f

typedef enum {
    APP_STATE_DISPLAY = 0,  /**< 主页面：显示温度、风扇状态和阈值。 */
    APP_STATE_SET_HIGH,     /**< 高温报警阈值设置页面。 */
    APP_STATE_SET_LOW,      /**< 低温报警阈值设置页面。 */
    APP_STATE_COUNT         /**< 状态数量，同时用作显示缓存的无效哨兵值。 */
} AppState_t;

typedef enum {
    APP_MESSAGE_NONE = 0,     /**< 没有临时状态提示。 */
    APP_MESSAGE_SAVED,        /**< 参数已成功写入 Flash。 */
    APP_MESSAGE_SAVE_FAILED   /**< 参数写入或回读校验失败。 */
} AppMessage_t;

/** 业务运行上下文，是应用层唯一的实时状态来源。 */
typedef struct {
    AppState_t state;               /**< 当前界面/设置状态。 */
    float high_limit;               /**< 高温报警阈值，单位 ℃。 */
    float low_limit;                /**< 低温报警阈值，单位 ℃。 */
    float current_temperature;      /**< 当前用于控制的滤波温度，单位 ℃。 */
    uint8_t fan_enabled;            /**< 自动风扇模式开关。 */
    uint8_t fan_percent;            /**< 已下发的风扇占空比，范围 0–100。 */
    NTC_Status_t sensor_status;     /**< 最新传感器状态。 */
    uint8_t settings_dirty;         /**< 阈值已修改但尚未保存。 */
    uint32_t settings_changed_at;   /**< 最后一次阈值修改时间。 */
    AppMessage_t message;           /**< 当前临时提示类型。 */
    uint32_t message_until;         /**< 临时提示的结束 Tick。 */
} AppContext_t;

/** OLED 已显示内容的镜像，用于只刷新发生变化的字段。 */
typedef struct {
    AppState_t state;
    char temperature[10];
    char high_limit[10];
    char low_limit[10];
    uint8_t fan_enabled;
    uint8_t fan_percent;
    NTC_Status_t sensor_status;
    AppMessage_t message;
    uint8_t settings_dirty;
} DisplayCache_t;

static AppContext_t app;
static DisplayCache_t display_cache;

static Button_t mode_button;
static Button_t temperature_up_button;
static Button_t temperature_down_button;
static Button_t fan_mode_button;

static uint32_t last_button_scan_at;
static uint32_t last_temperature_at;
static uint32_t last_control_at;
static uint32_t last_display_at;
static uint32_t last_telemetry_at;

/* USART1 中断发送期间 HAL 持续读取该缓冲区，因此发送完成前不得改写。 */
static uint8_t telemetry_buffer[APP_TELEMETRY_BUFFER_SIZE];

/* 风扇在 25–50 ℃之间由 10% 线性增加到 100%。 */
static const ThermostatConfig_t thermostat_config = {
    25.0f,
    50.0f,
    10U,
    100U
};

/**
 * @brief 判断周期任务是否到期，并在到期时更新任务时间戳。
 *
 * 使用无符号减法计算 Tick 差值，即使 HAL Tick 在约 49 天后回绕仍然有效。
 */
static uint8_t App_PeriodElapsed(uint32_t now, uint32_t *last_at, uint32_t period)
{
    if ((uint32_t)(now - *last_at) < period) {
        return 0U;
    }

    *last_at = now;
    return 1U;
}

/** 将 OLED 驱动的非 const 字符串接口隔离在一个适配函数中。 */
static void App_ShowText(uint8_t x, uint8_t y, const char *text)
{
    OLED_ShowStr(x, y, (unsigned char *)text, 1U);
}

/**
 * @brief 将浮点温度转换为一位小数字符串。
 *
 * 不直接使用 printf 的 %f，可避免在 Cortex-M3 工程中链接较大的浮点格式化代码。
 */
static void App_FormatTemperature(float value, char *buffer, uint8_t buffer_size)
{
    int scaled;
    int integer_part;
    int decimal_part;

    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    scaled = (int)((value >= 0.0f) ? (value * 10.0f + 0.5f) : (value * 10.0f - 0.5f));
    integer_part = scaled / 10;
    decimal_part = scaled % 10;
    if (decimal_part < 0) {
        decimal_part = -decimal_part;
    }

    if ((scaled < 0) && (integer_part == 0)) {
        snprintf(buffer, buffer_size, "-0.%d", decimal_part);
    } else {
        snprintf(buffer, buffer_size, "%d.%d", integer_part, decimal_part);
    }
}

/**
 * @brief 使全部 OLED 字段缓存失效，强制下一次渲染完整更新。
 *
 * 0xFF 不属于各业务字段的正常范围，适合作为“尚未显示”的哨兵值。
 */
static void App_InvalidateDisplayCache(void)
{
    memset(&display_cache, 0, sizeof(display_cache));
    display_cache.state = APP_STATE_COUNT;
    display_cache.fan_enabled = 0xFFU;
    display_cache.fan_percent = 0xFFU;
    display_cache.sensor_status = (NTC_Status_t)0xFFU;
    display_cache.message = (AppMessage_t)0xFFU;
    display_cache.settings_dirty = 0xFFU;
}

/** 标记阈值待保存，并重新开始 3 秒延迟保存计时。 */
static void App_MarkSettingsDirty(uint32_t now)
{
    app.settings_dirty = 1U;
    app.settings_changed_at = now;
}

/**
 * @brief 调整当前设置页面对应的阈值。
 *
 * 除限制在 0–80 ℃外，还保证 high_limit 至少比 low_limit 高一个调整步长，
 * 从数据源头维护阈值不变量，后续控制逻辑无需重复防御非法区间。
 */
static void App_AdjustSelectedLimit(float delta, uint32_t now)
{
    if (app.state == APP_STATE_SET_HIGH) {
        app.high_limit += delta;
        if (app.high_limit > APP_MAX_TEMP) {
            app.high_limit = APP_MAX_TEMP;
        }
        if (app.high_limit < (app.low_limit + APP_TEMP_STEP)) {
            app.high_limit = app.low_limit + APP_TEMP_STEP;
        }
    } else if (app.state == APP_STATE_SET_LOW) {
        app.low_limit += delta;
        if (app.low_limit < APP_MIN_TEMP) {
            app.low_limit = APP_MIN_TEMP;
        }
        if (app.low_limit > (app.high_limit - APP_TEMP_STEP)) {
            app.low_limit = app.high_limit - APP_TEMP_STEP;
        }
    } else {
        return;
    }

    App_MarkSettingsDirty(now);
}

/**
 * @brief 每 5 ms 扫描四个按键，并消费已经通过防抖的按下事件。
 *
 * 模式键按“显示→设置上限→设置下限→显示”循环切换；加减键只在设置页面
 * 生效；温控键独立切换自动风扇模式。
 */
static void App_ProcessButtons(uint32_t now)
{
    if (!App_PeriodElapsed(now, &last_button_scan_at, APP_BUTTON_SCAN_PERIOD_MS)) {
        return;
    }

    if (Button_Update(&mode_button, now, APP_BUTTON_DEBOUNCE_MS) == BUTTON_EVENT_PRESSED) {
        if (app.state == APP_STATE_DISPLAY) {
            app.state = APP_STATE_SET_HIGH;
        } else if (app.state == APP_STATE_SET_HIGH) {
            app.state = APP_STATE_SET_LOW;
        } else {
            app.state = APP_STATE_DISPLAY;
        }
        display_cache.state = APP_STATE_COUNT;
    }

    if (Button_Update(&temperature_up_button, now, APP_BUTTON_DEBOUNCE_MS) == BUTTON_EVENT_PRESSED) {
        App_AdjustSelectedLimit(APP_TEMP_STEP, now);
    }

    if (Button_Update(&temperature_down_button, now, APP_BUTTON_DEBOUNCE_MS) == BUTTON_EVENT_PRESSED) {
        App_AdjustSelectedLimit(-APP_TEMP_STEP, now);
    }

    if (Button_Update(&fan_mode_button, now, APP_BUTTON_DEBOUNCE_MS) == BUTTON_EVENT_PRESSED) {
        app.fan_enabled = app.fan_enabled ? 0U : 1U;
    }
}

/** 每 100 ms 获取一个新样本，并把 NTC 状态同步到业务上下文。 */
static void App_UpdateTemperature(uint32_t now)
{
    if (!App_PeriodElapsed(now, &last_temperature_at, APP_TEMPERATURE_PERIOD_MS)) {
        return;
    }

    NTC_UpdateMovingAverage();
    app.current_temperature = NTC_GetStableTemperature();
    app.sensor_status = NTC_CheckStatus();
}

/** 封装低电平点亮的 LED 电气特性，使上层参数仍使用直观的 on=1。 */
static void App_SetLed(GPIO_TypeDef *port, uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief 每 50 ms 执行一次温控决策并更新 LED、蜂鸣器和风扇。
 *
 * 传感器故障时两个 LED 同步闪烁；普通高低温状态使用不同颜色区分。
 * 只有风扇指令发生变化时才写 PWM，减少无意义的外设访问。
 */
static void App_ApplyControl(uint32_t now)
{
    ThermostatOutput_t output;
    uint8_t alarm_on;
    uint8_t fault_blink_on;

    if (!App_PeriodElapsed(now, &last_control_at, APP_CONTROL_PERIOD_MS)) {
        return;
    }

    Thermostat_Evaluate(&thermostat_config,
                        app.current_temperature,
                        app.low_limit,
                        app.high_limit,
                        app.fan_enabled,
                        (app.sensor_status == NTC_STATUS_OK),
                        &output);

    /* 故障灯以 500 ms 为周期翻转，便于与持续高/低温报警区分。 */
    fault_blink_on = ((now % 500U) >= 250U) ? 1U : 0U;
    if (output.state == THERMOSTAT_STATE_TOO_HOT) {
        App_SetLed(LED_RED_GPIO_Port, LED_RED_Pin, 1U);
        App_SetLed(LED_GREEN_GPIO_Port, LED_GREEN_Pin, 0U);
    } else if (output.state == THERMOSTAT_STATE_TOO_COLD) {
        App_SetLed(LED_RED_GPIO_Port, LED_RED_Pin, 0U);
        App_SetLed(LED_GREEN_GPIO_Port, LED_GREEN_Pin, 1U);
    } else if (output.state == THERMOSTAT_STATE_SENSOR_FAULT) {
        App_SetLed(LED_RED_GPIO_Port, LED_RED_Pin, fault_blink_on);
        App_SetLed(LED_GREEN_GPIO_Port, LED_GREEN_Pin, fault_blink_on);
    } else {
        App_SetLed(LED_RED_GPIO_Port, LED_RED_Pin, 0U);
        App_SetLed(LED_GREEN_GPIO_Port, LED_GREEN_Pin, 0U);
    }

    /* 蜂鸣器每秒鸣叫 500 ms，不使用 HAL_Delay() 阻塞其他任务。 */
    alarm_on = output.alarm_enabled && ((now % 1000U) >= 500U);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port,
                      BUZZER_Pin,
                      alarm_on ? GPIO_PIN_SET : GPIO_PIN_RESET);

    if (output.fan_percent != app.fan_percent) {
        app.fan_percent = output.fan_percent;
        if (app.fan_percent == 0U) {
            Motor_Stop();
        } else {
            Motor_SetSpeed(app.fan_percent);
        }
    }
}

/**
 * @brief 阈值停止变化 3 秒后再写入 Flash。
 *
 * 延迟保存可以把连续按键调整合并为一次擦写，降低片内 Flash 的擦写次数。
 */
static void App_SaveSettingsIfNeeded(uint32_t now)
{
    TempSettings_t settings;

    if (!app.settings_dirty ||
        ((uint32_t)(now - app.settings_changed_at) < APP_SETTINGS_SAVE_DELAY_MS)) {
        return;
    }

    settings.temp_high = app.high_limit;
    settings.temp_low = app.low_limit;
    app.message = (EEPROM_WriteSettings(&settings) == HAL_OK) ?
                  APP_MESSAGE_SAVED : APP_MESSAGE_SAVE_FAILED;
    app.message_until = now + APP_STATUS_MESSAGE_MS;
    app.settings_dirty = 0U;
}

/** 到达截止时间后清除 OLED 临时保存结果提示。 */
static void App_UpdateMessage(uint32_t now)
{
    if ((app.message != APP_MESSAGE_NONE) &&
        ((int32_t)(now - app.message_until) >= 0)) {
        app.message = APP_MESSAGE_NONE;
    }
}

/** 按“保存结果 > 传感器故障 > 待保存 > 正常”的优先级绘制状态行。 */
static void App_RenderStatusLine(void)
{
    OLED_FillArea(0U, 6U, 128U, 1U, 0x00U);

    if (app.message == APP_MESSAGE_SAVED) {
        App_ShowText(0U, 6U, "SETTINGS SAVED");
    } else if (app.message == APP_MESSAGE_SAVE_FAILED) {
        App_ShowText(0U, 6U, "SAVE FAILED");
    } else if (app.sensor_status != NTC_STATUS_OK) {
        App_ShowText(0U, 6U, "SENSOR ERROR");
    } else if (app.settings_dirty) {
        App_ShowText(0U, 6U, "PENDING SAVE *");
    } else {
        App_ShowText(0U, 6U, "RUNNING");
    }
}

/**
 * @brief 每 500 ms 更新 OLED；页面切换时重绘框架，页面内只刷新变化字段。
 * @param force 非零时忽略显示周期，立即渲染。
 *
 * OLED 通过软件 I²C 通信，字段缓存可以明显减少总线占用和整屏闪烁。
 */
static void App_RenderDisplay(uint32_t now, uint8_t force)
{
    char temperature[10];
    char high_limit[10];
    char low_limit[10];
    char fan_percent[4];

    if (!force && !App_PeriodElapsed(now, &last_display_at, APP_DISPLAY_PERIOD_MS)) {
        return;
    }
    last_display_at = now;

    App_FormatTemperature(app.current_temperature, temperature, sizeof(temperature));
    App_FormatTemperature(app.high_limit, high_limit, sizeof(high_limit));
    App_FormatTemperature(app.low_limit, low_limit, sizeof(low_limit));

    if (display_cache.state != app.state) {
        OLED_CLS();
        App_InvalidateDisplayCache();
        display_cache.state = app.state;

        if (app.state == APP_STATE_DISPLAY) {
            App_ShowText(0U, 0U, "Temp:       C");
            App_ShowText(0U, 2U, "Mode:          %");
            App_ShowText(0U, 4U, "A:    B:");
        } else if (app.state == APP_STATE_SET_HIGH) {
            App_ShowText(10U, 0U, "Set HIGH Temp");
            App_ShowText(5U, 5U, "Range: 0-80 C");
        } else {
            App_ShowText(10U, 0U, "Set LOW Temp");
            App_ShowText(5U, 5U, "Range: 0-80 C");
        }
    }

    if (app.state == APP_STATE_DISPLAY) {
        if (strcmp(temperature, display_cache.temperature) != 0) {
            OLED_FillArea(30U, 0U, 42U, 1U, 0x00U);
            App_ShowText(30U, 0U, temperature);
            strcpy(display_cache.temperature, temperature);
        }

        if (display_cache.fan_enabled != app.fan_enabled) {
            OLED_FillArea(30U, 2U, 20U, 1U, 0x00U);
            App_ShowText(30U, 2U, app.fan_enabled ? "ON " : "OFF");
            display_cache.fan_enabled = app.fan_enabled;
        }

        if (display_cache.fan_percent != app.fan_percent) {
            snprintf(fan_percent, sizeof(fan_percent), "%3d", (int)app.fan_percent);
            OLED_FillArea(60U, 2U, 20U, 1U, 0x00U);
            App_ShowText(60U, 2U, fan_percent);
            display_cache.fan_percent = app.fan_percent;
        }

        if (strcmp(high_limit, display_cache.high_limit) != 0) {
            OLED_FillArea(10U, 4U, 28U, 1U, 0x00U);
            App_ShowText(10U, 4U, high_limit);
            strcpy(display_cache.high_limit, high_limit);
        }

        if (strcmp(low_limit, display_cache.low_limit) != 0) {
            OLED_FillArea(48U, 4U, 28U, 1U, 0x00U);
            App_ShowText(48U, 4U, low_limit);
            strcpy(display_cache.low_limit, low_limit);
        }
    } else if (app.state == APP_STATE_SET_HIGH) {
        if (strcmp(high_limit, display_cache.high_limit) != 0) {
            OLED_FillArea(30U, 2U, 42U, 1U, 0x00U);
            App_ShowText(30U, 2U, high_limit);
            strcpy(display_cache.high_limit, high_limit);
        }
    } else {
        if (strcmp(low_limit, display_cache.low_limit) != 0) {
            OLED_FillArea(30U, 2U, 42U, 1U, 0x00U);
            App_ShowText(30U, 2U, low_limit);
            strcpy(display_cache.low_limit, low_limit);
        }
    }

    if ((display_cache.sensor_status != app.sensor_status) ||
        (display_cache.message != app.message) ||
        (display_cache.settings_dirty != app.settings_dirty)) {
        App_RenderStatusLine();
        display_cache.sensor_status = app.sensor_status;
        display_cache.message = app.message;
        display_cache.settings_dirty = app.settings_dirty;
    }
}

/**
 * @brief 每秒通过 USART1 异步发送一行可解析的系统遥测。
 *
 * 只有 UART 处于 READY 状态时才格式化并启动发送，确保静态缓冲区不会在
 * 中断发送过程中被覆盖。若串口仍忙则跳过本次，不阻塞控制与采样任务。
 */
static void App_SendTelemetry(uint32_t now)
{
    NTC_Diagnostics_t diagnostics;
    char raw_temperature[10];
    char filtered_temperature[10];
    uint32_t resistance_ohm;
    int length;

    if ((uint32_t)(now - last_telemetry_at) < APP_TELEMETRY_PERIOD_MS) {
        return;
    }

    /* 上一帧未完成时直接返回，下一次主循环不会等待串口。 */
    if (HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY) {
        return;
    }

    NTC_GetDiagnostics(&diagnostics);
    App_FormatTemperature(diagnostics.raw_temperature_c,
                          raw_temperature,
                          sizeof(raw_temperature));
    App_FormatTemperature(diagnostics.filtered_temperature_c,
                          filtered_temperature,
                          sizeof(filtered_temperature));
    resistance_ohm = (diagnostics.resistance_ohm > 0.0f) ?
                     (uint32_t)(diagnostics.resistance_ohm + 0.5f) : 0U;

    /* 温度已预先格式化为字符串，避免 snprintf 链接浮点格式化支持。 */
    length = snprintf((char *)telemetry_buffer,
                      sizeof(telemetry_buffer),
                      "time_ms=%lu adc=%u r_ohm=%lu raw_c=%s filt_c=%s "
                      "fan_pct=%u fault=%u\r\n",
                      (unsigned long)now,
                      (unsigned int)diagnostics.adc_raw,
                      (unsigned long)resistance_ohm,
                      raw_temperature,
                      filtered_temperature,
                      (unsigned int)app.fan_percent,
                      (unsigned int)diagnostics.status);
    if (length <= 0) {
        return;
    }
    if ((uint32_t)length >= sizeof(telemetry_buffer)) {
        length = (int)(sizeof(telemetry_buffer) - 1U);
    }

    if (HAL_UART_Transmit_IT(&huart1,
                            telemetry_buffer,
                            (uint16_t)length) == HAL_OK) {
        last_telemetry_at = now;
    }
}

/**
 * @brief 初始化业务上下文、BSP 模块、按键对象和周期调度基准。
 * @return 全部关键模块初始化成功时返回 HAL_OK。
 */
HAL_StatusTypeDef App_Init(void)
{
    TempSettings_t stored_settings;
    uint32_t now;

    /* 先建立可预测的默认状态，Flash 无有效记录时仍可正常运行。 */
    memset(&app, 0, sizeof(app));
    app.state = APP_STATE_DISPLAY;
    app.high_limit = APP_DEFAULT_HIGH_TEMP;
    app.low_limit = APP_DEFAULT_LOW_TEMP;
    app.current_temperature = 25.0f;
    /* 0xFF 强制第一次控制计算把真实风扇指令下发到 BSP。 */
    app.fan_percent = 0xFFU;
    app.sensor_status = (NTC_Status_t)0xFFU;

    OLED_Init_With_Flip(1U);
    OLED_CLS();
    App_ShowText(10U, 2U, "INITIALIZING");

    /* 只有魔数、版本、CRC 和阈值范围全部有效时才采用持久化参数。 */
    if (EEPROM_ReadSettings(&stored_settings) == HAL_OK) {
        app.high_limit = stored_settings.temp_high;
        app.low_limit = stored_settings.temp_low;
    }

    if (NTC_Start() != HAL_OK) {
        App_ShowText(0U, 6U, "ADC START FAILED");
        return HAL_ERROR;
    }

    if (Motor_Init() != HAL_OK) {
        App_ShowText(0U, 6U, "PWM START FAILED");
        return HAL_ERROR;
    }

    now = HAL_GetTick();
    /* 板上 KEY_A/B/C 为高有效；PC13 温控开关为低有效。 */
    Button_Init(&mode_button, MODE_SWITCH_GPIO_Port, MODE_SWITCH_Pin, GPIO_PIN_SET, now);
    Button_Init(&temperature_up_button, TEMP_UP_GPIO_Port, TEMP_UP_Pin, GPIO_PIN_SET, now);
    Button_Init(&temperature_down_button, TEMP_DOWN_GPIO_Port, TEMP_DOWN_Pin, GPIO_PIN_SET, now);
    Button_Init(&fan_mode_button, THERMO_MODE_GPIO_Port, THERMO_MODE_Pin, GPIO_PIN_RESET, now);

    last_button_scan_at = now;
    /* 周期时间戳回退一个周期，使关键任务进入主循环后立即执行一次。 */
    last_temperature_at = now - APP_TEMPERATURE_PERIOD_MS;
    last_control_at = now - APP_CONTROL_PERIOD_MS;
    last_display_at = now - APP_DISPLAY_PERIOD_MS;
    last_telemetry_at = now - APP_TELEMETRY_PERIOD_MS;
    App_InvalidateDisplayCache();
    App_RenderDisplay(now, 1U);

    return HAL_OK;
}

/** @brief 主循环入口，按固定数据依赖顺序执行一次到期任务检查。 */
void App_Process(void)
{
    uint32_t now = HAL_GetTick();

    /* 任务顺序体现数据依赖：输入→测量→控制→保存/显示→遥测。 */
    App_ProcessButtons(now);
    App_UpdateTemperature(now);
    App_ApplyControl(now);
    App_SaveSettingsIfNeeded(now);
    App_UpdateMessage(now);
    App_RenderDisplay(now, 0U);
    App_SendTelemetry(now);
}
