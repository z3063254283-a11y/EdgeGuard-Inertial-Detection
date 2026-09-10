#include "stm32f10x.h"
#include "MyI2C.h"

// ===== GPIO 操作宏 =====
#define I2C_SCL_H()     GPIO_SetBits(MYI2C_GPIO, MYI2C_SCL_PIN)
#define I2C_SCL_L()     GPIO_ResetBits(MYI2C_GPIO, MYI2C_SCL_PIN)
#define I2C_SDA_H()     GPIO_SetBits(MYI2C_GPIO, MYI2C_SDA_PIN)
#define I2C_SDA_L()     GPIO_ResetBits(MYI2C_GPIO, MYI2C_SDA_PIN)
#define I2C_SDA_READ()  GPIO_ReadInputDataBit(MYI2C_GPIO, MYI2C_SDA_PIN)

// ===== 微秒级延时 =====
static void I2C_Delay(void)
{
    uint8_t i;
    for (i = 0; i < 20; i++);
}

// ===== 软件 I2C 基础时序 =====
static void I2C_Start(void)
{
    I2C_SDA_H();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SDA_L();
    I2C_Delay();
    I2C_SCL_L();
}

static void I2C_Stop(void)
{
    I2C_SDA_L();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SDA_H();
    I2C_Delay();
}

// 发一个字节，返回从机 ACK（0=正常, 1=没收到 ACK）
static uint8_t I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80) I2C_SDA_H();
        else             I2C_SDA_L();
        byte <<= 1;
        I2C_Delay();
        I2C_SCL_H();
        I2C_Delay();
        I2C_SCL_L();
    }
    // 第 9 个时钟：读 ACK
    I2C_SDA_H();
    I2C_Delay();
    I2C_SCL_H();
    I2C_Delay();
    uint8_t ack = I2C_SDA_READ();
    I2C_SCL_L();
    return ack;
}

// 读一个字节，ack=1 继续（发ACK），ack=0 最后一个（发NACK）
static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t val = 0;
    I2C_SDA_H();
    for (i = 0; i < 8; i++)
    {
        val <<= 1;
        I2C_SCL_H();
        I2C_Delay();
        if (I2C_SDA_READ()) val |= 0x01;
        I2C_SCL_L();
        I2C_Delay();
    }
    // 第 9 个时钟：发 ACK 或 NACK
    if (ack) I2C_SDA_L();
    else     I2C_SDA_H();
    I2C_Delay();
    I2C_SCL_H();
    I2C_Delay();
    I2C_SCL_L();
    I2C_SDA_H();
    return val;
}

// ===== 对外接口 =====
void MyI2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = MYI2C_SCL_PIN | MYI2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MYI2C_GPIO, &GPIO_InitStructure);

    I2C_SCL_H();
    I2C_SDA_H();
}

void MyI2C_Write(uint8_t addr, uint8_t reg, uint8_t data)
{
    I2C_Start();
    I2C_SendByte(addr << 1);
    I2C_SendByte(reg);
    I2C_SendByte(data);
    I2C_Stop();
}

uint8_t MyI2C_Read(uint8_t addr, uint8_t reg)
{
    uint8_t val;
    I2C_Start();
    I2C_SendByte(addr << 1);
    I2C_SendByte(reg);
    I2C_Start();
    I2C_SendByte((addr << 1) | 0x01);
    val = I2C_ReadByte(0);
    I2C_Stop();
    return val;
}

void MyI2C_ReadBurst(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    I2C_Start();
    I2C_SendByte(addr << 1);
    I2C_SendByte(reg);
    I2C_Start();
    I2C_SendByte((addr << 1) | 0x01);
    for (i = 0; i < len; i++)
    {
        buf[i] = I2C_ReadByte(i != len - 1);
    }
    I2C_Stop();
}
