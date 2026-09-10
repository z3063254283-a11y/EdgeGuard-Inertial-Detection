#ifndef __MOTOR_H
#define __MOTOR_H

#define TB6612_STBY_PIN  GPIO_Pin_0
#define TB6612_STBY_PORT GPIOB
#define TB6612_AIN1_PIN  GPIO_Pin_1
#define TB6612_AIN1_PORT GPIOB
#define TB6612_AIN2_PIN  GPIO_Pin_10
#define TB6612_AIN2_PORT GPIOB

void Motor_Init(void);

#endif
