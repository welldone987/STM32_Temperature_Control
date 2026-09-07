#pragma once

#include <cstddef>
#include <cstdint>

namespace app {

/*
 * 持久化的温度阈值，存放在 Flash 最后一页。
 *
 * 布局必须与历史已保存的设备保持二进制兼容（升级固件不能读坏旧数据）：
 *   两个 float（高/低阈值）+ 1 字节校验和（覆盖前 8 字节）+ 3 字节填充。
 * packed 去掉对齐填充，确保恰好 12 字节，与 Flash 里的物理布局一致。
 */
struct TempSettings {
    float high;
    float low;
    std::uint8_t checksum;
    std::uint8_t reserved[3];
} __attribute__((packed));

// 防止有人改结构体导致与已保存数据不兼容。
static_assert(sizeof(TempSettings) == 12, "flash layout must stay 12 bytes");

enum class StoreStatus : std::uint8_t { ok, error };

/*
 * 基于 Flash 的设置存储。
 *
 * 为什么用 Flash 模拟：F103C8 没有 EEPROM 外设，只能占用一页 Flash。
 * 代价：写之前必须整页擦除，擦一次要几十毫秒，且有擦写寿命——
 * 所以调用方（存储任务）做了 3 秒防抖，避免频繁擦写。本类的所有操作
 * 都是同步阻塞的，务必只从低优先级上下文（存储任务）调用。
 */
class SettingsStore {
public:
    // 从 Flash 读设置。以下任一情况返回 error：
    // 页是空白(全 0xFF)、校验和不匹配、数值超出合理范围（防脏数据）。
    StoreStatus read(TempSettings& out) const;

    // 擦除设置页并写入给定设置（内部会重算 checksum 字段）。
    StoreStatus write(TempSettings& settings) const;

private:
    // 简单异或校验和（对数据前 size 字节）。
    static std::uint8_t checksum(const std::uint8_t* data, std::size_t size);

    static constexpr std::uint32_t kPageAddress = 0x0800FC00;  // 最后 1K 页
    static constexpr std::size_t kChecksumBytes = 8;  // 参与校验的字节数
    static constexpr std::size_t kWordCount = 3;      // 12 字节 = 3 个 32 位字
    // 读回值的合理性校验范围：超出说明 Flash 数据损坏，宁可用默认值。
    static constexpr float kPlausibleMin = 1.0f;
    static constexpr float kPlausibleMax = 80.0f;
};

}  // namespace app
