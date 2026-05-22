#include "stm32f10x.h"                  // Device header


void PWM_Led_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	//����ʱ����Ԫ
	TIM_TimeBaseInitTypeDef TIM_BaseInitStructure = {0};
	TIM_BaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_BaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_BaseInitStructure.TIM_Period = 100 - 1 ;//ARR
	TIM_BaseInitStructure.TIM_Prescaler =720 - 1;//PSC
	TIM_BaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2,&TIM_BaseInitStructure);
	
	//��������Ƚϲ���
	TIM_OCInitTypeDef TIM_OCInitStruct = {0};
	TIM_OCStructInit(&TIM_OCInitStruct);//���ṹ�帳��ʼֵ
	TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;//����Ƚ�ģʽ
	TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;//����Ƚϼ���
	TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_Pulse = 0;//CCR��ֵ
	TIM_OC1Init(TIM2, &TIM_OCInitStruct);                   
	
	//led��ʼ��
	
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	TIM_Cmd(TIM2,ENABLE);
}

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

void PWM_Motor_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	//����ʱ����Ԫ
	TIM_TimeBaseInitTypeDef TIM_BaseInitStructure = {0};
	TIM_BaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_BaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	/* 电机使用独立 TIM4，恢复 20kHz PWM */
	TIM_BaseInitStructure.TIM_Period = 50 - 1 ;//ARR
	TIM_BaseInitStructure.TIM_Prescaler = 72 - 1;//PSC
	TIM_BaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM4,&TIM_BaseInitStructure);
	
	//��������Ƚϲ���
	TIM_OCInitTypeDef TIM_OCInitStruct = {0};
	TIM_OCStructInit(&TIM_OCInitStruct);//���ṹ�帳��ʼֵ
	TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;//����Ƚ�ģʽ
	TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;//����Ƚϼ���
	TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_Pulse = 0;//CCR��ֵ
	TIM_OC1Init(TIM4, &TIM_OCInitStruct); 
	TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(TIM4, ENABLE);
	

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	
	TIM_Cmd(TIM4,ENABLE);
}



void PWM_Led_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM2,Compare);
}

void PWM_Servo_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare1(TIM3,Compare);
}

void PWM_Motor_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM4,Compare);
}
