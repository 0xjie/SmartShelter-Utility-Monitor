#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== 硬件引脚定义 ==================== */
#define SERIAL_USARTx          USART1
#define SERIAL_APB2Periph      RCC_APB2Periph_USART1
#define SERIAL_GPIO_RCC        RCC_APB2Periph_GPIOA
#define SERIAL_GPIO_PORT       GPIOA
#define SERIAL_TX_PIN          GPIO_Pin_9
#define SERIAL_RX_PIN          GPIO_Pin_10

/** 单行接收缓冲（JSON 下行命令最大长度） */
#define SERIAL_RX_LINE_MAX    256

/* ==================== 告警码体系 ==================== */
/* 温度 100-109 */
#define ALM_TEMP_HI_WARN    101
#define ALM_TEMP_HI_ALARM   102
#define ALM_TEMP_LO_WARN    103
#define ALM_TEMP_LO_ALARM   104
/* 湿度 110-119 */
#define ALM_HUMI_HI_WARN    111
#define ALM_HUMI_HI_ALARM   112
#define ALM_HUMI_LO_WARN    113
#define ALM_HUMI_LO_ALARM   114
/* PM2.5 120-129 */
#define ALM_PM25_WARN       121
#define ALM_PM25_ALARM      122
/* 空气质量 130-139 */
#define ALM_AQ_WARN         131
#define ALM_AQ_ALARM        132
/* 电流 140-149 */
#define ALM_CUR_WARN        141
#define ALM_CUR_ALARM       142
/* 水流 150-159 */
#define ALM_FLW_WARN        151
#define ALM_FLW_ALARM       152
/* 组合联动 200-209 */
#define ALM_CMB_TEMP_PM25   200
#define ALM_CMB_PM25_AQ     201
#define ALM_CMB_FLW_CUR     202
/* 传感器故障 900-909 */
#define FLT_DHT11           901
#define FLT_MQ135           902
#define FLT_GP2Y10          903
#define FLT_ACS712          904
#define FLT_FLOW            905

/* ==================== 动作标志位 ==================== */
#define ACT_FLAG_BUZZER_ON   0x01
#define ACT_FLAG_BUZZER_OFF  0x02
#define ACT_FLAG_FAN_ON      0x04
#define ACT_FLAG_FAN_OFF     0x08
#define ACT_FLAG_SERVO_OPEN  0x10
#define ACT_FLAG_SERVO_CLOSE 0x20

/* ==================== 上传数据包 ==================== */
typedef struct {
    /* 传感器原始值 */
    uint8_t  temp, humi;
    uint32_t pm25, aq;
    float    flow;
    int32_t  current_ma;
    uint8_t  battery_pct, water_pct;
    uint8_t  dht_ok;
    /* 联动级别 */
    uint8_t link_level, env_level;
    uint8_t temp_lv, humi_lv, pm25_lv, aq_lv, cur_lv, flw_lv;
    /* 执行器状态 (0/1) */
    uint8_t buzzer_on, fan_on, servo_angle;
    uint8_t led_r, led_g, led_y;
    /* 模式: 0=AUTO, 1=MANUAL */
    uint8_t global_manual;
    uint8_t buzzer_manual, fan_manual, servo_manual, led_manual;
    /* 告警列表 */
    uint8_t  alm_count;
    uint16_t alm_codes[8];
    uint8_t  alm_levels[8];
    /* 动作标志 */
    uint8_t  action_flags;
    /* 累计用量（STM32 自开机累计，日累计） */
    uint16_t total_power_mah;   // 已用电量 mAh
    uint16_t total_flow_cl;     // 已用水量 cL (0.01L)
    /* 水电详细值（避免Web/Qt本地计算） */
    uint16_t battery_remain_mah; // 电池剩余 mAh
    uint16_t water_remain_cl;    // 水量剩余 cL (0.01L)
    uint8_t  power_status;       // 电力状态: 0=正常/无负载 1=预警 2=告警
    uint16_t battery_capacity_mah; // 电池总容量 mAh
    uint16_t tank_capacity_cl;    // 水箱总容量 cL (0.01L)
    /* 预估剩余时间 */
    uint16_t battery_remain_min;
    uint16_t water_remain_min;
    /* 故障列表 */
    uint8_t  flt_count;
    uint16_t flt_codes[8];
} UploadPacket;

/* ==================== JSON 下行命令 ==================== */
typedef enum {
    JSONCMD_NONE = 0,
    JSONCMD_CTRL,
    JSONCMD_MODE,
    JSONCMD_TH,
    JSONCMD_RESET,
    JSONCMD_RESET_TH
} JsonCmdType;

typedef struct {
    JsonCmdType type;
    char  actuator[8];
    char  action[8];
    uint16_t angle;
    char  mode[8];
    char  th_key[4];
    uint16_t th_val;
} JsonCommand;

/* ==================== 函数声明 ==================== */

void Serial_Init(void);
void Serial_SendChar(uint8_t ch);
void Serial_SendString(const char *str);

/** 发送综合上传 JSON */
void Serial_SendUploadPacket(const UploadPacket *pkt);

uint8_t Serial_ReadLine(char *out_line, uint16_t max_len);

/** 解析下行命令（兼容旧数字/kkVVV + 新 JSON） */
uint8_t Serial_ParseCommand(const char *line, JsonCommand *cmd);

/* -- 旧接口保留兼容 -- */
void Serial_SendSensorToESP(uint8_t temp, uint8_t humi, uint32_t pm25_ugm3, uint32_t aq_ppm,
                            float flow_lpm, int32_t current_ma,
                            uint8_t battery_pct, uint8_t water_pct,
                            uint8_t link_level);
void Serial_ProcessControlDownlink(const char *line,
                                   uint8_t *buzzer_remote_mode,
                                   uint8_t *buzzer_remote_on,
                                   uint8_t *fan_manual_mode,
                                   uint8_t *fan_manual_on,
                                   uint8_t *servo_manual_mode,
                                   uint16_t *servo_manual_angle,
                                   uint8_t *led_alarm_manual_mode);
uint8_t Serial_ParseThresholdCmd(const char *line, char *out_key, uint16_t *out_value);

#endif
