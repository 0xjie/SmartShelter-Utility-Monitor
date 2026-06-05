// ============================================================
// 风扇控制驱动（原"Motor"文件改名，实际驱动5V散热风扇）
// ============================================================
// 引脚: PB6 (推挽输出)
// 极性: 低电平转、高电平停（NPN/MOS低端驱动）
//
// 系统中风扇用途：
//   正常: 停（无需通风）
//   预警: 转（开始通风降温）
//   告警: 转（持续通风排烟）
//   手动模式: 远程可单独开关
// ============================================================

#include "stm32f10x.h"

// ---- 初始化PB6为推挽输出，默认高电平（停）----
void Fan_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_6);  // 高电平=停
}

// ---- 风扇控制：run=1转, run=0停 ----
void Fan_Set(uint8_t run)
{
    if (run)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_6);  // 低电平→风扇转
    }
    else
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_6);    // 高电平→风扇停
    }
}
