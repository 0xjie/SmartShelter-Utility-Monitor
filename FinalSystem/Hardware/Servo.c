#include "stm32f10x.h"                  // Device header
#include "pwm.h"

void Servo_Init(void)
{
	PWM_Servo_Init();
}

void Servo_SetAngle(uint16_t Angle)
{
	if (Angle > 180) Angle = 180;
    
    // ½Ç¶È×ªÂö¿í£º500 + angle * (2000 / 180)
    uint16_t pulse = 500 + (uint16_t)((float)Angle * 2000 / 180);
    
    PWM_Servo_SetCompare2(pulse);
}