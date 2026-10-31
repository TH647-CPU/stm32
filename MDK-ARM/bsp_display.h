/* bsp_display.h —— 4 位 7 段数码管 GPIO 动态扫描驱动
 * 引脚全部用宏定义，按你的实际接线改这里即可，无需动 CubeMX
 */
#ifndef __BSP_DISPLAY_H
#define __BSP_DISPLAY_H

#include "stm32f1xx_hal.h"

#define DISP_DIGITS    4u          /* 位数 */

/* ===== 8 根段码引脚（a b c d e f g dp）===== */
#define DISP_SEG_A_PORT   GPIOB
#define DISP_SEG_A_PIN    GPIO_PIN_0
#define DISP_SEG_B_PORT   GPIOB
#define DISP_SEG_B_PIN    GPIO_PIN_1
#define DISP_SEG_C_PORT   GPIOB
#define DISP_SEG_C_PIN    GPIO_PIN_8
#define DISP_SEG_D_PORT   GPIOB
#define DISP_SEG_D_PIN    GPIO_PIN_9
#define DISP_SEG_E_PORT   GPIOB
#define DISP_SEG_E_PIN    GPIO_PIN_12
#define DISP_SEG_F_PORT   GPIOB
#define DISP_SEG_F_PIN    GPIO_PIN_13
#define DISP_SEG_G_PORT   GPIOB
#define DISP_SEG_G_PIN    GPIO_PIN_14
#define DISP_SEG_DP_PORT  GPIOB
#define DISP_SEG_DP_PIN   GPIO_PIN_15

/* ===== 4 根位选引脚（第 0~3 位）=====
   注意：第 3 位(DISP_DIG3)原默认 PA8 与按键 KEY_TIMER(PA8) 冲突，已改到 PA9。
   请把数码管第 3 位公共端接到 PA9（或按你的实际接线改这里的宏）。 */
#define DISP_DIG0_PORT    GPIOC
#define DISP_DIG0_PIN     GPIO_PIN_13
#define DISP_DIG1_PORT    GPIOC
#define DISP_DIG1_PIN     GPIO_PIN_14
#define DISP_DIG2_PORT    GPIOC
#define DISP_DIG2_PIN     GPIO_PIN_15
#define DISP_DIG3_PORT    GPIOA
#define DISP_DIG3_PIN     GPIO_PIN_9

/* ===== 显示类型 =====
 * 共阴：1  → 段码高电平点亮，位选高电平选通（常用，配 NPN 三极管）
 * 共阳：0  → 段码低电平点亮，位选低电平选通（自行把下面两个电平宏对调）
 * 若你的位选是“共阴数码管公共端直接接 MCU 引脚、低电平选通”，
 * 把 DISP_DIG_ON_LEVEL 改为 GPIO_PIN_RESET、OFF 改为 GPIO_PIN_SET 即可。 */
#define DISP_COMMON_CATHODE   1u

#define DISP_SEG_LIT_LEVEL   (DISP_COMMON_CATHODE ? GPIO_PIN_SET   : GPIO_PIN_RESET)
#define DISP_SEG_OFF_LEVEL   (DISP_COMMON_CATHODE ? GPIO_PIN_RESET : GPIO_PIN_SET)
#define DISP_DIG_ON_LEVEL     GPIO_PIN_SET
#define DISP_DIG_OFF_LEVEL    GPIO_PIN_RESET

/* ===== 字形索引（seg 表下标，供应用层组合显示内容）===== */
typedef enum
{
    DISP_G_BLANK = 0,          /* 全灭 */
    DISP_G_0, DISP_G_1, DISP_G_2, DISP_G_3, DISP_G_4,
    DISP_G_5, DISP_G_6, DISP_G_7, DISP_G_8, DISP_G_9,
    DISP_G_A, DISP_G_U, DISP_G_T, DISP_G_O, DISP_G_F,
    DISP_G_MINUS, DISP_G_H, DISP_G_D, DISP_G_L,
    DISP_G_NUM_GLYPHS          /* 表项数 */
} DispGlyph_t;

void DISPLAY_Init(void);

/* 每 1ms 调用一次，刷新下一位（动态扫描，勿阻塞） */
void DISPLAY_Tick(void);

/* 设置 4 位字形与小数点。dp_mask 的 bit0~bit3 分别对应第 0~3 位的小数点 */
void DISPLAY_Set(const uint8_t glyph[4], uint8_t dp_mask);

/* 便捷接口：显示一个 0~9999 的非负整数（右对齐，高位空位熄灭） */
void DISPLAY_ShowNumber(uint16_t val, uint8_t dp_mask);

#endif /* __BSP_DISPLAY_H */
