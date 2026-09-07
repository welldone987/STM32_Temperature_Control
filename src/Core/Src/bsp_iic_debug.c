#include "bsp_iic_debug.h"
#include "stm32f1xx_hal.h"

// =================================================================
// 辅助函数
// =================================================================

static void IIC_Delay(void)
{
    uint8_t i;
    for (i = 0; i < 20; i++) 
        ;
}

// =================================================================
// I2C 基础函数实现
// =================================================================

void IIC_Start(void) 
{
    IIC_SDA_1(); 
    IIC_SCL_1(); 
    IIC_Delay();
    IIC_SDA_0(); 
    IIC_Delay();
    IIC_SCL_0(); 
    IIC_Delay();
}

void IIC_Stop(void)
{
    IIC_SCL_0(); 
    IIC_Delay();
    IIC_SDA_0(); 
    IIC_Delay();
    IIC_SCL_1(); 
    IIC_Delay();
    IIC_SDA_1(); 
    IIC_Delay();
}

void IIC_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            IIC_SDA_1();
        else
            IIC_SDA_0();
        IIC_Delay(); 

        IIC_SCL_1();
        IIC_Delay();
        
        IIC_SCL_0();
        IIC_Delay();
        
        byte <<= 1; 
    }
    
    IIC_SDA_1(); 
    IIC_Delay();
}

uint8_t IIC_Wait_ACK(void)
{
    uint8_t ack;

    IIC_SDA_1(); 
    IIC_Delay();
    
    IIC_SCL_1();
    IIC_Delay(); 

    if (IIC_SDA_READ()) 
        ack = 1; // 无应答
    else 
        ack = 0; // 有应答
        
    IIC_SCL_0();
    IIC_Delay();

    return ack;
}

// =================================================================
// I2C 读取函数实现
// =================================================================

uint8_t IIC_Read_Byte_ACK(void)
{
    uint8_t i, byte = 0;
    
    IIC_SDA_1(); // 释放SDA线
    IIC_Delay();
    
    for (i = 0; i < 8; i++)
    {
        IIC_SCL_1();
        IIC_Delay();
        byte <<= 1;
        if (IIC_SDA_READ()) byte |= 0x01;
        IIC_SCL_0();
        IIC_Delay();
    }
    
    // 发送ACK
    IIC_SDA_0();
    IIC_Delay();
    IIC_SCL_1();
    IIC_Delay();
    IIC_SCL_0();
    IIC_Delay();
    IIC_SDA_1(); // 释放SDA
    
    return byte;
}

uint8_t IIC_Read_Byte_NACK(void)
{
    uint8_t i, byte = 0;
    
    IIC_SDA_1(); // 释放SDA线
    IIC_Delay();
    
    for (i = 0; i < 8; i++)
    {
        IIC_SCL_1();
        IIC_Delay();
        byte <<= 1;
        if (IIC_SDA_READ()) byte |= 0x01;
        IIC_SCL_0();
        IIC_Delay();
    }
    
    // 发送NACK
    IIC_SDA_1();
    IIC_Delay();
    IIC_SCL_1();
    IIC_Delay();
    IIC_SCL_0();
    IIC_Delay();
    
    return byte;
}

// =================================================================
// I2C 设备读写函数
// =================================================================

uint8_t IIC_Write_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    IIC_Start();
    
    // 发送设备地址（写模式）
    IIC_SendByte(dev_addr & 0xFE); // 清除最低位（写模式）
    if (IIC_Wait_ACK()) { IIC_Stop(); return 0; }
    
    // 发送寄存器地址
    IIC_SendByte(reg_addr);
    if (IIC_Wait_ACK()) { IIC_Stop(); return 0; }
    
    // 发送数据
    IIC_SendByte(data);
    if (IIC_Wait_ACK()) { IIC_Stop(); return 0; }
    
    IIC_Stop();
    return 1;
}

uint8_t IIC_Read_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data)
{
    IIC_Start();
    
    // 发送设备地址（写模式）
    IIC_SendByte(dev_addr & 0xFE); // 写模式
    if (IIC_Wait_ACK()) { IIC_Stop(); return 0; }
    
    // 发送寄存器地址
    IIC_SendByte(reg_addr);
    if (IIC_Wait_ACK()) { IIC_Stop(); return 0; }
    
    // 重新启动
    IIC_Start();
    
    // 发送设备地址（读模式）
    IIC_SendByte(dev_addr | 0x01); // 读模式
    if (IIC_Wait_ACK()) { IIC_Stop(); return 0; }
    
    // 读取数据（发送NACK表示只读一个字节）
    *data = IIC_Read_Byte_NACK();
    
    IIC_Stop();
    return 1;
}

void IIC_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能GPIOB时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // 配置SCL和SDA为开漏输出模式
    GPIO_InitStruct.Pin = IIC_SCL_GPIO_PIN | IIC_SDA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;          // 上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IIC_GPIO_PORT, &GPIO_InitStruct);
    
    // 初始状态：SCL和SDA置高
    IIC_SCL_1();
    IIC_SDA_1();
}
