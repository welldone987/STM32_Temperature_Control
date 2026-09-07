#ifndef __BSP_OLED_CODETAB_H
#define __BSP_OLED_CODETAB_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

// 汉字数量定义
#define CN_WORD_COUNT 5  // 根据实际汉字数量修改

// 汉字代码列表
extern const uint16_t CHN_CODE_LIST[CN_WORD_COUNT];

// 6x8 ASCII字体
extern const unsigned char OLED_F6x8[][6];

// 16x16 中文字库
extern const unsigned char OLED_CN_FONT_16x16[][32];

// 函数声明
int find_chinese_index(uint8_t Byte1, uint8_t Byte2);

#endif /* __BSP_OLED_CODETAB_H */
