// ============================================================
// MQ-135 空气质量传感器驱动
// ============================================================
// 引脚: PA1 (ADC1_IN1, 与ACS712共用ADC1)
// 原理: 金属氧化物半导体，气体浓度↑→传感器电阻↓→输出电压↑
// 简化方案: ADC读数0~4095 线性映射到 0~500ppm（适合演示）
// 低通滤波: α=0.2, 平滑气体浓度的缓慢变化
// ============================================================

#include "MQ_135.h"
#include "Delay.h"

// ---- ADC多次采样取平均 ----
static uint16_t MQ135_ReadAverageADC(uint8_t samples)
{
    uint32_t sum = 0;
    uint8_t i = 0;

    // 每次读取前显式切换通道（ADC1被ACS712和MQ135共用）
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

    for (i = 0; i < samples; i++)
    {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
        sum += ADC_GetConversionValue(ADC1);
        Delay_us(100);  // 采样间隔100µs
    }

    return (uint16_t)(sum / samples);
}

// ---- Rs计算（备用，当前未使用）----
static float MQ135_CalcRsKOhm(float vout)
{
    if (vout < 0.05f)
        vout = 0.05f;  // 防除零
    if (vout > (MQ135_SENSOR_VCC - 0.05f))
        vout = MQ135_SENSOR_VCC - 0.05f;

    // Rs = RL × (VCC - Vout) / Vout
    return MQ135_LOAD_RESISTOR_KOHM * (MQ135_SENSOR_VCC - vout) / vout;
}

// ---- MQ-135初始化（ADC1被共用，注意与ACS712协调）----
void MQ135_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    // PA1 模拟输入
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

    // ADC校准
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

// ---- 获取ADC原始值 ----
uint16_t MQ135_Get_ADC(void)
{
    return MQ135_ReadAverageADC(MQ135_AVG_SAMPLES);  // 16次采样取平均
}

// ---- 获取电压值(V) ----
float MQ135_Get_Voltage(void)
{
    uint16_t adc = MQ135_Get_ADC();
    return ((float)adc * 3.3f) / 4095.0f;  // 12位ADC → 3.3V
}

// ---- 获取空气质量指数(ppm) ----
// 简化方案：ADC原始值0~4095 线性映射到 0~500ppm
// 低通滤波 α=0.2：平滑缓慢变化的气体浓度
float MQ135_Get_PPM(void)
{
    static float s_aq_lp = 0.0f;  // 低通滤波状态变量
    uint16_t adc = MQ135_Get_ADC();
    float aq_index;

    // 线性映射：0~4095 → 0~500ppm
    aq_index = ((float)adc * 500.0f) / 4095.0f;

    // 一阶低通滤波：s_aq_lp = s_aq_lp×0.8 + 新值×0.2（τ≈5秒）
    s_aq_lp = s_aq_lp * 0.8f + aq_index * 0.2f;
    return s_aq_lp;
}
