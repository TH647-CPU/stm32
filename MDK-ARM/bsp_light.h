/* bsp_light.h —— 光敏采样与 PWM 分段映射头文件 */
#ifndef __BSP_LIGHT_H
#define __BSP_LIGHT_H

#include "main.h"

/* ===== 一阶低通滤波系数：新值占 1/2，历史值占 1/2（比原来 1/4 响应快一倍） ===== */
/* 想彻底不滤波：把 NEW 改 1、OLD 改 0、TOTAL 改 1 即可 */
#define LIGHT_FILTER_NEW_WEIGHT   1u   /* 新采样值权重 */
#define LIGHT_FILTER_OLD_WEIGHT   1u   /* 历史滤波值权重 */
#define LIGHT_FILTER_TOTAL        2u   /* 权重总和 */

/* ===== 量程定义 ===== */
#define LIGHT_ADC_MAX             4095u  /* ADC 12 位满量程 */
#define LIGHT_PWM_MAX             999u   /* TIM2 ARR=999，占空比 0~999 */

/* ===== ADC 区间 → 固定 PWM 表项结构 ===== */
typedef struct
{
    uint16_t adc_low;    /* 区间下限（含） */
    uint16_t adc_high;   /* 区间上限（含） */
    uint16_t pwm_val;    /* 该区间对应的固定 PWM 比较值 */
} LightLevel_t;

/* 一阶低通滤波函数：输入本次 ADC 原始值，返回滤波后的 ADC 值 */
uint16_t Light_Filter(uint16_t new_val);

/* ADC 值 → 固定 PWM 比较值函数：ADC 落在哪个区间就输出该区间的固定 PWM */
uint16_t Light_AdcToPwm(uint16_t adc_val);

#endif /* __BSP_LIGHT_H */

