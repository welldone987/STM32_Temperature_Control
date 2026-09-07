#ifndef APP_BUTTON_H
#define APP_BUTTON_H

#include "stm32f1xx_hal.h"

typedef enum {
    BUTTON_EVENT_NONE = 0,  /**< 当前没有新的稳定边沿。 */
    BUTTON_EVENT_PRESSED,   /**< 按键已经稳定进入按下状态。 */
    BUTTON_EVENT_RELEASED   /**< 按键已经稳定进入释放状态。 */
} ButtonEvent_t;

/** 单个 GPIO 按键的防抖上下文。 */
typedef struct {
    GPIO_TypeDef *port;          /**< GPIO 端口。 */
    uint16_t pin;                /**< GPIO 引脚掩码。 */
    GPIO_PinState active_level;  /**< 按下时的有效电平，可兼容高/低有效按键。 */
    uint8_t raw_pressed;         /**< 最近一次采到的原始逻辑状态。 */
    uint8_t stable_pressed;      /**< 已通过防抖确认的稳定逻辑状态。 */
    uint32_t raw_changed_at;     /**< 原始状态最后一次变化的系统时间，单位 ms。 */
} Button_t;

/**
 * @brief 初始化一个按键对象，并把当前电平作为初始稳定状态。
 * @param button 按键上下文。
 * @param port GPIO 端口。
 * @param pin GPIO 引脚掩码。
 * @param active_level 按下时的有效电平。
 * @param now 当前 HAL Tick，单位 ms。
 */
void Button_Init(Button_t *button,
                 GPIO_TypeDef *port,
                 uint16_t pin,
                 GPIO_PinState active_level,
                 uint32_t now);

/**
 * @brief 更新按键防抖状态机。
 * @param button 按键上下文。
 * @param now 当前 HAL Tick，单位 ms。
 * @param debounce_ms 原始电平必须连续稳定的最短时间。
 * @return 仅在稳定状态发生变化时返回一次按下或释放事件。
 */
ButtonEvent_t Button_Update(Button_t *button, uint32_t now, uint32_t debounce_ms);

#endif /* APP_BUTTON_H */
