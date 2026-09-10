#include "stm32f10x.h"                  // Device header
#include <stdio.h>
#include "MyUART.h"

// ===== 新增：环形缓冲区全局变量 =====
static uint8_t  ring_buf[UART_RING_SIZE];   // 环形缓冲区本体
static volatile uint16_t wr_idx = 0;        // 写指针（中断里写）
static volatile uint16_t rd_idx = 0;        // 读指针（任务里读）

void MyUART_Init(uint32_t baud)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;     // TX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;    // RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = baud;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1,&USART_InitStructure);
	
	USART_Cmd(USART1,ENABLE);
}

void MyUART_EnableRxIT(void)
{
	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
}


void MyUART_SendByte(uint8_t data)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, data);
}

void MyUART_SendString(char *str)
{
    while (*str)
    {
        MyUART_SendByte((uint8_t)(*str));
        str++;
    }
}

int fputc(int ch, FILE *f)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, (uint8_t)ch);
    return ch;
}

uint8_t MyUART_ReadByte(uint8_t *ch)
{
	if(wr_idx == rd_idx)	return 0;
	*ch = ring_buf[rd_idx];
	rd_idx = (rd_idx + 1) % UART_RING_SIZE;
	return 1;
}

void USART1_IRQHandler(void)          
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = USART_ReceiveData(USART1);
        uint16_t next_wr = (wr_idx + 1) % UART_RING_SIZE;  
        if (next_wr != rd_idx)                          
        {
            ring_buf[wr_idx] = ch;
            wr_idx = next_wr;
        }
    }
}

