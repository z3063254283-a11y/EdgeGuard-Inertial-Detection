#include "stm32f10x.h"
#include "Motor.h"

void Motor_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    
    GPIO_InitStructure.GPIO_Pin = TB6612_STBY_PIN;
    GPIO_Init(TB6612_STBY_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = TB6612_AIN1_PIN;
    GPIO_Init(TB6612_AIN1_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = TB6612_AIN2_PIN;
    GPIO_Init(TB6612_AIN2_PORT, &GPIO_InitStructure);
    
    GPIO_SetBits(TB6612_STBY_PORT, TB6612_STBY_PIN);   // STBY=1 唤醒TB6612！
    GPIO_SetBits(TB6612_AIN1_PORT, TB6612_AIN1_PIN);   // AIN1=1
    GPIO_ResetBits(TB6612_AIN2_PORT, TB6612_AIN2_PIN); // AIN2=0 → 正转
}
