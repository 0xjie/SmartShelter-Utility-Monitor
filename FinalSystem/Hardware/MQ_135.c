#include "MQ_135.h"
#include "Delay.h"

static uint16_t MQ135_ReadAverageADC(uint8_t samples)
{
    uint32_t sum = 0;
    uint8_t i = 0;

    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

    for (i = 0; i < samples; i++)
    {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
        sum += ADC_GetConversionValue(ADC1);
        Delay_us(100);
    }

    return (uint16_t)(sum / samples);
}

static float MQ135_CalcRsKOhm(float vout)
{
    if (vout < 0.05f)
    {
        vout = 0.05f;
    }
    if (vout > (MQ135_SENSOR_VCC - 0.05f))
    {
        vout = MQ135_SENSOR_VCC - 0.05f;
    }

    return MQ135_LOAD_RESISTOR_KOHM * (MQ135_SENSOR_VCC - vout) / vout;
}

void MQ135_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
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

    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

uint16_t MQ135_Get_ADC(void)
{
    return MQ135_ReadAverageADC(MQ135_AVG_SAMPLES);
}

float MQ135_Get_Voltage(void)
{
    uint16_t adc = MQ135_Get_ADC();
    return ((float)adc * 3.3f) / 4095.0f;
}

float MQ135_Get_PPM(void)
{
    static float s_aq_lp = 0.0f;
    uint16_t adc = MQ135_Get_ADC();
    float aq_index;

    // 按你的要求：AQ 直接基于 MQ-135 模拟输入值映射（0..4095 -> 0..500）
    aq_index = ((float)adc * 500.0f) / 4095.0f;
    s_aq_lp = s_aq_lp * 0.8f + aq_index * 0.2f;
    return s_aq_lp;
}
