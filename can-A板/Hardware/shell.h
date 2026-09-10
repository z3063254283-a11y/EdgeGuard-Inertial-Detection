#ifndef __SHELL_H
#define __SHELL_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

// ===== 命令行最大长度 =====
#define SHELL_CMD_LEN  32

// ===== FreeRTOS 任务函数 =====
void Task_Shell(void *pvParameters);

#endif
