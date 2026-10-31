/* bsp_pwm.c —— TIM2_CH1 PWM 输出封装 */
#include "bsp_pwm.h"

/* CubeMX 生成的 TIM2 句柄，在 tim.c 中定义 */
extern TIM_HandleTypeDef htim2;

/**
 * @brief  设置 TIM2_CH1（PA0）的 PWM 比较值
 * @param  ccr：比较值，0 表示全灭，999 表示全亮
 * @retval 无
 */
void PWM_SetDuty(uint16_t ccr)
{
    /* 限幅保护：CCR 不得超过 ARR */
    if (ccr > PWM_MAX_DUTY)
    {
        ccr = PWM_MAX_DUTY;
    }

    /* 直接写 TIM2_CH1 比较寄存器 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
}