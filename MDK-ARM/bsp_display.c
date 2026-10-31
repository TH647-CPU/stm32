/* bsp_display.c —— 4 位 7 段数码管 GPIO 动态扫描实现 */
#include "bsp_display.h"

/* 段码引脚表（顺序：a b c d e f g dp） */
static GPIO_TypeDef* const seg_port[8] =
{
    DISP_SEG_A_PORT, DISP_SEG_B_PORT, DISP_SEG_C_PORT, DISP_SEG_D_PORT,
    DISP_SEG_E_PORT, DISP_SEG_F_PORT, DISP_SEG_G_PORT, DISP_SEG_DP_PORT,
};
static const uint16_t seg_pin[8] =
{
    DISP_SEG_A_PIN, DISP_SEG_B_PIN, DISP_SEG_C_PIN, DISP_SEG_D_PIN,
    DISP_SEG_E_PIN, DISP_SEG_F_PIN, DISP_SEG_G_PIN, DISP_SEG_DP_PIN,
};

/* 位选引脚表（第 0~3 位） */
static GPIO_TypeDef* const dig_port[DISP_DIGITS] =
{
    DISP_DIG0_PORT, DISP_DIG1_PORT, DISP_DIG2_PORT, DISP_DIG3_PORT,
};
static const uint16_t dig_pin[DISP_DIGITS] =
{
    DISP_DIG0_PIN, DISP_DIG1_PIN, DISP_DIG2_PIN, DISP_DIG3_PIN,
};

/* 共阴段码表：bit0=a ... bit6=g，bit7=dp，1=点亮。
   0-9 及常用字母的段码（共阴）。共阳时在下方取反处理。 */
static const uint8_t seg_tab[DISP_G_NUM_GLYPHS] =
{
    /* BLANK */ 0x00,
    /* 0  1  2  3  4  5  6  7  8  9 */
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,
    /* A      U      t      O      F      -      H      d      L */
    0x77,   0x3E,   0x78,   0x3F,   0x71,   0x40,   0x76,   0x5E,   0x38,
};

/* 显示缓冲区：每个字形的字形索引与小数点标志 */
static uint8_t disp_glyph[DISP_DIGITS];
static uint8_t disp_dp[DISP_DIGITS];

/**
 * @brief  初始化全部段码与位选引脚为推挽输出
 */
void DISPLAY_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t i;

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    for (i = 0u; i < 8u; i++)
    {
        gpio.Pin = seg_pin[i];
        HAL_GPIO_Init(seg_port[i], &gpio);
        HAL_GPIO_WritePin(seg_port[i], seg_pin[i], DISP_SEG_OFF_LEVEL);
    }
    for (i = 0u; i < DISP_DIGITS; i++)
    {
        gpio.Pin = dig_pin[i];
        HAL_GPIO_Init(dig_port[i], &gpio);
        HAL_GPIO_WritePin(dig_port[i], dig_pin[i], DISP_DIG_OFF_LEVEL);
    }

    for (i = 0u; i < DISP_DIGITS; i++)
    {
        disp_glyph[i] = DISP_G_BLANK;
        disp_dp[i]    = 0u;
    }
}

/**
 * @brief  设置显示内容
 * @param  glyph : 4 个字形索引
 * @param  dp_mask : bit0~bit3 对应第 0~3 位小数点
 */
void DISPLAY_Set(const uint8_t glyph[4], uint8_t dp_mask)
{
    uint8_t i;
    for (i = 0u; i < DISP_DIGITS; i++)
    {
        disp_glyph[i] = glyph[i];
        disp_dp[i]    = (uint8_t)((dp_mask >> i) & 0x01u);
    }
}

/**
 * @brief  便捷显示非负整数（0~9999），右对齐，高位空位熄灭
 */
void DISPLAY_ShowNumber(uint16_t val, uint8_t dp_mask)
{
    uint8_t g[DISP_DIGITS] = { DISP_G_BLANK, DISP_G_BLANK, DISP_G_BLANK, DISP_G_BLANK };
    uint16_t a = val;
    uint8_t pos = DISP_DIGITS;

    do
    {
        pos--;
        g[pos] = (uint8_t)(DISP_G_0 + (a % 10u));
        a /= 10u;
    } while ((a > 0u) && (pos > 0u));

    DISPLAY_Set(g, dp_mask);
}

/**
 * @brief  动态扫描刷新，每 1ms 调用一次
 */
void DISPLAY_Tick(void)
{
    static uint8_t cur = 0u;
    uint8_t i;

    /* 1. 先关闭所有位，避免残影串扰 */
    for (i = 0u; i < DISP_DIGITS; i++)
    {
        HAL_GPIO_WritePin(dig_port[i], dig_pin[i], DISP_DIG_OFF_LEVEL);
    }

    /* 2. 写当前位的段码（含小数点） */
    {
        uint8_t seg = seg_tab[disp_glyph[cur]];
        if (disp_dp[cur] != 0u)
        {
            seg |= 0x80u;
        }
        for (i = 0u; i < 8u; i++)
        {
            uint8_t lit = (uint8_t)((seg >> i) & 0x01u);
            HAL_GPIO_WritePin(seg_port[i], seg_pin[i],
                              lit ? DISP_SEG_LIT_LEVEL : DISP_SEG_OFF_LEVEL);
        }
    }

    /* 3. 打开当前位 */
    HAL_GPIO_WritePin(dig_port[cur], dig_pin[cur], DISP_DIG_ON_LEVEL);

    /* 4. 指向下一位 */
    cur++;
    if (cur >= DISP_DIGITS) cur = 0u;
}
