#include "storage/settings_store.hpp"

#include <cstring>

#include "main.h"

namespace app {

// 异或校验和：对前 size 个字节逐字节异或。
std::uint8_t SettingsStore::checksum(const std::uint8_t* data, std::size_t size) {
    std::uint8_t sum = 0;
    for (std::size_t i = 0; i < size; ++i) {
        sum ^= data[i];
    }
    return sum;
}

StoreStatus SettingsStore::read(TempSettings& out) const {
    TempSettings stored{};
    // Flash 是内存映射的，直接按地址读出 12 字节。
    std::memcpy(&stored, reinterpret_cast<const void*>(kPageAddress), sizeof(stored));

    // 全 0xFF 表示这一页从未写过（擦除后的默认值）。
    std::uint32_t words[kWordCount];
    std::memcpy(words, &stored, sizeof(words));
    if (words[0] == 0xFFFFFFFF && words[1] == 0xFFFFFFFF && words[2] == 0xFFFFFFFF) {
        return StoreStatus::error;
    }

    // 校验和不匹配说明数据被写坏，拒绝使用。
    if (stored.checksum != checksum(reinterpret_cast<const std::uint8_t*>(&stored),
                                    kChecksumBytes)) {
        return StoreStatus::error;
    }

    // 合理性校验：阈值必须在物理可能范围内，且高阈值严格大于低阈值。
    // 任一条不满足都视为脏数据，让调用方退回默认值。
    if (stored.high < kPlausibleMin || stored.high > kPlausibleMax ||
        stored.low < kPlausibleMin || stored.low > kPlausibleMax ||
        stored.high <= stored.low) {
        return StoreStatus::error;
    }

    out = stored;
    return StoreStatus::ok;
}

StoreStatus SettingsStore::write(TempSettings& settings) const {
    // 写入前先重算校验和（覆盖除 checksum 自身之外的前 8 字节）。
    settings.checksum =
        checksum(reinterpret_cast<const std::uint8_t*>(&settings), kChecksumBytes);

    // Flash 只能按 32 位字编程，把 12 字节结构体复制成 3 个字。
    std::uint32_t words[kWordCount];
    std::memcpy(words, &settings, sizeof(words));

    HAL_FLASH_Unlock();

    // Flash 写入前必须先擦除目标页（NOR Flash 只能 1->0，擦除整页变回 1）。
    FLASH_EraseInitTypeDef erase{};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = kPageAddress;
    erase.NbPages = 1;

    std::uint32_t page_error = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);

    // 擦除成功后逐字写入；任一步失败即停止。
    std::uint32_t address = kPageAddress;
    for (std::size_t i = 0; i < kWordCount && status == HAL_OK; ++i) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, words[i]);
        address += 4;
    }

    HAL_FLASH_Lock();  // 无论成败都要重新上锁，防止误写

    return status == HAL_OK ? StoreStatus::ok : StoreStatus::error;
}

}  // namespace app
