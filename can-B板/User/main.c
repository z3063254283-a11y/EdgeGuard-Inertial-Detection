#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "MyCAN.h"
#include "queue.h"
#include "OLED.h"
#include "semphr.h"
#include "PWM.h"
#include "Motor.h"
#include "can_protocol.h"
#include "MyI2C.h"
#include "MPU6050.h"
#include "MyUART.h"
#include "Key.h"
#include <stdio.h>
#include "ml_model.h"

#define HEARTBEAT_TIMEOUT_MS 1500

typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;
} CanFrame_t;

QueueHandle_t canQueue;
SemaphoreHandle_t canSem;
SemaphoreHandle_t pwmSem;

volatile MPU6050_RawData_t g_mpu_raw;
volatile TickType_t last_rx_tick = 0;
volatile uint8_t g_targetSpeed = 0; 
volatile uint8_t g_alarm = 0;
volatile uint16_t g_temp = 0;
volatile uint8_t g_safeMode = 0;
volatile uint8_t g_mpu_whoami = 0;
volatile uint8_t g_collect_mode = 0;   // 新增：0=正常, 1=采集模式
volatile uint8_t g_detected_abnormal = 0;     // 0=正常, 1=异常（决策树输出）
volatile MlSubtype_t g_detected_subtype = SUBTYPE_NORMAL;  // 异常子类型

void Task_CanSend(void *pvParameters)
{
	while(1)
	{
		Proto_SendPwm(g_targetSpeed);
		vTaskDelay(1000);
	}
}

void Task_CanRecv(void *pvParameters)
{
	while(1)
	{
		if(xSemaphoreTake(canSem, portMAX_DELAY) == pdTRUE)
		{
			while (MyCAN_IsDataReceive())
			{
				CanFrame_t frame;
				MyCAN_Receive(&frame.id, frame.data, &frame.len);
				if (!Proto_CheckFrame(frame.data)) continue;
				
				last_rx_tick = xTaskGetTickCount();
				
				if(frame.id == CAN_ID_SPEED_CMD)
				{
					g_targetSpeed = frame.data[0];
					xSemaphoreGive(pwmSem);
				}
				else if(frame.id == CAN_ID_ALARM)
				{
					g_alarm = frame.data[0];
				}
				else if(frame.id == CAN_ID_TEMP)
				{
					g_temp = frame.data[0] | ((uint16_t)frame.data[1] << 8);
				}
				xQueueSend(canQueue,&frame,0);
			}
			CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
		}
	}
}

void Task_PWM(void *pvparameters)
{
	while(1)
	{
		xSemaphoreTake(pwmSem,portMAX_DELAY);
		PWM_SetDuty(g_targetSpeed);
	}
}

void Task_MPU6050(void *pvParameters)
{
    uint8_t lastKey = 0;
    MyI2C_Init();
    g_mpu_whoami = MPU6050_Init();
    
    while(1)
    {
        MPU6050_ReadAll((MPU6050_RawData_t*)&g_mpu_raw);
        
        // ==================== 新增：ML 推理 ====================
        // 1. 把最新一帧塞进环形缓冲
        ml_push_frame(g_mpu_raw.ax, g_mpu_raw.ay, g_mpu_raw.az,
                      g_mpu_raw.gx, g_mpu_raw.gy, g_mpu_raw.gz);
        
        // 2. 缓冲满了 + 到点了 → 跑推理
        if (ml_should_predict()) {
            MlSubtype_t subtype;
            uint8_t result = ml_run_full_inference(&subtype);
            
            g_detected_abnormal = result;          // 存结果供 OLED / CAN 用
            g_detected_subtype = subtype;
            
            if (result == 1) {
                // 异常：设置 CAN 告警 + 串口吐信息（调试用）
                g_alarm = 1;
                printf("!!! DETECTED ABNORMAL subtype=%d !!!\r\n", subtype);
                // 以后你可以加：Proto_SendAlarm(g_alarm) 发 CAN 告警给 A 板
            } else {
                g_alarm = 0;
            }
        }
        // ==================== 新增结束 ====================
        
        // ---- 原有的：按键翻转采集模式 ----
        uint8_t key = Key_GetNum();
        if (key == 1 && key != lastKey) {
            g_collect_mode = !g_collect_mode;
            if (g_collect_mode)
                printf("\r\n=== COLLECT START ===\r\n");
            else
                printf("\r\n=== COLLECT STOP ===\r\n");
        }
        lastKey = key;
        
        // ---- 原有的：采集模式下吐 CSV ----
        if (g_collect_mode) {
            printf("%d,%d,%d,%d,%d,%d\r\n",
                g_mpu_raw.ax, g_mpu_raw.ay, g_mpu_raw.az,
                g_mpu_raw.gx, g_mpu_raw.gy, g_mpu_raw.gz);
        }
        
        vTaskDelay(50);   // 50ms 一次 = 20Hz
    }
}

