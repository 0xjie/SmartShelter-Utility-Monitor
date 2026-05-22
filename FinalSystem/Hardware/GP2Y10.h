#ifndef __GP2Y10_H
#define __GP2Y10_H

#include "stm32f10x.h"

/* 用户硬件引脚配置（根据实际接线修改）*/
#define GP2Y_LED_PORT       GPIOA
#define GP2Y_LED_PIN        GPIO_Pin_7
#define GP2Y_ADC_CHANNEL    ADC_Channel_2      /* PA2 */

/* 模块参数（PM2.5空气质量V2.1 模块专用 — GP2Y1014AU0F）
 *
 * 转接板有 1K+10K 电阻分压（11:1），商家例程已确认需乘 11 补偿。
 * 公式：
 *   voltage_mv = ADC_raw x 3300/4096 x 11
 *   PM2.5(ug/m3) = (voltage_mv - NO_DUST_MV) x 0.20
 *
 * NO_DUST_MV 校准（每颗传感器个体差异大）：
 * 1. 若静止空气读数为 0，逐步降低 NO_DUST_MV（每次减 30~50）
 * 2. 使静止读数在 5~30 ug/m3 之间即可
 * 3. 跳线穿过感应孔应能跳到 200+，说明传感器正常
 */
#define GP2Y_ADC_REF_MV     3300                /* ADC 参考电压 mV */
#define GP2Y_ADC_FULL_SCALE 4096                /* 12位ADC满量程 */
#define GP2Y_VOLTAGE_SCALE  11.0f               /* 转接板 1K+10K 分压补偿 */
#define GP2Y_NO_DUST_MV     200                 //无尘基准 mV（模块改3.3V供电后输出降低，调回200使静止~50）
#define GP2Y_COV_RATIO      0.20f               // mV -> ug/m3 转换系数
#define GP2Y_FILTER_WIN     10                  /* 滑动平均窗口 */

extern float dust_density;                      /* 最终粉尘浓度 µg/m³ */

void GP2Y_Init(void);
void Read_GP2Y10(void);                         /* 执行一次完整测量 */

#endif
