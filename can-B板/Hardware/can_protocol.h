#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include "stm32f10x.h"

// ==================== 帧 ID 定义 ====================
#define CAN_ID_SPEED_CMD    0x101   // A→B 车速命令
#define CAN_ID_TEMP         0x200   // A→B 温度
#define CAN_ID_PWM_FB       0x300   // B→A PWM反馈
#define CAN_ID_ALARM        0x400   // 任意 报警
#define CAN_ID_HEARTBEAT    0x500   // A→B 心跳

// ==================== 发送接口 ====================
void Proto_SendSpeed(uint8_t speed);
void Proto_SendTemp(uint16_t temp);
void Proto_SendPwm(uint8_t duty);
void Proto_SendAlarm(uint8_t type);
void Proto_SendHeartbeat(uint8_t counter);

// ==================== 校验接口 ====================
// 返回1=校验通过, 0=校验失败
uint8_t Proto_CheckFrame(uint8_t *data);

#endif
