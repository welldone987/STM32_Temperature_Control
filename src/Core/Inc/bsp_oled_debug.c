#include "bsp_oled_debug.h"
#include "bsp_iic_debug.h"
#include "bsp_oled_codetab.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// =================================================================
// OLED 基础I/O函数
// =================================================================

/**
 * @brief 写命令到OLED
 * @param cmd: 命令字节
 */
void Oled_Write_Cmd(uint8_t cmd) 
{
    IIC_Start();
    
    // 发送设备地址
    IIC_SendByte(OLED_I2C_ADDR);   
    if (IIC_Wait_ACK()) { IIC_Stop(); return; }

    // 发送控制字节（命令模式）
    IIC_SendByte(OLED_WR_CMD);   
    if (IIC_Wait_ACK()) { IIC_Stop(); return; }

    // 发送命令
    IIC_SendByte(cmd);           
    if (IIC_Wait_ACK()) { IIC_Stop(); return; }

    IIC_Stop();
}

/**
 * @brief 写数据到OLED
 * @param data: 数据字节
 */
void Oled_Write_Data(uint8_t data) 
{
    IIC_Start();
    
    // 发送设备地址
    IIC_SendByte(OLED_I2C_ADDR);   
    if (IIC_Wait_ACK()) { IIC_Stop(); return; }

    // 发送控制字节（数据模式）
    IIC_SendByte(OLED_WR_DATA);  
    if (IIC_Wait_ACK()) { IIC_Stop(); return; }

    // 发送数据
    IIC_SendByte(data);          
    if (IIC_Wait_ACK()) { IIC_Stop(); return; }

    IIC_Stop();
}

// =================================================================
// OLED 控制函数
// =================================================================

/**
 * @brief OLED初始化
 * @param flip: 是否翻转显示 (0:正常, 1:翻转)
 */
void OLED_Init_With_Flip(uint8_t flip)
{
    IIC_GPIO_Config();
    HAL_Delay(100);  // 重要：等待OLED上电稳定

    // 关闭显示
    Oled_Write_Cmd(0xAE);
    
    // 设置显示时钟分频比/振荡器频率
    Oled_Write_Cmd(0xD5);
    Oled_Write_Cmd(0x80);
    
    // 设置多路复用率
    Oled_Write_Cmd(0xA8);
    Oled_Write_Cmd(0x3F);  // 128x64: 0x3F, 128x32: 0x1F
    
    // 设置显示偏移
    Oled_Write_Cmd(0xD3);
    Oled_Write_Cmd(0x00);
    
    // 设置显示开始行
    Oled_Write_Cmd(0x40);
    
    // 设置电荷泵
    Oled_Write_Cmd(0x8D);
    Oled_Write_Cmd(0x14);  // 开启电荷泵
    
    // 设置内存地址模式
    Oled_Write_Cmd(0x20);
    Oled_Write_Cmd(0x00);  // 水平地址模式
    
    // 设置段重映射 - 根据flip参数决定
    if (flip) {
        Oled_Write_Cmd(0xA1);  // 0xA1: 段重映射 (左右翻转)
    } else {
        Oled_Write_Cmd(0xA0);  // 0xA0: 正常
    }
    
    // 设置COM扫描方向 - 根据flip参数决定
    if (flip) {
        Oled_Write_Cmd(0xC8);  // 0xC8: COM扫描方向反向 (上下翻转)
    } else {
        Oled_Write_Cmd(0xC0);  // 0xC0: 正常
    }
    
    // 设置COM引脚硬件配置
    Oled_Write_Cmd(0xDA);
    Oled_Write_Cmd(0x12);  // 128x64: 0x12, 128x32: 0x02
    
    // 设置对比度
    Oled_Write_Cmd(0x81);
    Oled_Write_Cmd(0xCF);  // 对比度值
    
    // 设置预充电周期
    Oled_Write_Cmd(0xD9);
    Oled_Write_Cmd(0xF1);
    
    // 设置VCOMH取消选择级别
    Oled_Write_Cmd(0xDB);
    Oled_Write_Cmd(0x40);
    
    // 整个显示打开
    Oled_Write_Cmd(0xA4);
    
    // 设置正常/反色显示
    Oled_Write_Cmd(0xA6);  // 0xA6: 正常, 0xA7: 反色
    
    // 设置显示振荡器频率
    Oled_Write_Cmd(0xD5);
    Oled_Write_Cmd(0x80);
    
    // 打开显示
    Oled_Write_Cmd(0xAF);
    
    HAL_Delay(100);
    
    // 清屏
    OLED_CLS();
}

