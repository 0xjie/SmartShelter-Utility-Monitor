#include "dht11.h"
#include "delay.h"

void DHT11_Mode_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Pin = DHT11_PIN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

void DHT11_Mode_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_InitStruct.GPIO_Pin = DHT11_PIN;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

void DHT11_Init(void)
{
    DHT11_Mode_OUT();
    DHT11_SetHigh();
    Delay_ms(1000);  // 初始化后等待1秒
}

void DHT11_Start(void)
{
    DHT11_Mode_OUT();

    DHT11_SetLow();
    Delay_ms(18);  // 至少18ms

    DHT11_SetHigh();
    Delay_us(30);  // 20-40us

    DHT11_Mode_IN();
}

uint8_t DHT11_Check_Response(void)
{
    uint8_t retry = 0;

    // 等待高电平结束
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN) == SET && retry < 100)
    {
        Delay_us(1);
        retry++;
    }
    if(retry >= 100) return 0;

    // 等待低电平结束
    retry = 0;
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN) == RESET && retry < 100)
    {
        Delay_us(1);
        retry++;
    }
    if(retry >= 100) return 0;

    // 等待高电平结束
    retry = 0;
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN) == SET && retry < 100)
    {
        Delay_us(1);
        retry++;
    }
    if(retry >= 100) return 0;

    return 1;
}

uint8_t DHT11_Read_Byte(uint8_t *byte)
{
    uint8_t i;
    uint16_t timeout;

    *byte = 0;

    for(i = 0; i < 8; i++)
    {
        // 等待低电平结束（加超时避免卡死）
        timeout = 0;
        while(DHT11_Read() == 0)
        {
            if (++timeout > 200) return 0;
        }

        // 延时30us
        Delay_us(30);

        // 读取数据位
        if(DHT11_Read() == 1)
        {
            *byte |= (1 << (7 - i));
            // 等待高电平结束（加超时）
            timeout = 0;
            while(DHT11_Read() == 1)
            {
                if (++timeout > 200) return 0;
            }
        }
    }

    return 1;
}

uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t buffer[5];
    uint8_t i;

    DHT11_Start();

    if(DHT11_Check_Response() == 0)
        return 0;

    for(i = 0; i < 5; i++)
    {
        if(DHT11_Read_Byte(&buffer[i]) == 0)
            return 0;
    }

    // 校验
    if(buffer[4] == (buffer[0] + buffer[1] + buffer[2] + buffer[3]))
    {
        *humi = buffer[0];
        *temp = buffer[2];
        return 1;
    }

    return 0;
}
