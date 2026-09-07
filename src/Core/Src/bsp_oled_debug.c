/**
 ******************************************************************************
 * @file    main.c
 * @author  fire
 * @version V1.0
 * @date    2024-xx-xx
 * @brief   OLED
 ******************************************************************************
 * @attention
 *
 * 实验平台:野火 F103 STM32 开发板
 * 论坛    :http://www.firebbs.cn
 * 淘宝    :https://fire-stm32.taobao.com
 *
 ******************************************************************************
 */
#include "oled/bsp_oled_debug.h"
#include "oled/bsp_oled_codetab.h"
#include "iic/bsp_iic_debug.h"


extern I2C_HandleTypeDef iic_initstruct;


/* oled写数据 */
void Oled_Write_Data(uint8_t data)
{
#if IIC_SELECT
    HAL_I2C_Mem_Write(&iic_initstruct, OLED_ID, OLED_WR_DATA, I2C_MEMADD_SIZE_8BIT, &data, 1, 0x100);
#else
    IIC_Start();
    IIC_SendByte(OLED_ID);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(OLED_WR_DATA);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(data);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_Stop();
#endif
}

/* oled写命令 */
void Oled_Write_Cmd(uint8_t cmd)
{
#if IIC_SELECT
    HAL_I2C_Mem_Write(&iic_initstruct, OLED_ID, OLED_WR_CMD, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 0x100);
#else
    IIC_Start();
    IIC_SendByte(OLED_ID);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(OLED_WR_CMD);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(cmd);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_Stop();
#endif
}

void OLED_Init(void)
{
    /* 设置显示打开/关闭
     * AE--->显示打开
     * AF--->显示关闭(休眠模式)
     */
    Oled_Write_Cmd(0xAE);

    /* ================== 基本命令表 ===================*/
    /* 设置对比度
     * 0~255：数值越大 亮度越亮
     */
    Oled_Write_Cmd(0x81);
    Oled_Write_Cmd(0xFF);

    /* 使能全屏显示
     * A4--->恢复到RAM内容显示
     * A5--->忽略RAM内容显示
     */
    Oled_Write_Cmd(0xA4);

    /* 设置显示模式
     * A6--->正常显示：0灭1亮
     * A7--->逆显示：1灭0亮
     */
    Oled_Write_Cmd(0xA6);

    /* ================== 滚动命令表 ===================*/
    /* 滚动使能/失能
     * 2E--->失能
     * 2F--->使能
     */
    Oled_Write_Cmd(0x2E);

    /* 七字节命令： 连续水平滚动设置 */

    /* 左/右水平滚动设置
     * 26--->右水平滚动
     * 27--->左水平滚动
     */
    Oled_Write_Cmd(0x26);
    /* 虚拟字节 */
    Oled_Write_Cmd(0x00);
    /* 设置滚动起始页地址 */
    Oled_Write_Cmd(0x00);
    /* 设置滚动间隔 */
    Oled_Write_Cmd(0x03);
    /* 设置滚动结束地址 */
    Oled_Write_Cmd(0x07);
    /* 虚拟字节 */
    Oled_Write_Cmd(0x00);
    Oled_Write_Cmd(0xFF);

    /* =============== 寻址设置命令表 ==================*/

    /* 双字节命令:寄存器寻址模式 */
    Oled_Write_Cmd(0x20);

    /* 10:页寻址模式
     * 01:垂直寻址模式
     * 00:水平寻址模式
     */
    Oled_Write_Cmd(0x10);
    /* 单字节命令:设置页寻址的起始页地址 */
    Oled_Write_Cmd(0xB0);
    /* 单字节命令:设置页寻址的起始列地址低位 */
    Oled_Write_Cmd(0x00);
    /* 单字节命令:设置页寻址的起始列地址高位 */
    Oled_Write_Cmd(0x10);

    /*=============== 硬件配置命令表 ==================*/

    /* 设置显示开始线
     * 0x40~0x7F对应0~63
     */
    Oled_Write_Cmd(0x40);

    /* 设置列重映射
     * A0:addressX--->segX
     * A1:addressX--->seg(127-X)
     */
    Oled_Write_Cmd(0xA1);

    /* 设置多路复用比 */
    Oled_Write_Cmd(0xA8);
    Oled_Write_Cmd(0x3F);

    /* 设置COM输出扫描方向
     * C0：COM0--->COM63(从上往下扫描)
     * C8：COM63--->COM0(从下往上扫描)
     */
    Oled_Write_Cmd(0xC8);

    /* 双字节命令：设置COM显示偏移量 */
    Oled_Write_Cmd(0xD3);
    Oled_Write_Cmd(0x00); /* COM不偏移 */

    /* 双字节命令：配置COM重映射 */
    Oled_Write_Cmd(0xDA);
    Oled_Write_Cmd(0x12);

    /* 双字节命令：设置预充期 */
    Oled_Write_Cmd(0xD9);
    Oled_Write_Cmd(0x22); /* 阶段一2个无效DCLK时钟/阶段二2个无效DCLK时钟 */

    /* 设置VCOMH取消选择电平
     * 00:0.65xVcc
     * 20:0.77xVcc
     * 30:0.83xVcc
     */
    Oled_Write_Cmd(0xDB);
    Oled_Write_Cmd(0x20);

    /* 双字节命令：设置电荷泵 */
    Oled_Write_Cmd(0x8d);
    Oled_Write_Cmd(0x14);

    Oled_Write_Cmd(0xAF);
}

