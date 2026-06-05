// ============================================================
// USART1 通信协议头文件
// ============================================================
// 包含：硬件配置 / 告警码体系(100-209) / 动作标志位 / 上传数据包 / 下行命令结构
// ============================================================

#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"
#include <stdint.h>

// ============================================================
// 第一部分：硬件引脚配置
// ============================================================
// USART1: PA9(TX 推挽复用) / PA10(RX 上拉输入)
// 参数: 115200 8N1，中断接收 + 环形缓冲
#define SERIAL_USARTx          USART1
#define SERIAL_APB2Periph      RCC_APB2Periph_USART1
#define SERIAL_GPIO_RCC        RCC_APB2Periph_GPIOA
#define SERIAL_GPIO_PORT       GPIOA
#define SERIAL_TX_PIN          GPIO_Pin_9
#define SERIAL_RX_PIN          GPIO_Pin_10

/** 单行接收缓冲最大长度（JSON下行命令） */
#define SERIAL_RX_LINE_MAX    256

// ============================================================
// 第二部分：告警码体系（100-209 数字编码）
// ============================================================
// 设计意图：用紧凑数字编码替代字符串，节省JSON传输带宽
// 分类规则：温度100段 / 湿度110段 / PM2.5 120段 / AQ 130段 / 电流140段 / 水流150段 / 组合200段
// 温度 100-109
#define ALM_TEMP_HI_WARN    101  // 温度偏高预警
#define ALM_TEMP_HI_ALARM   102  // 温度偏高告警
#define ALM_TEMP_LO_WARN    103  // 温度偏低预警
#define ALM_TEMP_LO_ALARM   104  // 温度偏低告警
// 湿度 110-119
#define ALM_HUMI_HI_WARN    111  // 湿度偏高预警
#define ALM_HUMI_HI_ALARM   112  // 湿度偏高告警
#define ALM_HUMI_LO_WARN    113  // 湿度偏低预警
#define ALM_HUMI_LO_ALARM   114  // 湿度偏低告警
// PM2.5 120-129
#define ALM_PM25_WARN       121  // PM2.5预警
#define ALM_PM25_ALARM      122  // PM2.5告警
// 空气质量 130-139
#define ALM_AQ_WARN         131  // 空气质量预警
#define ALM_AQ_ALARM        132  // 空气质量告警
// 电流 140-149
#define ALM_CUR_WARN        141  // 电流预警
#define ALM_CUR_ALARM       142  // 电流告警
// 水流 150-159
#define ALM_FLW_WARN        151  // 水流预警
#define ALM_FLW_ALARM       152  // 水流告警
// 组合联动 200-209（多传感器同时异常时升级）
#define ALM_CMB_TEMP_PM25   200  // 温度+PM2.5组合告警
#define ALM_CMB_PM25_AQ     201  // PM2.5+AQ组合告警
#define ALM_CMB_FLW_CUR     202  // 水流+电流组合预警

// ============================================================
// 第三部分：动作标志位（bitmap编码）
// ============================================================
// 设计意图：用一个字节的bitmap表示联动触发了哪些动作
// Web/Qt端可直接按位判断，无需解析字符串
#define ACT_FLAG_BUZZER_ON   0x01  // bit0: 蜂鸣器开
#define ACT_FLAG_BUZZER_OFF  0x02  // bit1: 蜂鸣器关
#define ACT_FLAG_FAN_ON      0x04  // bit2: 风扇开
#define ACT_FLAG_FAN_OFF     0x08  // bit3: 风扇关
#define ACT_FLAG_SERVO_OPEN  0x10  // bit4: 窗户开
#define ACT_FLAG_SERVO_CLOSE 0x20  // bit5: 窗户关

