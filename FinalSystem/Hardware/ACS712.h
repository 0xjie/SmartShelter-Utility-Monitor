#ifndef __ACS712_H
#define __ACS712_H

#include "stm32f10x.h"

// ADC通道（PA0）
#define ACS712_ADC_CHANNEL ADC_Channel_0

// 5A量程灵敏度（厂商参数：185mV/A）
#define ACS712_SENSITIVITY 0.185f

// 采样次数（600次×68µs≈41ms，覆盖2个50Hz工频周期）
#define ACS712_SAMPLE_COUNT 600

void ACS712_Init(void);
float ACS712_GetCurrent(void);

#endif
