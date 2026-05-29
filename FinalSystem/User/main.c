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

// ========== 配置参数（RAM 变量，可通过远程命令修改）==========
// Qt下发告警值 → STM32自动推算预警值（预警=告警的推导）
// 告警阈值（默认值，远程可改）
static uint16_t TEMP_ALARM_H  = 38;   // ta → tr=32
static uint16_t TEMP_ALARM_L  = 10;   // tb → tc=18
static uint16_t HUMI_ALARM_H  = 85;   // ha → hw=70
static uint16_t HUMI_ALARM_L  = 20;   // hb → hd=30
static uint16_t PM25_ALARM    = 150;  // pa → ph=75
static uint16_t AQ_ALARM      = 200;  // aa → aq=100
static uint16_t CUR_ALARM     = 15;   // ca → ci=10
static uint16_t FLW_ALARM     = 10;   // fa → fl=5
// 预警阈值（由告警自动推导，也可通过旧命令直接覆盖）
static uint16_t TEMP_WARN_H   = 32;
static uint16_t TEMP_WARN_L   = 18;
static uint16_t HUMI_WARN_H   = 70;
static uint16_t HUMI_WARN_L   = 30;
static uint16_t PM25_WARN     = 75;
static uint16_t AQ_WARN       = 100;
static uint16_t CUR_WARN      = 10;
static uint16_t FLW_WARN      = 5;

// 告警→预警自动推导
static void derive_warn_from_alarm(void) {
    TEMP_WARN_H = TEMP_ALARM_H - 6;
    TEMP_WARN_L = TEMP_ALARM_L + 8;
    HUMI_WARN_H = HUMI_ALARM_H - 15;
    HUMI_WARN_L = HUMI_ALARM_L + 10;
    PM25_WARN   = PM25_ALARM / 2;
    AQ_WARN     = AQ_ALARM / 2;
    CUR_WARN    = CUR_ALARM * 2 / 3;
    FLW_WARN    = FLW_ALARM / 2;
}

// 恢复出厂默认告警值+推导预警
static void reset_thresholds_to_default(void) {
    TEMP_ALARM_H = 38; TEMP_ALARM_L = 10;
    HUMI_ALARM_H = 85; HUMI_ALARM_L = 20;
    PM25_ALARM   = 150;
    AQ_ALARM     = 200;
    CUR_ALARM    = 15;
    FLW_ALARM    = 10;
    derive_warn_from_alarm();
}

// 手动模式看门狗：超时(5分钟)后自动切回AUTO
#define MANUAL_WDOG_TICKS   30000  // 30000×10ms=300秒=5分钟

// ========== 滞回恢复偏移量 ==========
#define TEMP_HYST_WARN   2
#define TEMP_HYST_ALARM  3
#define HUMI_HYST_WARN   5
#define HUMI_HYST_ALARM  5
#define PM25_HYST_WARN   10
#define PM25_HYST_ALARM  20
#define AQ_HYST_WARN     15
#define AQ_HYST_ALARM    30
#define CUR_HYST_WARN    1     // A (rpt_ma用，×1000)
#define CUR_HYST_ALARM   2     // A
#define FLW_HYST_WARN    2     // L/min (rpt_f用)
#define FLW_HYST_ALARM   3     // L/min

// 连续触发确认次数
#define WARN_ENTRY_CNT   3
#define ALARM_ENTRY_CNT  5
#define WARN_EXIT_CNT    3
#define ALARM_EXIT_CNT   5

// 水电剩余量模拟
static uint16_t BATTERY_CAPACITY_mAh = 10000;
static uint16_t TANK_CAPACITY_L      = 10;

// 传感器缩放系数（DEMO：放大到真实比例；真实场景改为1）
#define CUR_SCALE  10    // 电流×10: 400mA→4A
#define FLW_SCALE  5     // 水流×5

#define LINK_NORMAL  0
#define LINK_WARN    1
#define LINK_ALARM   2

#define BUZZER_BEEP_ON_TICKS   10
#define BUZZER_BEEP_PERIOD     200
#define BUZZER_ALARM_ON1_END    10
#define BUZZER_ALARM_ON2_START  20
#define BUZZER_ALARM_ON2_END    30
#define BUZZER_ALARM_PERIOD     100

