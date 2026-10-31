/* app.c —— 应用层实现
 *
 * ===== 工作模式（KEY_MODE 按键循环切换）=====
 *   MANUAL 手动调光：UP/DOWN 调目标电流（20~200mA），OK 开关输出
 *   AUTO   自动光控：光敏传感器自动调节明暗，UP/DOWN 调亮度偏移
 *   TIMER  定时开关：UP/DOWN 设定分钟数，OK 启动倒计时，到点自动关机
 *
 * ===== 显示约定（4 位共阴数码管）=====
 *   手动：显示实时电流，如 " 100"（mA）
 *   自动：最高位 'A' + 电流，如 "A100"
 *   定时设置/运行：最高位 't' + 分钟，如 "t 30"；运行中每 2s 与实时电流交替
 *   输出关闭：显示 "OFF"
 */
#include "app.h"
#include "bsp_key.h"
#include "bsp_display.h"
#include "bsp_adc.h"
#include "bsp_light.h"
#include "bsp_led.h"
#include "bsp_pwm.h"   /* PWM_MAX_DUTY */

/* ===== 可调参数 ===== */
#define APP_MANUAL_STEP_MA     5u     /* 手动档每次调整 5mA */
#define APP_AUTO_STEP_MA       5u     /* 自动偏移每次调整 5mA */
#define APP_AUTO_OFFSET_MAX    80u    /* 自动偏移上限 ±80mA */
#define APP_TMR_MIN_MIN        1u     /* 定时分钟下限 */
#define APP_TMR_MIN_MAX        99u    /* 定时分钟上限 */
#define APP_TMR_DEFAULT_MIN    30u    /* 默认定时分钟 */

/* 定时到点后的动作：0=关机（默认，倒计时结束关闭输出）；1=开机 */
#define APP_TIMER_ACTION_ON    0u

/* 自动光控是否反相：
 * 0=沿用 bsp_light 表（现表为“环境越亮 ADC 越小 → PWM 越大 → LED 越亮”）
 * 1=反相（环境越暗 LED 越亮，夜灯模式）。按你的传感器接线与预期效果设置 */
#define APP_AUTO_INVERT        0u

/* 定时运行中，每隔多少 ms 在“剩余分钟”与“实时电流”之间切换显示 */
#define APP_TMR_DISP_ALT_MS    2000u

/* ===== 模式 / 定时状态 ===== */
typedef enum { MODE_MANUAL, MODE_AUTO, MODE_TIMER } AppMode_t;
typedef enum { TMR_IDLE, TMR_RUN } TimerState_t;

static AppMode_t     mode          = MODE_MANUAL;
static TimerState_t  tmr_state     = TMR_IDLE;
static uint8_t       output_on     = 1u;          /* 总输出开关 */
static uint16_t      manual_ma     = 100u;        /* 手动目标电流 */
static int16_t       auto_offset   = 0;           /* 自动亮度偏移 ±mA */
static uint16_t      tmr_minutes   = APP_TMR_DEFAULT_MIN;
static uint32_t      tmr_remain_s  = 0u;          /* 剩余秒 */
static uint16_t      auto_last_ma  = 100u;        /* 最近一次自动算出的目标电流 */
static uint8_t       tmr_show_alt  = 0u;          /* 定时运行中显示切换标志 */
static uint32_t      tmr_alt_ms    = 0u;

/* 调度器时间戳 */
static uint32_t t_disp=0, t_key=0, t_led=0, t_btn=0, t_light=0, t_sec=0;

/* ---------- 内部函数原型 ---------- */
static void app_handle_buttons(void);
static void app_apply_target(void);
static void app_auto_light(void);
static void app_timer_1s(void);
static void app_display_update(void);

static void app_adjust_up(void);
static void app_adjust_down(void);
static void app_ok(void);
static void app_show_letter_num(uint8_t letter, uint16_t num, uint8_t dp);

/**
 * @brief  应用初始化：初始化各外设驱动并上电显示
 */
