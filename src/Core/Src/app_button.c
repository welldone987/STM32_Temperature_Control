/**
 * @file app_button.c
 * @brief 与有效电平无关、基于时间确认的 GPIO 按键防抖模块。
 *
 * 防抖模块同时保存“原始状态”和“稳定状态”。原始状态发生变化后必须连续
 * 保持 debounce_ms，才会更新稳定状态并产生一次边沿事件。
 */

#include "app_button.h"

/** 将 GPIO 电气电平统一转换成 0/1 的“是否按下”逻辑状态。 */
static uint8_t Button_ReadPressed(const Button_t *button)
{
    return (HAL_GPIO_ReadPin(button->port, button->pin) == button->active_level) ? 1U : 0U;
}

void Button_Init(Button_t *button,
                 GPIO_TypeDef *port,
                 uint16_t pin,
                 GPIO_PinState active_level,
                 uint32_t now)
{
    uint8_t pressed;

    if (button == NULL) {
        return;
    }

    /* 初始化时直接接受当前状态，避免上电后凭空产生一次按下事件。 */
    button->port = port;
    button->pin = pin;
    button->active_level = active_level;
    pressed = Button_ReadPressed(button);
    button->raw_pressed = pressed;
    button->stable_pressed = pressed;
    button->raw_changed_at = now;
}

ButtonEvent_t Button_Update(Button_t *button, uint32_t now, uint32_t debounce_ms)
{
    uint8_t pressed;

    if (button == NULL) {
        return BUTTON_EVENT_NONE;
    }

    pressed = Button_ReadPressed(button);
    /* 原始状态一旦变化，就从当前时刻重新开始稳定计时。 */
    if (pressed != button->raw_pressed) {
        button->raw_pressed = pressed;
        button->raw_changed_at = now;
    }

    /*
     * 只有原始状态与稳定状态不同，且原始状态已保持足够长时间，才确认边沿。
     * 使用无符号 Tick 差值，可自然处理 HAL Tick 回绕。
     */
    if ((pressed != button->stable_pressed) &&
        ((uint32_t)(now - button->raw_changed_at) >= debounce_ms)) {
        button->stable_pressed = pressed;
        return pressed ? BUTTON_EVENT_PRESSED : BUTTON_EVENT_RELEASED;
    }

    return BUTTON_EVENT_NONE;
}
