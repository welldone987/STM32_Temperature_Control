#pragma once

#include <cstdint>

namespace app {

/*
 * 位带（软件）I2C 主机，驱动 OLED 的 SCL/SDA 两根线。
 *
 * 为什么用位带而不用硬件 I2C1：历史实现即如此，且 OLED 写命令/写数据
 * 是单主机、低速、写为主的场景，位带足够且时序可控；硬件 I2C1 外设
 * 在 .ioc 中已停用。
 *
 * 电气：SCL/SDA 配置为开漏 + 上拉，符合 I2C 总线要求。
 * 时序：由 bit_delay() 的空循环决定，刻意与原驱动验证过的时序一致。
 *
 * 线程安全：无。只允许显示任务独占使用（见 OledDisplay）。
 */
class BitBangI2c {
public:
    BitBangI2c() = default;
    // 驱动代表硬件资源，禁止拷贝，避免两个对象同时操作同一组引脚。
    BitBangI2c(const BitBangI2c&) = delete;
    BitBangI2c& operator=(const BitBangI2c&) = delete;

    // 把 SCL/SDA 初始化为开漏带上拉输出，并释放两根线为高电平（空闲态）。
    // 必须在任何 start()/write_byte() 之前调用一次。
    void configure_pins();

    // I2C 起始信号：SCL 高时 SDA 由高到低。
    void start();

    // I2C 停止信号：SCL 高时 SDA 由低到高，释放总线。
    void stop();

    // 按 MSB 优先发送一个字节，并在第 9 个时钟采样应答位。
    // 返回 true 表示从机应答（ACK），false 表示无应答（NACK）。
    bool write_byte(std::uint8_t byte);

private:
    // 单个位间隔的延时。用 volatile 计数器防止编译器在任何优化等级下
    // 把空循环优化掉，保证时序与显示验证时一致。
    static void bit_delay();
};

}  // namespace app
