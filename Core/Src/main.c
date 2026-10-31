/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *   STM32F103C8 · 5V 输入输出恒流 LED（20mA~200mA）
  *   · PWM 调光（TIM2_CH1/PA0）     · ADC 实时采样电流（ADC2/PA2）
  *   · 恒流闭环（PI 稳定到目标电流）· 自动光控（光敏 ADC1/PA1）· 定时开关
  *   · 数码管实时显示电流 + OLED 状态显示
  *   · 3 键：PB12 开关 / PB13 手动↔自动光控 / PB14 短按档位·长按定时
  *
  *   烧录后默认：输出开、恒流闭环、目标 200mA → LED 直接亮。
  *
  *   【最小点亮测试】若灯不亮，把下面 DEBUG_FORCE_PWM_ON 改为 1 重新编译烧录，
  *   此时主循环只输出固定 PWM(800)，绕过所有按键/恒流逻辑：
  *     - 灯亮     → 硬件 OK，问题在按键/逻辑代码
  *     - 灯仍不亮 → 检查 PA0 PWM 波形 / 恒流电路 / LED 接线
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_pwm.h"
#include "bsp_adc.h"
#include "bsp_light.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_display.h"
#include "key_app.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 最小点亮测试开关：1=主循环只输出固定 PWM，用于排查硬件；0=正常运行 */
#define DEBUG_FORCE_PWM_ON   0u

/* USER CODE END PD */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_TIM2_Init();

  /* 开启 TIM2_CH1(PA0) 的 PWM 输出 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  /* 6 键按键处理初始化（内部会初始化按键引脚并默认输出 50% 亮度） */
  KEY_AppInit();

  /* Infinite loop */
  while (1)
  {
    /* 最小点亮测试：强制固定 PWM，绕过全部按键/恒流逻辑 */
    if (DEBUG_FORCE_PWM_ON)
    {
      PWM_SetDuty(800u);
      HAL_Delay(10u);
      continue;
    }

    /* 正常运行：5ms 扫键 + 按键动作 + 光控 + 定时 + 恒流闭环 */
    KEY_AppHandle();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
