/* bsp_led.h —— 恒流 LED 驱动：目标电流 + ADC 实时采样 + PI 闭环输出 PWM
 *
 * 硬件前提（对应“5V 输入输出 20mA~200mA 恒流 + PWM 调光”方案）：
 *   PA0(TIM2_CH1) --PWM--> RC 滤波 --Vset--> 运放恒流源
 *   运放使 I = Vset / Rsense，LED 电流被恒定在设定值
 *   Rsense 上端电压接到 PA2(ADC2_IN2) 用于实时采样电流
 */
#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f1xx_hal.h"

/* ===== 电流档位 ===== */
#define LED_CUR_MIN_MA      20u      /* 最小目标电流 20mA */
#define LED_CUR_MAX_MA      200u     /* 最大目标电流 200mA */

/* ===== 采样电阻与参考电压（按你的实际硬件改）=====
   LED_CUR_AT_FULL_DUTY_MA = 满占空比(PWM=999)时的理论电流
   = Vref / Rsense，用于开环前馈，加快启动收敛 */
#define LED_RSENSE_OHM        10.0f   /* 电流采样电阻，单位 Ω */
#define LED_ADC_VREF_MV       3300.0f /* ADC 参考电压，单位 mV */
#define LED_ADC_MAX_CNT       4095.0f /* 12 位满量程 */
#define LED_CUR_AT_FULL_DUTY_MA ((uint16_t)(LED_ADC_VREF_MV / LED_RSENSE_OHM)) /* =330mA */

/* ===== PI 闭环参数（可按实际手感微调）=====
   比例 Kp：每 1mA 误差补偿的 PWM 计数
   积分 Ki：消除稳态误差
   若硬件 PWM→电流已是线性且稳定，可把 LED_CLOSED_LOOP_ENABLE 改 0 走纯开环 */
#define LED_CLOSED_LOOP_ENABLE  1u
#define LED_PI_KP               3.0f
#define LED_PI_KI               0.3f
#define LED_PI_INT_LIMIT        200   /* 积分限幅 */
#define LED_PI_OUT_MIN_CCR      10u   /* 最小 PWM，避免瞬态完全关断 */
#define LED_PI_OUT_MAX_CCR      999u  /* 最大 PWM，等于 ARR */

void LED_Init(void);

/* 设置目标电流：0=关闭输出；20~200=目标毫安 */
void LED_SetTarget(uint16_t mA);

uint16_t LED_GetTarget(void);   /* 目标电流 mA，0 表示关闭 */
uint16_t LED_GetMeas(void);     /* 最近一次实测电流 mA */
uint8_t  LED_IsOn(void);        /* 目标是否为 0 */

/* 每 20ms 调用一次：采样 ADC2、跑恒流闭环、刷新 PWM */
void LED_ControlTick(void);

#endif /* __BSP_LED_H */
