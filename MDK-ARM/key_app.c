/* key_app.c —— 恒流 LED 全功能应用层（推翻重来 · 全功能完整版）
 *
 * 功能（全部真正接通，无“未启用”半成品）：
 *   1) 恒流闭环：LED_ControlTick() 每 20ms 运行，把实测电流闭环稳定到目标值（20~200mA）
 *   2) 手动调光：KEY_DIM 短按循环 4 档亮度（100/75/50/25% → 200/150/100/50mA）
 *   3) 自动光控：KEY_MODE 切换，光敏 ADC 自动调目标电流（环境暗→亮，亮→暗），同样走恒流闭环
 *   4) 定时开关：KEY_DIM 长按(≥1s) 启动/关闭定时，到点自动关机
 *   5) 显示：数码管实时显示实测电流，OLED 显示 电流/目标/模式/开关/定时
 *
 * 按键（引脚见 bsp_key.h）：
 *   KEY_POWER(PB12) 短按  = 输出 开/关
 *   KEY_MODE (PB13) 短按  = 手动 ↔ 自动光控
 *   KEY_DIM  (PB14) 短按  = 循环 4 档亮度；长按(≥1s) = 定时开关
 */
#include "key_app.h"
#include "bsp_key.h"
#include "bsp_pwm.h"
#include "bsp_adc.h"
#include "bsp_light.h"
#include "bsp_led.h"
#include "Segment.h"
#include "OLED.h"

/* ===== 长按判定时间 ===== */
#define KEYAPP_LONG_PRESS_MS    1000u   /* 长按 1 秒 */

/* 亮度档位表：百分比（100=最亮） */
static const uint8_t ka_step_tbl[4] = {100u, 75u, 50u, 25u};

/* 档位百分比 → 目标电流 mA：百分比 × 最大电流 200mA / 100 */
static uint16_t ka_step_to_ma(uint8_t pct)
{
    return (uint16_t)(((uint32_t)pct * (uint32_t)LED_CUR_MAX_MA) / 100u);
}

/* ===== 运行状态 ===== */
static uint8_t  ka_power_on   = 1u;    /* 输出开关：1开 0关 */
static uint8_t  ka_auto_light = 0u;    /* 0=手动 1=自动光控 */
static uint8_t  ka_step       = 0u;    /* 档位索引 0~3 */
static uint8_t  ka_brightness = 100u;  /* 当前档位 % */
static uint16_t ka_target_ma  = 200u;  /* 当前目标电流 mA */
static uint8_t  ka_timer_on   = 0u;    /* 定时开关 */
static uint8_t  ka_timer_sec  = 0u;    /* 定时剩余秒（倒计时到 0 自动关机） */

/* 调度时间戳 */
static uint32_t ka_t_key = 0u, ka_t_light = 0u, ka_t_sec = 0u,
                ka_t_led = 0u, ka_t_disp = 0u;

/* KEY_DIM 长按检测 */
static uint32_t ka_dim_t0    = 0u;   /* 按下起始时刻 */
static uint8_t  ka_dim_down  = 0u;   /* 当前是否按住 */
static uint8_t  ka_dim_long  = 0u;   /* 长按已触发 */

/* 内部函数声明 */
static void ka_key_power(void);        /* POWER：开关 */
static void ka_key_mode(void);         /* MODE：手动↔自动 */
static void ka_key_cycle(void);        /* DIM 短按：循环档位 */
static void ka_timer_toggle(void);     /* DIM 长按：定时开关 */
static void ka_apply_output(void);     /* 把当前状态落到目标电流 */
static void ka_light(void);            /* 自动光控：光敏→目标电流 */
static void ka_timer_1s(void);         /* 定时每秒倒计时 */
static void ka_handle_dim(uint32_t now);/* DIM 短按/长按区分 */
static void ka_disp_refresh(void);     /* 刷新数码管电流 + OLED 状态 */

/**
 * @brief  初始化按键、恒流、数码管、OLED
 */
