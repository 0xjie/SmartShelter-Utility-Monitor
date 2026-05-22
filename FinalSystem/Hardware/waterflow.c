#include "stm32f10x.h"
#include "waterflow.h"
#include <stdio.h>

#define FLOW_PULSE_PIN          GPIO_Pin_5
#define FLOW_PULSE_PORT         GPIOA

/* 厂商校准系数：450 脉冲/升 */
#define FLOW_CONSTANT           450.0f

volatile uint32_t pulse_count = 0;
static volatile uint32_t s_total_pulses = 0;
float flow_rate_L_per_min = 0.0f;
float total_flow_L = 0.0f;

/* EXTI 中断：下降沿计数（厂商参考：下降沿触发） */
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line5) != RESET)
    {
        pulse_count++;
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
}

/* TIM2 中断：每秒累计脉冲数 */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        s_total_pulses += pulse_count;
        pulse_count = 0;
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

void Flow_Sensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA5 上拉输入（厂商参考：INPUT_PULLUP，抗干扰） */
    GPIO_InitStructure.GPIO_Pin   = FLOW_PULSE_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FLOW_PULSE_PORT, &GPIO_InitStructure);

    /* EXTI 下降沿触发（厂商参考：FALLING） */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);
    EXTI_InitStructure.EXTI_Line    = EXTI_Line5;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* TIM2 1s 定时 */
    TIM_TimeBaseStructure.TIM_Period = 9999;
    TIM_TimeBaseStructure.TIM_Prescaler = 7199;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

/* 厂商公式：流量(L/min) = 每秒脉冲数 × 60 / 450 */
float Flow_Sensor_Get_FlowRate(void)
{
    static uint32_t last_total = 0;
    uint32_t current;

    __disable_irq();
    current = s_total_pulses;
    __enable_irq();

    uint32_t diff = current - last_total;
    last_total = current;

    flow_rate_L_per_min = (float)diff * 60.0f / FLOW_CONSTANT;
    total_flow_L += flow_rate_L_per_min / 60.0f;

    return flow_rate_L_per_min;
}

float Flow_Sensor_Get_TotalFlow(void)
{
    return total_flow_L;
}

void Flow_Sensor_Reset_Total(void)
{
    total_flow_L = 0.0f;
}
