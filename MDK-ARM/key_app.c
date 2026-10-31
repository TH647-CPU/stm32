/* key_app.c —— 6 键按键处理实现
 * 依赖工程里已有的外设接口：
 *   bsp_pwm.c  ：PWM_SetDuty()            （PWM 直调模式）
 *   bsp_light.c：Light_Filter()/Light_AdcToPwm() （自动光控映射）
 *   bsp_adc.c  ：ADC1_Read()              （光敏采样）
 *   bsp_led.c  ：LED_SetTarget()/LED_IsOn() （恒流闭环模式，20~200mA）
 * 若你暂时不要恒流闭环，把 DIM 模式 1 里的 LED_SetTarget 调用去掉即可。
 */
#include "key_app.h"
#include "bsp_key.h"
#include "bsp_pwm.h"
#include "bsp_adc.h"
#include "bsp_light.h"
#include "bsp_led.h"
#include "bsp_display.h"
#include "Segment.h"
#include "OLED.h"

/* ===== 状态 ===== */
static uint8_t  ka_power_on    = 1u;        /* 输出开关：1 开 0 关 */
static uint8_t  ka_auto_light  = 0u;        /* 0=手动调光，1=自动光控 */
static uint8_t  ka_dim_mode    = 0u;        /* 0=PWM 直调，1=恒流闭环 */
static uint16_t ka_brightness  = 50u;       /* 手动亮度 0~100 */
static uint16_t ka_timer_min   = KEYAPP_TMR_DEFAULT_MIN;
static uint8_t  ka_timer_run   = 0u;        /* 定时是否运行 */
static uint32_t ka_timer_sec   = 0u;        /* 剩余秒 */
static uint16_t ka_light_ccr   = 999u;      /* 最近一次光控算出的 PWM */
static uint8_t  ka_step        = 0u;        /* 单键循环档位索引 0~3 */

/* 调度时间戳 */
static uint32_t ka_t_key = 0u, ka_t_light = 0u, ka_t_sec = 0u, ka_t_led = 0u, ka_t_disp = 0u;

/* ===== 内部函数 ===== */
static void ka_key_power(void);     /* KEY_POWER  开关 */
static void ka_key_mode(void);      /* KEY_MODE   手动↔自动光控 */
static void ka_key_cycle(void);     /* KEY_DIM(PA5) 单键循环 4 档亮度 */
static void ka_apply_output(void);  /* 把当前状态落到 PWM / 恒流输出 */
static void ka_light(void);    /* 自动光控计算 */
static void ka_timer_1s(void);      /* 定时每秒递减 */
static void ka_disp_refresh(void);  /* 刷新数码管电流 + OLED 状态 */

/**
 * @brief  初始化按键与按键处理状态
 */
void KEY_AppInit(void)
{
    KEY_Init();
    ka_power_on   = 1u;
    ka_auto_light = 0u;
    ka_dim_mode   = 0u;
    ka_step       = 0u;
    ka_brightness = 100u;             /* 上电默认第 1 档：100%（最亮） */
    ka_timer_min  = KEYAPP_TMR_DEFAULT_MIN;
    ka_timer_run  = 0u;
    ka_timer_sec  = 0u;
    ka_light_ccr  = 999u;
    Segment_Init();             /* 4 位数码管初始化 */
    OLED_Init();                /* OLED 初始化 */
    ka_disp_refresh();          /* 上电先显示一次电流与状态 */
    ka_apply_output();
}

/**
 * @brief  按键处理主函数，主循环反复调用
 * @note   内部自动按 5ms 扫键、200ms 光控、1s 定时，无需外部再调度
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

    /* 处理按键事件：
       PA3 开关 / PA4 手动↔自动光控 / PA5 单键循环 4 档亮度
       PA6/PA7/PA8 预留（暂不处理） */
    if (KEY_IsPressed(KEY_POWER))            ka_key_power();
    if (KEY_IsPressed(KEY_MODE))             ka_key_mode();
    if (KEY_IsPressed(KEY_DIM))              ka_key_cycle();
    /* PA6/PA7/PA8 预留 */

    /* 200ms：自动光控重新计算 */
    if (ka_auto_light != 0u)
    {
        if ((now - ka_t_light) >= 200u)
        {
            ka_t_light = now;
            ka_light();
        }
    }

    /* 1s：定时倒计时 */
    if (ka_timer_run != 0u)
    {
        if ((now - ka_t_sec) >= 1000u)
        {
            ka_t_sec = now;
            ka_timer_1s();
        }
    }

    /* 20ms：恒流闭环输出 PWM（仅在“手动 + 恒流模式”下运行，
       否则会与 PWM 直调/自动光控的输出冲突） */
    if ((ka_dim_mode == 1u) && (ka_auto_light == 0u))
    {
        if ((now - ka_t_led) >= 20u)
        {
            ka_t_led = now;
            LED_ControlTick();
        }
    }

    /* 数码管动态扫描（每次调用刷一位，4 次一帧，刷新率高不闪烁） */
    Segment_Scan();

    /* 200ms：刷新数码管实时电流 + OLED 状态 */
    if ((now - ka_t_disp) >= 200u)
    {
        ka_t_disp = now;
        ka_disp_refresh();
    }
}

