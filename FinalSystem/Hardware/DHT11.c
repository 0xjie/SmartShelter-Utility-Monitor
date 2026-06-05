// ============================================================
// DHT11 数字温湿度传感器驱动
// ============================================================
// 引脚: PB13 (单总线双向数据)
// 协议: 主机发起→DHT11应答→40bit数据(湿度8+湿度小数8+温度8+温度小数8+校验8)
// 时序: 起始信号(18ms低+30µs高) → 响应(80µs低+80µs高) → 逐位50µs低+26/70µs高
//
// 关键技术:
//   1. 所有while等待加入超时计数(retry>200)，防止传感器接触不良导致MCU死锁
//   2. 校验和验证(前4字节和=第5字节)，防止数据传输出错
//   3. GPIO模式动态切换: 主机通信用推挽输出，接收数据用上拉输入
// ============================================================

#include "dht11.h"
#include "delay.h"

// ---- GPIO切换为输出模式（主机→传感器）----
void DHT11_Mode_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;   // 推挽输出
    GPIO_InitStruct.GPIO_Pin = DHT11_PIN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

// ---- GPIO切换为输入模式（传感器→主机）----
void DHT11_Mode_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入（总线空闲时高电平）
    GPIO_InitStruct.GPIO_Pin = DHT11_PIN;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

// ---- DHT11初始化 ----
void DHT11_Init(void)
{
    DHT11_Mode_OUT();
    DHT11_SetHigh();      // 总线拉高，进入空闲状态
    Delay_ms(1000);        // 上电后等待1秒让DHT11稳定（厂家建议）
}

// ---- 发送起始信号：拉低18ms + 拉高30µs ----
void DHT11_Start(void)
{
    DHT11_Mode_OUT();

    DHT11_SetLow();        // 拉低总线
    Delay_ms(18);          // 至少18ms（厂家要求）

    DHT11_SetHigh();       // 拉高总线
    Delay_us(30);          // 20~40µs等待DHT11响应

    DHT11_Mode_IN();       // 切换到输入模式准备接收
}

// ---- 检测DHT11响应信号（80µs低+80µs高）----
// 返回值: 1=正常响应, 0=超时/未连接
uint8_t DHT11_Check_Response(void)
{
    uint8_t retry = 0;

    // 阶段1: 等待DHT11拉低结束（应答低电平80µs）
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN) == SET && retry < 100)
    {
        Delay_us(1);
        retry++;
    }
    if(retry >= 100) return 0;  // 超时→传感器未连接

    // 阶段2: 等待DHT11拉高结束（应答高电平80µs）
    retry = 0;
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN) == RESET && retry < 100)
    {
        Delay_us(1);
        retry++;
    }
    if(retry >= 100) return 0;

    // 阶段3: 等待高电平结束（准备接收数据）
    retry = 0;
    while(GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_PIN) == SET && retry < 100)
    {
        Delay_us(1);
        retry++;
    }
    if(retry >= 100) return 0;

    return 1;
}

// ---- 读取一个字节（8位，高位在前）----
// 每位: 50µs低电平 + 26µs(0)/70µs(1)高电平 → 延时30µs后采样
// 返回值: 1=成功, 0=超时
uint8_t DHT11_Read_Byte(uint8_t *byte)
{
    uint8_t i;
    uint16_t timeout;

    *byte = 0;

    for(i = 0; i < 8; i++)
    {
        // 等待50µs低电平结束（加超时防卡死）
        timeout = 0;
        while(DHT11_Read() == 0)
        {
            if (++timeout > 200) return 0;  // 超时→传感器异常
        }

        // 延时30µs到高电平中点，判断0或1
        Delay_us(30);

        // 读取数据位：高电平=1，低电平=0
        if(DHT11_Read() == 1)
        {
            *byte |= (1 << (7 - i));  // 高位在前
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

// ---- 完整读取温湿度数据 ----
// 协议: Start → Check_Response → 读5字节 → 校验
// buffer[0]=湿度整数, [1]=湿度小数, [2]=温度整数, [3]=温度小数, [4]=校验和
// 返回值: 1=成功, 0=失败
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t buffer[5];
    uint8_t i;

    DHT11_Start();  // 发起通信

    if(DHT11_Check_Response() == 0)
        return 0;  // 传感器无应答

    // 读5字节数据
    for(i = 0; i < 5; i++)
    {
        if(DHT11_Read_Byte(&buffer[i]) == 0)
            return 0;  // 某字节读取超时
    }

    // 校验和验证：前4字节和 == 第5字节
    if(buffer[4] == (buffer[0] + buffer[1] + buffer[2] + buffer[3]))
    {
        *humi = buffer[0];  // 湿度整数部分
        *temp = buffer[2];  // 温度整数部分
        return 1;
    }

    return 0;  // 校验失败
}
