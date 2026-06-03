// ============================================================
// 灾后临时安置点智慧水电管理与环境监测系统 — STM32F103C8T6 主固件
// ============================================================
// 功能概述：
//   1. 6路传感器采集（温湿度/PM2.5/空气质量/电流/水流），1秒周期
//   2. 三级联动告警状态机（正常→预警→告警），含滞回+连续确认+组合联动
//   3. 水电剩余量模拟（库仑计数+脉冲累加），无额外硬件
//   4. 手动/自动双模式执行器控制，5分钟看门狗自动恢复
//   5. USART1 与 ESP32 通信：上传传感器JSON，接收远程命令
//   6. OLED 双页显示（传感器数据页 / 告警阈值页）
//   7. LED三色状态指示 + 蜂鸣器分级报警 + 舵机/风扇联动
// ============================================================

#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "led.h"
#include "buzzer.h"
#include "dht11.h"
#include "serial.h"
#include "servo.h"
#include "motor.h"
#include "MQ_135.h"
#include "waterflow.h"
#include "GP2Y10.h"
#include "ACS712.h"
#include <stdio.h>
#include <string.h>

// ============================================================
// 第一部分：阈值配置（RAM变量，可通过远程命令实时修改）
// ============================================================
// 设计理念：Qt端只下发告警阈值→STM32自动推算出预警阈值
// 这样保证预警和告警的比例关系始终一致，不会因人工设错而失调

// ---- 告警阈值（默认值，远程可改）----
static uint16_t TEMP_ALARM_H  = 38;   // 温度告警上限(℃)，命令ta → 推导tr=32
static uint16_t TEMP_ALARM_L  = 10;   // 温度告警下限(℃)，命令tb → 推导tc=18
static uint16_t HUMI_ALARM_H  = 85;   // 湿度告警上限(%)，命令ha → 推导hw=70
static uint16_t HUMI_ALARM_L  = 20;   // 湿度告警下限(%)，命令hb → 推导hd=30
static uint16_t PM25_ALARM    = 150;  // PM2.5告警(µg/m³)，命令pa → 推导ph=75
static uint16_t AQ_ALARM      = 200;  // 空气质量告警(ppm)，命令aa → 推导aq=100
static uint16_t CUR_ALARM     = 20;   // 电流告警(A)，命令ca → 推导ci=10
static uint16_t FLW_ALARM     = 20;   // 水流告警(L/min)，命令fa → 推导fl=5

// ---- 预警阈值（由告警阈值自动推导，也可通过旧命令直接覆盖）----
static uint16_t TEMP_WARN_H   = 32;   // 温度预警上限(℃)
static uint16_t TEMP_WARN_L   = 18;   // 温度预警下限(℃)
static uint16_t HUMI_WARN_H   = 70;   // 湿度预警上限(%)
static uint16_t HUMI_WARN_L   = 30;   // 湿度预警下限(%)
static uint16_t PM25_WARN     = 75;   // PM2.5预警(µg/m³)
static uint16_t AQ_WARN       = 100;  // 空气质量预警(ppm)
static uint16_t CUR_WARN      = 10;   // 电流预警(A)
static uint16_t FLW_WARN      = 5;    // 水流预警(L/min)

// ---- 告警→预警自动推导函数 ----
// 被所有告警阈值修改命令（ta/tb/ha/hb/pa/aa/ca/fa）调用
// 推导规则确保预警值始终比告警值宽松，且比例合理
static void derive_warn_from_alarm(void) {
    TEMP_WARN_H = TEMP_ALARM_H - 6;    // 温度：告警-6℃ = 预警
    TEMP_WARN_L = TEMP_ALARM_L + 8;    // 温度：告警+8℃ = 预警
    HUMI_WARN_H = HUMI_ALARM_H - 15;   // 湿度：告警-15% = 预警
    HUMI_WARN_L = HUMI_ALARM_L + 10;   // 湿度：告警+10% = 预警
    PM25_WARN   = PM25_ALARM / 2;      // PM2.5：告警的一半
    AQ_WARN     = AQ_ALARM / 2;        // AQ：告警的一半
    CUR_WARN    = CUR_ALARM * 2 / 3;   // 电流：告警的2/3
    FLW_WARN    = FLW_ALARM / 2;       // 水流：告警的一半
}

// ---- 恢复出厂默认告警值 + 自动推导预警 ----
// 被 JSON 命令 {"cmd":"reset_th"} 触发
static void reset_thresholds_to_default(void) {
    TEMP_ALARM_H = 38; TEMP_ALARM_L = 10;
    HUMI_ALARM_H = 85; HUMI_ALARM_L = 20;
    PM25_ALARM   = 150;
    AQ_ALARM     = 200;
    CUR_ALARM    = 15;
    FLW_ALARM    = 10;
    derive_warn_from_alarm();
}



// ---- 手动模式看门狗 ----
// 
#define MANUAL_WDOG_TICKS   30000  

// ---- 滞回(Hysteresis)偏移量 ----

#define TEMP_HYST_WARN   2       // 温度预警滞回 ±2℃
#define TEMP_HYST_ALARM  3       // 温度告警滞回 ±3℃
#define HUMI_HYST_WARN   5       // 湿度预警滞回 ±5%
#define HUMI_HYST_ALARM  5       // 湿度告警滞回 ±5%
#define PM25_HYST_WARN   10      // PM2.5预警滞回 ±10µg/m³
#define PM25_HYST_ALARM  20      // PM2.5告警滞回 ±20µg/m³
#define AQ_HYST_WARN     15      // AQ预警滞回 ±15ppm
#define AQ_HYST_ALARM    30      // AQ告警滞回 ±30ppm
#define CUR_HYST_WARN    1       // 电流预警滞回 ±1A (rpt_ma用，×1000)
#define CUR_HYST_ALARM   2       // 电流告警滞回 ±2A
#define FLW_HYST_WARN    2       // 水流预警滞回 ±2L/min (rpt_f用)
#define FLW_HYST_ALARM   3       // 水流告警滞回 ±3L/min

// ---- 连续触发确认次数 ----

#define WARN_ENTRY_CNT   3       // 进入预警需连续3次(3秒)
#define ALARM_ENTRY_CNT  5       // 进入告警需连续5次(5秒)
#define WARN_EXIT_CNT    3       // 退出预警需连续3次
#define ALARM_EXIT_CNT   5       // 退出告警需连续5次

// ---- 水电剩余量默认容量 ----
// 演示用小容量（方便吹气/开关负载触发变化），真实场景通过 bc/wc 命令修改
static uint16_t BATTERY_CAPACITY_mAh = 10000;  // 电池总容量 10000mAh
static uint16_t TANK_CAPACITY_L      = 500;    // 水箱总容量 500L

// ---- 传感器缩放系数 ----
// DEMO阶段放大读数模拟真实场景；真实部署时改为1
#define CUR_SCALE  10    // 电流×10倍：400mA→上报4A（模拟大功率负载）
#define FLW_SCALE  1     // 水流真实值（不做缩放）

// ---- 联动级别枚举 ----
#define LINK_NORMAL  0   // 正常
#define LINK_WARN    1   // 预警
#define LINK_ALARM   2   // 告警

// ---- 蜂鸣器报警节奏参数 ----

#define BUZZER_BEEP_ON_TICKS   10     // 预警单次响10tick(100ms)
#define BUZZER_BEEP_PERIOD     200    // 预警周期200tick(2秒)
#define BUZZER_ALARM_ON1_END    10    // 告警第一段响0~10tick
#define BUZZER_ALARM_ON2_START  20    // 告警第二段响20~30tick
#define BUZZER_ALARM_ON2_END    30
#define BUZZER_ALARM_PERIOD     100    // 告警周期100tick(1秒)

#define SERVO_HOME       0            // 舵机初始角度(关窗)
#define OLED_REFRESH_MS  300          // OLED刷新间隔300ms
#define OLED_PAGE_MS     3000         // OLED页面切换间隔3秒

// ============================================================
// 第三部分：工具函数
// ============================================================

