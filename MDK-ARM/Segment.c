#include "Segment.h"

#include <stddef.h>

/* 共阴段码表：bit0=A ... bit6=G，bit7=DP */
static const uint8_t s_seg_code[18] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
    0x40, 0x00
};

/* 段码位到 GPIO 引脚的映射（端口 + 引脚），顺序与段码 bit0~bit7 一致 */
static GPIO_TypeDef* const s_seg_port[8] = {
    SEG_A_PORT, SEG_B_PORT, SEG_C_PORT, SEG_D_PORT,
    SEG_E_PORT, SEG_F_PORT, SEG_G_PORT, SEG_DP_PORT
};
static const uint16_t s_seg_pins[8] = {
    SEG_A_PIN, SEG_B_PIN, SEG_C_PIN, SEG_D_PIN,
    SEG_E_PIN, SEG_F_PIN, SEG_G_PIN, SEG_DP_PIN
};

/* 位选引脚，下标 0 对应最左边 DIG1 */
#if SEG_DIG_ORDER_REVERSED
static GPIO_TypeDef* const s_dig_port[4] = { DIG4_PORT, DIG3_PORT, DIG2_PORT, DIG1_PORT };
static const uint16_t     s_dig_pins[4] = { DIG4_PIN,  DIG3_PIN,  DIG2_PIN,  DIG1_PIN  };
#else
static GPIO_TypeDef* const s_dig_port[4] = { DIG1_PORT, DIG2_PORT, DIG3_PORT, DIG4_PORT };
static const uint16_t     s_dig_pins[4] = { DIG1_PIN,  DIG2_PIN,  DIG3_PIN,  DIG4_PIN  };
#endif

/* 显示缓冲区，每个字节保存一位的段码；主循环写，扫描读 */
static volatile uint8_t s_display[4];

/* 当前扫描位 */
static uint8_t s_scan_pos;

/**
 * 将段码输出到 8 根段选引脚
 * @param code 段码，bit0=A ... bit6=G，bit7=DP
 */
static void segment_write_segments(uint8_t code)
{
    uint8_t i;

#if SEG_COMMON_ANODE
    /* 共阳管段码低电平点亮，存入缓冲的是共阴逻辑码，输出前整体取反 */
    code = (uint8_t)~code;
#endif

    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(s_seg_port[i], s_seg_pins[i],
                          (code & (uint8_t)(1u << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

/**
 * 关闭所有位选，用于换位前消隐
 */
static void segment_all_digits_off(void)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
#if SEG_DIG_USE_TRANSISTOR
        /* 位选高有效（晶体管低边驱动/共阳直连）：关 = 低 */
        HAL_GPIO_WritePin(s_dig_port[i], s_dig_pins[i], GPIO_PIN_RESET);
#else
        /* 位选低有效（共阴直连）：关 = 高 */
        HAL_GPIO_WritePin(s_dig_port[i], s_dig_pins[i], GPIO_PIN_SET);
#endif
    }
}

/**
 * 选通指定的一位
 * @param pos 0~3，0 为最左边 DIG1
 */
static void segment_select_digit(uint8_t pos)
{
#if SEG_DIG_USE_TRANSISTOR
    HAL_GPIO_WritePin(s_dig_port[pos], s_dig_pins[pos], GPIO_PIN_SET);   /* 高有效：开 = 高 */
#else
    HAL_GPIO_WritePin(s_dig_port[pos], s_dig_pins[pos], GPIO_PIN_RESET); /* 低有效：开 = 低 */
#endif
}

/**
 * 将 ASCII 字符转换为段码
 * @param ch 待显示字符
 * @return 段码；无法显示时返回空白
 */
static uint8_t segment_char_to_code(char ch)
{
    switch (ch)
    {
        case '0': return 0x3F;
        case '1': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;
        case 'A': case 'a': return 0x77;
        case 'B': case 'b': return 0x7C;
        case 'C': case 'c': return 0x39;
        case 'D': case 'd': return 0x5E;
        case 'E': case 'e': return 0x79;
        case 'F': case 'f': return 0x71;
        case 'G': case 'g': return 0x7D;
        case 'H': case 'h': return 0x76;
        case 'L': case 'l': return 0x38;
        case 'N': case 'n': return 0x54;
        case 'O': case 'o': return 0x3F;
        case 'P': case 'p': return 0x73;
        case 'R': case 'r': return 0x50;
        case 'S': case 's': return 0x6D;
        case 'T': case 't': return 0x78;
        case 'U': case 'u': return 0x3E;
        case '-': return 0x40;
        case ' ': return 0x00;
        default:  return 0x00;
    }
}

/**
 * 初始化段选、位选引脚，并熄灭显示
 */
void Segment_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t i;

    /* 使能 GPIOA/GPIOB 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 初始化前先输出熄灭电平，避免上电瞬间亮一下 */
    segment_write_segments(0x00);
    segment_all_digits_off();

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;

    for (i = 0; i < 8; i++)
    {
        gpio.Pin = s_seg_pins[i];
        HAL_GPIO_Init(s_seg_port[i], &gpio);
    }
    for (i = 0; i < 4; i++)
    {
        gpio.Pin = s_dig_pins[i];
        HAL_GPIO_Init(s_dig_port[i], &gpio);
    }

    Segment_Clear();
}

/**
 * 清空显示缓冲区并关闭所有位选
 */
void Segment_Clear(void)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        s_display[i] = s_seg_code[SEG_CHAR_BLANK];
    }

    segment_write_segments(0x00);
    segment_all_digits_off();
}

