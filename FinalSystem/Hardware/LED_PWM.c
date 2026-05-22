#include "stm32f10x.h"                  // Device header
#include "pwm.h"
#include "delay.h"

void LED_Pwm_Init(void)
{
	PWM_Led_Init();
}
void LED_Breath(void)
{
	uint8_t i = 0;
	for(i = 0;i<100;i++)
		{
			PWM_Led_SetCompare1(99 - i);
			Delay_ms(10);
		}
		Delay_ms(100);
		for(i = 0;i<100;i++)
		{
			PWM_Led_SetCompare1(i);
			Delay_ms(10);
		}
}
