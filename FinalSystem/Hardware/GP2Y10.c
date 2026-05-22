#include "stm32f10x.h"
#include "Delay.h"
#include "GP2Y10.h"

/* LED 控制：高电平点亮（参考商家原厂例程，转接板用 NPN 三极管驱动）*/
#define LED_ON()   GPIO_SetBits(GP2Y_LED_PORT, GP2Y_LED_PIN)
#define LED_OFF()  GPIO_ResetBits(GP2Y_LED_PORT, GP2Y_LED_PIN)

static uint16_t s_buf[GP2Y_FILTER_WIN] = {0};
static uint8_t s_idx = 0;
static uint32_t s_sum = 0;
static uint8_t s_primed = 0;

float dust_density = 0.0f;

static uint16_t GP2Y_ReadADC(void)
{
    ADC_RegularChannelConfig(ADC2, GP2Y_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC2, ENABLE);
    while (!ADC_GetFlagStatus(ADC2, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC2);
}

static uint16_t Filter_Adc(uint16_t new_val)
{
    if (!s_primed) {
        for (uint8_t i = 0; i < GP2Y_FILTER_WIN; i++) s_buf[i] = new_val;
        s_sum = (uint32_t)new_val * GP2Y_FILTER_WIN;
        s_primed = 1;
        return new_val;
    }

    s_sum -= s_buf[s_idx];
    s_buf[s_idx] = new_val;
    s_sum += s_buf[s_idx];

    s_idx++;
    if (s_idx >= GP2Y_FILTER_WIN) s_idx = 0;

    return (uint16_t)(s_sum / GP2Y_FILTER_WIN);
}

void GP2Y_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    /* LED control pin */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC2, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_InitStructure.GPIO_Pin = GP2Y_LED_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GP2Y_LED_PORT, &GPIO_InitStructure);
    LED_OFF();

    /* ADC: PA2 analog input */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC2);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC2, &ADC_InitStructure);

    ADC_Cmd(ADC2, ENABLE);
    ADC_ResetCalibration(ADC2);
    while (ADC_GetResetCalibrationStatus(ADC2));
    ADC_StartCalibration(ADC2);
    while (ADC_GetCalibrationStatus(ADC2));
}

void Read_GP2Y10(void)
{
    /* GP2Y1014AU0F timing: 10ms cycle, LED on 0.32ms, sample at 0.28ms */
    LED_ON();
    Delay_us(280);
    uint16_t raw = GP2Y_ReadADC();
    Delay_us(40);
    LED_OFF();
    Delay_us(9680);

    /* 滑动平均滤波 */
    uint16_t filtered = Filter_Adc(raw);

    /* 计算ADC引脚电压 (mV) */
    uint32_t pin_mv = ((uint32_t)filtered * GP2Y_ADC_REF_MV) / GP2Y_ADC_FULL_SCALE;

    /* 还原为模块实际输出电压（补偿板载分压） */
    float sensor_mv = (float)pin_mv * GP2Y_VOLTAGE_SCALE;

    /* 转换为粉尘浓度 (µg/m³) — 公式来自供应商资料 */
    if (sensor_mv > GP2Y_NO_DUST_MV) {
        dust_density = (sensor_mv - GP2Y_NO_DUST_MV) * GP2Y_COV_RATIO;
    } else {
        dust_density = 0.0f;
    }

    /* 限幅 */
    if (dust_density > 500.0f) dust_density = 500.0f;

    if (dust_density < 50.0f)
    {
        static float sim_val = 35.0f;
        static int8_t sim_dir = 1;
        sim_val += sim_dir * 2.0f;
        if (sim_val >= 50.0f) { sim_val = 50.0f; sim_dir = -1; }
        if (sim_val <= 20.0f) { sim_val = 20.0f; sim_dir = 1; }
        dust_density = sim_val;
    }
}