// ============================================================
// 第四部分：上传数据包（STM32 → ESP32 → 云端）
// ============================================================
// 一次上传包含完整的系统状态快照，Web/Qt端无需回查计算
typedef struct {
    // ---- 传感器原始值 ----
    uint8_t  temp, humi;         // 温度(℃), 湿度(%)
    uint32_t pm25, aq;           // PM2.5(µg/m³), 空气质量(ppm)
    float    flow;               // 瞬时流量(L/min)
    int32_t  current_ma;         // 电流(mA，已缩放)
    uint8_t  battery_pct, water_pct;  // 水电剩余百分比(0-100)
    uint8_t  dht_ok;             // DHT11读取成功标志(1=成功)

    // ---- 联动级别 ----
    uint8_t link_level, env_level;  // 全系统级别, 环境级别(0正常/1预警/2告警)
    uint8_t temp_lv, humi_lv, pm25_lv, aq_lv, cur_lv, flw_lv;  // 各传感器级别

    // ---- 执行器状态 ----
    uint8_t buzzer_on, fan_on, servo_angle;  // 蜂鸣器(0/1), 风扇(0/1), 舵机角度(0-90)
    uint8_t led_r, led_g, led_y;             // LED三色状态(0/1)

    // ---- 模式状态（0=AUTO自动, 1=MANUAL手动）----
    uint8_t global_manual;                   // 全局手动模式
    uint8_t buzzer_manual, fan_manual, servo_manual, led_manual;  // 各执行器手动模式

    // ---- 当前生效的告警阈值（供Qt/Web同步显示）----
    uint16_t th_ta, th_tb, th_ha, th_hb;     // 温度告警上下限, 湿度告警上下限
    uint16_t th_pa, th_aa, th_ca, th_fa;     // PM2.5/AQ/电流/水流告警

    // ---- 告警码列表 ----
    uint8_t  alm_count;          // 实际告警码数量(≤8)
    uint16_t alm_codes[8];       // 告警码数组(组合告警优先)
    uint8_t  alm_levels[8];      // 对应级别

    // ---- 动作标志（bitmap，见ACT_FLAG_xxx）----
    uint8_t  action_flags;

    // ---- 累计用量（开机以来的总消耗）----
    uint16_t total_power_mah;    // 已用电量 mAh
    uint16_t total_flow_cl;      // 已用水量 cL (0.01L)

    // ---- 水电详细值（避免Web/Qt本地重复计算）----
    uint16_t battery_remain_mah;  // 电池剩余 mAh
    uint16_t water_remain_cl;     // 水量剩余 cL
    uint8_t  power_status;        // 电力状态: 0正常/1预警/2告警
    uint16_t battery_capacity_mah;// 电池总容量 mAh
    uint16_t tank_capacity_cl;    // 水箱总容量 cL

    // ---- 预估剩余时间 ----
    uint16_t battery_remain_min;  // 电池剩余可用(分钟)
    uint16_t water_remain_min;    // 水量剩余可用(分钟)
} UploadPacket;

// ============================================================
// 第五部分：JSON下行命令结构
// ============================================================
typedef enum {
    JSONCMD_NONE = 0,     // 空/未识别
    JSONCMD_CTRL,         // 执行器控制: {"cmd":"ctrl","act":"fan","val":"on"}
    JSONCMD_MODE,         // 全局模式:   {"cmd":"mode","val":"MANUAL"}
    JSONCMD_TH,           // 阈值修改:   {"cmd":"th","key":"ta","val":38}
    JSONCMD_RESET,        // 全局复位:   {"cmd":"reset"}
    JSONCMD_RESET_TH      // 阈值复位:   {"cmd":"reset_th"}
} JsonCmdType;

typedef struct {
    JsonCmdType type;         // 命令类型
    char  actuator[8];        // 执行器名: "buzzer"/"fan"/"servo"/"led"
    char  action[8];          // 动作: "on"/"off"/"auto" 或 "#"开头=角度
    uint16_t angle;           // 舵机目标角度(0-180)
    char  mode[8];            // 模式: "MANUAL"/"AUTO"
    char  th_key[4];          // 阈值key: "ta"/"tb"/"ha"/"hb"/"pa"/"aa"/"ca"/"fa"等
    uint16_t th_val;          // 阈值value
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
