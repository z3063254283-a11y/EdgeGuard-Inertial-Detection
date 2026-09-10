#include "stm32f10x.h"
#include "MyI2C.h"
#include "MPU6050.h"

// 当前灵敏度（根据量程设置）
static float accel_sensitivity;
static float gyro_sensitivity;

uint8_t MPU6050_Init(void)
{
    uint8_t who_am_i;
    
    // 1. 读 WHO_AM_I，验证芯片在线
    who_am_i = MyI2C_Read(MPU6050_ADDR, WHO_AM_I);
    
    // 2. 唤醒：PWR_MGMT_1 写 0x00（bit7=0 表示不睡眠）
    MyI2C_Write(MPU6050_ADDR, PWR_MGMT_1, 0x00);
    
    // 3. 加速度计量程 ±2g → sensitivity = 16384 LSB/g
    MyI2C_Write(MPU6050_ADDR, ACCEL_CONFIG, ACCEL_RANGE_2G);
    accel_sensitivity = 16384.0f;
    
    // 4. 陀螺计量程 ±500°/s → sensitivity = 65.5 LSB/(°/s)
    MyI2C_Write(MPU6050_ADDR, GYRO_CONFIG, GYRO_RANGE_500);
    gyro_sensitivity = 65.5f;
    
    // 5. 采样率分频：内部采样率 1kHz，分频后 = 1000/(1+div)
    MyI2C_Write(MPU6050_ADDR, SMPLRT_DIV, 9);   // 1000/(1+9) = 100Hz
    
    // 6. 配置寄存器：低通滤波 DLPF_CFG=3（带宽 44Hz，降低噪声）
    MyI2C_Write(MPU6050_ADDR, CONFIG, 0x03);
    
    return who_am_i;    // 返回 WHO_AM_I（正常范围 0x68~0x70，因模块批次而异）
}

void MPU6050_ReadAll(MPU6050_RawData_t *raw)
{
    uint8_t buf[14];
    
    // 一次性读 14 字节（6 轴 × 2 字节，从 ACCEL_XOUT_H 0x3B 开始）
    MyI2C_ReadBurst(MPU6050_ADDR, ACCEL_XOUT_H, buf, 14);
    
    // 组合高低字节，注意大端序（高字节在前）
    raw->ax = (int16_t)(buf[0] << 8 | buf[1]);
    raw->ay = (int16_t)(buf[2] << 8 | buf[3]);
    raw->az = (int16_t)(buf[4] << 8 | buf[5]);
    raw->gx = (int16_t)(buf[8] << 8 | buf[9]);
    raw->gy = (int16_t)(buf[10] << 8 | buf[11]);
    raw->gz = (int16_t)(buf[12] << 8 | buf[13]);
}

void MPU6050_Scale(MPU6050_RawData_t *raw, MPU6050_ScaledData_t *scaled)
{
    scaled->ax_g   = (float)raw->ax / accel_sensitivity;
    scaled->ay_g   = (float)raw->ay / accel_sensitivity;
    scaled->az_g   = (float)raw->az / accel_sensitivity;
    scaled->gx_dps = (float)raw->gx / gyro_sensitivity;
    scaled->gy_dps = (float)raw->gy / gyro_sensitivity;
    scaled->gz_dps = (float)raw->gz / gyro_sensitivity;
}
