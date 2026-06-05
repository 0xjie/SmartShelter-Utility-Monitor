// ============================================================
// 三色LED状态指示灯驱动
// ============================================================
// 红灯: PA15 (高电平亮，需禁用JTAG释放引脚)
// 绿灯: PA11 (高电平亮)
// 黄灯: PA12 (高电平亮)
//
// JTAG问题：F103默认为JTAG调试模式，PA15用作JTDI
// 必须先调用 GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)
// 禁用JTAG只保留SWD(PA13/PA14)，PA15才能正常驱动GPIO
// ============================================================

#include "stm32f10x.h"

#define LED_RED_PORT     GPIOA
#define LED_RED_PIN      GPIO_Pin_15  // 需禁用JTAG
#define LED_GREEN_PORT   GPIOA
#define LED_GREEN_PIN    GPIO_Pin_11
#define LED_YELLOW_PORT  GPIOA
#define LED_YELLOW_PIN   GPIO_Pin_12

// ---- 初始化三色LED ----
// PA15: 需先禁用JTAG再配置为GPIO（否则驱动微弱→红灯微亮）
// PA11/PA12: 正常GPIO，无需特殊处理
void LED_Init(void)
{
    // 关闭JTAG、保留SW-DP（PA13/PA14仍可用于下载调试）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = LED_RED_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_RED_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = LED_GREEN_PIN | LED_YELLOW_PIN;
    GPIO_Init(LED_GREEN_PORT, &GPIO_InitStructure);

    // 初始状态：全部灭
    GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);
    GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN | LED_YELLOW_PIN);
}

// ---- 红灯控制（高电平亮）----
void LED_RED_ON(void)  { GPIO_SetBits(LED_RED_PORT, LED_RED_PIN); }
void LED_RED_OFF(void) { GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN); }
void LED_RED_Turn(void) {  // 翻转
    if (GPIO_ReadOutputDataBit(LED_RED_PORT, LED_RED_PIN) == 0)
        GPIO_SetBits(LED_RED_PORT, LED_RED_PIN);
    else
        GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);
}

// ---- 绿灯控制（高电平亮）----
void LED_GREEN_ON(void)  { GPIO_SetBits(LED_GREEN_PORT, LED_GREEN_PIN); }
void LED_GREEN_OFF(void) { GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN); }
void LED_GREEN_Turn(void) {
    if (GPIO_ReadOutputDataBit(LED_GREEN_PORT, LED_GREEN_PIN) == 0)
        GPIO_SetBits(LED_GREEN_PORT, LED_GREEN_PIN);
    else
        GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN);
}

// ---- 黄灯控制（高电平亮）----
void LED_YELLOW_ON(void)  { GPIO_SetBits(LED_YELLOW_PORT, LED_YELLOW_PIN); }
void LED_YELLOW_OFF(void) { GPIO_ResetBits(LED_YELLOW_PORT, LED_YELLOW_PIN); }
void LED_YELLOW_Turn(void) {
    if (GPIO_ReadOutputDataBit(LED_YELLOW_PORT, LED_YELLOW_PIN) == 0)
        GPIO_SetBits(LED_YELLOW_PORT, LED_YELLOW_PIN);
    else
        GPIO_ResetBits(LED_YELLOW_PORT, LED_YELLOW_PIN);
}
