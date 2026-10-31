#pragma once

#include "stm32f1xx_hal.h"

/*
 * 4 位数码管驱动（HAL 库移植版，源工程：LED照明系统 Segment）
 *
 * 引脚（正面观看，左下角为 1 脚，逆时针排列）：
 *   1=E  2=D  3=DP  4=C  5=G  6=DIG4
 *   7=B  8=DIG3 9=DIG2 10=F 11=A 12=DIG1
 *
 * 当前接线（避开 PWM/ADC/按键/I2C/JTAG）：
 *   段选 a~dp：PB0、PB1、PB2、PB5、PA7、PA8、PA9、PB15，经 220Ω 限流
 *   位选 DIG1~DIG4：PA3~PA6 直连公共脚，高电平选中
 *
 * 注意：实物若为共阴，公共脚直连 GPIO 时应使用低电平选中，
 *       即 SEG_DIG_USE_TRANSISTOR 改为 0、SEG_COMMON_ANODE 改为 0；
 *       若公共脚经 S8050 低边驱动，位选仍为高电平有效，
 *       即 SEG_DIG_USE_TRANSISTOR 保持 1、SEG_COMMON_ANODE 改为 0。
 *       当前现象符合共阳，故 SEG_COMMON_ANODE 默认为 1。
 *
 * 扫描方式：每 1ms 调用一次 Segment_Scan()，4ms 完成一帧。
 */

#define SEG_DIGITS    4u          /* 位数 */

/* ===== 8 根段码引脚（a b c d e f g dp）===== */
#define SEG_A_PORT    GPIOB
#define SEG_A_PIN     GPIO_PIN_0
#define SEG_B_PORT    GPIOB
#define SEG_B_PIN     GPIO_PIN_1
#define SEG_C_PORT    GPIOB
#define SEG_C_PIN     GPIO_PIN_2
#define SEG_D_PORT    GPIOB
#define SEG_D_PIN     GPIO_PIN_5
#define SEG_E_PORT    GPIOA
#define SEG_E_PIN     GPIO_PIN_7
#define SEG_F_PORT    GPIOA
#define SEG_F_PIN     GPIO_PIN_8
#define SEG_G_PORT    GPIOA
#define SEG_G_PIN     GPIO_PIN_9
#define SEG_DP_PORT   GPIOB
#define SEG_DP_PIN    GPIO_PIN_15

/* ===== 4 根位选引脚（DIG1 最左 … DIG4 最右）===== */
#define DIG1_PORT     GPIOA
#define DIG1_PIN      GPIO_PIN_3
#define DIG2_PORT     GPIOA
#define DIG2_PIN      GPIO_PIN_4
#define DIG3_PORT     GPIOA
#define DIG3_PIN      GPIO_PIN_5
#define DIG4_PORT     GPIOA
#define DIG4_PIN      GPIO_PIN_6

/*
 * 位选有效电平：
 * 1 = 高电平选中（共阴管经 NPN 低边驱动，或共阳管公共脚直连 GPIO）
 * 0 = 低电平选中（共阴管公共脚直连 GPIO，不推荐，电流可能超 GPIO 限额）
 */
#define SEG_DIG_USE_TRANSISTOR  1

/*
 * 位选左右顺序：
 * 0 = 正常顺序（PA3=DIG1 最左 ... PA6=DIG4 最右）
 * 1 = 反序（适用于位选线左右接反的板子）
 */
#ifndef SEG_DIG_ORDER_REVERSED
#define SEG_DIG_ORDER_REVERSED  0
#endif

/*
 * 段码极性：
 * 0 = 共阴（段码高电平点亮）
 * 1 = 共阳（段码低电平点亮，驱动内部自动取反）
 */
#ifndef SEG_COMMON_ANODE
#define SEG_COMMON_ANODE  1
#endif

/* Segment_ShowDigit() 的特殊字符编号 */
#define SEG_CHAR_DASH       16
#define SEG_CHAR_BLANK      17

/**
 * 初始化数码管段选、位选引脚，并清空显示
 */
void Segment_Init(void);

/**
 * 清空显示缓冲区并关闭所有位选
 */
void Segment_Clear(void);

/**
 * 更新某一位的显示内容（只写缓冲区，需由 Segment_Scan() 输出）
 * @param pos   位号 0~3，0 为最左边 DIG1
 * @param value 0~15 对应 0~9/A~F；16='-'；17=空格
 * @param dot   1=该位小数点亮，0=灭
 */
void Segment_ShowDigit(uint8_t pos, uint8_t value, uint8_t dot);

/**
 * 显示 4 位十进制数（只写缓冲区，需由 Segment_Scan() 输出）
 * @param number  0~9999，超过范围按 9999 处理
 * @param dot_pos 0=无小数点；1/2/3/4=个/十/百/千位带小数点
 */
void Segment_ShowNum(uint16_t number, uint8_t dot_pos);

/**
 * 显示最多 4 个字符（只写缓冲区，需由 Segment_Scan() 输出）
 * @param p_str 以 '\0' 结尾的字符串；支持 0~9、A~F、
 *              O/P/E/N/H/r/S/t/d/L/C/U、'-'、空格和 '.'
 */
void Segment_ShowString(const char *p_str);

/**
 * 动态扫描一位，必须在 1ms 定时中断中调用
 * 每次调用刷新一位，4 次完成一帧，每位刷新率 250Hz
 */
void Segment_Scan(void);