/* =====================================================================
 * 六个按键的动作
 * ===================================================================== */

/** 开关：切换输出 开/关 */
static void ka_key_power(void)
{
    ka_power_on = (uint8_t)(ka_power_on ^ 1u);
    ka_apply_output();
}

/** 光敏 ↔ 手动：二选一切换 */
static void ka_key_mode(void)
{
    ka_auto_light = (uint8_t)(ka_auto_light ^ 1u);
    if (ka_auto_light != 0u)
    {
        ka_light();              /* 立即算一次光控输出 */
    }
    ka_apply_output();
}

/* 4 档亮度：第1按 100%(最亮) → 75%(较亮) → 50%(中等) → 25%(暗)，循环 */
static const uint8_t ka_step_tbl[4] = {100u, 75u, 50u, 25u};

/** 单键循环 4 档亮度：每按一次跳一档，到第 4 档后回到第 1 档 */
static void ka_key_cycle(void)
{
    ka_step++;
    if (ka_step >= 4u) ka_step = 0u;
    ka_brightness = ka_step_tbl[ka_step];
    ka_apply_output();
}

/* =====================================================================
 * 输出与内部逻辑
 * ===================================================================== */

/**
 * @brief  把当前状态落到输出：
 *        关闭 → PWM=0（恒流目标=0）
 *        自动光控 → 用光敏映射的 PWM
 *        手动 PWM 直调 → brightness 0~100 → CCR 0~999
 *        手动恒流闭环 → brightness 0~100 → 20~200mA
 */
static void ka_apply_output(void)
{
    if (ka_power_on == 0u)
    {
        PWM_SetDuty(0u);
        LED_SetTarget(0u);
        return;
    }

    if (ka_auto_light != 0u)
    {
        /* 自动光控：直接使用光敏算出的 PWM */
        PWM_SetDuty(ka_light_ccr);
        LED_SetTarget(0u);            /* 光控模式走 PWM 直调，恒流目标清零 */
        return;
    }

    if (ka_dim_mode == 0u)
    {
        /* PWM 直调：0~100 亮度 → 0~999 CCR */
        PWM_SetDuty((uint16_t)(((uint32_t)ka_brightness * 999u) / 100u));
        LED_SetTarget(0u);
    }
    else
    {
        /* 恒流闭环：0~100 亮度 → 20~200mA 目标电流
           PWM 由 KEY_AppHandle 里每 20ms 调用的 LED_ControlTick() 恒流输出 */
        LED_SetTarget((uint16_t)(20u + ((uint32_t)ka_brightness * 180u) / 100u));
    }
}

/** 自动光控：ADC1(光敏) → 滤波 → bsp_light 映射表 → PWM */
static void ka_light(void)
{
    uint16_t raw  = ADC1_Read();
    uint16_t filt = Light_Filter(raw);
    ka_light_ccr  = Light_AdcToPwm(filt);
    ka_apply_output();
}

/** 定时每秒递减，到点自动关机 */
static void ka_timer_1s(void)
{
    if (ka_timer_sec > 0u)
    {
        ka_timer_sec--;
    }
    if (ka_timer_sec == 0u)
    {
        ka_timer_run = 0u;
        ka_power_on  = 0u;            /* 到点自动关机 */
        ka_apply_output();
    }
}

/* ===== 供显示层读取 ===== */
uint8_t  KEYAPP_IsPowerOn(void)      { return ka_power_on; }
uint8_t  KEYAPP_IsAutoLight(void)    { return ka_auto_light; }
uint8_t  KEYAPP_GetDimMode(void)     { return ka_dim_mode; }
uint16_t KEYAPP_GetBrightness(void)  { return ka_brightness; }
uint16_t KEYAPP_GetTimerMin(void)    { return ka_timer_min; }
uint8_t  KEYAPP_IsTimerRunning(void) { return ka_timer_run; }

/* ===== 显示刷新：数码管实时电流 + OLED 状态 ===== */
static void ka_disp_refresh(void)
{
    uint16_t ma;

    /* 实测电流 mA = Rsense 上端电压 / Rsense = (ADC2 * 3300/4095) / 10Ω */
    ma = (uint16_t)(((uint32_t)ADC2_Read() * 3300UL) / 4095UL / 10UL);
    if (ma > 999)
    {
        ma = 999;
    }

    /* 数码管实时显示电流 mA */
    Segment_ShowNum(ma, 0);

    /* OLED 第 1 行：电流 */
    OLED_ShowString(1, 1, "I:   mA");
    OLED_ShowNum(1, 3, ma, 3);

    /* OLED 第 2 行：亮度档位 */
    OLED_ShowString(2, 1, "Dim:");
    OLED_ShowNum(2, 5, ka_brightness, 3);
    OLED_ShowString(2, 8, "%");

    /* OLED 第 3 行：模式 */
    if (ka_auto_light != 0u)
    {
        OLED_ShowString(3, 1, "Mode:AUTO");
    }
    else
    {
        OLED_ShowString(3, 1, "Mode:MANU");
    }

    /* OLED 第 4 行：开关 */
    if (ka_power_on != 0u)
    {
        OLED_ShowString(4, 1, "PWR:ON ");
    }
    else
    {
        OLED_ShowString(4, 1, "PWR:OFF");
    }
}
