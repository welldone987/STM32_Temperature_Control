#include "display/bit_bang_i2c.hpp"

#include "main.h"

namespace app {

namespace {

// 位延时的循环次数：经验值，与 OLED 验证通过的时序匹配，勿随意改动。
constexpr std::uint8_t kBitDelayIterations = 20;

// 引脚级原语：直接读写 SCL/SDA（引脚宏由 CubeMX 从 .ioc 用户标签生成）。
void scl_high() { HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, GPIO_PIN_SET); }
void scl_low() { HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, GPIO_PIN_RESET); }
void sda_high() { HAL_GPIO_WritePin(OLED_SDA_GPIO_Port, OLED_SDA_Pin, GPIO_PIN_SET); }
void sda_low() { HAL_GPIO_WritePin(OLED_SDA_GPIO_Port, OLED_SDA_Pin, GPIO_PIN_RESET); }
bool sda_read() { return HAL_GPIO_ReadPin(OLED_SDA_GPIO_Port, OLED_SDA_Pin) == GPIO_PIN_SET; }

}  // namespace

void BitBangI2c::bit_delay() {
    // volatile 计数器保证空循环在所有优化等级下都真实执行（见头文件说明）。
    for (volatile std::uint8_t i = 0; i < kBitDelayIterations; ++i) {
    }
}

void BitBangI2c::configure_pins() {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio{};
    gpio.Pin = OLED_SCL_Pin | OLED_SDA_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(OLED_SCL_GPIO_Port, &gpio);

    // 释放总线，进入 I2C 空闲态（两线均为高）。
    scl_high();
    sda_high();
}

void BitBangI2c::start() {
    sda_high();
    scl_high();
    bit_delay();
    sda_low();  // SCL 高时 SDA 下降沿 = 起始信号
    bit_delay();
    scl_low();
    bit_delay();
}

void BitBangI2c::stop() {
    scl_low();
    bit_delay();
    sda_low();
    bit_delay();
    scl_high();
    bit_delay();
    sda_high();  // SCL 高时 SDA 上升沿 = 停止信号
    bit_delay();
}

bool BitBangI2c::write_byte(std::uint8_t byte) {
    // 逐位发送 8 个数据位（MSB 优先）。
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
        if (byte & 0x80) {
            sda_high();
        } else {
            sda_low();
        }
        bit_delay();

        scl_high();  // 数据在 SCL 高电平期间被从机采样
        bit_delay();

        scl_low();
        bit_delay();

        byte <<= 1;
    }

    // 第 9 个时钟：释放 SDA 交由从机驱动，在 SCL 高时采样应答位。
    // 从机拉低 SDA 表示 ACK；保持高则为 NACK。
    sda_high();
    bit_delay();

    scl_high();
    bit_delay();

    const bool acknowledged = !sda_read();

    scl_low();
    bit_delay();

    return acknowledged;
}

}  // namespace app
