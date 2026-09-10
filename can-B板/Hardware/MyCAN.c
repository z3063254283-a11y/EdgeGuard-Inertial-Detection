#include "stm32f10x.h"                  // Device header
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

extern SemaphoreHandle_t canSem;

void MyCAN_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	CAN_InitTypeDef CAN_InitStructure;
	CAN_InitStructure.CAN_TTCM = DISABLE;                  // 时间触发模式（不用）
    CAN_InitStructure.CAN_ABOM = ENABLE;                   // 自动离线管理（推荐开启）
    CAN_InitStructure.CAN_AWUM = DISABLE;                  // 自动唤醒（不用）
    CAN_InitStructure.CAN_NART = DISABLE;                  // 自动重传（允许重传，提高可靠性）
    CAN_InitStructure.CAN_RFLM = DISABLE;                  // 接收 FIFO 锁定（不锁定，新数据覆盖旧数据）
    CAN_InitStructure.CAN_TXFP = DISABLE;                  // 发送优先级（按 ID 优先级，不是按发送顺序）
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;          // 普通模式（先正常模式，Loopback 后面单独测）
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;               // 同步跳转宽度
    CAN_InitStructure.CAN_BS1 = CAN_BS1_9tq;               // 时间段1（9个时间单元）
    CAN_InitStructure.CAN_BS2 = CAN_BS2_8tq;               // 时间段2（8个时间单元）
    CAN_InitStructure.CAN_Prescaler = 3;                   // 分频系数：36M / 4 / (1+9+8) = 500kbps
    CAN_Init(CAN1, &CAN_InitStructure);
	
	CAN_FilterInitTypeDef CAN_FilterInitStructure;

	// 过滤器组0：接收 ID=0x101
	CAN_FilterInitStructure.CAN_FilterNumber = 0;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;  
	CAN_FilterInitStructure.CAN_FilterIdHigh = (0x101 << 5);          
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x7FF << 5);       
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);

	// 过滤器组0：接收 ID=0x200
	CAN_FilterInitStructure.CAN_FilterNumber = 1;
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;  
	CAN_FilterInitStructure.CAN_FilterIdHigh = (0x200 << 5);          
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x7FF << 5);       
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);
	
	// 过滤器组1：接收 ID=0x400(报警)
	CAN_FilterInitStructure.CAN_FilterNumber = 2;
	CAN_FilterInitStructure.CAN_FilterIdHigh = (0x400 << 5);
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x7FF << 5);
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);
	
	// 过滤器组1：接收 ID=0x500(心跳帧)
	CAN_FilterInitStructure.CAN_FilterNumber = 3;
	CAN_FilterInitStructure.CAN_FilterIdHigh = (0x500 << 5);
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x7FF << 5);
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&CAN_FilterInitStructure);
}

uint8_t MyCAN_Transmit(uint32_t id, uint8_t *data, uint8_t len)
{
	CanTxMsg TxMessage;
	
	TxMessage.DLC = len;
	TxMessage.ExtId = 0;
	TxMessage.StdId = id; 
	TxMessage.IDE = CAN_Id_Standard;
	TxMessage.RTR = CAN_RTR_DATA;
	
	for (uint8_t i = 0; i < len && i < 8;i++)
	{
		TxMessage.Data[i] = data[i];
	}
	
	uint8_t mailbox = CAN_Transmit(CAN1,&TxMessage);
	uint32_t flag;
	
	if (mailbox > 2)
        return 0;
    
    if (mailbox == 0)
        flag = CAN_FLAG_RQCP0;
    else if (mailbox == 1)
        flag = CAN_FLAG_RQCP1;
    else if (mailbox == 2)
        flag = CAN_FLAG_RQCP2;
    else
        return 0;
	
	uint32_t timeout = 0;
	while (CAN_GetFlagStatus(CAN1,flag) == RESET)
	{
		timeout ++;
		if(timeout > 100000)
			return 0;
	}
	return 1;
}

uint8_t MyCAN_IsDataReceive(void)
{
	if(CAN_GetFlagStatus(CAN1,CAN_FLAG_FMP0) != RESET)
		return 1;
	else
		return 0;
}

uint8_t MyCAN_Receive(uint32_t *id, uint8_t *data, uint8_t *len)
{
	CanRxMsg RxMessage;
	
	if(MyCAN_IsDataReceive() == 0)
		return 0;
	
	CAN_Receive(CAN1,CAN_FIFO0,&RxMessage);
	
	*id = RxMessage.StdId;
	*len = RxMessage.DLC;
	for (uint8_t i = 0; i < RxMessage.DLC && i < 8;i++)
	{
		data[i] = RxMessage.Data[i];
	}
	
	return 1;
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	if(CAN_GetITStatus(CAN1,CAN_IT_FMP0) != RESET)
	{
		CAN_ITConfig(CAN1,CAN_IT_FMP0,DISABLE);
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(canSem,&xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  
	}
}

void MyCAN_EnableRxInterrupt(void)
{
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);   // 开 CAN 控制器内部 FMP0 中断源
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 14;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
}