/**
 * @brief OLED初始化 (保持原有接口，默认不翻转)
 */
void OLED_Init(void)
{
    OLED_Init_With_Flip(0);  // 默认不翻转
}
/**
 * @brief 设置OLED显示位置
 * @param x: 列地址 (0-127)
 * @param y: 页地址 (0-7)
 */
void OLED_SetPos(uint8_t x, uint8_t y) 
{ 
    // Set Page Address (0xB0 to 0xB7)
    Oled_Write_Cmd(0xB0 + y);       
    // Set Column Address
    Oled_Write_Cmd(((x & 0xF0) >> 4) | 0x10); 
    Oled_Write_Cmd(x & 0x0F);       
}

/**
 * @brief 清屏
 */
void OLED_CLS(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        OLED_SetPos(0, page);
        for (uint8_t col = 0; col < 128; col++)
            Oled_Write_Data(0x00);
    }
}

/**
 * @brief 打开OLED显示
 */
void OLED_ON(void)
{
    Oled_Write_Cmd(0x8D); 
    Oled_Write_Cmd(0x14); 
    Oled_Write_Cmd(0xAF); 
}

/**
 * @brief 关闭OLED显示
 */
void OLED_OFF(void)
{
    Oled_Write_Cmd(0x8D); 
    Oled_Write_Cmd(0x10); 
    Oled_Write_Cmd(0xAE); 
}
/**
 * @brief 设置显示方向
 * @param flip: 0-正常, 1-翻转
 */
void OLED_Set_Flip(uint8_t flip)
{
    // 设置段重映射
    if (flip) {
        Oled_Write_Cmd(0xA1);  // 段重映射 (左右翻转)
    } else {
        Oled_Write_Cmd(0xA0);  // 正常
    }
    
    // 设置COM扫描方向
    if (flip) {
        Oled_Write_Cmd(0xC8);  // COM扫描方向反向 (上下翻转)
    } else {
        Oled_Write_Cmd(0xC0);  // 正常
    }
    
    // 重新设置显示开始行，确保显示正确
    Oled_Write_Cmd(0x40);
}

/**
 * @brief 水平翻转显示
 * @param flip: 0-正常, 1-水平翻转
 */
void OLED_Set_Horizontal_Flip(uint8_t flip)
{
    if (flip) {
        Oled_Write_Cmd(0xA1);  // 段重映射 (左右翻转)
    } else {
        Oled_Write_Cmd(0xA0);  // 正常
    }
}

/**
 * @brief 垂直翻转显示
 * @param flip: 0-正常, 1-垂直翻转
 */
void OLED_Set_Vertical_Flip(uint8_t flip)
{
    if (flip) {
        Oled_Write_Cmd(0xC8);  // COM扫描方向反向 (上下翻转)
    } else {
        Oled_Write_Cmd(0xC0);  // 正常
    }
    
    // 重新设置显示开始行
    Oled_Write_Cmd(0x40);
}

/**
 * @brief 同时设置水平和垂直翻转
 * @param h_flip: 水平翻转 (0-正常, 1-翻转)
 * @param v_flip: 垂直翻转 (0-正常, 1-翻转)
 */
