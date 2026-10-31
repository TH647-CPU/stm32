/* key_app.h —— 6 键按键处理程序（按键语义 + 动作）
 *
 * 六个按键（引脚见 bsp_key.h，PA3~PA8）：
 *  KEY_POWER   开关：切换输出 开/关
 *  KEY_MODE    光敏↔手动：自动光控 / 手动调光 二选一切换
 *  KEY_DIM     调光模式：0=PWM 直调(占空比)，1=恒流闭环(20~200mA)
 *  KEY_UP      亮度 +：手动模式下增亮（长按连发）
 *  KEY_DOWN    亮度 -：手动模式下减亮（长按连发）
 *  KEY_TIMER   定时开关：按下启动/停止定时倒计时，到点自动关机
 *
 * 用法：
 *   KEY_AppInit();          // 初始化
 *   主循环 while(1) 中调用： KEY_AppHandle();   // 自动 5ms 扫键 + 按键动作 + 输出
 */
#ifndef __KEY_APP_H
#define __KEY_APP_H

#include "stm32f1xx_hal.h"

/* 定时参数：长按 KEY_DIM 启动后从该秒数倒计时，到 0 自动关机 */
#define KEYAPP_TMR_DEFAULT_SEC   30u

void KEY_AppInit(void);          /* 初始化按键与状态 */
void KEY_AppHandle(void);        /* 主循环反复调用：扫键 + 处理 + 输出 */

/* 供显示层读取的状态（数码管等外部程序按需使用） */
uint8_t  KEYAPP_IsPowerOn(void);       /* 1=输出开，0=输出关 */
uint8_t  KEYAPP_IsAutoLight(void);     /* 1=自动光控，0=手动调光 */
uint8_t  KEYAPP_GetDimMode(void);      /* 0=PWM 直调，1=恒流闭环 */
uint16_t KEYAPP_GetBrightness(void);   /* 手动亮度 0~100 */
uint16_t KEYAPP_GetTimerMin(void);     /* 定时分钟数 */
uint8_t  KEYAPP_IsTimerRunning(void);  /* 1=定时倒计时运行中 */

#endif /* __KEY_APP_H */