void KEY_AppInit(void)
{
    KEY_Init();
    LED_Init();
    ka_power_on   = 1u;
    ka_auto_light = 0u;
    ka_step       = 0u;
    ka_brightness = 100u;               /* 上电默认第 1 档 100%（最亮） */
    ka_target_ma  = ka_step_to_ma(ka_brightness);
    ka_timer_on   = 0u;
    ka_timer_sec  = 0u;
    Segment_Init();                     /* 4 位数码管初始化 */
    OLED_Init();                        /* OLED 初始化 */
    ka_apply_output();
    ka_disp_refresh();
}

/**
 * @brief  按键处理主函数，主循环反复调用
 * @note   内部自动 5ms 扫键、200ms 光控、20ms 恒流闭环、1s 定时
 */
void KEY_AppHandle(void)
{
    uint32_t now = HAL_GetTick();

    /* 5ms：扫描按键 */
    if ((now - ka_t_key) >= 5u)
    {
        ka_t_key = now;
        KEY_Scan();
    }

    /* 按键动作 */
    if (KEY_IsPressed(KEY_POWER))        ka_key_power();
    if (KEY_IsPressed(KEY_MODE))         ka_key_mode();
    ka_handle_dim(now);                 /* KEY_DIM：短按档位 / 长按定时 */

    /* 200ms：自动光控重新计算目标电流 */
    if (ka_auto_light != 0u)
    {
        if ((now - ka_t_light) >= 200u)
        {
            ka_t_light = now;
            ka_light();
        }
    }

    /* 20ms：恒流闭环输出 PWM（始终运行，把实测电流闭环稳定到目标值） */
    if ((now - ka_t_led) >= 20u)
    {
        ka_t_led = now;
        LED_ControlTick();
    }

    /* 1s：定时倒计时 */
    if (ka_timer_on != 0u)
    {
        if ((now - ka_t_sec) >= 1000u)
        {
            ka_t_sec = now;
            ka_timer_1s();
        }
    }

    /* 数码管动态扫描（每次刷一位，4 次一帧） */
    Segment_Scan();

    /* 200ms：刷新数码管电流 + OLED 状态 */
    if ((now - ka_t_disp) >= 200u)
    {
        ka_t_disp = now;
        ka_disp_refresh();
    }
}

/* =====================================================================
 * 按键动作
 * ===================================================================== */

/** 开关：切换输出 开/关 */
static void ka_key_power(void)
{
    ka_power_on = (uint8_t)(ka_power_on ^ 1u);
    ka_apply_output();
}

/** 手动 ↔ 自动光控：二选一切换 */
static void ka_key_mode(void)
{
    ka_auto_light = (uint8_t)(ka_auto_light ^ 1u);
    if (ka_auto_light != 0u)
    {
        ka_light();              /* 立即算一次光控目标电流 */
    }
    ka_apply_output();
}

/** KEY_DIM 短按：单键循环 4 档亮度（手动模式下生效） */
static void ka_key_cycle(void)
{
    ka_step = (uint8_t)((ka_step + 1u) % 4u);
    ka_brightness = ka_step_tbl[ka_step];
    ka_apply_output();
}

/** KEY_DIM 长按：切换定时开关 */
static void ka_timer_toggle(void)
{
    if (ka_timer_on == 0u)
    {
        ka_timer_on  = 1u;
        ka_timer_sec = KEYAPP_TMR_DEFAULT_SEC;   /* 默认 30 秒 */
        if (ka_power_on == 0u)
        {
            ka_power_on = 1u;      /* 开定时同时把输出打开 */
        }
        ka_apply_output();
    }
    else
    {
        ka_timer_on = 0u;
    }
}

/* =====================================================================
 * 输出 / 光控 / 定时
 * ===================================================================== */

/** 把当前状态落到目标电流（恒流闭环用） */
static void ka_apply_output(void)
{
    if (ka_power_on == 0u)
    {
        LED_SetTarget(0u);               /* 关灯 */
    }
    else if (ka_auto_light != 0u)
    {
        LED_SetTarget(ka_target_ma);     /* 自动：目标由 ka_light 维护 */
    }
    else
    {
        ka_target_ma = ka_step_to_ma(ka_brightness);
        LED_SetTarget(ka_target_ma);     /* 手动：档位对应目标电流 */
    }
}

