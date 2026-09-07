#ifndef __BSP_IIC_DEBUG_H
#define __BSP_IIC_DEBUG_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

// I2C引脚定义
#define IIC_SCL_GPIO_PORT        GPIOB
#define IIC_SCL_GPIO_PIN         GPIO_PIN_6
#define IIC_SDA_GPIO_PORT        GPIOB
#define IIC_SDA_GPIO_PIN         GPIO_PIN_7
#define IIC_GPIO_PORT            GPIOB

// I2C GPIO操作宏
#define IIC_SCL_1()  HAL_GPIO_WritePin(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, GPIO_PIN_SET)
#define IIC_SCL_0()  HAL_GPIO_WritePin(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, GPIO_PIN_RESET)
#define IIC_SDA_1()  HAL_GPIO_WritePin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, GPIO_PIN_SET)
#define IIC_SDA_0()  HAL_GPIO_WritePin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, GPIO_PIN_RESET)
#define IIC_SDA_READ() HAL_GPIO_ReadPin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)

// 函数声明
void IIC_GPIO_Config(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_SendByte(uint8_t byte);
uint8_t IIC_Wait_ACK(void);
uint8_t IIC_Read_Byte_ACK(void);
uint8_t IIC_Read_Byte_NACK(void);
uint8_t IIC_Write_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
uint8_t IIC_Read_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);
uint8_t IIC_Write_Bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
uint8_t IIC_Read_Bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);

#endif /* __BSP_IIC_DEBUG_H */
