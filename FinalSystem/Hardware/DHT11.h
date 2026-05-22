#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

#define DHT11_GPIO_PORT     GPIOB
#define DHT11_PIN           GPIO_Pin_13

#define DHT11_SetLow()      GPIO_ResetBits(DHT11_GPIO_PORT, DHT11_PIN)
#define DHT11_SetHigh()     GPIO_SetBits(DHT11_GPIO_PORT, DHT11_PIN)
#define DHT11_Read()        GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN)

void DHT11_Init(void);
void DHT11_Mode_OUT(void);
void DHT11_Mode_IN(void);
void DHT11_Start(void);
uint8_t DHT11_Check_Response(void);
uint8_t DHT11_Read_Byte(uint8_t *byte);
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);

#endif