void OLED_Set_Flip_Direction(uint8_t h_flip, uint8_t v_flip)
{
    // 水平翻转
    if (h_flip) {
        Oled_Write_Cmd(0xA1);  // 段重映射
    } else {
        Oled_Write_Cmd(0xA0);  // 正常
    }
    
    // 垂直翻转
    if (v_flip) {
        Oled_Write_Cmd(0xC8);  // COM扫描方向反向
    } else {
        Oled_Write_Cmd(0xC0);  // 正常
    }
    
    // 重新设置显示开始行
    Oled_Write_Cmd(0x40);
}
/**
 * @brief 填充整个屏幕
 * @param fill_data: 填充数据
 */
void OLED_Fill(uint8_t fill_data)
{ 
    for (uint8_t page = 0; page < 8; page++)
    {
        OLED_SetPos(0, page); 
        for (uint8_t col = 0; col < 128; col++)
        {
            Oled_Write_Data(fill_data);
        }
    }
}

/**
 * @brief 填充指定区域
 * @param x: 起始列
 * @param y: 起始页
 * @param width: 区域宽度
 * @param height: 区域高度（页数）
 * @param pattern: 填充模式
 */
void OLED_FillArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t pattern)
{
    for (uint8_t page = y; page < y + height; page++) {
        if (page > 7) break; // 防止越界
        
        OLED_SetPos(x, page);
        for (uint8_t col = 0; col < width; col++) {
            if (x + col > 127) break; // 防止越界
            Oled_Write_Data(pattern);
        }
    }
}

// =================================================================
// 文本显示函数
// =================================================================

/**
 * @brief 显示6x8 ASCII字符
 * @param x: 起始列
 * @param y: 起始页
 * @param ch: 要显示的字符
 */
void OLED_ShowChar_6x8(uint8_t x, uint8_t y, uint8_t ch)
{
    // 检查字符是否在可显示范围内 (空格到波浪线)
    if (ch < ' ' || ch > '~') {
        ch = '?'; // 不可显示字符显示问号
    }
    
    uint8_t c = ch - ' '; // 空格是第一个字符，索引0
    if (c > 94) c = 0; // 防止越界
    
    if (x > 122) {
        x = 0;
        y = y + 1; 
    }
    
    OLED_SetPos(x, y);
    for (uint8_t i = 0; i < 6; i++) {
        Oled_Write_Data(OLED_F6x8[c][i]); 
    }
}

/**
 * @brief 显示字符串
 * @param x: 起始列
 * @param y: 起始页
 * @param chr: 字符串指针
 * @param size: 字体大小 (1:6x8, 2:8x16)
 */
void OLED_ShowStr(uint8_t x, uint8_t y, unsigned char *chr, uint8_t size) 
{
    if (chr == NULL) return;
    
    while (*chr != '\0') {
        if (x > 122) { // 换行检查
            x = 0;
            y++;
            if (y > 7) y = 7; // 防止越界
        }
        
        if (size == 1) { // 6x8字体
            OLED_ShowChar_6x8(x, y, *chr);
            x += 6;
        } else if (size == 2) { // 8x16字体
            // 这里可以添加8x16字体支持
            x += 8;
        }
        
        chr++;
    }
}

/**
 * @brief 显示数字
 * @param x: 起始列
 * @param y: 起始页
 * @param num: 数字
 * @param len: 数字长度
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len)
{
    uint8_t t, temp;
    uint8_t enShow = 0;
    
    for (t = 0; t < len; t++) {
        temp = (num / OLED_Pow(10, len - t - 1)) % 10;
        if (enShow == 0 && t < (len - 1)) {
            if (temp == 0) {
                OLED_ShowChar_6x8(x + 6 * t, y, ' ');
                continue;
            } else enShow = 1;
        }
        OLED_ShowChar_6x8(x + 6 * t, y, temp + '0');
    }
}

/**
 * @brief 计算幂
 * @param m: 底数
 * @param n: 指数
 * @return 结果
 */
