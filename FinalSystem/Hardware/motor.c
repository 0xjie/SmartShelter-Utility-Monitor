#include "stm32f10x.h"                  // Device header
#include "pwm.h"

void Motor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	PWM_Motor_Init();
}

void Motor_SetSpeed(int8_t Speed)
{
	if (Speed > 100) Speed = 100;
	if (Speed < -100) Speed = -100;

	uint16_t compare = (uint16_t)((Speed >= 0 ? Speed : -Speed) * 50 / 100);

	if(Speed>=0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_0);
		GPIO_ResetBits(GPIOB,GPIO_Pin_1);
		PWM_Motor_SetCompare1(compare);
	}
	else
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_1);
		GPIO_ResetBits(GPIOB,GPIO_Pin_0);
		PWM_Motor_SetCompare1(compare);
	}

}


void Fan_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_6);
}

void Fan_Set(uint8_t run)
{
	if (run)
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_6);
	}
	else
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_6);
	}
}