void Task_OLED(void *pvParameters)
{
	while(1)
	{
		CanFrame_t frame;
		xQueueReceive(canQueue, &frame, 50);
		
		__disable_irq();
		// 第1行：WHO + 采集状态
		OLED_ShowString(1, 1, "WHO:");
		OLED_ShowNum(1, 5, g_mpu_whoami, 3);
		OLED_ShowString(1, 10, "COL:");
		if (g_collect_mode) OLED_ShowString(1, 14, "ON ");
		else                OLED_ShowString(1, 14, "OFF");
		// 第2行：AX AY
		OLED_ShowString(2, 1, "AX:");
		OLED_ShowNum(2, 4, (uint16_t)g_mpu_raw.ax, 5);
		OLED_ShowString(2, 10, "AY:");
		OLED_ShowNum(2, 13, (uint16_t)g_mpu_raw.ay, 4);
		// 第3行：AZ SPD
		OLED_ShowString(3, 1, "AZ:");
		OLED_ShowNum(3, 4, (uint16_t)g_mpu_raw.az, 5);
		OLED_ShowString(3, 10, "SPD:");
		OLED_ShowNum(3, 14, g_targetSpeed, 3);
		// 第4行：ALM TMP
		OLED_ShowString(4, 1, "ALM:");
		if (g_safeMode == 1)
			OLED_ShowString(4, 5, "LOST");
		else if(g_detected_abnormal == 0)
			OLED_ShowString(4, 5, "OK  ");
		else
		{
			// abnormal：根据子类型显示不同简写
			if (g_detected_subtype == SUBTYPE_SHAKE)
				OLED_ShowString(4, 5, "SHK ");   // 震动
			else if (g_detected_subtype == SUBTYPE_IMPACT)
				OLED_ShowString(4, 5, "IMP ");   // 冲击
			else if (g_detected_subtype == SUBTYPE_TILT)
				OLED_ShowString(4, 5, "TILT");   // 倾斜
			else
				OLED_ShowString(4, 5, "ALM!");
		}
		__enable_irq();
		vTaskDelay(50);
	}
}

void Task_Watchdog(void *pvParameters)
{
	while(1)
	{
		if((xTaskGetTickCount() - last_rx_tick) > pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS))
		{
			if(g_safeMode == 0)
			{
				g_safeMode = 1;
				PWM_SetDuty(0);
				g_alarm = 0xFF;
			}
		}
		else
		{
			if(g_safeMode == 1)
			{
				g_safeMode = 0;
				g_alarm = 0;
			}
		}
		vTaskDelay(100);
	}
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
    PWM_Init();
    Motor_Init();
    MyCAN_Init();
    OLED_Init();
    OLED_Clear();
    MyUART_Init(115200);
    Key_Init();
	ml_init();
	
	MyUART_SendString("B_BOOT_OK\r\n");   // ← 加这一行，在 FreeRTOS 启动前
    
    canQueue = xQueueCreate(5, sizeof(CanFrame_t));
    canSem = xSemaphoreCreateBinary();
    pwmSem = xSemaphoreCreateBinary();
    MyCAN_EnableRxInterrupt();

    xTaskCreate(Task_CanSend,"can_tx",256,NULL,2,NULL);
    xTaskCreate(Task_CanRecv, "can_rx", 256, NULL, 2, NULL);
    xTaskCreate(Task_OLED, "oled", 256, NULL, 2, NULL);
    xTaskCreate(Task_PWM, "pwm", 128,NULL,2,NULL);
    xTaskCreate(Task_Watchdog, "dog",128,NULL,1,NULL);
    xTaskCreate(Task_MPU6050, "mpu", 384, NULL, 2, NULL);

    vTaskStartScheduler();
    while (1);
}