uint32_t OLED_Pow(uint32_t m, uint32_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

// =================================================================
// 图形显示函数
// =================================================================

/**
 * @brief 显示16x16汉字
 * @param x: 起始列
 * @param y: 起始页
 * @param n: 汉字索引
 */
void OLED_ShowCN(uint8_t x, uint8_t y, uint8_t n)
{
    if (n < CN_WORD_COUNT) {
        OLED_Draw_16x16_From_Codetable(x, y, OLED_CN_FONT_16x16[n]);
    }
}

/**
 * @brief 从字库绘制16x16汉字
 * @param x: 起始列
 * @param y: 起始页
 * @param data_ptr: 字库数据指针
 */
void OLED_Draw_16x16_From_Codetable(uint8_t x, uint8_t y, const uint8_t *data_ptr)
{
    uint8_t i;

    // 显示汉字下半部分（8-15行）
    OLED_SetPos(x, y);
    for(i = 16; i < 32; i++)
        Oled_Write_Data(data_ptr[i]);

    // 显示汉字上半部分（0-7行）
    OLED_SetPos(x, y + 1);
    for(i = 0; i < 16; i++)
        Oled_Write_Data(data_ptr[i]);
}

/**
 * @brief 显示位图
 * @param x0: 起始列
 * @param y0: 起始页
 * @param x1: 结束列
 * @param y1: 结束页
 * @param BMP: 位图数据
 */
void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t BMP[]) 
{
    uint32_t j = 0;
    uint8_t x, y;
    
    if (x1 > 128) x1 = 128;
    if (y1 > 8) y1 = 8;
    
    for (y = y0; y < y1; y++)
    {
        OLED_SetPos(x0, y);
        for (x = x0; x < x1; x++)
        {
            Oled_Write_Data(BMP[j++]);
        }
    }
}

/**
 * @brief 显示有符号数字
 * @param x: 起始列
 * @param y: 起始页
 * @param num: 数字（支持负数）
 * @param len: 数字长度
 */
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len)
{
    if (num < 0) {
        OLED_ShowChar_6x8(x, y, '-');
        num = -num;
        x += 6;
    } else {
        OLED_ShowChar_6x8(x, y, '+');
        x += 6;
    }
    OLED_ShowNum(x, y, num, len);
}

/**
 * @brief 显示浮点数
 * @param x: 起始列
 * @param y: 起始页
 * @param num: 浮点数
 * @param integer_len: 整数部分长度
 * @param decimal_len: 小数部分长度
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t integer_len, uint8_t decimal_len)
{
    if (num < 0) {
        OLED_ShowChar_6x8(x, y, '-');
        num = -num;
        x += 6;
    }
    
    // 显示整数部分
    uint32_t integer_part = (uint32_t)num;
    OLED_ShowNum(x, y, integer_part, integer_len);
    x += 6 * integer_len;
    
    // 显示小数点
    OLED_ShowChar_6x8(x, y, '.');
    x += 6;
    
    // 显示小数部分
    float decimal = num - integer_part;
    for (uint8_t i = 0; i < decimal_len; i++) {
        decimal *= 10;
        uint8_t digit = (uint8_t)decimal;
        OLED_ShowChar_6x8(x + 6 * i, y, digit + '0');
        decimal -= digit;
    }
}

// =================================================================
// 高级功能函数
// =================================================================

/**
 * @brief 滚动显示
 * @param direction: 滚动方向 (0:向右, 1:向左)
 * @param start_page: 起始页
 * @param end_page: 结束页
 * @param scroll_speed: 滚动速度
 */
