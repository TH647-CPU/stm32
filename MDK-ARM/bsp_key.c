/* bsp_key.c —— 按键扫描驱动实现
 * 引脚表：PB12(开关)  PB13(手动↔自动光控)  PB14(档位调节)
 */
#include "bsp_key.h"

/* 按键引脚表：KEY_POWER=PB12, KEY_MODE=PB13, KEY_DIM=PB14 */
static GPIO_TypeDef* const key_port[KEY_NUM] =
{ GPIOB, GPIOB, GPIOB };
static const uint16_t key_pin[KEY_NUM] =
{ GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14 };

/* 滤波后的电平状态：1=按下，0=松开 */
static uint8_t  key_state[KEY_NUM];
/* 按下沿标志（一次性，读后清零） */
static uint8_t  key_edge[KEY_NUM];
/* 连发触发标志（一次性，读后清零） */
static uint8_t  key_repeat[KEY_NUM];
/* 按住计数（每次 KEY_Scan 计一次，即 5ms 一拍） */
static uint16_t key_cnt[KEY_NUM];

/**
 * @brief  初始化按键引脚（配置内部上拉，主动屏蔽噪声）
 * @note   直接用寄存器配置（不依赖 HAL_GPIO_Init），确保上拉真正生效
 */
void KEY_Init(void)
{
    uint8_t i;

    /* 使能按键用到的 GPIO 时钟（GPIOA、GPIOB），避免未在 CubeMX 配置时无法使用 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 直接寄存器配置 PB12/PB13/PB14 = 输入 + 内部上拉
       F1 每脚在 CRH 占 4bit = CNF[3:2] MODE[1:0]
       输入上拉/下拉 = CNF=10b、MODE=00b → 0b1000 = 0x8
       PB12 offset 16 / PB13 20 / PB14 24 */
    GPIOB->CRH &= ~(0xFu << 16);  GPIOB->CRH |= (0x8u << 16);   /* PB12 输入PU/PD */
    GPIOB->CRH &= ~(0xFu << 20);  GPIOB->CRH |= (0x8u << 20);   /* PB13 输入PU/PD */
    GPIOB->CRH &= ~(0xFu << 24);  GPIOB->CRH |= (0x8u << 24);   /* PB14 输入PU/PD */
    /* F1 内部上拉由 ODR 位决定：置 1 = 上拉 */
    GPIOB->BSRR = (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14);

    for (i = 0u; i < KEY_NUM; i++)
    {
        key_edge[i]   = 0u;
        key_repeat[i] = 0u;
        key_cnt[i]    = 0u;

        /* 关键修复：把“当前实际电平”作为初始状态，而不是默认“未按下”。
           这样上电时若引脚已经是低电平（按下/被拉低/悬空噪声），
           第一次扫描不会把它误判成“刚按下”，避免上电即误触发按键动作。 */
        key_state[i] = (uint8_t)((HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET) ? 1u : 0u);
        if (KEY_ACTIVE_LOW == 0u) key_state[i] ^= 1u;
    }
}

/**
 * @brief  按键扫描，每 5ms 调用一次
 * @note   采用电平比较法检测变化沿，无抖动采样时间，天然自带消抖
 */
void KEY_Scan(void)
{
    uint8_t i;

    for (i = 0u; i < KEY_NUM; i++)
    {
        uint8_t raw = (uint8_t)((HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET) ? 1u : 0u);
        /* KEY_ACTIVE_LOW=0（高有效）时，按下=高电平，把电平翻转统一成“1=按下” */
        if (KEY_ACTIVE_LOW == 0u)
        {
            raw = (uint8_t)(raw ^ 1u);
        }

        if (raw != key_state[i])
        {
            /* 电平发生变化：记录新状态并复位计数 */
            key_state[i] = raw;
            key_cnt[i]   = 0u;
            key_repeat[i] = 0u;

            if (raw == 1u)
            {
                key_edge[i] = 1u;   /* 刚按下，产生一次按下沿 */
                key_cnt[i]  = 1u;
            }
        }
        else if (raw == 1u)
        {
            /* 持续按住：计数累加，达到阈值后按固定间隔产生连发 */
            if (key_cnt[i] < 0xFFFFu) key_cnt[i]++;

            if ((key_cnt[i] >= KEY_REPEAT_START) &&
                (((uint32_t)(key_cnt[i] - KEY_REPEAT_START) % KEY_REPEAT_STEP) == 0u))
            {
                key_repeat[i] = 1u;  /* 连发触发点 */
            }
        }
    }
}

uint8_t KEY_IsPressed(uint8_t idx)
{
    uint8_t e;
    if (idx >= KEY_NUM) return 0u;
    e = key_edge[idx];
    key_edge[idx] = 0u;
    return e;
}

uint8_t KEY_IsHeld(uint8_t idx)
{
    uint8_t r;
    if (idx >= KEY_NUM) return 0u;
    r = key_repeat[idx];
    key_repeat[idx] = 0u;
    return r;
}

uint8_t KEY_IsDown(uint8_t idx)
{
    if (idx >= KEY_NUM) return 0u;
    return key_state[idx];
}
