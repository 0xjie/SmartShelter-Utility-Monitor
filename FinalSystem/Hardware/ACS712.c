#include "stm32f10x.h"
#include "ACS712.h"
#include "Delay.h"

/* =========================
   ADC 采样取平均（厂商参考方式：多次采样→平均→套公式）
   ========================= */
static float ACS712_ReadAverageVoltage(uint16_t samples)
{
    uint32_t sum = 0;

    ADC_RegularChannelConfig(ADC1, ACS712_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);

    for (uint16_t i = 0; i < samples; i++)
    {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        sum += ADC_GetConversionValue(ADC1);
        Delay_us(68);
    }

    /* ADC 读数 → STM32 引脚电压（3.3V 参考，12-bit） */
    float adc_voltage = ((float)sum / samples) / 4095.0f * 3.3f;
    return adc_voltage;
}

/* =========================
   初始化：ADC 时钟、GPIO、校准
   ========================= */
void ACS712_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ACS712_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);

    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    Delay_ms(100);
}

/* =========================
   获取电流（厂商参考公式）
   5A 版 ACS712 模块，VCC=5V，灵敏度 185mV/A
   公式：I = (Vout - 2.50) / 0.185
   ========================= */
float ACS712_GetCurrent(void)
{
    float voltage = ACS712_ReadAverageVoltage(ACS712_SAMPLE_COUNT);

    /* 厂商公式：电流 = (Vout - Vcc/2) / 灵敏度 */
    float current = (voltage - 2.50f) / 0.185f;

    /* 取绝对值（只测大小，不辨方向） */
    if (current < 0.0f) current = -current;

    /* 限幅 */
    if (current > 5.0f) current = 5.0f;

    return current;
}
