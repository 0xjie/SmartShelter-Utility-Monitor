#ifndef __MOTOR_H
#define __MOTOR_H

void Fan_Init(void);                        /* 初始化风扇控制引脚 PB6 */

/* PB6 低电平有效：拉低=风扇转，拉高=停 */
void Fan_Set(uint8_t run);

#endif

