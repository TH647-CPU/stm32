/* app.h —— 应用层：模式状态机 + 按键动作 + 自动光控 + 定时开关 + 显示 + 调度 */
#ifndef __APP_H
#define __APP_H

#include "stm32f1xx_hal.h"

/* 在 MX_*_Init() 之后、主循环之前调用一次 */
void App_Init(void);

/* 在主循环 while(1) 内反复调用 */
void App_Loop(void);

#endif /* __APP_H */
