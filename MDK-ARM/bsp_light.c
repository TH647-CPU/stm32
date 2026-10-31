/* bsp_light.c —— 光敏 ADC 一阶低通滤波 + ADC 区间到固定 PWM 的分段映射 */
#include "bsp_light.h"

/* ===== ADC 区间表：4096 分成 8 段，每段 512；ADC 越大（越暗）→ PWM 越大 ===== */
/* 环境越亮 ADC 越小 → 段号越小 → PWM 越小 → LED 越暗；最亮段 PWM=0 全灭 */
static const LightLevel_t light_level_table[] =
{
    {   0u, 1023u, 999u },   /* 段0 ：环境最亮，LED 全亮，宽 1024（两倍） */
    {1024u, 1279u, 916u },   /* 段1 ，宽 256 */
    {1280u, 1535u, 833u },   /* 段2 ，宽 256 */
    {1536u, 1791u, 749u },   /* 段3 ，宽 256 */
    {1792u, 2047u, 666u },   /* 段4 ，宽 256 */
    {2048u, 2303u, 583u },   /* 段5 ，宽 256 */
    {2304u, 2559u, 499u },   /* 段6 ，宽 256 */
    {2560u, 2815u, 416u },   /* 段7 ，宽 256 */
    {2816u, 3071u, 333u },   /* 段8 ，宽 256 */
    {3072u, 3327u, 249u },   /* 段9 ，宽 256 */
    {3328u, 3583u, 166u },   /* 段10，宽 256 */
    {3584u, 3839u,  83u },   /* 段11，宽 256 */
    {3840u, 4095u,   0u },   /* 段12：环境最暗，LED 全灭，宽 256 */
};

/**
 * @brief  一阶低通滤波：新值占 1/2，历史值占 1/2，抑制抖动且响应比 1/4 快一倍
 * @param  new_val：本次 ADC 原始采样值（0~4095）
 * @retval 滤波后的 ADC 值（0~4095）
 */
uint16_t Light_Filter(uint16_t new_val)
{
    static uint32_t light_filter_buf   = 0u;  /* 滤波历史值寄存器 */
    static uint8_t  light_filter_first = 1u;  /* 首次采样标志，用于赋初值 */

    /* 限幅保护：ADC 值不得超过 12 位满量程 */
    if (new_val > LIGHT_ADC_MAX)
    {
        new_val = LIGHT_ADC_MAX;
    }

    if (light_filter_first != 0u)
    {
        /* 第一次采样直接作为初值，避免从 0 慢慢爬升 */
        light_filter_buf   = (uint32_t)new_val;
        light_filter_first = 0u;
    }
    else
    {
        /* 一阶低通：历史值 * OLD + 新值 * NEW，再除以权重总和 */
        light_filter_buf = ((light_filter_buf * LIGHT_FILTER_OLD_WEIGHT) +
                            ((uint32_t)new_val * LIGHT_FILTER_NEW_WEIGHT))
                           / LIGHT_FILTER_TOTAL;
    }

    return (uint16_t)light_filter_buf;
}

/**
 * @brief  ADC 值 → 固定 PWM 比较值：ADC 落在哪个区间就直接输出该区间的固定 PWM
 *         环境越亮 ADC 越小 → 输出 PWM 越小；环境越暗 ADC 越大 → 输出 PWM 越大
 * @param  adc_val：滤波后的 ADC 值（0~4095）
 * @retval TIM2_CH1 比较值 CCR（0~999）
 */
uint16_t Light_AdcToPwm(uint16_t adc_val)
{
    uint8_t i;         /* 区间表遍历索引 */
    uint8_t table_len; /* 区间表项数 */

    /* 限幅保护：ADC 值不得超过 12 位满量程 */
    if (adc_val > LIGHT_ADC_MAX)
    {
        adc_val = LIGHT_ADC_MAX;
    }

    /* 计算区间表项数 */
    table_len = (uint8_t)(sizeof(light_level_table) / sizeof(light_level_table[0]));

    /* 遍历区间表，命中哪一段就直接返回该段的固定 PWM 值 */
    for (i = 0u; i < table_len; i++)
    {
        if ((adc_val >= light_level_table[i].adc_low) &&
            (adc_val <= light_level_table[i].adc_high))
        {
            return light_level_table[i].pwm_val;
        }
    }

    /* 理论上不会执行到这里；兜底按最暗处理，输出全亮 */
    return LIGHT_PWM_MAX;
}