// ============================================================
// 有源蜂鸣器驱动（报警声响）
// ============================================================
// 引脚: PB14 (推挽输出)
// 极性: 低电平响、高电平不响（NPN三极管或MOS驱动）
// 报警策略（由main.c控制节奏）：
//   正常: 不响
//   预警: 每2秒滴一声（不扰民）
//   告警: 每秒滴滴两声（急促提醒）
// ============================================================

#include "stm32f10x.h"

// ---- 初始化PB14为推挽输出，默认高电平（不响）----
void Buzzer_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_14);  // 高电平=不响
}

// ---- 蜂鸣器响（PB14拉低）----
void Buzzer_ON(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);
}

// ---- 蜂鸣器关（PB14拉高）----
void Buzzer_OFF(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_14);
}
