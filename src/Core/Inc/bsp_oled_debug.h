#ifndef __BSP_OLED_DEBUG_H
#define __BSP_OLED_DEBUG_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

// OLED配置
#define OLED_I2C_ADDR    0x78
#define OLED_WR_CMD      0x00
#define OLED_WR_DATA     0x40

// 函数声明
void Oled_Write_Cmd(uint8_t cmd);
void Oled_Write_Data(uint8_t data);
void OLED_Init(void);
void OLED_CLS(void);
void OLED_ON(void);
void OLED_OFF(void);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_Fill(uint8_t fill_data);
void OLED_FillArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t pattern);
void OLED_ShowChar_6x8(uint8_t x, uint8_t y, uint8_t ch);
void OLED_ShowStr(uint8_t x, uint8_t y, unsigned char *chr, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len);
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t integer_len, uint8_t decimal_len);
void OLED_ShowCN(uint8_t x, uint8_t y, uint8_t n);
void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t BMP[]);
void OLED_Draw_16x16_From_Codetable(uint8_t x, uint8_t y, const uint8_t *data_ptr);
void OLED_Scroll(uint8_t direction, uint8_t start_page, uint8_t end_page, uint8_t scroll_speed);
void OLED_Scroll_Stop(void);
void OLED_Invert(uint8_t invert);
void OLED_SetContrast(uint8_t contrast);
void OLED_ShowProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t progress);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t filled);
void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t length);
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t length);
void OLED_Init_With_Flip(uint8_t flip);
void OLED_Set_Flip(uint8_t flip);
void OLED_Set_Horizontal_Flip(uint8_t flip);
void OLED_Set_Vertical_Flip(uint8_t flip);
void OLED_Set_Flip_Direction(uint8_t h_flip, uint8_t v_flip);
// 辅助函数
uint32_t OLED_Pow(uint32_t m, uint32_t n);
int find_chinese_index(uint8_t Byte1, uint8_t Byte2);

#endif /* __BSP_OLED_DEBUG_H */
