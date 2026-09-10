#include "shell.h"
#include "MyUART.h"
#include "can_protocol.h"
#include "OLED.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint8_t g_speed;
extern volatile uint16_t g_temp;
extern volatile uint8_t  g_pwm;
extern volatile uint8_t  g_alarm;

static uint8_t Shell_ReadLine(char *cmd_buf, uint16_t buf_size)
{
	uint8_t len = 0;
	uint8_t ch;
	
	while(1)
	{
		if(MyUART_ReadByte(&ch))
		{
			if(ch == '\r' || ch == '\n')
			{
				cmd_buf[len] = '\0';
				return (len > 0)?1:0;
			}
			if(ch == '\b' || ch == 127)//退格键
			{
				if(len > 0)	len--;
				continue;
			}
			if(len < buf_size -1)
			{
				cmd_buf[len++] = ch;
			}
		}
		else
		{
			vTaskDelay(pdMS_TO_TICKS(10));
		}
	}
}

static void Cmd_Help(void)
{
	printf("\r\n=== Available Commands ===\r\n");
    printf("  temp              - Print temperature\r\n");
    printf("  speed [0-100]     - Print/Set speed\r\n");
    printf("  pwm               - Print PWM feedback\r\n");
    printf("  alarm             - Print alarm status\r\n");
    printf("  can <0xID> <data> - Send raw CAN frame\r\n");
    printf("  help              - This list\r\n");
}

static void Cmd_Temp(void)
{
	printf("[Shell] TEMP = %d\r\n", g_temp);
}

// "speed" 或 "speed 50" → 打印或设置车速
static void Cmd_Speed(char *val_str)
{
    if (val_str == NULL || val_str[0] == '\0')
    {
        printf("[Shell] SPEED = %d\r\n", g_speed);
        return;
    }
    uint8_t val = (uint8_t)atoi(val_str);
    if (val > 100) val = 100;
    g_speed = val;
    Proto_SendSpeed(val);

    if (val >= 80) { Proto_SendAlarm(1); g_alarm = 1; }
    else           { Proto_SendAlarm(0); g_alarm = 0; }
    
    printf("[Shell] Speed set to %d\r\n", val);
}

static void Cmd_Pwm(void)
{
    printf("[Shell] PWM feedback = %d\r\n", g_pwm);
}

static void Cmd_Alarm(void)
{
     printf("[Shell] ALARM = %d (0=OK, 1=OVER, 2=HOT)\r\n", g_alarm);
}

static void Cmd_Can(char *id_str, char *data_str)
{
    if (!id_str || !data_str)
    {
        printf("Usage: can <0xID> <data>\r\n");
        return;
    }
    
    uint32_t id = strtoul(id_str, NULL, 0); 
    uint8_t  data = (uint8_t)atoi(data_str);
    uint8_t  payload[1] = {data};
    Proto_SendRaw(id, payload, 1);
    
    printf("[Shell] CAN sent: ID=0x%03X data=%d\r\n", id, data);
}

static void Shell_ParseAndExecute(char *cmd_line)
{
    // 先提取第一个词（命令本身）
    char *cmd = strtok(cmd_line, " \t");
    if (!cmd) return;
    
    if      (strcmp(cmd, "help")  == 0) Cmd_Help();
    else if (strcmp(cmd, "temp")  == 0) Cmd_Temp();
    else if (strcmp(cmd, "speed") == 0)
    {
        char *val = strtok(NULL, " \t");
        Cmd_Speed(val);
    }
    else if (strcmp(cmd, "pwm")   == 0) Cmd_Pwm();
    else if (strcmp(cmd, "alarm") == 0) Cmd_Alarm();
    else if (strcmp(cmd, "can")   == 0)
    {
        char *id_str = strtok(NULL, " \t");
        char *data_str = strtok(NULL, " \t");
        Cmd_Can(id_str, data_str);
    }   
	else
    {
        printf("[Shell] Unknown command: %s (type 'help')\r\n", cmd);
    }
}
//FreeRTOS
void Task_Shell(void *pvParameters)
{
    char cmd_buf[SHELL_CMD_LEN];
    
    // 启动问候
    printf("\r\n===== BodyCtrl Shell Ready =====\r\n");
    printf("Type 'help' for commands.\r\n");
    
    while (1)
    {
        printf("# ");   // 提示符
        if (Shell_ReadLine(cmd_buf, SHELL_CMD_LEN))
        {
            Shell_ParseAndExecute(cmd_buf);
        }
    }
}
