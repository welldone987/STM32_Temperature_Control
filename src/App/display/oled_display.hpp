#pragma once

#include <cstdint>
#include <string_view>

#include "display/bit_bang_i2c.hpp"

namespace app {

/*
 * SSD1306 128x64 OLED 显示器，走位带 I2C（见 BitBangI2c）。
 *
 * 坐标模型：x 为列（0-127，像素），y 为"页"（0-7），一页高 8 像素；
 * 6x8 字体正好占一页高。这是 SSD1306 的原生寻址方式。
 *
 * 线程安全：无。整机只有显示任务可以操作本对象（OLED 总线与刷新都由
 * 显示任务独占），其他任务要看屏幕请通过快照队列把数据交给显示任务。
 */
class OledDisplay {
public:
    OledDisplay() = default;
    // 显示器是独占硬件资源，禁止拷贝。
    OledDisplay(const OledDisplay&) = delete;
    OledDisplay& operator=(const OledDisplay&) = delete;

    // 执行完整的 SSD1306 初始化序列并清屏。
    // flip=true 时把输出旋转 180 度——本板 OLED 的安装方向需要翻转。
    void init(bool flip);

    // 全屏熄灭（写入全 0）。
    void clear();

    // 用指定图案填充一个矩形区域（用于局部擦除/绘制）。
    // x=起始列，y=起始页，width=列数，height=页数，pattern=填充字节。
    // 超出屏幕的部分会被自动裁剪，不会越界写坏显存。
    void fill_area(std::uint8_t x, std::uint8_t y, std::uint8_t width,
                   std::uint8_t height, std::uint8_t pattern);

    // 用 6x8 字体渲染一段文本。
    // 到达右边界时自动换到下一行；已到最后一行则钳制不再下移。
    // 不可打印字符（<0x20 或 >0x7E）会被替换成 '?'。
    void show_text(std::uint8_t x, std::uint8_t y, std::string_view text);

private:
    static constexpr std::uint8_t kColumns = 128;      // 水平像素数
    static constexpr std::uint8_t kPages = 8;          // 垂直页数（64/8）
    static constexpr std::uint8_t kI2cAddress = 0x78;  // SSD1306 写地址
    // SSD1306 的控制字节：0x00 表示后续字节是命令，0x40 表示是显存数据。
    static constexpr std::uint8_t kControlCmd = 0x00;
    static constexpr std::uint8_t kControlData = 0x40;

    // 向 SSD1306 写一条命令 / 一个显存数据字节（内部处理 I2C 帧）。
    void write_cmd(std::uint8_t cmd);
    void write_data(std::uint8_t data);

    // 把显存写指针定位到 (列, 页)。
    void set_pos(std::uint8_t x, std::uint8_t y);

    // 渲染单个 6x8 字符，调用前不做边界检查（由 show_text 负责）。
    void show_char(std::uint8_t x, std::uint8_t y, char ch);

    BitBangI2c bus_;  // 本显示器独占的位带 I2C 总线
};

}  // namespace app
