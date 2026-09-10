#ifndef __MYUART_H
#define __MYUART_H

#include "stm32f10x.h"
#include <stdio.h> 

#define UART_RING_SIZE 128

void MyUART_Init(uint32_t baud);
void MyUART_SendByte(uint8_t data);
void MyUART_SendString(char *str);

uint8_t MyUART_ReadByte(uint8_t *ch);
void MyUART_EnableRxIT(void);

#endif