void App_Init(void)
{
    KEY_Init();
    DISPLAY_Init();
    LED_Init();

    manual_ma    = 100u;
    auto_offset  = 0;
    tmr_minutes  = APP_TMR_DEFAULT_MIN;
    tmr_remain_s = 0u;
    tmr_state    = TMR_IDLE;
    output_on    = 1u;

    app_apply_target();
    app_display_update();
}

/**
 * @brief  主循环体：基于 HAL_GetTick 的协作式调度
 */
void App_Loop(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - t_disp) >= 1u)     { t_disp  = now; DISPLAY_Tick(); }
    if ((now - t_key)  >= 5u)     { t_key   = now; KEY_Scan(); }
    if ((now - t_led)  >= 20u)    { t_led   = now; LED_ControlTick(); }
    if ((now - t_btn)  >= 20u)
    {
        t_btn = now;
        app_handle_buttons();
        app_apply_target();
        app_display_update();
    }
    if ((now - t_light) >= 200u)  { t_light = now; app_auto_light(); }
    if ((now - t_sec)  >= 1000u)
    {
        t_sec = now;
        app_timer_1s();
        if ((mode == MODE_TIMER) && (tmr_state == TMR_RUN))
        {
            if ((now - tmr_alt_ms) >= APP_TMR_DISP_ALT_MS)
            {
                tmr_alt_ms = now;
                tmr_show_alt ^= 1u;
            }
        }
    }
}

/* =====================================================================
 * 按键动作
 * ===================================================================== */
static void app_handle_buttons(void)
{
    if (KEY_IsPressed(KEY_MODE))
    {
        if (mode == MODE_MANUAL)      mode = MODE_AUTO;
        else if (mode == MODE_AUTO)   mode = MODE_TIMER;
        else                          mode = MODE_MANUAL;
        tmr_state = TMR_IDLE;          /* 离开定时模式即停止倒计时，输出保持 */
    }

    if (KEY_IsPressed(KEY_UP) || KEY_IsHeld(KEY_UP))       app_adjust_up();
    if (KEY_IsPressed(KEY_DOWN) || KEY_IsHeld(KEY_DOWN))   app_adjust_down();
    if (KEY_IsPressed(KEY_OK))                             app_ok();
}

static void app_adjust_up(void)
{
    if (mode == MODE_MANUAL)
    {
        if (manual_ma < LED_CUR_MAX_MA) manual_ma += APP_MANUAL_STEP_MA;
        if (manual_ma > LED_CUR_MAX_MA) manual_ma = LED_CUR_MAX_MA;
    }
    else if (mode == MODE_AUTO)
    {
        if (auto_offset < APP_AUTO_OFFSET_MAX) auto_offset += APP_AUTO_STEP_MA;
    }
    else /* MODE_TIMER */
    {
        if (tmr_state != TMR_RUN)
        {
            if (tmr_minutes < APP_TMR_MIN_MAX) tmr_minutes++;
        }
    }
}

static void app_adjust_down(void)
{
    if (mode == MODE_MANUAL)
    {
        if (manual_ma > LED_CUR_MIN_MA) manual_ma -= APP_MANUAL_STEP_MA;
        if (manual_ma < LED_CUR_MIN_MA) manual_ma = LED_CUR_MIN_MA;
    }
    else if (mode == MODE_AUTO)
    {
        if (auto_offset > -(int16_t)APP_AUTO_OFFSET_MAX) auto_offset -= APP_AUTO_STEP_MA;
    }
    else /* MODE_TIMER */
    {
        if (tmr_state != TMR_RUN)
        {
            if (tmr_minutes > APP_TMR_MIN_MIN) tmr_minutes--;
        }
    }
}

static void app_ok(void)
{
    if (mode == MODE_TIMER)
    {
        if (tmr_state == TMR_RUN)
        {
            /* 运行中按 OK：停止定时，输出恢复常开 */
            tmr_state = TMR_IDLE;
            output_on = 1u;
        }
        else
        {
            /* 未运行按 OK：以当前分钟数启动倒计时，输出开启 */
            tmr_state = TMR_RUN;
            tmr_remain_s = (uint32_t)tmr_minutes * 60u;
            tmr_show_alt = 0u;
            tmr_alt_ms   = HAL_GetTick();
            output_on    = 1u;
        }
    }
    else
    {
        /* 手动 / 自动：OK 开关输出 */
        output_on = (uint8_t)(output_on ^ 1u);
    }
}

