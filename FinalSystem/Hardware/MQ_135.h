#ifndef __MQ_135_H
#define __MQ_135_H

#include "stm32f10x.h"

#define MQ135_AVG_SAMPLES          16
#define MQ135_BASELINE_SAMPLES     20
#define MQ135_SENSOR_VCC           5.0f
#define MQ135_LOAD_RESISTOR_KOHM   10.0f
#define MQ135_RS_DEADBAND_PERCENT  5.0f

void MQ135_Init(void);
uint16_t MQ135_Get_ADC(void);
float MQ135_Get_Voltage(void);
float MQ135_Get_PPM(void);

#endif
