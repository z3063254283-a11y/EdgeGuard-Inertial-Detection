#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

// I2C 地址（AD0 接 GND 时是 0x68，接 VCC 时是 0x69）
#define MPU6050_ADDR    0x68

// ===== 寄存器地址 =====
#define WHO_AM_I        0x75    // 存储 0x68 或 0x69，用来验证通信
#define PWR_MGMT_1      0x6B    // 电源管理寄存器（bit7=睡眠模式，要写 0 唤醒）
#define PWR_MGMT_2      0x6C
#define SMPLRT_DIV      0x19    // 采样率分频
#define CONFIG          0x1A    // 配置寄存器
#define GYRO_CONFIG     0x1B    // 陀螺仪量程
#define ACCEL_CONFIG    0x1C    // 加速度计量程

// 加速度计原始数据寄存器（每个 2 字节，高字节在前）
#define ACCEL_XOUT_H    0x3B
#define ACCEL_XOUT_L    0x3C
#define ACCEL_YOUT_H    0x3D
#define ACCEL_YOUT_L    0x3E
#define ACCEL_ZOUT_H    0x3F
#define ACCEL_ZOUT_L    0x40

// 陀螺仪原始数据寄存器
#define GYRO_XOUT_H     0x43
#define GYRO_XOUT_L     0x44
#define GYRO_YOUT_H     0x45
#define GYRO_YOUT_L     0x46
#define GYRO_ZOUT_H     0x47
#define GYRO_ZOUT_L     0x48

// ===== 量程配置 =====
// 加速度计量程: 2g / 4g / 8g / 16g
#define ACCEL_RANGE_2G  0x00    // 默认，灵敏度 16384 LSB/g
#define ACCEL_RANGE_4G  0x08    // 灵敏度 8192 LSB/g
#define ACCEL_RANGE_8G  0x10    // 灵敏度 4096 LSB/g
#define ACCEL_RANGE_16G 0x18    // 灵敏度 2048 LSB/g

// 陀螺计量程: ±250 / ±500 / ±1000 / ±2000 °/s
#define GYRO_RANGE_250  0x00    // 默认，灵敏度 131 LSB/(°/s)
#define GYRO_RANGE_500  0x08    // 灵敏度 65.5 LSB/(°/s)
#define GYRO_RANGE_1000 0x10    // 灵敏度 32.8 LSB/(°/s)
#define GYRO_RANGE_2000 0x18    // 灵敏度 16.4 LSB/(°/s)

// ===== 数据结构体 =====
typedef struct {
    int16_t ax;     // 加速度 X 原始值
    int16_t ay;     // 加速度 Y
    int16_t az;     // 加速度 Z
    int16_t gx;     // 角速度 X 原始值
    int16_t gy;     // 角速度 Y
    int16_t gz;     // 角速度 Z
} MPU6050_RawData_t;

typedef struct {
    float ax_g;     // 加速度 X，单位 g
    float ay_g;
    float az_g;
    float gx_dps;   // 角速度 X，单位 °/s
    float gy_dps;
    float gz_dps;
} MPU6050_ScaledData_t;

// ===== 对外接口 =====
uint8_t MPU6050_Init(void);                        // 初始化并返回 WHO_AM_I 值（验证用）
void    MPU6050_ReadAll(MPU6050_RawData_t *raw);   // 读六轴原始值
void    MPU6050_Scale(MPU6050_RawData_t *raw, MPU6050_ScaledData_t *scaled);  // 原始值转物理量

#endif