/** 自动光控：光敏 ADC → 目标电流（环境暗→目标高→灯亮；亮→目标低→灯暗） */
static void ka_light(void)
{
    uint16_t adc = ADC1_Read();
    uint16_t flt = Light_Filter(adc);
    uint16_t pwm = Light_AdcToPwm(flt);   /* 0~999 */
    uint16_t ma  = (uint16_t)(((uint32_t)pwm * (uint32_t)LED_CUR_MAX_MA) /
                              (uint32_t)PWM_MAX_DUTY);

    ka_target_ma = ma;
    if (ka_power_on != 0u)
    {
        LED_SetTarget(ka_target_ma);
    }
}

/** 定时每秒：倒计时，到点自动关机 */
static void ka_timer_1s(void)
{
    if (ka_timer_on == 0u)
    {
        return;
    }
    if (ka_timer_sec > 0u)
    {
        ka_timer_sec--;
    }
    if (ka_timer_sec == 0u)
    {
        ka_power_on = 0u;      /* 到时自动关机 */
        ka_timer_on = 0u;
        LED_SetTarget(0u);
    }
}

/** KEY_DIM：短按=循环档位，长按(≥1s)=切换定时开关 */
static void ka_handle_dim(uint32_t now)
{
    if (KEY_IsPressed(KEY_DIM))
    {
        ka_dim_down = 1u;
        ka_dim_long = 0u;
        ka_dim_t0   = now;
    }
    if (ka_dim_down != 0u)
    {
        /* 长按判定：按住 ≥ 1s 且未触发过 */
        if ((ka_dim_long == 0u) && ((now - ka_dim_t0) >= KEYAPP_LONG_PRESS_MS))
        {
            ka_dim_long = 1u;
            ka_timer_toggle();
        }
        /* 松开 */
        if (KEY_IsDown(KEY_DIM) == 0u)
        {
            ka_dim_down = 0u;
            if (ka_dim_long == 0u)
            {
                ka_key_cycle();          /* 未达长按阈值 → 视为短按：循环档位 */
            }
        }
    }
}

/* =====================================================================
 * 显示
 * ===================================================================== */

/** 刷新数码管（实时电流）与 OLED（电流/目标/模式/开关/定时） */
static void ka_disp_refresh(void)
{
    uint16_t ma = LED_GetMeas();         /* 实测电流 mA（恒流闭环已换算好） */

    /* 数码管：实时显示实测电流 */
    Segment_ShowNum(ma, 0);

    /* OLED 行 1：实测电流 */
    OLED_ShowString(1, 1, "I:   mA");
    OLED_ShowNum(1, 3, ma, 3);

    /* OLED 行 2：目标电流 */
    OLED_ShowString(2, 1, "Tgt:");
    OLED_ShowNum(2, 5, LED_GetTarget(), 3);
    OLED_ShowString(2, 8, "mA");

    /* OLED 行 3：模式 */
    if (ka_auto_light != 0u)
    {
        OLED_ShowString(3, 1, "Mode:AUTO");
    }
    else
    {
        OLED_ShowString(3, 1, "Mode:MANU");
    }

    /* OLED 行 4：开关 + 定时 */
    if (ka_power_on != 0u)
    {
        OLED_ShowString(4, 1, "PWR:ON ");
    }
    else
    {
        OLED_ShowString(4, 1, "PWR:OFF");
    }
    if (ka_timer_on != 0u)
    {
        OLED_ShowString(4, 9, "T:");
        OLED_ShowNum(4, 11, ka_timer_sec, 2);
    }
    else
    {
        OLED_ShowString(4, 9, "T:--");
    }
}

/* ===== 供外部读取的状态（兼容 key_app.h 声明）===== */
uint8_t  KEYAPP_IsPowerOn(void)      { return ka_power_on; }
uint8_t  KEYAPP_IsAutoLight(void)    { return ka_auto_light; }
uint8_t  KEYAPP_GetDimMode(void)     { return 1u; }        /* 恒流闭环始终启用 */
uint16_t KEYAPP_GetBrightness(void)  { return ka_brightness; }
uint16_t KEYAPP_GetTimerMin(void)    { return ka_timer_sec; }
uint8_t  KEYAPP_IsTimerRunning(void) { return ka_timer_on; }
