/**
 * @file bsp_eeprom.c
 * @brief 使用 STM32F103 片内 Flash 模拟的小型参数存储。
 *
 * 链接脚本已经预留最后 1 KiB Flash 页（0x0800FC00）。应用层会在阈值停止
 * 变化 3 秒后才调用写入，因此采用一页一条记录的简单布局，优先保证代码
 * 容易审查、版本可识别、数据可校验。
 */

#include "bsp_eeprom.h"

#include <stddef.h>
#include <string.h>

/* STM32F103C8T6 最后一页 Flash，禁止链接器在此区域放置程序代码。 */
#define EEPROM_PAGE_ADDRESS        0x0800FC00UL
#define EEPROM_PAGE_SIZE           1024U
#define EEPROM_SETTINGS_MAGIC      0x544D5031UL
#define EEPROM_SETTINGS_VERSION    1U

typedef struct {
    uint32_t magic;        /**< 固定魔数，用于识别本项目参数记录。 */
    uint16_t version;      /**< 存储格式版本，结构变化时必须递增。 */
    uint16_t payload_size; /**< 业务载荷大小，用于发现结构不兼容。 */
    float temp_high;       /**< 高温阈值。 */
    float temp_low;        /**< 低温阈值。 */
    uint16_t crc;          /**< 从 magic 到 temp_low 的 CRC16-CCITT。 */
    uint16_t reserved;     /**< 保持记录按 32 位对齐，便于 Flash 字写入。 */
} EEPROM_Record_t;

/** 在写入前和读取后统一检查业务阈值，防止非法参数进入系统。 */
static uint8_t EEPROM_SettingsInRange(const TempSettings_t *settings)
{
    return ((settings->temp_high >= 0.1f) &&
            (settings->temp_high <= 80.0f) &&
            (settings->temp_low >= 0.0f) &&
            (settings->temp_low < settings->temp_high)) ? 1U : 0U;
}

/**
 * @brief 计算 CRC16-CCITT，初值 0xFFFF，多项式 0x1021。
 *
 * CRC 比简单异或校验更容易发现多位错误，适合容量很小的配置记录。
 */
static uint16_t EEPROM_CalculateCrc16(const uint8_t *data, uint32_t size)
{
    uint16_t crc = 0xFFFFU;
    uint32_t index;
    uint8_t bit;

    for (index = 0U; index < size; index++) {
        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0U; bit < 8U; bit++) {
            crc = ((crc & 0x8000U) != 0U) ?
                  (uint16_t)((crc << 1) ^ 0x1021U) :
                  (uint16_t)(crc << 1);
        }
    }

    return crc;
}

/**
 * @brief 依次检查记录身份、格式、CRC 和业务范围。
 *
 * 只有全部检查通过的数据才会交给应用层；擦除态 0xFF、写入中断或旧版本
 * 记录都会自然地返回无效。
 */
static uint8_t EEPROM_RecordIsValid(const EEPROM_Record_t *record)
{
    TempSettings_t settings;
    uint16_t expected_crc;

    if ((record->magic != EEPROM_SETTINGS_MAGIC) ||
        (record->version != EEPROM_SETTINGS_VERSION) ||
        (record->payload_size != (uint16_t)sizeof(TempSettings_t))) {
        return 0U;
    }

    expected_crc = EEPROM_CalculateCrc16((const uint8_t *)record,
                                          offsetof(EEPROM_Record_t, crc));
    if (record->crc != expected_crc) {
        return 0U;
    }

    settings.temp_high = record->temp_high;
    settings.temp_low = record->temp_low;
    return EEPROM_SettingsInRange(&settings);
}

/** @brief 校验、擦除、写入并回读一条完整设置记录。 */
HAL_StatusTypeDef EEPROM_WriteSettings(const TempSettings_t *settings)
{
    FLASH_EraseInitTypeDef erase = {0};
    EEPROM_Record_t record;
    const uint32_t *words;
    HAL_StatusTypeDef status;
    uint32_t page_error = 0U;
    uint32_t index;

    if ((settings == NULL) || !EEPROM_SettingsInRange(settings)) {
        return HAL_ERROR;
    }

    /* 在 RAM 中完整组装记录并计算 CRC，再开始不可逆的 Flash 擦除。 */
    memset(&record, 0, sizeof(record));
    record.magic = EEPROM_SETTINGS_MAGIC;
    record.version = EEPROM_SETTINGS_VERSION;
    record.payload_size = (uint16_t)sizeof(TempSettings_t);
    record.temp_high = settings->temp_high;
    record.temp_low = settings->temp_low;
    record.crc = EEPROM_CalculateCrc16((const uint8_t *)&record,
                                       offsetof(EEPROM_Record_t, crc));

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = EEPROM_PAGE_ADDRESS;
    erase.NbPages = EEPROM_PAGE_SIZE / FLASH_PAGE_SIZE;

    /* STM32F1 必须先按页擦除，再按 32 位字顺序编程。 */
    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &page_error);

    words = (const uint32_t *)&record;
    for (index = 0U;
         (status == HAL_OK) && (index < (sizeof(record) / sizeof(uint32_t)));
         index++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   EEPROM_PAGE_ADDRESS + index * sizeof(uint32_t),
                                   words[index]);
    }
    /* 无论成功与否都重新锁定 Flash，避免其他代码意外写入。 */
    HAL_FLASH_Lock();

    /* 写入成功后直接从映射地址回读，确认最终记录仍可通过全部校验。 */
    if ((status == HAL_OK) &&
        !EEPROM_RecordIsValid((const EEPROM_Record_t *)EEPROM_PAGE_ADDRESS)) {
        status = HAL_ERROR;
    }

    return status;
}

/** @brief 从固定 Flash 地址读取一条通过完整性检查的设置记录。 */
HAL_StatusTypeDef EEPROM_ReadSettings(TempSettings_t *settings)
{
    const EEPROM_Record_t *record =
        (const EEPROM_Record_t *)EEPROM_PAGE_ADDRESS;

    /* Flash 可直接按内存地址读取，不需要解锁。 */
    if ((settings == NULL) || !EEPROM_RecordIsValid(record)) {
        return HAL_ERROR;
    }

    settings->temp_high = record->temp_high;
    settings->temp_low = record->temp_low;
    return HAL_OK;
}
