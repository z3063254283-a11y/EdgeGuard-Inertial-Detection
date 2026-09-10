#ifndef __MYCAN_H
#define __MYCAN_H

#include "stm32f10x.h"

void MyCAN_Init(void);
uint8_t MyCAN_Transmit(uint32_t id, uint8_t *data, uint8_t len);
uint8_t MyCAN_IsDataReceive(void);
uint8_t MyCAN_Receive(uint32_t *id, uint8_t *data, uint8_t *len);
void MyCAN_EnableRxInterrupt(void);

#endif