// ---- 三色LED统一控制 ----
// 输入参数为布尔值：1=亮，0=灭
// LED高电平亮（PA15红灯需先禁用JTAG才能正常驱动，见LED_Init）
static void Apply_RGB_Leds(uint8_t red, uint8_t green, uint8_t yellow)
{
    if (red)   LED_RED_ON();    else LED_RED_OFF();
    if (green) LED_GREEN_ON();  else LED_GREEN_OFF();
    if (yellow)LED_YELLOW_ON(); else LED_YELLOW_OFF();
}

// ---- 告警码列表生成 ----
// 将6个传感器的告警级别转换为结构化告警码数组，供上传JSON使用
// 告警码范围：温度100-109, 湿度110-119, PM2.5 120-129, AQ 130-139,
//             电流140-149, 水流150-159, 组合联动200-209
// 组合告警优先排在数组前面，方便Web/Qt端优先展示
static uint8_t build_alarm_codes(uint16_t *codes, uint8_t *levels, uint8_t max_n,
                                 uint8_t temp_lv, uint8_t humi_lv, uint8_t pm25_lv,
                                 uint8_t aq_lv, uint8_t cur_lv, uint8_t flw_lv,
                                 uint8_t link_level, uint8_t has_combo_temp_pm25,
                                 uint8_t has_combo_pm25_aq, uint8_t has_combo_flw_cur)
{
    uint8_t n = 0;
    if (n >= max_n) return n;

    // 组合告警优先排在数组前面
    if (has_combo_temp_pm25) { codes[n] = ALM_CMB_TEMP_PM25; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n; }
    if (has_combo_pm25_aq)   { codes[n] = ALM_CMB_PM25_AQ;   levels[n] = LINK_ALARM; n++; if (n >= max_n) return n; }
    if (has_combo_flw_cur)   { codes[n] = ALM_CMB_FLW_CUR;   levels[n] = LINK_WARN;  n++; if (n >= max_n) return n; }

    // 温度告警码
    if (temp_lv == LINK_ALARM) {
        codes[n] = ALM_TEMP_HI_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (temp_lv == LINK_WARN) {
        codes[n] = ALM_TEMP_HI_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 湿度告警码
    if (humi_lv == LINK_ALARM) {
        codes[n] = ALM_HUMI_HI_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (humi_lv == LINK_WARN) {
        codes[n] = ALM_HUMI_HI_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // PM2.5告警码
    if (pm25_lv == LINK_ALARM) {
        codes[n] = ALM_PM25_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (pm25_lv == LINK_WARN) {
        codes[n] = ALM_PM25_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 空气质量告警码
    if (aq_lv == LINK_ALARM) {
        codes[n] = ALM_AQ_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (aq_lv == LINK_WARN) {
        codes[n] = ALM_AQ_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 电流告警码
    if (cur_lv == LINK_ALARM) {
        codes[n] = ALM_CUR_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (cur_lv == LINK_WARN) {
        codes[n] = ALM_CUR_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 水流告警码
    if (flw_lv == LINK_ALARM) {
        codes[n] = ALM_FLW_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (flw_lv == LINK_WARN) {
        codes[n] = ALM_FLW_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    return n;  // 返回实际填充的告警码数量
}

// ---- 判断某传感器是否处于预警或更高级别 ----
// 用于组合联动判断（温度+PM2.5同时≥预警 → 升级为告警）
static uint8_t is_warn_or_alarm(uint8_t lv) { return lv >= LINK_WARN ? 1 : 0; }

// ---- 阈值修改应答 ----
// 每次远程修改阈值后，回传ACK确认，包含当前告警阈值供Qt/Web同步
static void send_threshold_ack(const char *key, uint16_t val)
{
    char ack[96];
    sprintf(ack, "{\"ack\":\"th\",\"key\":\"%s\",\"val\":%u,\"ta\":%u,\"ha\":%u}\r\n",
            key, (unsigned int)val,
            (unsigned int)TEMP_ALARM_H, (unsigned int)HUMI_ALARM_H);
    Serial_SendString(ack);
}

// ---- 阈值复位应答 ----
static void send_reset_threshold_ack(void)
{
    char ack[80];
    sprintf(ack, "{\"ack\":\"reset_th\",\"ta\":%u,\"ha\":%u}\r\n",
            (unsigned int)TEMP_ALARM_H, (unsigned int)HUMI_ALARM_H);
    Serial_SendString(ack);
}

// ---- 单执行器控制命令执行 ----
// 接收 Serial_ParseCommand() 解析后的 JSONCMD_CTRL 命令，将其翻译为各执行器的
// 手动模式标志。每个执行器独立维护手动/自动状态，互不干扰。
// 调用链：Qt按钮 → MQTT → ESP32 → UART → Serial_ParseCommand() → 本函数
// 命令格式示例：
//   蜂鸣器开: {"cmd":"ctrl","act":"buzzer","val":"on"}   → bz_rm=1, bz_ro=1
//   风扇关:   {"cmd":"ctrl","act":"fan","val":"off"}     → fan_mm=1, fan_mo=0
//   窗户开90°:{"cmd":"ctrl","act":"servo","val":"90"}    → svo_mm=1, svo_ma=90
//   恢复自动: {"cmd":"ctrl","act":"buzzer","val":"auto"} → bz_rm=0
// 手动 vs 自动优先级：
//   manual_mode=1 → 按手动设定值执行（忽略自动联动）
//   manual_mode=0 → 由自动联动逻辑根据 link_level 决定执行器状态
// 手动模式看门狗：main.c 主循环中 manual_wdog_tick 每秒+1，
//   达到30000（约5分钟）后自动将全部执行器恢复为自动模式
static void exec_control_cmd(const JsonCommand *cmd,
                             uint8_t *bz_rm, uint8_t *bz_ro,
                             uint8_t *fan_mm, uint8_t *fan_mo,
                             uint8_t *svo_mm, uint16_t *svo_ma,
                             uint8_t *led_mm)
{
    if (cmd->type != JSONCMD_CTRL) return;

    // 蜂鸣器控制：on/off/auto 三态
    if (strcmp(cmd->actuator, "buzzer") == 0) {
        if (strcmp(cmd->action, "on") == 0)      { *bz_rm = 1; *bz_ro = 1; }  // 手动开
        else if (strcmp(cmd->action, "off") == 0) { *bz_rm = 1; *bz_ro = 0; }  // 手动关
        else if (strcmp(cmd->action, "auto") == 0){ *bz_rm = 0; *bz_ro = 0; }  // 恢复自动
    }
    // 风扇控制
    else if (strcmp(cmd->actuator, "fan") == 0) {
        if (strcmp(cmd->action, "on") == 0)      { *fan_mm = 1; *fan_mo = 1; }
        else if (strcmp(cmd->action, "off") == 0) { *fan_mm = 1; *fan_mo = 0; }
        else if (strcmp(cmd->action, "auto") == 0){ *fan_mm = 0; *fan_mo = 0; }
    }
    // 舵机控制：支持角度设置(action以#开头表示角度值)
    else if (strcmp(cmd->actuator, "servo") == 0) {
        if (cmd->action[0] == '#')                { *svo_mm = 1; *svo_ma = cmd->angle; }  // 手动设角度
        else if (strcmp(cmd->action, "auto") == 0){ *svo_mm = 0; *svo_ma = 0; }           // 恢复自动
    }
    // LED控制（手动模式下三色全亮表示人工接管）
    else if (strcmp(cmd->actuator, "led") == 0) {
        if (strcmp(cmd->action, "on") == 0)       { *led_mm = 1; }
        else if (strcmp(cmd->action, "auto") == 0){ *led_mm = 0; }
    }
}

// ---- 全局模式切换（一键切换所有执行器的手动/自动）----
// 命令格式：{"cmd":"mode","val":"MANUAL"} 或 {"cmd":"mode","val":"AUTO"}
// MANUAL: 所有执行器进入手动模式（保持当前状态不变）
// AUTO:   所有执行器恢复自动联动（清除所有手动标志）
// 此功能用于紧急情况下的人工接管或恢复正常运行
static void exec_mode_cmd(const JsonCommand *cmd,
                          uint8_t *global_manual,
                          uint8_t *bz_rm, uint8_t *fan_mm, uint8_t *svo_mm, uint8_t *led_mm)
{
    if (cmd->type != JSONCMD_MODE) return;
    if (strcmp(cmd->mode, "MANUAL") == 0) {
        // 全局切手动：所有执行器进入手动模式
        *global_manual = 1;
        *bz_rm = 1; *fan_mm = 1; *svo_mm = 1; *led_mm = 1;
    } else if (strcmp(cmd->mode, "AUTO") == 0) {
        // 全局切自动：所有执行器恢复自动联动
        *global_manual = 0;
        *bz_rm = 0; *fan_mm = 0; *svo_mm = 0; *led_mm = 0;
    }
}

// ---- 全局复位（所有执行器恢复自动 + 清除手动状态）----
// 命令格式：{"cmd":"reset"} 或 数字码"8"
// 将所有执行器的手动标志清零，恢复自动联动控制。
// 同时重置手动模式看门狗（0→约5分钟后触发自动恢复）
// 来源：Qt设备页右上角恢复按钮 / Web看板reset / 数字码8
static void exec_reset_cmd(uint8_t *global_manual,
                           uint8_t *bz_rm, uint8_t *bz_ro,
                           uint8_t *fan_mm, uint8_t *fan_mo,
                           uint8_t *svo_mm, uint16_t *svo_ma,
                           uint8_t *led_mm)
{
    *global_manual = 0;
    *bz_rm = 0; *bz_ro = 0;
    *fan_mm = 0; *fan_mo = 0;
    *svo_mm = 0; *svo_ma = 0;
    *led_mm = 0;
}

// ============================================================
// 第四部分：主函数
// ============================================================
// 主循环以10ms为Tick，所有定时间隔通过累加器实现：
//   sensor_interval: 传感器采样周期(1000ms)
//   oled_interval:   OLED刷新周期(300ms)
//   oled_page_interval: OLED页面切换周期(3000ms)
// ============================================================
int main(void)
{
    // ---- 传感器原始值 ----
    uint8_t temp = 0, humi = 0, dht_ok = 0;  // dht_ok: DHT11读取成功标志
    uint32_t sensor_interval = 0, oled_interval = 0;  // Tick累加器
    uint32_t pm25_ugm3 = 0, aq_ppm = 0;      // PM2.5(µg/m³), 空气质量(ppm)
    float flow_lpm = 0.0f;                    // 瞬时流量(L/min)
    int32_t current_ma = 0;                   // 原始电流(mA)
    int32_t rpt_ma = 0;                       // 上报电流(原始×缩放系数)
    float   rpt_f  = 0.0f;                    // 上报流量(原始×缩放系数)
    char line[17];                            // OLED行缓冲区(16字符+空字符)
    char esp_line[SERIAL_RX_LINE_MAX];        // 串口接收行缓冲区
    uint32_t oled_page_interval = 0;          // OLED页面切换Tick累加器
    uint8_t oled_page = 0;                    // 当前OLED页面: 0=传感器数据, 1=告警阈值

    // ---- 模式/手动状态变量 ----
    // 设计理念：每个执行器独立维护手动/自动状态，互不干扰
    uint8_t global_manual = 0;                // 全局手动模式标志(1=全手动)
    // 蜂鸣器：remote_mode=手动接管, remote_on=手动开关状态
    uint8_t buzzer_remote_mode = 0, buzzer_remote_on = 0;
    // 风扇：manual_mode=手动接管, manual_on=手动开关状态
    uint8_t fan_manual_mode = 0, fan_manual_on = 0;
    // 舵机：manual_mode=手动接管, manual_angle=手动目标角度
    uint8_t servo_manual_mode = 0; uint16_t servo_manual_angle = 0;
    // LED：manual_mode=手动接管(手动模式下三色全亮)
    uint8_t led_manual_mode = 0;

    // ---- 水电剩余量模拟变量 ----
    float battery_used_mAh = 0.0f;            // 已消耗电量(mAh)，每秒积分累加
    uint8_t battery_pct = 100, water_pct = 100; // 剩余百分比(0-100)

    // ---- 联动级别 ----
    // link_level: 全系统最差级别（LED/蜂鸣器据此动作）
    // env_level: 环境传感器最差级别（风扇/舵机据此动作，不含水流/电流）
    uint8_t link_level = LINK_NORMAL, env_level = LINK_NORMAL;
    // 6个传感器各自的告警级别
    uint8_t temp_level = LINK_NORMAL, humi_level = LINK_NORMAL;
    uint8_t pm25_level = LINK_NORMAL, aq_level = LINK_NORMAL;
    uint8_t cur_level = LINK_NORMAL, flw_level = LINK_NORMAL;

    // 自动联动产生的执行器目标值
    uint8_t fan_auto_on = 0, servo_auto_angle = 0;
    uint16_t buzzer_tick = 0;                 // 蜂鸣器节奏计数器

    // ---- 滞回计数器 ----
    // 每个传感器4个计数器：
    //   xxx_warn_cnt: 进入预警的连续命中计数
    //   xxx_alarm_cnt: 进入告警的连续命中计数
    //   xxx_warn_rec: 退出预警的连续恢复计数
    //   xxx_alarm_rec: 退出告警的连续恢复计数
    uint8_t temp_warn_cnt = 0, temp_alarm_cnt = 0, temp_warn_rec = 0, temp_alarm_rec = 0;
    uint8_t humi_warn_cnt = 0, humi_alarm_cnt = 0, humi_warn_rec = 0, humi_alarm_rec = 0;
    uint8_t pm25_warn_cnt = 0, pm25_alarm_cnt = 0, pm25_warn_rec = 0, pm25_alarm_rec = 0;
    uint8_t aq_warn_cnt   = 0, aq_alarm_cnt   = 0, aq_warn_rec   = 0, aq_alarm_rec   = 0;
    uint8_t cur_warn_cnt  = 0, cur_alarm_cnt  = 0, cur_warn_rec  = 0, cur_alarm_rec  = 0;
    uint8_t flw_warn_cnt  = 0, flw_alarm_cnt  = 0, flw_warn_rec  = 0, flw_alarm_rec  = 0;

    // 手动模式看门狗Tick计数器（每秒+1，达30000即5分钟触发自动恢复）
    uint32_t manual_wdog_tick = 0;

    // ---- 组合联动标志 ----
    // combo_temp_pm25: 温度+PM2.5同时≥预警 → 环境级别强制升级告警
    // combo_pm25_aq:   PM2.5+AQ同时≥预警 → 环境级别强制升级告警
    // combo_flw_cur:   水流+电流同时≥预警 → 仅上报告警码，不触发执行器
    uint8_t combo_temp_pm25 = 0, combo_pm25_aq = 0, combo_flw_cur = 0;

    // 上传数据包和下行命令解析结构体
    UploadPacket pkt;
    JsonCommand jcmd;


    LED_Init();           
    Buzzer_Init();        
    DHT11_Init();         
    MQ135_Init();        
    OLED_Init();          
    GP2Y_Init();         
    Servo_Init();        
    Serial_Init();       
    ACS712_Init();      
    OLED_Clear();
    Delay_ms(500);        // 等待所有传感器稳定

    // 初始状态：绿灯亮(正常)，蜂鸣器不响，舵机关窗
    Apply_RGB_Leds(0, 1, 0);
    Buzzer_OFF();
    Servo_SetAngle(SERVO_HOME);  // 0° = 关窗
    Flow_Sensor_Init();          // PA5 EXTI下降沿中断+TIM2 1秒定时器
    Fan_Init();                  // PB6 推挽输出，高电平停

    while (1)
    {
        // ============================================================
        // 区块A：下行命令处理（最高优先级，每个10ms Tick都检查）
        // ============================================================
        // 数据来源：ESP32 → UART2(TX=GPIO17) → STM32 PA10(USART1 RX)
        // 缓冲机制：USART1 RXNE中断 → rx_ring_push(256字节环形缓冲)
        // 解析路径：Serial_ReadLine() 读一行 → Serial_ParseCommand() 解析 → 本switch分发
        //
        // 全链路（Qt一键下发阈值）：
        //   Qt [一键下发] → MQTT("command") → 巴法云 → ESP32 callback()
        //   → handleCommandPayload() 识别 kind="threshold"
        //   → forwardThresholdToStm32() → Serial2 UART → STM32 USART1 RXNE
        //   → 环形缓冲 → Serial_ReadLine() → Serial_ParseCommand() → JSONCMD_TH
        //
        // 全链路（Qt远程控制）：
        //   Qt [警报开启] → MQTT("command") → 巴法云 → ESP32 callback()
        //   → forwardDigitToStm32(1) → Serial2.println("1")
        //   → 环形缓冲 → Serial_ReadLine() → Serial_ParseCommand() → JSONCMD_CTRL
        //   → exec_control_cmd() → GPIO/PWM立即动作
        while (Serial_ReadLine(esp_line, (uint16_t)sizeof(esp_line)))
        {
            if (Serial_ParseCommand(esp_line, &jcmd))
            {
                switch (jcmd.type) {
                case JSONCMD_RESET_TH:
                    reset_thresholds_to_default();
                    send_reset_threshold_ack();
                    temp_level = humi_level = pm25_level = aq_level = cur_level = flw_level = LINK_NORMAL;
                    link_level = env_level = LINK_NORMAL;
                    temp_warn_cnt = 0; temp_alarm_cnt = 0; temp_warn_rec = 0; temp_alarm_rec = 0;
                    humi_warn_cnt = 0; humi_alarm_cnt = 0; humi_warn_rec = 0; humi_alarm_rec = 0;
                    pm25_warn_cnt = 0; pm25_alarm_cnt = 0; pm25_warn_rec = 0; pm25_alarm_rec = 0;
                    aq_warn_cnt = 0; aq_alarm_cnt = 0; aq_warn_rec = 0; aq_alarm_rec = 0;
                    cur_warn_cnt = 0; cur_alarm_cnt = 0; cur_warn_rec = 0; cur_alarm_rec = 0;
                    flw_warn_cnt = 0; flw_alarm_cnt = 0; flw_warn_rec = 0; flw_alarm_rec = 0;
                    break;
                case JSONCMD_RESET:
                    manual_wdog_tick = 0;
                    exec_reset_cmd(&global_manual,
                                   &buzzer_remote_mode, &buzzer_remote_on,
                                   &fan_manual_mode, &fan_manual_on,
                                   &servo_manual_mode, &servo_manual_angle,
                                   &led_manual_mode);
                    break;
                case JSONCMD_MODE:
                    manual_wdog_tick = 0;
                    exec_mode_cmd(&jcmd, &global_manual,
                                  &buzzer_remote_mode, &fan_manual_mode,
                                  &servo_manual_mode, &led_manual_mode);
                    break;
                case JSONCMD_CTRL:
                    manual_wdog_tick = 0;  // 重置看门狗
                    exec_control_cmd(&jcmd,
                                     &buzzer_remote_mode, &buzzer_remote_on,
                                     &fan_manual_mode, &fan_manual_on,
                                     &servo_manual_mode, &servo_manual_angle,
                                     &led_manual_mode);
                    break;
                // ---- 阈值修改（来自 Qt [一键下发] 按钮）----
                // 数据来源：ESP32 forwardThresholdToStm32() 发送的
                //   {"cmd":"th","key":"ta","val":38}
                // Qt 只设置告警阈值（ta/tb/ha/hb/pa/aa/ca/fa），
                // STM32 自动推导对应的预警阈值，维持告警与预警的合理间距。
                // 推导规则：
                //   温度告警上限ta-6→预警上限    温度告警下限tb+8→预警下限
                //   湿度告警上限ha-15→预警上限   湿度告警下限hb+10→预警下限
                //   PM25/AQ/水流告警/2→预警      电流告警*2/3→预警
                // 同时兼容旧预警key（tr/tc/hw/hd/ph/aq/ci/fl）——只设预警不动告警
                case JSONCMD_TH: {
                    char *k = jcmd.th_key;
                    uint16_t v = jcmd.th_val;
                    // 告警key → 设告警 + 自动推导预警
                    if      (strcmp(k, "ta") == 0) { TEMP_ALARM_H = v; TEMP_WARN_H = v - 6; }
                    else if (strcmp(k, "tb") == 0) { TEMP_ALARM_L = v; TEMP_WARN_L = v + 8; }
                    else if (strcmp(k, "ha") == 0) { HUMI_ALARM_H = v; HUMI_WARN_H = v - 15; }
                    else if (strcmp(k, "hb") == 0) { HUMI_ALARM_L = v; HUMI_WARN_L = v + 10; }
                    else if (strcmp(k, "pa") == 0) { PM25_ALARM   = v; PM25_WARN   = v / 2; }
                    else if (strcmp(k, "aa") == 0) { AQ_ALARM     = v; AQ_WARN     = v / 2; }
                    else if (strcmp(k, "ca") == 0) { CUR_ALARM    = v; CUR_WARN    = v * 2 / 3; }
                    else if (strcmp(k, "fa") == 0) { FLW_ALARM    = v; FLW_WARN    = v / 2; }
                    // 旧预警key（兼容旧协议，仅设置预警值不推导告警值）
                    else if (strcmp(k, "tr") == 0) TEMP_WARN_H = v;
                    else if (strcmp(k, "tc") == 0) TEMP_WARN_L = v;
                    else if (strcmp(k, "hw") == 0) HUMI_WARN_H = v;
                    else if (strcmp(k, "hd") == 0) HUMI_WARN_L = v;
                    else if (strcmp(k, "ph") == 0) PM25_WARN = v;
                    else if (strcmp(k, "aq") == 0) AQ_WARN = v;
                    else if (strcmp(k, "ci") == 0) CUR_WARN = v;
                    else if (strcmp(k, "fl") == 0) FLW_WARN = v;
                    // 水电容量/校准命令（bc/bp=电池, wc/wp=水量）
                    else if (strcmp(k, "bc") == 0) { BATTERY_CAPACITY_mAh = v; battery_used_mAh = 0; }
                    else if (strcmp(k, "bp") == 0) { if (v > 100) v = 100; battery_used_mAh = BATTERY_CAPACITY_mAh * (100.0f - v) / 100.0f; }
                    else if (strcmp(k, "wc") == 0) { TANK_CAPACITY_L = v; Flow_Sensor_Reset_Total(); }
                    else if (strcmp(k, "wp") == 0) { if (v > 100) v = 100; total_flow_L = TANK_CAPACITY_L * (100.0f - v) / 100.0f; }
                    // 向 ESP32 发送 ACK 确认回执
                    send_threshold_ack(k, v);
                    // 阈值修改后，重置所有传感器的滞回计数器和级别，
                    // 确保新阈值在下一次采样时立即生效（不被旧滞回状态影响）
                    temp_level = humi_level = pm25_level = aq_level = cur_level = flw_level = LINK_NORMAL;
                    link_level = env_level = LINK_NORMAL;
                    temp_warn_cnt = 0; temp_alarm_cnt = 0; temp_warn_rec = 0; temp_alarm_rec = 0;
                    humi_warn_cnt = 0; humi_alarm_cnt = 0; humi_warn_rec = 0; humi_alarm_rec = 0;
                    pm25_warn_cnt = 0; pm25_alarm_cnt = 0; pm25_warn_rec = 0; pm25_alarm_rec = 0;
                    aq_warn_cnt = 0; aq_alarm_cnt = 0; aq_warn_rec = 0; aq_alarm_rec = 0;
                    cur_warn_cnt = 0; cur_alarm_cnt = 0; cur_warn_rec = 0; cur_alarm_rec = 0;
                    flw_warn_cnt = 0; flw_alarm_cnt = 0; flw_warn_rec = 0; flw_alarm_rec = 0;
                    break;
                }
                default: break;
                }
            }
        }

        // ============================================================
        // 区块B：传感器采集 + 水电积分 + 三级联动评估（1秒周期）
        // ============================================================
        if (sensor_interval >= 1000)
        {
            // --- B1: 传感器数据读取 ---
            dht_ok = DHT11_Read_Data(&temp, &humi);  // 单总线读取，带超时保护
            Read_GP2Y10();                             // 10ms完整时序：LED开0.28ms→ADC采样→LED关→等9.68ms
            pm25_ugm3 = (uint32_t)(dust_density + 0.5f);  // GP2Y10全局变量dust_density→四舍五入
            aq_ppm = (uint32_t)(MQ135_Get_PPM() + 0.5f);  // ADC→0~4095线性映射到0~500ppm，含低通滤波
            flow_lpm = Flow_Sensor_Get_FlowRate();         // 脉冲差×60/450，含EMA低通滤波(α=0.3)

            // ACS712电流：原始值含工频纹波，取值后经DC blocker处理
            float cur_f = ACS712_GetCurrent();             // 返回A，600次采样取平均
            current_ma = (int32_t)(cur_f * 1000.0f);       // A→mA
            if (current_ma >= 5000) current_ma = 0;        // 异常值剔除(5A以上视为无效)

            // --- B2: 传感器缩放（DEMO放大模拟真实场景，真实部署时CUR_SCALE=1） ---
            rpt_ma = current_ma * CUR_SCALE;   // 上报电流 = 原始×缩放系数
            rpt_f  = flow_lpm   * (float)FLW_SCALE;  // 上报流量

            // --- B3: 水电剩余量积分计算 ---
            // 电量：库仑计数法，每秒累加 I(mA)/3600 → 消耗mAh
            battery_used_mAh += (float)rpt_ma / 3600.0f;
            if (battery_used_mAh >= BATTERY_CAPACITY_mAh)
                battery_used_mAh = (float)BATTERY_CAPACITY_mAh;  // 下限钳位(不出现负%)
            // 水量：total_flow_L 由 Flow_Sensor_Get_FlowRate() 内部的 EMA滤波值 每秒积分
            if (total_flow_L >= TANK_CAPACITY_L)
                total_flow_L = TANK_CAPACITY_L;
            // 百分比计算
            battery_pct = (uint8_t)((1.0f - battery_used_mAh / BATTERY_CAPACITY_mAh) * 100.0f);
            water_pct = (uint8_t)((1.0f - total_flow_L / TANK_CAPACITY_L) * 100.0f);
            if (water_pct > 100) water_pct = 100;

           
            // DHT11温湿度传感器（温度+湿度共用DHT11，所以放在同一个dht_ok判断里）
            if (dht_ok)
            {

                // 分支1：当前告警→检查退出（值回到ALARM阈值-滞回以内+连续确认5次）
                if (temp_level == LINK_ALARM) {
                    temp_warn_cnt = 0; temp_alarm_cnt = 0; temp_warn_rec = 0;
                    if (temp <= (TEMP_ALARM_H - TEMP_HYST_ALARM) && temp >= (TEMP_ALARM_L + TEMP_HYST_ALARM)) {
                        temp_alarm_rec++;
                        if (temp_alarm_rec >= ALARM_EXIT_CNT) {
                            // ALARM退出→若值已在正常区间，直接跳到NORMAL（跳过WARN）
                            // 若只在WARN区间内→降为WARN
                            if (temp <= TEMP_WARN_H && temp >= TEMP_WARN_L)
                                temp_level = LINK_NORMAL;
                            else
                                temp_level = LINK_WARN;
                        }
                    } else temp_alarm_rec = 0;
                }
                // 分支2：当前预警→检查升级(to告警)或退出(to正常)
                else if (temp_level == LINK_WARN) {
                    temp_warn_cnt = 0; temp_alarm_rec = 0;
                    // 升级：达到告警阈值，连续5次→升级ALARM
                    if (temp >= TEMP_ALARM_H || temp <= TEMP_ALARM_L) {
                        temp_alarm_cnt++; temp_warn_rec = 0;
                        if (temp_alarm_cnt >= ALARM_ENTRY_CNT) temp_level = LINK_ALARM;
                    } else {
                        temp_alarm_cnt = 0;
                        // 退出：回到预警阈值-滞回以内，连续3次→恢复NORMAL
                        if (temp <= (TEMP_WARN_H - TEMP_HYST_WARN) && temp >= (TEMP_WARN_L + TEMP_HYST_WARN)) {
                            temp_warn_rec++;
                            if (temp_warn_rec >= WARN_EXIT_CNT) temp_level = LINK_NORMAL;
                        } else temp_warn_rec = 0;
                    }
                }
                // 分支3：当前正常→检查升级
                else {
                    temp_alarm_rec = 0; temp_warn_rec = 0;
                    if (temp >= TEMP_ALARM_H || temp <= TEMP_ALARM_L) {
                        // 值已在ALARM区间→强制先入WARN（保证NORMAL→WARN→ALARM路径，不跳跃）
                        temp_alarm_cnt = 0;
                        temp_warn_cnt = WARN_ENTRY_CNT;
                        temp_level = LINK_WARN;
                    } else {
                        temp_alarm_cnt = 0;
                        if (temp >= TEMP_WARN_H || temp <= TEMP_WARN_L) {
                            temp_warn_cnt++;
                            if (temp_warn_cnt >= WARN_ENTRY_CNT) temp_level = LINK_WARN;
                        } else temp_warn_cnt = 0;
                    }
                }

                // === 湿度状态机（逻辑与温度完全一致）===
                if (humi_level == LINK_ALARM) {
                    humi_warn_cnt = 0; humi_alarm_cnt = 0; humi_warn_rec = 0;
                    if (humi <= (HUMI_ALARM_H - HUMI_HYST_ALARM) && humi >= (HUMI_ALARM_L + HUMI_HYST_ALARM)) {
                        humi_alarm_rec++;
                        if (humi_alarm_rec >= ALARM_EXIT_CNT) {
                            // 值已回到正常范围 → 直接NORMAL，跳过WARN
                            if (humi <= HUMI_WARN_H && humi >= HUMI_WARN_L)
                                humi_level = LINK_NORMAL;
                            else
                                humi_level = LINK_WARN;
                        }
                    } else humi_alarm_rec = 0;
                } else if (humi_level == LINK_WARN) {
                    humi_warn_cnt = 0; humi_alarm_rec = 0;
                    if (humi >= HUMI_ALARM_H || humi <= HUMI_ALARM_L) {
                        humi_alarm_cnt++; humi_warn_rec = 0;
                        if (humi_alarm_cnt >= ALARM_ENTRY_CNT) humi_level = LINK_ALARM;
                    } else {
                        humi_alarm_cnt = 0;
                        if (humi <= (HUMI_WARN_H - HUMI_HYST_WARN) && humi >= (HUMI_WARN_L + HUMI_HYST_WARN)) {
                            humi_warn_rec++;
                            if (humi_warn_rec >= WARN_EXIT_CNT) humi_level = LINK_NORMAL;
                        } else humi_warn_rec = 0;
                    }
                } else {
                    humi_alarm_rec = 0; humi_warn_rec = 0;
                    if (humi >= HUMI_ALARM_H || humi <= HUMI_ALARM_L) {
                        // 跳变到ALARM区间 → 强制先入WARN，保证NORMAL→WARN→ALARM路径
                        humi_alarm_cnt = 0;
                        humi_warn_cnt = WARN_ENTRY_CNT;
                        humi_level = LINK_WARN;
                    } else {
                        humi_alarm_cnt = 0;
                        if (humi >= HUMI_WARN_H || humi <= HUMI_WARN_L) {
                            humi_warn_cnt++;
                            if (humi_warn_cnt >= WARN_ENTRY_CNT) humi_level = LINK_WARN;
                        } else humi_warn_cnt = 0;
                    }
                }
            }

            // PM2.5
            if (pm25_level == LINK_ALARM) {
                pm25_warn_cnt = 0; pm25_alarm_cnt = 0; pm25_warn_rec = 0;
                if (pm25_ugm3 <= (PM25_ALARM - PM25_HYST_ALARM)) {
                    pm25_alarm_rec++;
                    if (pm25_alarm_rec >= ALARM_EXIT_CNT) {
                        // 值已回到正常范围 → 直接NORMAL，跳过WARN
                        if (pm25_ugm3 <= PM25_WARN)
                            pm25_level = LINK_NORMAL;
                        else
                            pm25_level = LINK_WARN;
                    }
                } else pm25_alarm_rec = 0;
            } else if (pm25_level == LINK_WARN) {
                pm25_warn_cnt = 0; pm25_alarm_rec = 0;
                if (pm25_ugm3 >= PM25_ALARM) {
                    pm25_alarm_cnt++; pm25_warn_rec = 0;
                    if (pm25_alarm_cnt >= ALARM_ENTRY_CNT) pm25_level = LINK_ALARM;
                } else {
                    pm25_alarm_cnt = 0;
                    if (pm25_ugm3 <= (PM25_WARN - PM25_HYST_WARN)) {
                        pm25_warn_rec++;
                        if (pm25_warn_rec >= WARN_EXIT_CNT) pm25_level = LINK_NORMAL;
                    } else pm25_warn_rec = 0;
                }
            } else {
                pm25_alarm_rec = 0; pm25_warn_rec = 0;
                if (pm25_ugm3 >= PM25_ALARM) {
                    // 跳变到ALARM区间 → 强制先入WARN，保证NORMAL→WARN→ALARM路径
                    pm25_alarm_cnt = 0;
                    pm25_warn_cnt = WARN_ENTRY_CNT;
                    pm25_level = LINK_WARN;
                } else {
                    pm25_alarm_cnt = 0;
                    if (pm25_ugm3 >= PM25_WARN) {
                        pm25_warn_cnt++;
                        if (pm25_warn_cnt >= WARN_ENTRY_CNT) pm25_level = LINK_WARN;
                    } else pm25_warn_cnt = 0;
                }
            }

            // 空气质量
            if (aq_level == LINK_ALARM) {
                aq_warn_cnt = 0; aq_alarm_cnt = 0; aq_warn_rec = 0;
                if (aq_ppm <= (AQ_ALARM - AQ_HYST_ALARM)) {
                    aq_alarm_rec++;
                    if (aq_alarm_rec >= ALARM_EXIT_CNT) {
                        // 值已回到正常范围 → 直接NORMAL，跳过WARN
                        if (aq_ppm <= AQ_WARN)
                            aq_level = LINK_NORMAL;
                        else
                            aq_level = LINK_WARN;
                    }
                } else aq_alarm_rec = 0;
            } else if (aq_level == LINK_WARN) {
                aq_warn_cnt = 0; aq_alarm_rec = 0;
                if (aq_ppm >= AQ_ALARM) {
                    aq_alarm_cnt++; aq_warn_rec = 0;
                    if (aq_alarm_cnt >= ALARM_ENTRY_CNT) aq_level = LINK_ALARM;
                } else {
                    aq_alarm_cnt = 0;
                    if (aq_ppm <= (AQ_WARN - AQ_HYST_WARN)) {
                        aq_warn_rec++;
                        if (aq_warn_rec >= WARN_EXIT_CNT) aq_level = LINK_NORMAL;
                    } else aq_warn_rec = 0;
                }
            } else {
                aq_alarm_rec = 0; aq_warn_rec = 0;
                if (aq_ppm >= AQ_ALARM) {
                    // 跳变到ALARM区间 → 强制先入WARN，保证NORMAL→WARN→ALARM路径
                    aq_alarm_cnt = 0;
                    aq_warn_cnt = WARN_ENTRY_CNT;
                    aq_level = LINK_WARN;
                } else {
                    aq_alarm_cnt = 0;
                    if (aq_ppm >= AQ_WARN) {
                        aq_warn_cnt++;
                        if (aq_warn_cnt >= WARN_ENTRY_CNT) aq_level = LINK_WARN;
                    } else aq_warn_cnt = 0;
                }
            }

            // 电流
            if (cur_level == LINK_ALARM) {
                cur_warn_cnt = 0; cur_alarm_cnt = 0; cur_warn_rec = 0;
                if ((int32_t)rpt_ma <= (int32_t)(CUR_ALARM - CUR_HYST_ALARM) * 1000) {
                    cur_alarm_rec++;
                    if (cur_alarm_rec >= ALARM_EXIT_CNT) {
                        // 值已回到正常范围 → 直接NORMAL，跳过WARN
                        if ((int32_t)rpt_ma <= (int32_t)CUR_WARN * 1000)
                            cur_level = LINK_NORMAL;
                        else
                            cur_level = LINK_WARN;
                    }
                } else cur_alarm_rec = 0;
            } else if (cur_level == LINK_WARN) {
                cur_warn_cnt = 0; cur_alarm_rec = 0;
                if ((int32_t)rpt_ma >= (int32_t)CUR_ALARM * 1000) {
                    cur_alarm_cnt++; cur_warn_rec = 0;
                    if (cur_alarm_cnt >= ALARM_ENTRY_CNT) cur_level = LINK_ALARM;
                } else {
                    cur_alarm_cnt = 0;
                    if ((int32_t)rpt_ma <= (int32_t)(CUR_WARN - CUR_HYST_WARN) * 1000) {
                        cur_warn_rec++;
                        if (cur_warn_rec >= WARN_EXIT_CNT) cur_level = LINK_NORMAL;
                    } else cur_warn_rec = 0;
                }
            } else {
                cur_alarm_rec = 0; cur_warn_rec = 0;
                if ((int32_t)rpt_ma >= (int32_t)CUR_ALARM * 1000) {
                    // 跳变到ALARM区间 → 强制先入WARN，保证NORMAL→WARN→ALARM路径
                    cur_alarm_cnt = 0;
                    cur_warn_cnt = WARN_ENTRY_CNT;
                    cur_level = LINK_WARN;
                } else {
                    cur_alarm_cnt = 0;
                    if ((int32_t)rpt_ma >= (int32_t)CUR_WARN * 1000) {
                        cur_warn_cnt++;
                        if (cur_warn_cnt >= WARN_ENTRY_CNT) cur_level = LINK_WARN;
                    } else cur_warn_cnt = 0;
                }
            }

            // 水流
            if (flw_level == LINK_ALARM) {
                flw_warn_cnt = 0; flw_alarm_cnt = 0; flw_warn_rec = 0;
                if (rpt_f <= (FLW_ALARM - FLW_HYST_ALARM)) {
                    flw_alarm_rec++;
                    if (flw_alarm_rec >= ALARM_EXIT_CNT) {
                        // 值已回到正常范围 → 直接NORMAL，跳过WARN
                        if (rpt_f <= FLW_WARN)
                            flw_level = LINK_NORMAL;
                        else
                            flw_level = LINK_WARN;
                    }
                } else flw_alarm_rec = 0;
            } else if (flw_level == LINK_WARN) {
                flw_warn_cnt = 0; flw_alarm_rec = 0;
                if (rpt_f >= FLW_ALARM) {
                    flw_alarm_cnt++; flw_warn_rec = 0;
                    if (flw_alarm_cnt >= ALARM_ENTRY_CNT) flw_level = LINK_ALARM;
                } else {
                    flw_alarm_cnt = 0;
                    if (rpt_f <= (FLW_WARN - FLW_HYST_WARN)) {
                        flw_warn_rec++;
                        if (flw_warn_rec >= WARN_EXIT_CNT) flw_level = LINK_NORMAL;
                    } else flw_warn_rec = 0;
                }
            } else {
                flw_alarm_rec = 0; flw_warn_rec = 0;
                if (rpt_f >= FLW_ALARM) {
                    // 跳变到ALARM区间 → 强制先入WARN，保证NORMAL→WARN→ALARM路径
                    flw_alarm_cnt = 0;
                    flw_warn_cnt = WARN_ENTRY_CNT;
                    flw_level = LINK_WARN;
                } else {
                    flw_alarm_cnt = 0;
                    if (rpt_f >= FLW_WARN) {
                        flw_warn_cnt++;
                        if (flw_warn_cnt >= WARN_ENTRY_CNT) flw_level = LINK_WARN;
                    } else flw_warn_cnt = 0;
                }
            }

            // ============================================================
            // B5: 传感器级别聚合（取最差级别）+ 组合联动规则
            // ============================================================
            // link_level: 全系统最差级别，驱动LED/蜂鸣器（仅环境传感器参与，水流/电流不驱动执行器）
            link_level = LINK_NORMAL;
            if (temp_level == LINK_ALARM || humi_level == LINK_ALARM ||
                pm25_level == LINK_ALARM || aq_level  == LINK_ALARM)
                link_level = LINK_ALARM;
            else if (temp_level == LINK_WARN || humi_level == LINK_WARN ||
                     pm25_level == LINK_WARN || aq_level  == LINK_WARN)
                link_level = LINK_WARN;

            // env_level: 环境传感器最差级别，驱动风扇/舵机（与link_level相同逻辑，但语义独立）
            env_level = LINK_NORMAL;
            if (temp_level == LINK_ALARM || humi_level == LINK_ALARM ||
                pm25_level == LINK_ALARM || aq_level  == LINK_ALARM)
                env_level = LINK_ALARM;
            else if (temp_level == LINK_WARN || humi_level == LINK_WARN ||
                     pm25_level == LINK_WARN || aq_level  == LINK_WARN)
                env_level = LINK_WARN;

            // ---- 组合联动规则 ----
            // 创新点：多传感器同时异常时联动升级，比单传感器更敏感地反映复合风险
            combo_temp_pm25 = 0; combo_pm25_aq = 0; combo_flw_cur = 0;
            // 规则1: 高温+高粉尘同时≥预警 → 强制升级为告警（高温加剧粉尘危害）
            if (is_warn_or_alarm(temp_level) && is_warn_or_alarm(pm25_level)) {
                if (env_level < LINK_ALARM)  env_level  = LINK_ALARM;
                if (link_level < LINK_ALARM) link_level = LINK_ALARM;
                combo_temp_pm25 = 1;
            }
            // 规则2: PM2.5+AQ同时≥预警 → 强制升级为告警（双污染源叠加）
            if (is_warn_or_alarm(pm25_level) && is_warn_or_alarm(aq_level)) {
                if (env_level < LINK_ALARM)  env_level  = LINK_ALARM;
                if (link_level < LINK_ALARM) link_level = LINK_ALARM;
                combo_pm25_aq = 1;
            }

            // ---- 自动联动目标值的确定 ----
            // ALARM: 风扇转，窗户全开90°（通风排烟）
            // WARN:  风扇转，窗户关0°（预警阶段不急着开窗，开风扇降温通风即可）
            // NORMAL: 风扇停，窗户关
            if (env_level == LINK_ALARM)      { fan_auto_on = 1; servo_auto_angle = 90; }
            else if (env_level == LINK_WARN)  { fan_auto_on = 1; servo_auto_angle = 0; }
            else                              { fan_auto_on = 0; servo_auto_angle = 0; }

            // 规则3: 水流+电流同时≥预警 → 仅上报告警码，不执行任何物理措施
            // （水电同时异常可能是正常用水用电行为，不宜自动断电断水）
            if (is_warn_or_alarm(flw_level) && is_warn_or_alarm(cur_level)) {
                combo_flw_cur = 1;
            }

            // ---- 执行器输出：手动优先于自动 ----
            // 三目运算符：手动模式下用手动设定值，自动模式下用联动计算值
            Fan_Set(fan_manual_mode ? fan_manual_on : fan_auto_on);
            Servo_SetAngle(servo_manual_mode ? servo_manual_angle : servo_auto_angle);

            // ============================================================
            // B6: 填充上传数据包 → 通过USART1发送JSON到ESP32
            // ============================================================
            memset(&pkt, 0, sizeof(pkt));
            // ---- 传感器原始值 ----
            pkt.temp = temp; pkt.humi = humi; pkt.dht_ok = dht_ok;
            pkt.pm25 = pm25_ugm3; pkt.aq = aq_ppm;
            pkt.flow = rpt_f; pkt.current_ma = rpt_ma;
            pkt.battery_pct = battery_pct; pkt.water_pct = water_pct;
            // ---- 级别信息 ----
            pkt.link_level = link_level; pkt.env_level = env_level;
            pkt.temp_lv = temp_level; pkt.humi_lv = humi_level;
            pkt.pm25_lv = pm25_level; pkt.aq_lv = aq_level;
            pkt.cur_lv = cur_level; pkt.flw_lv = flw_level;
            // ---- 模式状态 ----
            pkt.global_manual = global_manual;
            pkt.buzzer_manual = buzzer_remote_mode;
            pkt.fan_manual = fan_manual_mode;
            pkt.servo_manual = servo_manual_mode;
            pkt.led_manual = led_manual_mode;
            // ---- 当前生效的告警阈值（供Qt/Web同步显示）----
            pkt.th_ta = TEMP_ALARM_H; pkt.th_tb = TEMP_ALARM_L;
            pkt.th_ha = HUMI_ALARM_H; pkt.th_hb = HUMI_ALARM_L;
            pkt.th_pa = PM25_ALARM; pkt.th_aa = AQ_ALARM;
            pkt.th_ca = CUR_ALARM; pkt.th_fa = FLW_ALARM;

            // ---- 执行器实际状态（手动优先）----
            {
                uint8_t fan_is_on = fan_manual_mode ? fan_manual_on : fan_auto_on;
                uint8_t svo_ang = servo_manual_mode ? servo_manual_angle : servo_auto_angle;
                uint8_t led_r = 0, led_g = 0, led_y = 0;
                if (led_manual_mode) { led_r = 1; led_g = 1; led_y = 1; }  // 手动模式三色全亮
                else {
                    if (link_level == LINK_ALARM)      led_r = 1;  // 告警→红灯
                    else if (link_level == LINK_WARN)  led_y = 1;  // 预警→黄灯
                    else                               led_g = 1;  // 正常→绿灯
                }
                pkt.buzzer_on = buzzer_remote_mode ? buzzer_remote_on : (link_level >= LINK_WARN ? 1 : 0);
                pkt.fan_on = fan_is_on;
                pkt.servo_angle = svo_ang;
                pkt.led_r = led_r; pkt.led_g = led_g; pkt.led_y = led_y;
            }

            // ---- 告警码列表（组合告警优先排列）----
            pkt.alm_count = build_alarm_codes(pkt.alm_codes, pkt.alm_levels, 8,
                temp_level, humi_level, pm25_level, aq_level, cur_level, flw_level,
                link_level, combo_temp_pm25, combo_pm25_aq, combo_flw_cur);

            // ---- 动作标志位（bitmap编码，供Web/Qt展示联动触发的动作）----
            pkt.action_flags = 0;
            if (fan_auto_on && !fan_manual_mode)
                pkt.action_flags |= ACT_FLAG_FAN_ON;     // 联动触发风扇开
            if (!fan_auto_on && !fan_manual_mode)
                pkt.action_flags |= ACT_FLAG_FAN_OFF;    // 联动触发风扇关
            if (servo_auto_angle >= 90 && !servo_manual_mode)
                pkt.action_flags |= ACT_FLAG_SERVO_OPEN; // 联动触发开窗
            if (servo_auto_angle == 0 && !servo_manual_mode)
                pkt.action_flags |= ACT_FLAG_SERVO_CLOSE;// 联动触发关窗

            // ---- 累计用量（开机以来的总消耗）----
            pkt.total_power_mah = (uint16_t)(battery_used_mAh + 0.5f);     // 已用电mAh
            pkt.total_flow_cl   = (uint16_t)(total_flow_L * 100.0f + 0.5f); // 已用水cL

            // ---- 水电详细值（避免Web/Qt端重复计算）----
            pkt.battery_remain_mah = (uint16_t)((float)battery_pct * (float)BATTERY_CAPACITY_mAh / 100.0f);
            pkt.water_remain_cl    = (uint16_t)(water_pct * TANK_CAPACITY_L);
            pkt.power_status       = cur_level;  // 电力状态：0正常/1预警/2告警
            pkt.battery_capacity_mah = BATTERY_CAPACITY_mAh;
            pkt.tank_capacity_cl     = (uint16_t)(TANK_CAPACITY_L * 100);

            // ---- 预估剩余时间（基于当前瞬时消耗速率）----
            // 电池剩余时间 = 剩余mAh / 当前mA × 60(分钟/小时)
            {
                float remain_mAh = (float)battery_pct / 100.0f * (float)BATTERY_CAPACITY_mAh;
                if (rpt_ma > 0)
                    pkt.battery_remain_min = (uint16_t)((remain_mAh / (float)rpt_ma) * 60.0f + 0.5f);
                else
                    pkt.battery_remain_min = 0;  // 无负载时显示0
            }
            // 水量剩余时间 = 剩余L / 当前L/min
            {
                float remain_L = (float)water_pct / 100.0f * (float)TANK_CAPACITY_L;
                if (rpt_f > 0.01f)
                    pkt.water_remain_min = (uint16_t)(remain_L / rpt_f + 0.5f);
                else
                    pkt.water_remain_min = 0;  // 无水流时显示0
            }

            // 通过USART1发送完整JSON到ESP32
            Serial_SendUploadPacket(&pkt);

            sensor_interval = 0;
        }

        // ============================================================
        // 区块C：LED 三色状态指示（每个Tick更新，手动优先）
        // ============================================================
        // 正常=绿灯 / 预警=黄灯 / 告警=红灯 / 手动模式=三色全亮
        if (led_manual_mode) {
            Apply_RGB_Leds(1, 1, 1);  // 手动模式：三色全亮表示人工接管
        } else {
            if (link_level == LINK_ALARM)       Apply_RGB_Leds(1, 0, 0);  // 告警：红灯
            else if (link_level == LINK_WARN)   Apply_RGB_Leds(0, 0, 1);  // 预警：黄灯
            else                                Apply_RGB_Leds(0, 1, 0);  // 正常：绿灯
        }

        // ============================================================
        // 区块D：OLED 双页显示（300ms刷新，3秒切页）
        // ============================================================
        // 第0页(传感器数据)：温湿度 / PM2.5+AQ / 水流 / 电流
        // 第1页(告警阈值)：温度上下限 / 湿度上下限 / PM2.5+AQ上限 / 电流+水流上限
        if (oled_interval >= OLED_REFRESH_MS) {
            if (oled_page == 0) {
                // ---- 第0页：实时传感器数据 ----
                snprintf(line, sizeof(line), "T:%2dC H:%2d%%   ", temp, humi);
                OLED_ShowString(1, 1, line);
                snprintf(line, sizeof(line), "PM:%3u AQ:%3u ", (unsigned int)pm25_ugm3, (unsigned int)aq_ppm);
                OLED_ShowString(2, 1, line);
                snprintf(line, sizeof(line), "Flow:%4.2fL/m  ", (double)rpt_f);
                OLED_ShowString(3, 1, line);
                {
                    int cur_a = (int)rpt_ma / 1000;        // 电流整数部分(A)
                    int cur_d = ((int)rpt_ma % 1000) / 100; // 电流小数部分(0.1A)
                    snprintf(line, sizeof(line), "I:%2d.%1dA        ", cur_a, cur_d);
                }
                OLED_ShowString(4, 1, line);
            } else {
                // ---- 第1页：告警阈值（工作人员查看/调试用）----
                snprintf(line, sizeof(line), "TH:%2uC TL:%2uC ",
                         (unsigned int)TEMP_ALARM_H, (unsigned int)TEMP_ALARM_L);
                OLED_ShowString(1, 1, line);
                snprintf(line, sizeof(line), "HH:%2u%% HL:%2u%% ",
                         (unsigned int)HUMI_ALARM_H, (unsigned int)HUMI_ALARM_L);
                OLED_ShowString(2, 1, line);
                snprintf(line, sizeof(line), "PH:%3u AH:%3u  ",
                         (unsigned int)PM25_ALARM, (unsigned int)AQ_ALARM);
                OLED_ShowString(3, 1, line);
                snprintf(line, sizeof(line), "IH:%2uA FH:%2uL ",
                         (unsigned int)CUR_ALARM, (unsigned int)FLW_ALARM);
                OLED_ShowString(4, 1, line);
            }
            oled_interval = 0;
        }

        // ---- OLED 页面切换（3秒自动轮播）----
        if (oled_page_interval >= OLED_PAGE_MS) {
            oled_page = (uint8_t)((oled_page + 1) % 2);  // 0↔1切换
            oled_page_interval = 0;
            OLED_Clear();  // 清屏消除上一页残留，避免闪烁（只清4行数据区即可）
        }

        // ============================================================
        // 区块E：手动模式看门狗（仅全局手动模式下计时）
        // 设计意图：工作人员临时手动操作后若忘记恢复，5分钟自动切回AUTO
        // ============================================================
        if (global_manual) {
            manual_wdog_tick++;
            if (manual_wdog_tick >= MANUAL_WDOG_TICKS) {  // 30000 tick = 5分钟
                exec_reset_cmd(&global_manual,
                               &buzzer_remote_mode, &buzzer_remote_on,
                               &fan_manual_mode, &fan_manual_on,
                               &servo_manual_mode, &servo_manual_angle,
                               &led_manual_mode);
                manual_wdog_tick = 0;
            }
        }

        // ============================================================
        // 区块F：主循环10ms Tick基准（所有定时器累加器在此更新）
        // ============================================================
        Delay_ms(10);                        // 10ms延时 = 系统心跳
        sensor_interval += 10;               // 传感器采样累加器(目标1000ms=1秒)
        oled_interval += 10;                 // OLED刷新累加器(目标300ms)
        oled_page_interval += 10;            // OLED切页累加器(目标3000ms=3秒)

        // ============================================================
        // 区块G：蜂鸣器报警节奏生成（手动优先）
        // 预警节奏：每2秒短促滴一声（100ms ON / 1900ms OFF）
        // 告警节奏：每秒滴滴两声（0~10+20~30两段ON / 100tick周期）
        // ============================================================
        if (buzzer_remote_mode) {
            // 手动模式：完全由远程命令控制
            buzzer_tick = 0;
            if (buzzer_remote_on) Buzzer_ON(); else Buzzer_OFF();
        } else {
            if (link_level == LINK_ALARM) {
                // 告警急促节奏：滴滴-滴滴（100tick周期内两段响）
                if (buzzer_tick < BUZZER_ALARM_ON1_END)             // tick 0~10: 响
                    Buzzer_ON();
                else if (buzzer_tick >= BUZZER_ALARM_ON2_START &&
                         buzzer_tick < BUZZER_ALARM_ON2_END)        // tick 20~30: 响
                    Buzzer_ON();
                else
                    Buzzer_OFF();                                    // tick 10~20, 30~100: 不响
                buzzer_tick++;
                if (buzzer_tick >= BUZZER_ALARM_PERIOD) buzzer_tick = 0;
            } else if (link_level == LINK_WARN) {
                // 预警温和节奏：每2秒滴一声
                if (buzzer_tick < BUZZER_BEEP_ON_TICKS)             // tick 0~10: 响
                    Buzzer_ON();
                else
                    Buzzer_OFF();                                    // tick 10~200: 不响
                buzzer_tick++;
                if (buzzer_tick >= BUZZER_BEEP_PERIOD) buzzer_tick = 0;
            } else {
                // 正常：不响
                Buzzer_OFF(); buzzer_tick = 0;
            }
        }
    }  // end while(1) — 主循环结束
}  // end main