#define SERVO_HOME       0
#define OLED_REFRESH_MS  300
#define OLED_PAGE_MS     3000

// ---- LED ----
static void Apply_RGB_Leds(uint8_t red, uint8_t green, uint8_t yellow)
{
    if (red)   LED_RED_ON();    else LED_RED_OFF();
    if (green) LED_GREEN_ON();  else LED_GREEN_OFF();
    if (yellow)LED_YELLOW_ON(); else LED_YELLOW_OFF();
}

// ---- 告警码生成 ----
static uint8_t build_alarm_codes(uint16_t *codes, uint8_t *levels, uint8_t max_n,
                                 uint8_t temp_lv, uint8_t humi_lv, uint8_t pm25_lv,
                                 uint8_t aq_lv, uint8_t cur_lv, uint8_t flw_lv,
                                 uint8_t link_level, uint8_t has_combo_temp_pm25,
                                 uint8_t has_combo_pm25_aq, uint8_t has_combo_flw_cur)
{
    uint8_t n = 0;
    if (n >= max_n) return n;

    // 组合告警优先
    if (has_combo_temp_pm25) { codes[n] = ALM_CMB_TEMP_PM25; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n; }
    if (has_combo_pm25_aq)   { codes[n] = ALM_CMB_PM25_AQ;   levels[n] = LINK_ALARM; n++; if (n >= max_n) return n; }
    if (has_combo_flw_cur)   { codes[n] = ALM_CMB_FLW_CUR;   levels[n] = LINK_WARN;  n++; if (n >= max_n) return n; }

    // 温度
    if (temp_lv == LINK_ALARM) {
        // 实际高温还是低温由阈值判断，简化为高温告警
        codes[n] = ALM_TEMP_HI_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (temp_lv == LINK_WARN) {
        codes[n] = ALM_TEMP_HI_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 湿度
    if (humi_lv == LINK_ALARM) {
        codes[n] = ALM_HUMI_HI_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (humi_lv == LINK_WARN) {
        codes[n] = ALM_HUMI_HI_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // PM2.5
    if (pm25_lv == LINK_ALARM) {
        codes[n] = ALM_PM25_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (pm25_lv == LINK_WARN) {
        codes[n] = ALM_PM25_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // AQ
    if (aq_lv == LINK_ALARM) {
        codes[n] = ALM_AQ_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (aq_lv == LINK_WARN) {
        codes[n] = ALM_AQ_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 电流
    if (cur_lv == LINK_ALARM) {
        codes[n] = ALM_CUR_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (cur_lv == LINK_WARN) {
        codes[n] = ALM_CUR_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    // 水流
    if (flw_lv == LINK_ALARM) {
        codes[n] = ALM_FLW_ALARM; levels[n] = LINK_ALARM; n++; if (n >= max_n) return n;
    } else if (flw_lv == LINK_WARN) {
        codes[n] = ALM_FLW_WARN; levels[n] = LINK_WARN; n++; if (n >= max_n) return n;
    }
    return n;
}

// ---- 阈值命中 = 预警或告警级别（用于组合联动判断） ----
static uint8_t is_warn_or_alarm(uint8_t lv) { return lv >= LINK_WARN ? 1 : 0; }

static void send_threshold_ack(const char *key, uint16_t val)
{
    char ack[96];
    sprintf(ack, "{\"ack\":\"th\",\"key\":\"%s\",\"val\":%u,\"ta\":%u,\"ha\":%u}\r\n",
            key, (unsigned int)val,
            (unsigned int)TEMP_ALARM_H, (unsigned int)HUMI_ALARM_H);
    Serial_SendString(ack);
}

static void send_reset_threshold_ack(void)
{
    char ack[80];
    sprintf(ack, "{\"ack\":\"reset_th\",\"ta\":%u,\"ha\":%u}\r\n",
            (unsigned int)TEMP_ALARM_H, (unsigned int)HUMI_ALARM_H);
    Serial_SendString(ack);
}

// ---- 命令执行 ----
static void exec_control_cmd(const JsonCommand *cmd,
                             uint8_t *bz_rm, uint8_t *bz_ro,
                             uint8_t *fan_mm, uint8_t *fan_mo,
                             uint8_t *svo_mm, uint16_t *svo_ma,
                             uint8_t *led_mm)
{
    if (cmd->type != JSONCMD_CTRL) return;

    if (strcmp(cmd->actuator, "buzzer") == 0) {
        if (strcmp(cmd->action, "on") == 0)      { *bz_rm = 1; *bz_ro = 1; }
        else if (strcmp(cmd->action, "off") == 0) { *bz_rm = 1; *bz_ro = 0; }
        else if (strcmp(cmd->action, "auto") == 0){ *bz_rm = 0; *bz_ro = 0; }
    }
    else if (strcmp(cmd->actuator, "fan") == 0) {
        if (strcmp(cmd->action, "on") == 0)      { *fan_mm = 1; *fan_mo = 1; }
        else if (strcmp(cmd->action, "off") == 0) { *fan_mm = 1; *fan_mo = 0; }
        else if (strcmp(cmd->action, "auto") == 0){ *fan_mm = 0; *fan_mo = 0; }
    }
    else if (strcmp(cmd->actuator, "servo") == 0) {
        if (cmd->action[0] == '#')                { *svo_mm = 1; *svo_ma = cmd->angle; }
        else if (strcmp(cmd->action, "auto") == 0){ *svo_mm = 0; *svo_ma = 0; }
    }
    else if (strcmp(cmd->actuator, "led") == 0) {
        if (strcmp(cmd->action, "on") == 0)       { *led_mm = 1; }
        else if (strcmp(cmd->action, "auto") == 0){ *led_mm = 0; }
    }
}

// ---- 全局模式切换 ----
static void exec_mode_cmd(const JsonCommand *cmd,
                          uint8_t *global_manual,
                          uint8_t *bz_rm, uint8_t *fan_mm, uint8_t *svo_mm, uint8_t *led_mm)
{
    if (cmd->type != JSONCMD_MODE) return;
    if (strcmp(cmd->mode, "MANUAL") == 0) {
        *global_manual = 1;
        *bz_rm = 1; *fan_mm = 1; *svo_mm = 1; *led_mm = 1;
    } else if (strcmp(cmd->mode, "AUTO") == 0) {
        *global_manual = 0;
        *bz_rm = 0; *fan_mm = 0; *svo_mm = 0; *led_mm = 0;
    }
}

// ---- 全局复位 ----
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

int main(void)
{
    uint8_t temp = 0, humi = 0, dht_ok = 0;
    uint32_t sensor_interval = 0, oled_interval = 0;
    uint32_t pm25_ugm3 = 0, aq_ppm = 0;
    float flow_lpm = 0.0f;
    int32_t current_ma = 0;
    int32_t rpt_ma = 0;
    float   rpt_f  = 0.0f;
    char line[17];
    char esp_line[SERIAL_RX_LINE_MAX];
    uint32_t oled_page_interval = 0;
    uint8_t oled_page = 0;

    // 模式 / 手动状态
    uint8_t global_manual = 0;
    uint8_t buzzer_remote_mode = 0, buzzer_remote_on = 0;
    uint8_t fan_manual_mode = 0, fan_manual_on = 0;
    uint8_t servo_manual_mode = 0; uint16_t servo_manual_angle = 0;
    uint8_t led_manual_mode = 0;

    // 水电
    float battery_used_mAh = 0.0f;
    uint8_t battery_pct = 100, water_pct = 100;

    // 联动
    uint8_t link_level = LINK_NORMAL, env_level = LINK_NORMAL;
    uint8_t temp_level = LINK_NORMAL, humi_level = LINK_NORMAL;
    uint8_t pm25_level = LINK_NORMAL, aq_level = LINK_NORMAL;
    uint8_t cur_level = LINK_NORMAL, flw_level = LINK_NORMAL;

    uint8_t fan_auto_on = 0, servo_auto_angle = 0;
    uint16_t buzzer_tick = 0;

    // 滞回计数器
    uint8_t temp_warn_cnt = 0, temp_alarm_cnt = 0, temp_warn_rec = 0, temp_alarm_rec = 0;
    uint8_t humi_warn_cnt = 0, humi_alarm_cnt = 0, humi_warn_rec = 0, humi_alarm_rec = 0;
    uint8_t pm25_warn_cnt = 0, pm25_alarm_cnt = 0, pm25_warn_rec = 0, pm25_alarm_rec = 0;
    uint8_t aq_warn_cnt   = 0, aq_alarm_cnt   = 0, aq_warn_rec   = 0, aq_alarm_rec   = 0;
    uint8_t cur_warn_cnt  = 0, cur_alarm_cnt  = 0, cur_warn_rec  = 0, cur_alarm_rec  = 0;
    uint8_t flw_warn_cnt  = 0, flw_alarm_cnt  = 0, flw_warn_rec  = 0, flw_alarm_rec  = 0;

    // 手动模式看门狗
    uint32_t manual_wdog_tick = 0;

    // 组合联动标志
    uint8_t combo_temp_pm25 = 0, combo_pm25_aq = 0, combo_flw_cur = 0;

    // 上传包
    UploadPacket pkt;
    JsonCommand jcmd;

    // 初始化
    LED_Init();
    Buzzer_Init();
    DHT11_Init();
    MQ135_Init();
    OLED_Init();
    GP2Y_Init();
    Servo_Init();
    Motor_Init();
    Serial_Init();
    ACS712_Init();
    OLED_Clear();
    Delay_ms(500);

    Apply_RGB_Leds(0, 1, 0);
    Buzzer_OFF();
    Servo_SetAngle(SERVO_HOME);
    Flow_Sensor_Init();
    Fan_Init();

    while (1)
    {
        // ---- 下行命令处理 ----
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
                case JSONCMD_TH: {
                    // 阈值修改
                    char *k = jcmd.th_key;
                    uint16_t v = jcmd.th_val;
                    // 告警key：设告警+自动推导预警（ta/tb/ha/hb/pa/aa/ca/fa）
                    if      (strcmp(k, "ta") == 0) { TEMP_ALARM_H = v; TEMP_WARN_H = v - 6; }
                    else if (strcmp(k, "tb") == 0) { TEMP_ALARM_L = v; TEMP_WARN_L = v + 8; }
                    else if (strcmp(k, "ha") == 0) { HUMI_ALARM_H = v; HUMI_WARN_H = v - 15; }
                    else if (strcmp(k, "hb") == 0) { HUMI_ALARM_L = v; HUMI_WARN_L = v + 10; }
                    else if (strcmp(k, "pa") == 0) { PM25_ALARM   = v; PM25_WARN   = v / 2; }
                    else if (strcmp(k, "aa") == 0) { AQ_ALARM     = v; AQ_WARN     = v / 2; }
                    else if (strcmp(k, "ca") == 0) { CUR_ALARM    = v; CUR_WARN    = v * 2 / 3; }
                    else if (strcmp(k, "fa") == 0) { FLW_ALARM    = v; FLW_WARN    = v / 2; }
                    // 旧预警key（兼容，只设预警不推导告警）
                    else if (strcmp(k, "tr") == 0) TEMP_WARN_H = v;
                    else if (strcmp(k, "tc") == 0) TEMP_WARN_L = v;
                    else if (strcmp(k, "hw") == 0) HUMI_WARN_H = v;
                    else if (strcmp(k, "hd") == 0) HUMI_WARN_L = v;
                    else if (strcmp(k, "ph") == 0) PM25_WARN = v;
                    else if (strcmp(k, "aq") == 0) AQ_WARN = v;
                    else if (strcmp(k, "ci") == 0) CUR_WARN = v;
                    else if (strcmp(k, "fl") == 0) FLW_WARN = v;
                    else if (strcmp(k, "bc") == 0) { BATTERY_CAPACITY_mAh = v; battery_used_mAh = 0; }
                    else if (strcmp(k, "bp") == 0) { if (v > 100) v = 100; battery_used_mAh = BATTERY_CAPACITY_mAh * (100.0f - v) / 100.0f; }
                    else if (strcmp(k, "wc") == 0) { TANK_CAPACITY_L = v; Flow_Sensor_Reset_Total(); }
                    else if (strcmp(k, "wp") == 0) { if (v > 100) v = 100; total_flow_L = TANK_CAPACITY_L * (100.0f - v) / 100.0f; }
                    send_threshold_ack(k, v);
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

        // ---- 传感器采集（1s 周期）----
        if (sensor_interval >= 1000)
        {
            dht_ok = DHT11_Read_Data(&temp, &humi);
            Read_GP2Y10();
            pm25_ugm3 = (uint32_t)(dust_density + 0.5f);
            aq_ppm = (uint32_t)(MQ135_Get_PPM() + 0.5f);
            flow_lpm = Flow_Sensor_Get_FlowRate();

            float cur_f = ACS712_GetCurrent();
            current_ma = (int32_t)(cur_f * 1000.0f);
            if (current_ma >= 5000) current_ma = 0;

            // 缩放用于上报和累加（阈值判断仍用原始值）
            rpt_ma = current_ma * CUR_SCALE;
            rpt_f  = flow_lpm   * (float)FLW_SCALE;

            // 水电剩余积分（用缩放值）
            battery_used_mAh += (float)rpt_ma / 3600.0f;
            if (battery_used_mAh >= BATTERY_CAPACITY_mAh)
                battery_used_mAh = (float)BATTERY_CAPACITY_mAh;
            battery_pct = (uint8_t)((1.0f - battery_used_mAh / BATTERY_CAPACITY_mAh) * 100.0f);
            water_pct = (uint8_t)((1.0f - total_flow_L / TANK_CAPACITY_L) * 100.0f);
            if (water_pct > 100) water_pct = 100;

            // ===== 三级联动评估（滞回 + 连续触发确认） =====
            if (dht_ok)
            {
                // 温度
                if (temp_level == LINK_ALARM) {
                    temp_warn_cnt = 0; temp_alarm_cnt = 0; temp_warn_rec = 0;
                    if (temp <= (TEMP_ALARM_H - TEMP_HYST_ALARM) && temp >= (TEMP_ALARM_L + TEMP_HYST_ALARM)) {
                        temp_alarm_rec++;
                        if (temp_alarm_rec >= ALARM_EXIT_CNT) temp_level = LINK_WARN;
                    } else temp_alarm_rec = 0;
                } else if (temp_level == LINK_WARN) {
                    temp_warn_cnt = 0; temp_alarm_rec = 0;
                    if (temp >= TEMP_ALARM_H || temp <= TEMP_ALARM_L) {
                        temp_alarm_cnt++; temp_warn_rec = 0;
                        if (temp_alarm_cnt >= ALARM_ENTRY_CNT) temp_level = LINK_ALARM;
                    } else {
                        temp_alarm_cnt = 0;
                        if (temp <= (TEMP_WARN_H - TEMP_HYST_WARN) && temp >= (TEMP_WARN_L + TEMP_HYST_WARN)) {
                            temp_warn_rec++;
                            if (temp_warn_rec >= WARN_EXIT_CNT) temp_level = LINK_NORMAL;
                        } else temp_warn_rec = 0;
                    }
                } else {
                    temp_alarm_rec = 0; temp_warn_rec = 0;
                    if (temp >= TEMP_ALARM_H || temp <= TEMP_ALARM_L) {
                        temp_alarm_cnt++; temp_warn_cnt = 0;
                        if (temp_alarm_cnt >= ALARM_ENTRY_CNT) temp_level = LINK_ALARM;
                    } else {
                        temp_alarm_cnt = 0;
                        if (temp >= TEMP_WARN_H || temp <= TEMP_WARN_L) {
                            temp_warn_cnt++;
                            if (temp_warn_cnt >= WARN_ENTRY_CNT) temp_level = LINK_WARN;
                        } else temp_warn_cnt = 0;
                    }
                }

                // 湿度
                if (humi_level == LINK_ALARM) {
                    humi_warn_cnt = 0; humi_alarm_cnt = 0; humi_warn_rec = 0;
                    if (humi <= (HUMI_ALARM_H - HUMI_HYST_ALARM) && humi >= (HUMI_ALARM_L + HUMI_HYST_ALARM)) {
                        humi_alarm_rec++;
                        if (humi_alarm_rec >= ALARM_EXIT_CNT) humi_level = LINK_WARN;
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
                        humi_alarm_cnt++; humi_warn_cnt = 0;
                        if (humi_alarm_cnt >= ALARM_ENTRY_CNT) humi_level = LINK_ALARM;
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
                    if (pm25_alarm_rec >= ALARM_EXIT_CNT) pm25_level = LINK_WARN;
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
                    pm25_alarm_cnt++; pm25_warn_cnt = 0;
                    if (pm25_alarm_cnt >= ALARM_ENTRY_CNT) pm25_level = LINK_ALARM;
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
                    if (aq_alarm_rec >= ALARM_EXIT_CNT) aq_level = LINK_WARN;
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
                    aq_alarm_cnt++; aq_warn_cnt = 0;
                    if (aq_alarm_cnt >= ALARM_ENTRY_CNT) aq_level = LINK_ALARM;
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
                    if (cur_alarm_rec >= ALARM_EXIT_CNT) cur_level = LINK_WARN;
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
                    cur_alarm_cnt++; cur_warn_cnt = 0;
                    if (cur_alarm_cnt >= ALARM_ENTRY_CNT) cur_level = LINK_ALARM;
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
                    if (flw_alarm_rec >= ALARM_EXIT_CNT) flw_level = LINK_WARN;
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
                    flw_alarm_cnt++; flw_warn_cnt = 0;
                    if (flw_alarm_cnt >= ALARM_ENTRY_CNT) flw_level = LINK_ALARM;
                } else {
                    flw_alarm_cnt = 0;
                    if (rpt_f >= FLW_WARN) {
                        flw_warn_cnt++;
                        if (flw_warn_cnt >= WARN_ENTRY_CNT) flw_level = LINK_WARN;
                    } else flw_warn_cnt = 0;
                }
            }

            // 全传感器最差级别 → link_level
            link_level = LINK_NORMAL;
            if (temp_level == LINK_ALARM || humi_level == LINK_ALARM ||
                pm25_level == LINK_ALARM || aq_level  == LINK_ALARM ||
                cur_level  == LINK_ALARM || flw_level == LINK_ALARM)
                link_level = LINK_ALARM;
            else if (temp_level == LINK_WARN || humi_level == LINK_WARN ||
                     pm25_level == LINK_WARN || aq_level  == LINK_WARN ||
                     cur_level  == LINK_WARN || flw_level == LINK_WARN)
                link_level = LINK_WARN;

            // 环境级别（温湿PM2.5/AQ） → env_level
            env_level = LINK_NORMAL;
            if (temp_level == LINK_ALARM || humi_level == LINK_ALARM ||
                pm25_level == LINK_ALARM || aq_level  == LINK_ALARM)
                env_level = LINK_ALARM;
            else if (temp_level == LINK_WARN || humi_level == LINK_WARN ||
                     pm25_level == LINK_WARN || aq_level  == LINK_WARN)
                env_level = LINK_WARN;

            // ===== 组合联动规则 =====
            combo_temp_pm25 = 0; combo_pm25_aq = 0; combo_flw_cur = 0;
            if (is_warn_or_alarm(temp_level) && is_warn_or_alarm(pm25_level)) {
                if (env_level < LINK_ALARM)  env_level  = LINK_ALARM;
                if (link_level < LINK_ALARM) link_level = LINK_ALARM;
                combo_temp_pm25 = 1;
            }
            if (is_warn_or_alarm(pm25_level) && is_warn_or_alarm(aq_level)) {
                if (env_level < LINK_ALARM)  env_level  = LINK_ALARM;
                if (link_level < LINK_ALARM) link_level = LINK_ALARM;
                combo_pm25_aq = 1;
            }

            // 自动联动目标
            if (env_level == LINK_ALARM)      { fan_auto_on = 1; servo_auto_angle = 90; }
            else if (env_level == LINK_WARN)  { fan_auto_on = 1; servo_auto_angle = 0; }
            else                              { fan_auto_on = 0; servo_auto_angle = 0; }

            if (is_warn_or_alarm(flw_level) && is_warn_or_alarm(cur_level)) {
                fan_auto_on = 0;
                combo_flw_cur = 1;
            }

            // 执行器：手动优先
            Fan_Set(fan_manual_mode ? fan_manual_on : fan_auto_on);
            Servo_SetAngle(servo_manual_mode ? servo_manual_angle : servo_auto_angle);

            // ---- 填充上传数据包 ----
            memset(&pkt, 0, sizeof(pkt));
            pkt.temp = temp; pkt.humi = humi; pkt.dht_ok = dht_ok;
            pkt.pm25 = pm25_ugm3; pkt.aq = aq_ppm;
            pkt.flow = rpt_f; pkt.current_ma = rpt_ma;
            pkt.battery_pct = battery_pct; pkt.water_pct = water_pct;
            pkt.link_level = link_level; pkt.env_level = env_level;
            pkt.temp_lv = temp_level; pkt.humi_lv = humi_level;
            pkt.pm25_lv = pm25_level; pkt.aq_lv = aq_level;
            pkt.cur_lv = cur_level; pkt.flw_lv = flw_level;
            pkt.global_manual = global_manual;
            pkt.buzzer_manual = buzzer_remote_mode;
            pkt.fan_manual = fan_manual_mode;
            pkt.servo_manual = servo_manual_mode;
            pkt.led_manual = led_manual_mode;
            pkt.th_ta = TEMP_ALARM_H; pkt.th_tb = TEMP_ALARM_L;
            pkt.th_ha = HUMI_ALARM_H; pkt.th_hb = HUMI_ALARM_L;
            pkt.th_pa = PM25_ALARM; pkt.th_aa = AQ_ALARM;
            pkt.th_ca = CUR_ALARM; pkt.th_fa = FLW_ALARM;

            // 执行器状态
            {
                uint8_t fan_is_on = fan_manual_mode ? fan_manual_on : fan_auto_on;
                uint8_t svo_ang = servo_manual_mode ? servo_manual_angle : servo_auto_angle;
                uint8_t led_r = 0, led_g = 0, led_y = 0;
                if (led_manual_mode) { led_r = 1; led_g = 1; led_y = 1; }
                else {
                    if (link_level == LINK_ALARM)      led_r = 1;
                    else if (link_level == LINK_WARN)  led_y = 1;
                    else                               led_g = 1;
                }
                pkt.buzzer_on = buzzer_remote_mode ? buzzer_remote_on : (link_level >= LINK_WARN ? 1 : 0);
                pkt.fan_on = fan_is_on;
                pkt.servo_angle = svo_ang;
                pkt.led_r = led_r; pkt.led_g = led_g; pkt.led_y = led_y;
            }

            // 告警码
            pkt.alm_count = build_alarm_codes(pkt.alm_codes, pkt.alm_levels, 8,
                temp_level, humi_level, pm25_level, aq_level, cur_level, flw_level,
                link_level, combo_temp_pm25, combo_pm25_aq, combo_flw_cur);

            // 动作标志
            pkt.action_flags = 0;
            // 风扇动作由联动产生
            if (fan_auto_on && !fan_manual_mode)
                pkt.action_flags |= ACT_FLAG_FAN_ON;
            if (!fan_auto_on && !fan_manual_mode)
                pkt.action_flags |= ACT_FLAG_FAN_OFF;
            if (servo_auto_angle >= 90 && !servo_manual_mode)
                pkt.action_flags |= ACT_FLAG_SERVO_OPEN;
            if (servo_auto_angle == 0 && !servo_manual_mode)
                pkt.action_flags |= ACT_FLAG_SERVO_CLOSE;

            // 累计用量
            pkt.total_power_mah = (uint16_t)(battery_used_mAh + 0.5f);
            pkt.total_flow_cl   = (uint16_t)(total_flow_L * 100.0f + 0.5f);

            // 水电详细值
            pkt.battery_remain_mah = (uint16_t)((float)battery_pct * (float)BATTERY_CAPACITY_mAh / 100.0f);
            pkt.water_remain_cl    = (uint16_t)(water_pct * TANK_CAPACITY_L);
            pkt.power_status       = cur_level;
            pkt.battery_capacity_mah = BATTERY_CAPACITY_mAh;
            pkt.tank_capacity_cl     = (uint16_t)(TANK_CAPACITY_L * 100);

            // 预估剩余时间（分钟）
            {
                float remain_mAh = (float)battery_pct / 100.0f * (float)BATTERY_CAPACITY_mAh;
                if (rpt_ma > 0)
                    pkt.battery_remain_min = (uint16_t)((remain_mAh / (float)rpt_ma) * 60.0f + 0.5f);
                else
                    pkt.battery_remain_min = 0;
            }
            {
                float remain_L = (float)water_pct / 100.0f * (float)TANK_CAPACITY_L;
                if (rpt_f > 0.01f)
                    pkt.water_remain_min = (uint16_t)((remain_L / rpt_f) * 60.0f + 0.5f);
                else
                    pkt.water_remain_min = 0;
            }

            // 故障检测
            pkt.flt_count = 0;
            if (!dht_ok)  { pkt.flt_codes[pkt.flt_count++] = FLT_DHT11; }

            // 发送
            Serial_SendUploadPacket(&pkt);

            sensor_interval = 0;
        }

        // ---- LED：手动优先 ----
        if (led_manual_mode) {
            Apply_RGB_Leds(1, 1, 1);
        } else {
            if (link_level == LINK_ALARM)       Apply_RGB_Leds(1, 0, 0);
            else if (link_level == LINK_WARN)   Apply_RGB_Leds(0, 0, 1);
            else                                Apply_RGB_Leds(0, 1, 0);
        }

        // ---- OLED 显示 ----
        if (oled_interval >= OLED_REFRESH_MS) {
            if (oled_page == 0) {
                snprintf(line, sizeof(line), "T:%2dC H:%2d%%   ", temp, humi);
                OLED_ShowString(1, 1, line);
                snprintf(line, sizeof(line), "PM:%3u AQ:%3u ", (unsigned int)pm25_ugm3, (unsigned int)aq_ppm);
                OLED_ShowString(2, 1, line);
                snprintf(line, sizeof(line), "Flow:%4.2fL/m  ", flow_lpm);
                OLED_ShowString(3, 1, line);
                {
                    int cur_a = (int)rpt_ma / 1000;
                    int cur_d = ((int)rpt_ma % 1000) / 100;
                    snprintf(line, sizeof(line), "I:%2d.%1dA        ", cur_a, cur_d);
                }
                OLED_ShowString(4, 1, line);
            } else {
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

        if (oled_page_interval >= OLED_PAGE_MS) {
            oled_page = (uint8_t)((oled_page + 1) % 2);
            oled_page_interval = 0;
            OLED_Clear();
        }

        // 手动模式看门狗：超时5分钟自动切回AUTO
        if (global_manual) {
            manual_wdog_tick++;
            if (manual_wdog_tick >= MANUAL_WDOG_TICKS) {
                exec_reset_cmd(&global_manual,
                               &buzzer_remote_mode, &buzzer_remote_on,
                               &fan_manual_mode, &fan_manual_on,
                               &servo_manual_mode, &servo_manual_angle,
                               &led_manual_mode);
                manual_wdog_tick = 0;
            }
        }

        Delay_ms(10);
        sensor_interval += 10;
        oled_interval += 10;
        oled_page_interval += 10;

        // ---- 蜂鸣器：手动优先 ----
        if (buzzer_remote_mode) {
            buzzer_tick = 0;
            if (buzzer_remote_on) Buzzer_ON(); else Buzzer_OFF();
        } else {
            if (link_level == LINK_ALARM) {
                if (buzzer_tick < BUZZER_ALARM_ON1_END) Buzzer_ON();
                else if (buzzer_tick >= BUZZER_ALARM_ON2_START && buzzer_tick < BUZZER_ALARM_ON2_END) Buzzer_ON();
                else Buzzer_OFF();
                buzzer_tick++;
                if (buzzer_tick >= BUZZER_ALARM_PERIOD) buzzer_tick = 0;
            } else if (link_level == LINK_WARN) {
                if (buzzer_tick < BUZZER_BEEP_ON_TICKS) Buzzer_ON();
                else Buzzer_OFF();
                buzzer_tick++;
                if (buzzer_tick >= BUZZER_BEEP_PERIOD) buzzer_tick = 0;
            } else {
                Buzzer_OFF(); buzzer_tick = 0;
            }
        }
    }
}