/**
 * @brief  设置光标
 * @param  x,光标x位置
 *		   y,光标y位置
 * @retval 无
 */
void OLED_SetPos(unsigned char x, unsigned char y) // 设置起始点坐标
{
    Oled_Write_Cmd(0xb0 + y);
    Oled_Write_Cmd(((x & 0xf0) >> 4) | 0x10);
    Oled_Write_Cmd((x & 0x0f) | 0x01);
}

/**
 * @brief  填充整个屏幕
 * @param  fill_data:要填充的数据
 * @retval 无
 */
void OLED_Fill(unsigned char fill_data) // 全屏填充
{
    unsigned char m, n;
    for (m = 0; m < 8; m++)
    {
        Oled_Write_Cmd(0xb0 + m); // page0-page1
        Oled_Write_Cmd(0x00);     // low column start address
        Oled_Write_Cmd(0x10);     // high column start address
        for (n = 0; n < 128; n++)
        {
            Oled_Write_Data(fill_data);
        }
    }
}

/**
 * @brief  清屏
 * @param  无
 * @retval 无
 */
void OLED_CLS(void) // 清屏
{
    OLED_Fill(0x00);
}

/**
 * @brief  将OLED从休眠中唤醒
 * @param  无
 * @retval 无
 */
void OLED_ON(void)
{
    Oled_Write_Cmd(0X8D); // 设置电荷泵
    Oled_Write_Cmd(0X14); // 开启电荷泵
    Oled_Write_Cmd(0XAF); // OLED唤醒
}

/**
 * @brief  让OLED休眠 -- 休眠模式下,OLED功耗不到10uA
 * @param  无
 * @retval 无
 */
void OLED_OFF(void)
{
    Oled_Write_Cmd(0X8D); // 设置电荷泵
    Oled_Write_Cmd(0X10); // 关闭电荷泵
    Oled_Write_Cmd(0XAE); // OLED休眠
}

/**
 * @brief 显示codetab.h中的ASCII字符,有6*8和8*16可选择
 * @param  x,y : 起始点坐标(x:0~127, y:0~7);
 *					ch[] :- 要显示的字符串;
 *					textsize : 字符大小(1:6*8 ; 2:8*16)
 * @retval 无
 */
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char textsize)
{
    unsigned char c = 0, i = 0, j = 0;
    switch (textsize)
    {
    case 1:
    {
        while (ch[j] != '\0')
        {
            c = ch[j] - 32;
            if (x > 126)
            {
                x = 0;
                y++;
            }
            OLED_SetPos(x, y);
            for (i = 0; i < 6; i++)
                Oled_Write_Data(F6x8[c][i]);
            x += 6;
            j++;
        }
    }
    break;
    case 2:
    {
        while (ch[j] != '\0')
        {
            c = ch[j] - 32;
            if (x > 120)
            {
                x = 0;
                y++;
            }
            OLED_SetPos(x, y);
            for (i = 0; i < 8; i++)
                Oled_Write_Data(F8X16[c * 16 + i]);
            OLED_SetPos(x, y + 1);
            for (i = 0; i < 8; i++)
                Oled_Write_Data(F8X16[c * 16 + i + 8]);
            x += 8;
            j++;
        }
    }
    break;
    }
}

/**
 * @brief  OLED_ShowCN，显示codetab.h中的汉字,16*16点阵
 * @param  x,y: 起始点坐标(x:0~127, y:0~7); N:汉字在codetab.h中的索引
 *
 * @retval 无
 */
void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char n)
{
    unsigned char wm = 0;
    unsigned int adder = 32 * n;
    OLED_SetPos(x, y);
    for (wm = 0; wm < 16; wm++)
    {
        Oled_Write_Data(F16x16[adder]);
        adder += 1;
    }
    OLED_SetPos(x, y + 1);
    for (wm = 0; wm < 16; wm++)
    {
        Oled_Write_Data(F16x16[adder]);
        adder += 1;
    }
}

/**
 * @brief  OLED_DrawBMP，显示BMP位图
 * @param  x0,y0 :起始点坐标(x0:0~127, y0:0~7);x1,y1 : 起点对角线(结束点)的坐标(x1:1~128,y1:1~8)
 * @retval 无
 */
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char BMP[])
{
    unsigned int j = 0;
    unsigned char x, y;

    if (y1 % 8 == 0)
    {
        y = y1 / 8;
    }
    else
    {
        y = y1 / 8 + 1;
    }
    for (y = y0; y < y1; y++)
    {
        OLED_SetPos(x0, y);
        for (x = x0; x < x1; x++)
        {
            Oled_Write_Data(BMP[j++]);
        }
    }
}
