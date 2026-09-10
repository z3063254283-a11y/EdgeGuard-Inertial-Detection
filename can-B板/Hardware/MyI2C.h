#ifndef __MYI2C_H
#define __MYI2C_H

#include "stm32f10x.h"

#define MYI2C_SCL_PIN   GPIO_Pin_6      // PB6
#define MYI2C_SDA_PIN   GPIO_Pin_7      // PB7
#define MYI2C_GPIO      GPIOB

void MyI2C_Init(void);
void MyI2C_Write(uint8_t addr, uint8_t reg, uint8_t data);
uint8_t MyI2C_Read(uint8_t addr, uint8_t reg);
void MyI2C_ReadBurst(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

#endif
