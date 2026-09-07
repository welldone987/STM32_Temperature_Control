#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include "stm32f1xx_hal.h"

typedef struct {
    float temp_high;  /**< 高温报警阈值，单位 ℃。 */
    float temp_low;   /**< 低温报警阈值，单位 ℃。 */
} TempSettings_t;

/**
 * @brief 擦除预留 Flash 页并写入一条带 CRC16 的阈值记录。
 * @param settings 待保存的高低温阈值。
 * @return HAL_OK 表示写入并回读校验成功。
 */
HAL_StatusTypeDef EEPROM_WriteSettings(const TempSettings_t *settings);

/**
 * @brief 从预留 Flash 页读取并校验阈值记录。
 * @param settings 有效时写入读取结果。
 * @return 记录为空、版本不匹配、CRC 错误或阈值越界时返回 HAL_ERROR。
 */
HAL_StatusTypeDef EEPROM_ReadSettings(TempSettings_t *settings);

#endif /* BSP_EEPROM_H */
