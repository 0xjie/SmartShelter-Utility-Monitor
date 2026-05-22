#include "stm32f10x.h"                  // Device header
#define LED_RED_PORT     GPIOA
#define LED_RED_PIN      GPIO_Pin_15
#define LED_GREEN_PORT   GPIOA
#define LED_GREEN_PIN    GPIO_Pin_11
#define LED_YELLOW_PORT  GPIOA
#define LED_YELLOW_PIN   GPIO_Pin_12

void LED_Init(void)
{
	/* 红灯使用 PA15。F103 默认把 PA15 当作 JTAG 的 JTDI，不释放则 GPIO 推挽很弱或微亮。 */
	/* 关闭 JTAG、保留 SW-DP，PA15 才能正常驱动 LED；下载调试仍可用 PA13/PA14（SWD）。 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = LED_RED_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_RED_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = LED_GREEN_PIN | LED_YELLOW_PIN;
	GPIO_Init(LED_GREEN_PORT, &GPIO_InitStructure);
	
	GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);
	GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN | LED_YELLOW_PIN);
}

void LED_RED_ON(void)
{
	GPIO_SetBits(LED_RED_PORT, LED_RED_PIN);
}

void LED_RED_OFF(void)
{
	GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);
}

void LED_RED_Turn(void)
{
	if (GPIO_ReadOutputDataBit(LED_RED_PORT, LED_RED_PIN) == 0)
	{
		GPIO_SetBits(LED_RED_PORT, LED_RED_PIN);
	}
	else
	{
		GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);
	}
}

void LED_GREEN_ON(void)
{
	GPIO_SetBits(LED_GREEN_PORT, LED_GREEN_PIN);
}

void LED_GREEN_OFF(void)
{
	GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN);
}

void LED_GREEN_Turn(void)
{
	if (GPIO_ReadOutputDataBit(LED_GREEN_PORT, LED_GREEN_PIN) == 0)
	{
		GPIO_SetBits(LED_GREEN_PORT, LED_GREEN_PIN);
	}
	else
	{
		GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN);
	}
}

void LED_YELLOW_ON(void)
{
	GPIO_SetBits(LED_YELLOW_PORT, LED_YELLOW_PIN);
}

void LED_YELLOW_OFF(void)
{
	GPIO_ResetBits(LED_YELLOW_PORT, LED_YELLOW_PIN);
}

void LED_YELLOW_Turn(void)
{
	if (GPIO_ReadOutputDataBit(LED_YELLOW_PORT, LED_YELLOW_PIN) == 0)
	{
		GPIO_SetBits(LED_YELLOW_PORT, LED_YELLOW_PIN);
	}
	else
	{
		GPIO_ResetBits(LED_YELLOW_PORT, LED_YELLOW_PIN);
	}
}