void OLED_Scroll(uint8_t direction, uint8_t start_page, uint8_t end_page, uint8_t scroll_speed)
{
    Oled_Write_Cmd(0x2E); // 关闭滚动
    
    Oled_Write_Cmd(0x2A); // 29h为水平滚动，2Ah为垂直和水平滚动
    Oled_Write_Cmd(0x00); // 虚拟字节
    Oled_Write_Cmd(start_page); // 起始页
    Oled_Write_Cmd(scroll_speed); // 滚动时间间隔
    Oled_Write_Cmd(end_page); // 结束页
    Oled_Write_Cmd(0x00); // 虚拟字节
    Oled_Write_Cmd(0xFF); // 虚拟字节
    
    if (direction == 0) {
        Oled_Write_Cmd(0x27); // 向右滚动
    } else {
        Oled_Write_Cmd(0x26); // 向左滚动
    }
    
    Oled_Write_Cmd(0x2F); // 开启滚动
}

/**
 * @brief 停止滚动
 */
void OLED_Scroll_Stop(void)
{
    Oled_Write_Cmd(0x2E); // 关闭滚动
}

/**
 * @brief 反色显示
 * @param invert: 是否反色 (1:反色, 0:正常)
 */
void OLED_Invert(uint8_t invert)
{
    if (invert) {
        Oled_Write_Cmd(0xA7); // 反色显示
    } else {
        Oled_Write_Cmd(0xA6); // 正常显示
    }
}

/**
 * @brief 设置对比度
 * @param contrast: 对比度值 (0-255)
 */
void OLED_SetContrast(uint8_t contrast)
{
    Oled_Write_Cmd(0x81); // 设置对比度
    Oled_Write_Cmd(contrast);
}

// =================================================================
// 辅助函数
// =================================================================

/**
 * @brief 查找汉字索引
 * @param Byte1: 汉字第一个字节
 * @param Byte2: 汉字第二个字节
 * @return 汉字索引，找不到返回-1
 */
int find_chinese_index(uint8_t Byte1, uint8_t Byte2) 
{
    uint16_t target_code = (uint16_t)((Byte1 << 8) | Byte2); 
    
    for (int i = 0; i < CN_WORD_COUNT; i++) {
        if (target_code == CHN_CODE_LIST[i]) {
            return i; 
        }
    }
    return -1; 
}

/**
 * @brief 显示进度条
 * @param x: 起始列
 * @param y: 起始页
 * @param width: 进度条宽度
 * @param height: 进度条高度
 * @param progress: 进度 (0-100)
 */
void OLED_ShowProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t progress)
{
    if (progress > 100) progress = 100;
    
    // 绘制边框
    OLED_DrawRect(x, y, width, height, 1);
    
    // 计算填充宽度
    uint8_t fill_width = (width - 2) * progress / 100;
    
    // 填充进度
    if (fill_width > 0) {
        OLED_FillArea(x + 1, y + 1, fill_width, height - 2, 0xFF);
    }
}

/**
 * @brief 绘制矩形
 * @param x: 起始列
 * @param y: 起始页
 * @param width: 宽度
 * @param height: 高度
 * @param filled: 是否填充 (1:填充, 0:空心)
 */

void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t filled)
{
    if (filled) {
        OLED_FillArea(x, y, width, height, 0xFF);
    } else {
        // 绘制上边框
        OLED_FillArea(x, y, width, 1, 0xFF);
        // 绘制下边框
        OLED_FillArea(x, y + height - 1, width, 1, 0xFF);
        // 绘制左边框
        OLED_FillArea(x, y, 1, height, 0xFF);
        // 绘制右边框
        OLED_FillArea(x + width - 1, y, 1, height, 0xFF);
    }
}

/**
 * @brief 绘制水平线
 * @param x: 起始列
 * @param y: 起始页
 * @param length: 长度
 */
void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t length)
{
    OLED_FillArea(x, y, length, 1, 0xFF);
}

/**
 * @brief 绘制垂直线
 * @param x: 起始列
 * @param y: 起始页
 * @param length: 长度
 */
void OLED_DrawVLine(uint8_t x, uint8_t y, uint8_t length)
{
    OLED_FillArea(x, y, 1, length, 0xFF);
}