/* =====================================================================
 * 目标电流 / 自动光控
 * ===================================================================== */
static void app_apply_target(void)
{
    uint16_t t = 0u;

    if (output_on != 0u)
    {
        if (mode == MODE_MANUAL)      t = manual_ma;
        else if (mode == MODE_AUTO)   t = auto_last_ma;
        else                          t = manual_ma;   /* 定时模式沿用手动亮度 */
    }
    LED_SetTarget(t);
}

static void app_auto_light(void)
{
    uint16_t raw  = ADC1_Read();
    uint16_t filt = Light_Filter(raw);
    uint16_t ccr  = Light_AdcToPwm(filt);
    int32_t  ma   = (int32_t)(((uint32_t)ccr * (uint32_t)LED_CUR_MAX_MA) / (uint32_t)PWM_MAX_DUTY);

    if (APP_AUTO_INVERT) ma = (int32_t)(LED_CUR_MAX_MA + LED_CUR_MIN_MA) - ma;

    ma += auto_offset;

    if (ma < LED_CUR_MIN_MA) ma = LED_CUR_MIN_MA;
    if (ma > LED_CUR_MAX_MA) ma = LED_CUR_MAX_MA;

    auto_last_ma = (uint16_t)ma;
}

/* =====================================================================
 * 定时倒计时
 * ===================================================================== */
static void app_timer_1s(void)
{
    if ((mode == MODE_TIMER) && (tmr_state == TMR_RUN))
    {
        if (tmr_remain_s > 0u) tmr_remain_s--;
        if (tmr_remain_s == 0u)
        {
            output_on = (uint8_t)(APP_TIMER_ACTION_ON ? 1u : 0u);
            tmr_state = TMR_IDLE;
        }
    }
}

/* =====================================================================
 * 显示
 * ===================================================================== */
static void app_show_letter_num(uint8_t letter, uint16_t num, uint8_t dp)
{
    uint8_t g[DISP_DIGITS] = { letter, DISP_G_BLANK, DISP_G_BLANK, DISP_G_BLANK };
    uint16_t a = num;
    uint8_t pos = DISP_DIGITS;

    do
    {
        pos--;
        g[pos] = (uint8_t)(DISP_G_0 + (a % 10u));
        a /= 10u;
    } while ((a > 0u) && (pos > 0u));

    DISPLAY_Set(g, dp);
}

static void app_display_update(void)
{
    uint8_t g[DISP_DIGITS];

    /* 输出关闭：显示 OFF */
    if (output_on == 0u)
    {
        g[0] = DISP_G_O; g[1] = DISP_G_F; g[2] = DISP_G_F; g[3] = DISP_G_BLANK;
        DISPLAY_Set(g, 0u);
        return;
    }

    switch (mode)
    {
        case MODE_MANUAL:
            /* 一直实时显示电流 */
            DISPLAY_ShowNumber(LED_GetMeas(), 0u);
            break;

        case MODE_AUTO:
            /* 最高位 'A' 表示自动，其余位实时显示电流 */
            app_show_letter_num(DISP_G_A, LED_GetMeas(), 0u);
            break;

        case MODE_TIMER:
        default:
            if (tmr_state == TMR_RUN)
            {
                /* 运行中：交替显示剩余分钟 与 实时电流 */
                if (tmr_show_alt == 0u)
                {
                    uint16_t remain_min = (uint16_t)(tmr_remain_s / 60u);
                    app_show_letter_num(DISP_G_T, remain_min, 0u);
                }
                else
                {
                    DISPLAY_ShowNumber(LED_GetMeas(), 0u);
                }
            }
            else
            {
                /* 设置中：显示定时分钟 */
                app_show_letter_num(DISP_G_T, tmr_minutes, 0u);
            }
            break;
    }
}
