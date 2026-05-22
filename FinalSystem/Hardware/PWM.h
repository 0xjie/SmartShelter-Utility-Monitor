#ifndef __PWM_H
#define __PWM_H

void PWM_Led_Init(void);
void PWM_Motor_Init(void);
void PWM_Servo_Init(void);

void PWM_Led_SetCompare1(uint16_t Compare);
void PWM_Servo_SetCompare2(uint16_t Compare);
void PWM_Motor_SetCompare1(uint16_t Compare);

#endif
