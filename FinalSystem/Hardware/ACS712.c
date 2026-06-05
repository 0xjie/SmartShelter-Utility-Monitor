// ============================================================
// ACS712 霍尔电流传感器驱动 (5A版)
// ============================================================
// 引脚: PA0 (ADC1_IN0)
// 原理: 霍尔效应，输出电压与电流线性关系
// 参数: VCC=5V, 灵敏度=185mV/A, 零电流输出=VCC/2=2.5V
// 公式: I = (Vout - 2.50) / 0.185
//
// 关键技术:
//   1. 600次采样 × 68µs = 40.8ms ≈ 2个50Hz工频周期，消除工频纹波
//   2. 取绝对值（不辨方向），限幅5A
//   3. main.c中额外做10mA死区过滤和连续3秒低于死区强制归零
// ============================================================

#include "stm32f10x.h"
#include "ACS712.h"
#include "Delay.h"

// ---- ADC多次采样取平均 ----
// 采样间隔68µs确保覆盖完整工频周期(20ms)，600次≈40ms=2周期
static float ACS712_ReadAverageVoltage(uint16_t samples)
{
    uint32_t sum = 0;

    // 显式切换ADC通道（ADC1被ACS712/MQ135共用）
    ADC_RegularChannelConfig(ADC1, ACS712_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);

    for (uint16_t i = 0; i < samples; i++)
    {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);           // 软件触发单次转换
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);  // 等待转换完成
        sum += ADC_GetConversionValue(ADC1);
        Delay_us(68);  // 等间隔采样，覆盖工频周期
    }

    // ADC读数 → 引脚电压（3.3V参考，12-bit分辨率→4095满量程）
    float adc_voltage = ((float)sum / samples) / 4095.0f * 3.3f;
    return adc_voltage;
}

// ---- 初始化：ADC1时钟/GPIO/校准 ----
void ACS712_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    // 使能ADC1和GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  // ADC时钟 = 72MHz/6 = 12MHz（允许范围）

    // PA0 模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);

    // ADC配置：独立模式，单通道，软件触发
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ACS712_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);

    ADC_Cmd(ADC1, ENABLE);

    // ADC校准（厂家推荐流程：复位校准→等待→开始校准→等待）
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    Delay_ms(100);  // 等待ADC稳定
}

// ---- 获取电流值(A)，含工频消除 + 绝对值 + 限幅 ----
float ACS712_GetCurrent(void)
{
    float voltage = ACS712_ReadAverageVoltage(ACS712_SAMPLE_COUNT);

    // 厂商公式：I = (Vout - Vcc/2) / 灵敏度 = (Vout - 2.5) / 0.185
    float current = (voltage - 2.50f) / 0.185f;

    // 取绝对值（ACS712不区分电流方向，只关心大小）
    if (current < 0.0f) current = -current;

    // 限幅（5A量程保护）
    if (current > 5.0f) current = 5.0f;

    return current;
}
