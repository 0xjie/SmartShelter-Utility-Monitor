#ifndef __WATERFLOW_H
#define __WATERFLOW_H

#include "stm32f10x.h"

extern float flow_rate_L_per_min;          // instantaneous flow L/min
extern float total_flow_L;                 // accumulated water volume L

void Flow_Sensor_Init(void);
float Flow_Sensor_Get_FlowRate(void);
float Flow_Sensor_Get_TotalFlow(void);
void Flow_Sensor_Reset_Total(void);

void Flow_Sensor_Display_On_OLED(void);   

#endif