/**
 * 更新某一位的显示内容（只写缓冲区，由 Segment_Scan() 输出）
 * @param pos   位号 0~3，0 为最左边 DIG1
 * @param value 0~15 对应 0~9/A~F；16='-'；17=空格
 * @param dot   1=该位小数点亮，0=灭
 */
void Segment_ShowDigit(uint8_t pos, uint8_t value, uint8_t dot)
{
    uint8_t code;

    if (pos > 3)
    {
        return;
    }

    if (value > SEG_CHAR_BLANK)
    {
        value = SEG_CHAR_BLANK;
    }

    code = s_seg_code[value];
    if (dot != 0)
    {
        code |= 0x80;
    }

    s_display[pos] = code;
}

/**
 * 显示 4 位十进制数（只写缓冲区，由 Segment_Scan() 输出）
 * @param number  0~9999，超过范围按 9999 处理
 * @param dot_pos 0=无小数点；1/2/3/4=个/十/百/千位带小数点
 */
void Segment_ShowNum(uint16_t number, uint8_t dot_pos)
{
    uint8_t digit[4];
    uint8_t i;

    if (number > 9999)
    {
        number = 9999;
    }

    digit[0] = (uint8_t)(number / 1000);
    digit[1] = (uint8_t)(number / 100 % 10);
    digit[2] = (uint8_t)(number / 10 % 10);
    digit[3] = (uint8_t)(number % 10);

    for (i = 0; i < 4; i++)
    {
        Segment_ShowDigit(i, digit[i], (dot_pos == (uint8_t)(4 - i)) ? 1 : 0);
    }
}

/**
 * 显示最多 4 个字符（只写缓冲区，由 Segment_Scan() 输出）
 */
void Segment_ShowString(const char *p_str)
{
    uint8_t pos;
    uint8_t code;
    char ch;

    for (pos = 0; pos < 4; pos++)
    {
        s_display[pos] = s_seg_code[SEG_CHAR_BLANK];
    }

    if (p_str == NULL)
    {
        return;
    }

    pos = 0;
    while ((*p_str != '\0') && (pos < 4))
    {
        ch = *p_str;
        p_str++;

        if (ch == '.')
        {
            if (pos > 0)
            {
                s_display[pos - 1] |= 0x80;
            }
            continue;
        }

        code = segment_char_to_code(ch);
        s_display[pos] = code;
        pos++;
    }
}

/**
 * 动态扫描一位，每 1ms 调用一次
 * 每次调用刷新一位，4 次完成一帧，每位刷新率 250Hz
 */
void Segment_Scan(void)
{
    uint8_t code;

    /* 先关全部位选，避免上一位的段码串到新位 */
    segment_all_digits_off();

    code = s_display[s_scan_pos];
    segment_write_segments(code);
    segment_select_digit(s_scan_pos);

    s_scan_pos++;
    if (s_scan_pos >= 4)
    {
        s_scan_pos = 0;
    }
}
