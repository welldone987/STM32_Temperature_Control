#include "display/oled_display.hpp"

#include "display/font_6x8.hpp"
#include "main.h"

namespace app {

// SSD1306 的一次"写"固定是一帧 I2C：[器件地址][控制字节][1 个字节]。
// 控制字节决定这个字节是命令（kControlCmd）还是显存数据（kControlData）。
// 任何一步无应答就提前 stop 放弃，避免在设备掉线时挂死总线。

void OledDisplay::write_cmd(std::uint8_t cmd) {
    bus_.start();
    if (!bus_.write_byte(kI2cAddress)) {
        bus_.stop();
        return;
    }
    if (!bus_.write_byte(kControlCmd)) {
        bus_.stop();
        return;
    }
    bus_.write_byte(cmd);
    bus_.stop();
}

void OledDisplay::write_data(std::uint8_t data) {
    bus_.start();
    if (!bus_.write_byte(kI2cAddress)) {
        bus_.stop();
        return;
    }
    if (!bus_.write_byte(kControlData)) {
        bus_.stop();
        return;
    }
    bus_.write_byte(data);
    bus_.stop();
}

void OledDisplay::init(bool flip) {
    bus_.configure_pins();
    HAL_Delay(100);  // 上电后等待面板内部稳定

    write_cmd(0xAE);  // 关显示（配置期间先熄灭）

    write_cmd(0xD5);  // 时钟分频 / 振荡频率
    write_cmd(0x80);

    write_cmd(0xA8);  // 多路复用比（扫描行数）
    write_cmd(0x3F);  // 64 行（128x64 屏）

    write_cmd(0xD3);  // 显示偏移
    write_cmd(0x00);

    write_cmd(0x40);  // 起始行 = 0

    write_cmd(0x8D);  // 电荷泵
    write_cmd(0x14);  // 使能（OLED 需要内部升压）

    write_cmd(0x20);  // 显存寻址模式
    write_cmd(0x00);  // 水平寻址（写完一行自动前进）

    // 翻转控制：同时改段重映射和 COM 扫描方向才能整体旋转 180 度。
    write_cmd(flip ? 0xA1 : 0xA0);  // 段重映射
    write_cmd(flip ? 0xC8 : 0xC0);  // COM 扫描方向

    write_cmd(0xDA);  // COM 引脚硬件配置
    write_cmd(0x12);  // 128x64 对应的配置

    write_cmd(0x81);  // 对比度
    write_cmd(0xCF);

    write_cmd(0xD9);  // 预充电周期
    write_cmd(0xF1);

    write_cmd(0xDB);  // VCOMH 取消选择电平
    write_cmd(0x40);

    write_cmd(0xA4);  // 输出跟随显存内容（非全屏点亮测试模式）
    write_cmd(0xA6);  // 正常配色（非反色）

    write_cmd(0xAF);  // 开显示

    HAL_Delay(100);
    clear();
}

void OledDisplay::set_pos(std::uint8_t x, std::uint8_t y) {
    write_cmd(static_cast<std::uint8_t>(0xB0 + y));                  // 页地址
    write_cmd(static_cast<std::uint8_t>(((x & 0xF0) >> 4) | 0x10));  // 列高 4 位
    write_cmd(static_cast<std::uint8_t>(x & 0x0F));                  // 列低 4 位
}

void OledDisplay::clear() {
    for (std::uint8_t page = 0; page < kPages; ++page) {
        set_pos(0, page);
        for (std::uint8_t col = 0; col < kColumns; ++col) {
            write_data(0x00);
        }
    }
}

void OledDisplay::fill_area(std::uint8_t x, std::uint8_t y, std::uint8_t width,
                            std::uint8_t height, std::uint8_t pattern) {
    for (std::uint8_t page = y; page < static_cast<std::uint8_t>(y + height); ++page) {
        if (page >= kPages) {
            break;  // 越界裁剪：页超界即停
        }
        set_pos(x, page);
        for (std::uint8_t col = 0; col < width; ++col) {
            if (static_cast<std::uint16_t>(x) + col >= kColumns) {
                break;  // 越界裁剪：列超界即停
            }
            write_data(pattern);
        }
    }
}

void OledDisplay::show_char(std::uint8_t x, std::uint8_t y, char ch) {
    char glyph = ch;
    if (glyph < ' ' || glyph > '~') {
        glyph = '?';  // 不可打印字符统一显示问号
    }

    // 6x8 字体表以空格(0x20)为第 0 个字形。
    std::size_t index = static_cast<std::size_t>(glyph - ' ');
    if (index >= kFontGlyphCount) {
        index = 0;
    }

    set_pos(x, y);
    for (std::uint8_t i = 0; i < kFontGlyphBytes; ++i) {
        write_data(kFont6x8[index][i]);
    }
}

void OledDisplay::show_text(std::uint8_t x, std::uint8_t y, std::string_view text) {
    for (const char ch : text) {
        if (x > kColumns - 6) {  // 剩余宽度放不下一个字宽(6列)，先换行
            x = 0;
            ++y;
            if (y >= kPages) {
                y = kPages - 1;  // 已到最底行，钳制住不再下移
            }
        }
        show_char(x, y, ch);
        x = static_cast<std::uint8_t>(x + 6);  // 每个字符占 6 列
    }
}

}  // namespace app
