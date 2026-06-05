#include "stm32f10x.h"                  // Device header


void PWM_Servo_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	//����ʱ����Ԫ
	TIM_TimeBaseInitTypeDef TIM_BaseInitStructure = {0};
	TIM_BaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_BaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_BaseInitStructure.TIM_Period = 20000 - 1 ;//ARR
	TIM_BaseInitStructure.TIM_Prescaler =72 - 1;//PSC
	TIM_BaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3,&TIM_BaseInitStructure);
	
	//��������Ƚϲ���
	TIM_OCInitTypeDef TIM_OCInitStruct = {0};
	TIM_OCStructInit(&TIM_OCInitStruct);//���ṹ�帳��ʼֵ
	TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;//����Ƚ�ģʽ
	TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;//����Ƚϼ���
	TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_Pulse = 0;//CCR��ֵ
	TIM_OC1Init(TIM3, &TIM_OCInitStruct); 
	

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	
	TIM_Cmd(TIM3,ENABLE);
}

void PWM_Servo_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare1(TIM3,Compare);
}

