// ============================================================
// YF-S401 水流水位传感器驱动
// ============================================================
// 引脚: PA5 (EXTI5 下降沿中断 + TIM2 1秒定时器)
// 原理: 霍尔效应脉冲输出，水流推动转子→产生方波
// 参数: 450脉冲/升（厂商校准系数）
// 公式: 流量(L/min) = 每秒脉冲数 × 60 / 450
//
// 关键技术:
//   1. 双中断设计: EXTI计脉冲 + TIM2每秒汇总（ISR只做整数加法）
//   2. EMA低通滤波(α=0.3): 抑制充电宝供电不稳导致的脉冲抖动
//   3. 临界区保护: 读取累计脉冲时关全局中断（32位变量非原子读）
// ============================================================

#include "stm32f10x.h"
#include "waterflow.h"
#include <stdio.h>

#define FLOW_PULSE_PIN          GPIO_Pin_5
#define FLOW_PULSE_PORT         GPIOA

// 厂商校准系数：每升水产生450个脉冲
#define FLOW_CONSTANT           450.0f

// 全局变量：脉冲累计
volatile uint32_t pulse_count = 0;       // 当前秒脉冲计数（TIM2每秒清零）
static volatile uint32_t s_total_pulses = 0;  // 累计总脉冲（永不归零，用于计算总流量）
float flow_rate_L_per_min = 0.0f;        // 瞬时流量(L/min)，供OLED显示
float total_flow_L = 0.0f;              // 累计总用水量(L)，供电量剩余计算

// ---- EXTI5下降沿中断：每次脉冲+1 ----
// 设计原则：ISR只做最简单的整数加法，浮点运算留到主循环
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line5) != RESET)
    {
        pulse_count++;                     // 仅整数累加，ISR极速退出
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
}

// ---- TIM2中断：每秒汇总脉冲数 ----
// 每秒触发一次，将pulse_count转存到s_total_pulses后清零
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        s_total_pulses += pulse_count;     // 累加到总计
        pulse_count = 0;                   // 清零本秒计数
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

// ---- 初始化：GPIO + EXTI + TIM2 ----
void Flow_Sensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    // 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // PA5 上拉输入（抗干扰，悬空时固定高电平）
    GPIO_InitStructure.GPIO_Pin   = FLOW_PULSE_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FLOW_PULSE_PORT, &GPIO_InitStructure);

    // EXTI5 下降沿触发（水流脉冲为下降沿）
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);
    EXTI_InitStructure.EXTI_Line    = EXTI_Line5;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  // 下降沿
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // EXTI中断优先级：Preemption=1, Sub=0
    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // TIM2 1秒定时器：72MHz / 7200 = 10kHz, 10000 / 10kHz = 1s
    TIM_TimeBaseStructure.TIM_Period = 9999;
    TIM_TimeBaseStructure.TIM_Prescaler = 7199;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // TIM2中断优先级：Preemption=2, Sub=0（低于EXTI的1,0）
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

// ---- 获取瞬时流量(L/min) + 累计总流量(L) ----
// 厂商公式：流量(L/min) = 每秒脉冲数 × 60 / 450
// EMA低通滤波(α=0.3)：抑制供电不稳导致的水流跳变
float Flow_Sensor_Get_FlowRate(void)
{
    static uint32_t last_total = 0;        // 上次读取时的累计脉冲
    static float filtered_flow = 0.0f;     // EMA滤波后的流量
    uint32_t current;

    // 临界区保护：32位变量在Cortex-M3上非原子读取
    __disable_irq();
    current = s_total_pulses;
    __enable_irq();

    uint32_t diff = current - last_total;  // 本次比上次多的脉冲数
    last_total = current;

    // 原始瞬时流量（每秒脉冲→L/min）
    float raw_flow = (float)diff * 60.0f / FLOW_CONSTANT;

    // EMA低通滤波：平滑流量波动
    if (filtered_flow < 0.01f)
        filtered_flow = raw_flow;          // 首次有效值→直接初始化
    else
        filtered_flow = filtered_flow * 0.7f + raw_flow * 0.3f;  // α=0.3

    flow_rate_L_per_min = filtered_flow;

    // 累计总用水量：每秒累加 filtered_flow / 60（L/min → L/s）
    total_flow_L += filtered_flow / 60.0f;

    return flow_rate_L_per_min;
}

// ---- 获取累计总用水量(L) ----
float Flow_Sensor_Get_TotalFlow(void)
{
    return total_flow_L;
}

// ---- 重置累计总用水量（换水箱时调用）----
void Flow_Sensor_Reset_Total(void)
{
    total_flow_L = 0.0f;
}
