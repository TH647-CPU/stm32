/* bsp_pwm.h —— TIM2_CH1 PWM 输出封装头文件 */
#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "main.h"

/* PWM 占空比上限，等于 TIM2 的 ARR */
#define PWM_MAX_DUTY   999u

/* 设置 TIM2_CH1 的 PWM 比较值（占空比 0~999） */
void PWM_SetDuty(uint16_t ccr);

#endif /* __BSP_PWM_H */