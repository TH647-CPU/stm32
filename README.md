# 20mA~200mA 恒流 LED 控制程序（STM32F103C8 / HAL 库）

本工程在原有 `bsp_pwm / bsp_adc / bsp_light` 基础上，补齐了**按键、数码管、恒流闭环、应用状态机**四部分，实现：

- 5V 输入、恒流输出 **20mA~200mA**（运放恒流源 + Rsense 采样）
- **PWM 调光**（PA0/TIM2_CH1）
- **ADC 实时采样电流**（PA2/ADC2）并显示在 4 位共阴数码管
- **手动 / 自动光控 / 定时开关** 三模式，按键切换与调节
- 光敏传感器（PA1/ADC1）自动调明暗
- 定时倒计时到点自动关机，数码管显示

---

## 一、新增文件清单（都放在 `MDK-ARM\` 下）

| 文件 | 作用 |
|------|------|
| `bsp_key.h/.c` | **6 键**扫描（PA3~PA8），消抖 + 按下沿 + 长按连发 |
| `key_app.h/.c` | **6 键按键处理**：开关 / 光敏↔手动 / 调光模式 / 亮度± / 定时开关 |
| `bsp_display.h/.c` | 4 位 7 段数码管 GPIO 动态扫描，字形/小数点，引脚宏可配 |
| `bsp_led.h/.c` | 恒流闭环：目标电流 + ADC2 采样 + PI 输出 PWM |
| `app.h/.c` | 应用状态机：模式切换、按键动作、自动光控、定时倒计时、显示、调度 |
| `Core\Src\main.c` | 已重写干净入口（原文件备份为 `main.c.bak`） |

原有文件：`bsp_pwm`（TIM2_CH1 PWM）、`bsp_adc`（ADC1/ADC2 读取）、`bsp_light`（光感滤波+映射），**保持不变**。

## 二、接线（外部器件）

```
【恒流驱动】 5V ── LED(串) ── D N-MOS ── S ── Rsense(10Ω) ── GND
                  │            │ G
                  │            └── 运放输出（LM358 等）
    PA0 PWM ── RC滤波 ── Vset ── 运放 +
                  运放 - ── Vsense(Rsense 上端) ── PA2(ADC2_IN2)
```
运放强制 `Vsense = Vset`，故 `I = Vset / Rsense`，PWM 占空比决定 Vset 即决定电流。

```
【光敏】 光敏电阻(或光敏二极管+电阻分压) ── PA1(ADC1_IN1)
【数码管】 段码 a~g、dp → PB0 PB1 PB8 PB9 PB12 PB13 PB14 PB15
          位选 0~3      → PC13 PC14 PC15 PA8
【按键】 PA3=开关  PA4=光敏↔手动  PA5=调光模式  PA6=亮度+  PA7=亮度-  PA8=定时
```
> 按键与数码管引脚全部在头文件里用宏定义（`bsp_key.c`、`bsp_display.h`），按实际接线改即可。
> 注意：PA7 原在 CubeMX 里是输出引脚、PA8 若被数码管位选占用，按键初始化会重新配置为输入+上拉，请改 `bsp_key.c` 引脚表避开冲突。

## 三、把新文件加进 Keil 工程
1. Keil 左侧工程树右键 `MDK-ARM` 下的用户源文件组（如 `bsp`/`User`），`Add Existing Files…`
2. 加入：`bsp_key.c`、`key_app.c`、`bsp_display.c`、`bsp_led.c`、`app.c`
3. 头文件路径若报找不到，在 `Options for Target → C/C++ → Include Paths` 添加 `MDK-ARM` 目录
4. 编译、烧录

### 只跑“6 键按键程序”（不跑 4 键 app 状态机）时
把 `main.c` 主循环两行替换为：
```c
  KEY_AppInit();              /* 初始化按键处理 */
  while (1)
  {
    KEY_AppHandle();          /* 5ms 扫键 + 按键动作 + 输出 */
  }
```
并把 `main.c` 里 `#include "app.h"` 改为 `#include "key_app.h"`（app.c 可不加入工程）。

## 四、CubeMX 配置（当前工程已具备，供核对）
- `PA0` → `TIM2_CH1`，ARR=999（PWM 0~999）
- `PA1` → `ADC1_IN1`（光敏）
- `PA2` → `ADC2_IN2`（电流采样）
- `PA3 PA4 PA5 PA6` → GPIO 输入（本代码会用内部上拉重新初始化）
- `PB6/PB7`、`PB10/PB11` → I2C（未使用，保留即可）
- 数码管 / 按键 / 电流采样引脚由驱动代码在 `App_Init` 中自行初始化，无需 CubeMX 配置

## 五、操作说明（6 个按键）
| 按键 | 引脚 | 功能 |
|------|------|------|
| 开关 | PA3 | 切换输出 开 / 关 |
| 光敏↔手动 | PA4 | 自动光控 / 手动调光 二选一切换 |
| 调光模式 | PA5 | 0=PWM 直调(占空比)，1=恒流闭环(20~200mA) |
| 亮度 + | PA6 | 手动模式下增亮（长按连发） |
| 亮度 - | PA7 | 手动模式下减亮（长按连发） |
| 定时 | PA8 | 启动 / 停止定时倒计时，到点自动关机 |

显示约定：手动显示 `电流`；自动最高位 `A`；定时运行最高位 `t` + 剩余分钟；输出关闭显示 `OFF`。

## 六、关键参数调整（都在对应 .h 顶部）
- **电流档位**：`bsp_led.h` 的 `LED_CUR_MIN_MA / LED_CUR_MAX_MA`（默认 20/200mA）
- **采样电阻**：`LED_RSENSE_OHM`（默认 10Ω），务必与硬件一致，否则电流显示不准
- **恒流闭环**：`LED_CLOSED_LOOP_ENABLE`（1 闭环 / 0 开环）、`LED_PI_KP / LED_PI_KI`
  - 若输出振荡（电流忽高忽低）：调小 `LED_PI_KP / LED_PI_KI`
  - 若响应慢：适当调大
- **自动光控极性**：`app.c` 的 `APP_AUTO_INVERT`
  - 0 = 沿用 `bsp_light` 表（现表为“环境越亮 → LED 越亮”）
  - 1 = 反相（环境越暗 → LED 越亮，夜灯模式）
- **定时到点动作**：`APP_TIMER_ACTION_ON`（0=到点关机，1=到点开机）
- **数码管类型/引脚**：`bsp_display.h` 顶部宏

## 七、注意事项
- 系统时钟为 **8MHz HSI**（CubeMX 原始配置），TIM2 输出约 111Hz，RC 滤波须滤掉该频率
- 共阳数码管请在 `bsp_display.h` 把 `DISP_COMMON_CATHODE` 改 0 并核对位选电平
- 位选若为共阴公共端直连引脚，把 `DISP_DIG_ON_LEVEL`/`OFF` 对调
