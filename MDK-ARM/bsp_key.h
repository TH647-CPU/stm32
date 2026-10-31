/* bsp_key.h —— 6 键扫描驱动：消抖 + 按下沿 + 长按连发
 *
 * ===== 按键功能分配 =====
 *  KEY_POWER  PB12 开关：按下切换输出 开/关
 *  KEY_MODE   PB13 手动↔自动光控：在“自动光控”与“手动调光”之间切换
 *  KEY_DIM    PB14 单键循环 4 档亮度：100%→75%→50%→25%→循环
 */
#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "stm32f1xx_hal.h"

/* ===== 按键数量与索引 ===== */
#define KEY_NUM        3u

#define KEY_POWER      0u   /* PB12：开关（输出开/关） */
#define KEY_MODE       1u   /* PB13：手动 ↔ 自动光控 */
#define KEY_DIM        2u   /* PB14：单键循环 4 档亮度 */

/* 按键有效电平：1=按下为低电平（外部上拉，按下接地）。若按键接 3.3V 高有效改为 0 */
#define KEY_ACTIVE_LOW  1u

/* 连发参数：KEY_Scan 需每 5ms 调用一次
   REpeat_START=25  → 按住 125ms 后开始连发
   REpeat_STEP =10  → 之后每 50ms 触发一次  */
#define KEY_REPEAT_START  25u
#define KEY_REPEAT_STEP   10u

void KEY_Init(void);

/* 每 5ms 调用一次，轮询 6 个按键 */
void KEY_Scan(void);

/* 查询某个按键是否有“按下沿”（按下瞬间返回 1，查询后自动清零） */
uint8_t KEY_IsPressed(uint8_t idx);

/* 查询某个按键是否处于“长按连发”触发点（返回 1 后自动清零） */
uint8_t KEY_IsHeld(uint8_t idx);

#endif /* __BSP_KEY_H */
