#include "stm32f10x.h"
#include "MyCAN.h"
#include "can_protocol.h"

// XOR 校验：把 data[0]~data[len-1] 全部异或起来
static uint8_t CalcChecksum(uint8_t *data, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++)
        sum ^= data[i];
    return sum;
}

// 统一打包 + 加校验 + 发送
static void SendFrame(uint32_t id, uint8_t *payload, uint8_t payload_len)
{
    uint8_t data[8] = {0};
    for (uint8_t i = 0; i < payload_len && i < 7; i++)
        data[i] = payload[i];
    data[7] = CalcChecksum(data, 7);     // data[0~6] 算校验，放 data[7]
    MyCAN_Transmit(id, data, 8);          // 统一发 8 字节
}

void Proto_SendSpeed(uint8_t speed)
{
    SendFrame(CAN_ID_SPEED_CMD, &speed, 1);
}

void Proto_SendTemp(uint16_t temp)
{
    uint8_t payload[2];
    payload[0] = temp & 0xFF;       // 低字节
    payload[1] = (temp >> 8) & 0xFF; // 高字节
    SendFrame(CAN_ID_TEMP, payload, 2);
}

void Proto_SendPwm(uint8_t duty)
{
    SendFrame(CAN_ID_PWM_FB, &duty, 1);
}

void Proto_SendAlarm(uint8_t type)
{
    SendFrame(CAN_ID_ALARM, &type, 1);
}

void Proto_SendHeartbeat(uint8_t counter)
{
    SendFrame(CAN_ID_HEARTBEAT, &counter, 1);
}

void Proto_SendRaw(uint32_t id, uint8_t *payload, uint8_t payload_len)
{
    SendFrame(id, payload, payload_len);
}

uint8_t Proto_CheckFrame(uint8_t *data)
{
    uint8_t calc = CalcChecksum(data, 7);
    return (calc == data[7]) ? 1 : 0;
}
