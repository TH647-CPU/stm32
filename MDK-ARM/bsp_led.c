/* bsp_led.c —— 恒流 LED 驱动实现 */
#include "bsp_led.h"
#include "bsp_pwm.h"
#include "bsp_adc.h"

static volatile uint16_t led_target_ma = 0u;   /* 0=关闭 */
static volatile uint16_t led_meas_ma   = 0u;   /* 实测电流 mA */
static int32_t led_pi_integral = 0;

/**
 * @brief  初始化恒流控制
 */
void LED_Init(void)
{
    led_target_ma  = 0u;
    led_meas_ma    = 0u;
    led_pi_integral = 0;
    PWM_SetDuty(0u);               /* 上电默认关闭 */
}

void LED_SetTarget(uint16_t mA)
{
    if (mA == 0u)
    {
        led_target_ma = 0u;
        return;
    }
    if (mA < LED_CUR_MIN_MA) mA = LED_CUR_MIN_MA;
    if (mA > LED_CUR_MAX_MA) mA = LED_CUR_MAX_MA;
    led_target_ma = mA;
}

uint16_t LED_GetTarget(void) { return led_target_ma; }
uint16_t LED_GetMeas(void)   { return led_meas_ma; }
uint8_t  LED_IsOn(void)      { return (led_target_ma != 0u) ? 1u : 0u; }

/**
 * @brief  恒流控制节拍，每 20ms 调用一次
 * @note   1) ADC2 采样采样电阻电压 → 折算实测电流
 *         2) 目标电流 0 → PWM 置 0（关断）
 *         3) 目标非 0 → 开环前馈 + PI 闭环逼近目标
 */
void LED_ControlTick(void)
{
    uint16_t raw;
    float    volt_mv;
    int32_t  meas;
    int32_t  err;

    /* ---- 1. 采样电流 ---- */
    raw = ADC2_Read();
    volt_mv = (float)raw * LED_ADC_VREF_MV / LED_ADC_MAX_CNT;
    meas = (int32_t)(volt_mv / LED_RSENSE_OHM);      /* I(mA) = U(mV)/R(Ω) */
    if (meas < 0) meas = 0;
    led_meas_ma = (uint16_t)meas;

    /* ---- 2. 关闭输出 ---- */
    if (led_target_ma == 0u)
    {
        PWM_SetDuty(0u);
        led_pi_integral = 0;
        return;
    }

    /* ---- 3. 开环前馈：目标电流 → 基础 PWM 计数 ---- */
    {
        uint32_t base = ((uint32_t)led_target_ma * (uint32_t)PWM_MAX_DUTY) /
                        (uint32_t)LED_CUR_AT_FULL_DUTY_MA;

        if (LED_CLOSED_LOOP_ENABLE)
        {
            /* PI 闭环：根据实测与目标误差修正 PWM */
            err = (int32_t)led_target_ma - (int32_t)led_meas_ma;
            led_pi_integral += err;
            if (led_pi_integral >  LED_PI_INT_LIMIT) led_pi_integral =  LED_PI_INT_LIMIT;
            if (led_pi_integral < -LED_PI_INT_LIMIT) led_pi_integral = -LED_PI_INT_LIMIT;

            {
                int32_t corr = (int32_t)(LED_PI_KP * (float)err) +
                               (int32_t)(LED_PI_KI * (float)led_pi_integral);
                int32_t ccr = (int32_t)base + corr;
                if (ccr < (int32_t)LED_PI_OUT_MIN_CCR) ccr = (int32_t)LED_PI_OUT_MIN_CCR;
                if (ccr > (int32_t)LED_PI_OUT_MAX_CCR) ccr = (int32_t)LED_PI_OUT_MAX_CCR;
                PWM_SetDuty((uint16_t)ccr);
            }
        }
        else
        {
            /* 纯开环：直接按满占空比电流换算 */
            if (base < (uint32_t)LED_PI_OUT_MIN_CCR) base = (uint32_t)LED_PI_OUT_MIN_CCR;
            if (base > (uint32_t)LED_PI_OUT_MAX_CCR) base = (uint32_t)LED_PI_OUT_MAX_CCR;
            PWM_SetDuty((uint16_t)base);
        }
    }
}
