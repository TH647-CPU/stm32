#include "bsp_adc.h"
#include "adc.h"

// 读取ADC1（光敏 PA1）
uint16_t ADC1_Read(void)
{
    uint16_t val;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

// 预留ADC2（PA2），以后接别的传感器
uint16_t ADC2_Read(void)
{
    uint16_t val;
    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2, 10);
    val = HAL_ADC_GetValue(&hadc2);
    HAL_ADC_Stop(&hadc2);
    return val;
}
