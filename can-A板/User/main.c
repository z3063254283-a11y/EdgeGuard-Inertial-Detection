#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "MyCAN.h"
#include "queue.h"
#include "OLED.h"
#include "semphr.h"
#include "Key.h"
#include "ADC.h"
#include "can_protocol.h"
#include "MyUART.h"
#include "shell.h"

typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;
} CanFrame_t;

QueueHandle_t canQueue;
SemaphoreHandle_t canSem;

volatile uint8_t g_speed = 0;
volatile uint16_t g_temp = 0;
volatile uint8_t g_pwm = 0;
volatile uint8_t g_alarm = 0;

void Task_Key(void *pvParameters)
{
	uint8_t lastKey = 0;
	while(1)
	{
		uint8_t key = Key_GetNum();
		if(key != 0 && key != lastKey)
		{
			if (key == 1 && g_speed < 100)	g_speed += 10;
			else if (key == 2 && g_speed > 0) g_speed -= 10;
			Proto_SendSpeed(g_speed);
			
			if (g_speed >= 80)
			{
				Proto_SendAlarm(1);
				g_alarm = 1;
			}
			else
			{
				Proto_SendAlarm(0);
				g_alarm = 0;
			}
		}
		lastKey = key;
		vTaskDelay(20);
	}
}

void Task_ADC(void *pvParameters)
{
	while(1)
	{
		uint16_t voltage = ADC_GetVoltage();
		g_temp = voltage / 10;
		
		Proto_SendTemp(g_temp);
		if(g_temp >= 80)
		{
			Proto_SendAlarm(2);  //高温警报
			if (g_alarm == 0) g_alarm = 2;
		}
		else if (g_alarm == 2)
		{
			Proto_SendAlarm(0);  //高温解除
			g_alarm = 0;
		}
		vTaskDelay(500);
	}
}

void Task_Heartbeat(void *pvParameters)
{
	uint8_t counter = 0;
	while(1)
	{
		Proto_SendHeartbeat(counter++);
		vTaskDelay(1000);
	}
}

void Task_CanRecv(void *pvParameters)
{
	while(1)
	{
		if((xSemaphoreTake(canSem, portMAX_DELAY) == pdTRUE))
		{
			while(MyCAN_IsDataReceive())
			{
				CanFrame_t frame;
				MyCAN_Receive(&frame.id, frame.data, &frame.len);
				if (!Proto_CheckFrame(frame.data)) continue;
				xQueueSend(canQueue,&frame,0);
			}
			CAN_ITConfig(CAN1,CAN_IT_FMP0,ENABLE);
		}
	}
}

void Task_OLED(void *pvParameters)
{
	while(1)
	{
		CanFrame_t frame;
		
		if (xQueueReceive(canQueue, &frame, 50) == pdTRUE)
		{
			if (frame.id == CAN_ID_PWM_FB)          // PWM反馈
				g_pwm = frame.data[0];
			else if (frame.id == CAN_ID_ALARM)      // 报警帧
				g_alarm = frame.data[0];
		}
		__disable_irq();
		OLED_ShowString(1, 1, "=== Body Ctrl ===");
		OLED_ShowString(2, 1, "SPD:");
		OLED_ShowNum(2, 5, g_speed, 3);
		OLED_ShowString(2, 9, "  TMP:");
		OLED_ShowNum(2, 14, g_temp, 3);
		OLED_ShowString(3, 1, "PWM:");
		OLED_ShowNum(3, 5, g_pwm, 3);
		OLED_ShowString(4, 1, "ALARM:");
		if (g_alarm == 0)
			OLED_ShowString(4, 7, "OK  ");
		else if (g_alarm == 1)
			OLED_ShowString(4, 7, "OVER");
		else if (g_alarm == 2)
			OLED_ShowString(4, 7, "HOT ");
		__enable_irq();
		vTaskDelay(50);
	}
}

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	
	Key_Init();
	MyADC_Init();
	MyCAN_Init();
	OLED_Init();
	OLED_Clear();
	MyUART_Init(115200);
	MyUART_EnableRxIT();
	
	canQueue = xQueueCreate(5, sizeof(CanFrame_t));
	canSem = xSemaphoreCreateBinary();
	MyCAN_EnableRxInterrupt();

	xTaskCreate(Task_Key, "key", 128, NULL, 2, NULL);
	xTaskCreate(Task_ADC, "adc", 128, NULL, 2, NULL);
	xTaskCreate(Task_CanRecv, "can_rx", 256, NULL, 2, NULL);
	xTaskCreate(Task_OLED, "oled", 256, NULL, 2, NULL);
	xTaskCreate(Task_Heartbeat,"hb",128,NULL,1,NULL);
	xTaskCreate(Task_Shell,"sh",256,NULL,1,NULL);

    vTaskStartScheduler();

    while (1);
}
