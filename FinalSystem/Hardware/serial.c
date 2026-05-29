#include "serial.h"
#include <stdio.h>
#include <string.h>
#include "stm32f10x.h"
#include "misc.h"
#include "delay.h"

/* RX ring buffer */
#define SERIAL_RX_RING_SIZE  256
static volatile uint8_t  g_rx_ring[SERIAL_RX_RING_SIZE];
static volatile uint16_t g_rx_head;
static volatile uint16_t g_rx_tail;

static void rx_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((g_rx_head + 1u) % SERIAL_RX_RING_SIZE);
    if (next == g_rx_tail) return;
    g_rx_ring[g_rx_head] = b;
    g_rx_head = next;
}

static uint8_t rx_ring_pop(uint8_t *out)
{
    if (g_rx_head == g_rx_tail) return 0;
    *out = g_rx_ring[g_rx_tail];
    g_rx_tail = (uint16_t)((g_rx_tail + 1u) % SERIAL_RX_RING_SIZE);
    return 1;
}

/* ---- USART1 ISR ---- */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(SERIAL_USARTx, USART_IT_RXNE) != RESET) {
        uint8_t b = (uint8_t)(USART_ReceiveData(SERIAL_USARTx) & 0xFFu);
        rx_ring_push(b);
    }
    if (USART_GetFlagStatus(SERIAL_USARTx, USART_FLAG_ORE) != RESET) {
        volatile uint32_t sr = SERIAL_USARTx->SR;
        volatile uint32_t dr = SERIAL_USARTx->DR;
        (void)sr; (void)dr;
    }
}

void Serial_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    USART_InitTypeDef USART_InitStruct = {0};
    NVIC_InitTypeDef nv = {0};

    g_rx_head = 0;
    g_rx_tail = 0;

    RCC_APB2PeriphClockCmd(SERIAL_APB2Periph, ENABLE);
    RCC_APB2PeriphClockCmd(SERIAL_GPIO_RCC, ENABLE);

    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Pin   = SERIAL_TX_PIN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SERIAL_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Pin   = SERIAL_RX_PIN;
    GPIO_Init(SERIAL_GPIO_PORT, &GPIO_InitStruct);

    USART_InitStruct.USART_BaudRate   = 115200;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits   = USART_StopBits_1;
    USART_InitStruct.USART_Parity     = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode       = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(SERIAL_USARTx, &USART_InitStruct);
    USART_ITConfig(SERIAL_USARTx, USART_IT_RXNE, ENABLE);

    nv.NVIC_IRQChannel                   = USART1_IRQn;
    nv.NVIC_IRQChannelPreemptionPriority = 1;
    nv.NVIC_IRQChannelSubPriority        = 1;
    nv.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nv);

    USART_Cmd(SERIAL_USARTx, ENABLE);
}

void Serial_SendChar(uint8_t ch)
{
    while (USART_GetFlagStatus(SERIAL_USARTx, USART_FLAG_TXE) == RESET);
    USART_SendData(SERIAL_USARTx, ch);
}

void Serial_SendString(const char *str)
{
    while (*str != '\0') {
        Serial_SendChar((uint8_t)*str);
        str++;
    }
}

/* ---- Upload JSON builder (no Chinese, ARMCC v5 safe) ---- */
void Serial_SendUploadPacket(const UploadPacket *pkt)
{
    static char buf[768];
    int pos = 0;
    int n, i;

    /* header: sen + res (avoid %.2f — newlib-nano lacks float format) */
    {
        int flow_int = (int)pkt->flow;
        int flow_dec = (int)((pkt->flow - (float)flow_int) * 100.0f + 0.5f);
        if (flow_dec < 0) flow_dec = 0;
        if (flow_dec > 99) flow_dec = 99;
        int cur_a = (int)(pkt->current_ma / 1000);
        int cur_d = (int)((pkt->current_ma % 1000) / 100);
        n = sprintf(buf,
            "{\"sen\":{\"t\":%d,\"h\":%d,\"pm\":%lu,\"aq\":%lu,\"f\":%d.%02d,\"i\":%d.%01d},"
            "\"res\":{\"bp\":%d,\"wp\":%d,\"tu\":%d,\"wu\":%d,\"br\":%d,\"wr\":%d,\"ps\":%d,\"bc\":%d,\"tc\":%d,\"bt\":%d,\"wt\":%d},",
            (int)pkt->temp, (int)pkt->humi,
            (unsigned long)pkt->pm25, (unsigned long)pkt->aq,
            flow_int, flow_dec, cur_a, cur_d,
            (int)pkt->battery_pct, (int)pkt->water_pct,
            (int)pkt->total_power_mah, (int)pkt->total_flow_cl,
            (int)pkt->battery_remain_mah, (int)pkt->water_remain_cl, (int)pkt->power_status,
            (int)pkt->battery_capacity_mah, (int)pkt->tank_capacity_cl,
            (int)pkt->battery_remain_min, (int)pkt->water_remain_min);
    }
    if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;

    /* levels */
    n = sprintf(buf + pos,
        "\"lv\":{\"link\":%d,\"env\":%d,"
        "\"d\":[%d,%d,%d,%d,%d,%d]},",
        (int)pkt->link_level, (int)pkt->env_level,
        (int)pkt->temp_lv, (int)pkt->humi_lv, (int)pkt->pm25_lv,
        (int)pkt->aq_lv, (int)pkt->cur_lv, (int)pkt->flw_lv);
    if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;

    /* actuators */
    n = sprintf(buf + pos,
        "\"act\":{\"bz\":%d,\"fan\":%d,\"svo\":%d,\"led\":[%d,%d,%d]},",
        (int)pkt->buzzer_on, (int)pkt->fan_on, (int)pkt->servo_angle,
        (int)pkt->led_r, (int)pkt->led_g, (int)pkt->led_y);
    if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;

    /* modes */
    n = sprintf(buf + pos,
        "\"mod\":{\"gl\":%d,\"bz\":%d,\"fan\":%d,\"svo\":%d,\"led\":%d},",
        (int)pkt->global_manual,
        (int)pkt->buzzer_manual, (int)pkt->fan_manual,
        (int)pkt->servo_manual, (int)pkt->led_manual);
    if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;

    /* active alarm thresholds */
    n = sprintf(buf + pos,
        "\"th\":{\"ta\":%d,\"tb\":%d,\"ha\":%d,\"hb\":%d,"
        "\"pa\":%d,\"aa\":%d,\"ca\":%d,\"fa\":%d},",
        (int)pkt->th_ta, (int)pkt->th_tb, (int)pkt->th_ha, (int)pkt->th_hb,
        (int)pkt->th_pa, (int)pkt->th_aa, (int)pkt->th_ca, (int)pkt->th_fa);
    if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;

    /* alarms: array of codes */
    pos += sprintf(buf + pos, "\"alm\":[");
    for (i = 0; i < (int)pkt->alm_count && i < 8; i++) {
        n = sprintf(buf + pos, "%s%d", (i > 0) ? "," : "", (int)pkt->alm_codes[i]);
        if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;
    }
    pos += sprintf(buf + pos, "],");

    /* action flags: bitmask int */
    n = sprintf(buf + pos, "\"actn\":%d,", (int)pkt->action_flags);
    if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;

    /* faults: array of codes */
    pos += sprintf(buf + pos, "\"flt\":[");
    for (i = 0; i < (int)pkt->flt_count && i < 8; i++) {
        n = sprintf(buf + pos, "%s%d", (i > 0) ? "," : "", (int)pkt->flt_codes[i]);
        if (n > 0 && n < (int)(sizeof(buf) - pos)) pos += n; else goto send_err;
    }
    pos += sprintf(buf + pos, "]}");

    buf[sizeof(buf) - 1] = '\0';
    Serial_SendString(buf);
    Serial_SendString("\r\n");
    return;

send_err:
    Serial_SendString("{\"sen\":{\"t\":0},\"lv\":{\"link\":0},\"err\":1}\r\n");
}

/* ---- Non-blocking line read ---- */
uint8_t Serial_ReadLine(char *out_line, uint16_t max_len)
{
    static char rx_line[SERIAL_RX_LINE_MAX];
    static uint16_t rx_idx = 0;
    uint8_t rb;

    if (out_line == 0 || max_len == 0) return 0;

    while (rx_ring_pop(&rb)) {
        char ch = (char)rb;
        if (ch == '\r') continue;

        if (ch == '\n') {
            uint16_t copy_len = rx_idx;
            if (copy_len >= (max_len - 1)) copy_len = max_len - 1;
            memcpy(out_line, rx_line, copy_len);
            out_line[copy_len] = '\0';
            rx_idx = 0;
            return 1;
        }

        if (rx_idx < (uint16_t)(sizeof(rx_line) - 1)) {
            rx_line[rx_idx++] = ch;
        } else {
            rx_idx = 0;
        }
    }
    return 0;
}

/* ---- JSON value extractors ---- */
static uint8_t json_extract_str(const char *line, const char *key, char *out_val, uint16_t max_val)
{
    const char *p;
    char search[32];
    uint16_t i;
    i = (uint16_t)strlen(key);
    if (i + 4 > sizeof(search)) return 0;
    search[0] = '\"';
    memcpy(search + 1, key, i);
    search[1 + i] = '\"';
    search[2 + i] = ':';
    search[3 + i] = '\0';

    p = strstr(line, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\"') {
        p++;
        i = 0;
        while (*p != '\"' && *p != '\0' && i < (max_val - 1)) {
            out_val[i++] = *p++;
        }
        out_val[i] = '\0';
        return 1;
    }
    return 0;
}

static uint8_t json_extract_int(const char *line, const char *key, int *out_val)
{
    const char *p;
    char search[32];
    uint16_t i;
    int sign = 1;

    i = (uint16_t)strlen(key);
    if (i + 4 > sizeof(search)) return 0;
    search[0] = '\"';
    memcpy(search + 1, key, i);
    search[1 + i] = '\"';
    search[2 + i] = ':';
    search[3 + i] = '\0';

    p = strstr(line, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') { sign = -1; p++; }
    if (*p < '0' || *p > '9') return 0;
    *out_val = 0;
    while (*p >= '0' && *p <= '9') {
        *out_val = *out_val * 10 + (int)(*p - '0');
        p++;
    }
    *out_val *= sign;
    return 1;
}

/* ---- Command parser (JSON + legacy compat) ---- */
uint8_t Serial_ParseCommand(const char *line, JsonCommand *cmd)
{
    const char *p;
    char tmp[16];
    int ival;

    if (!line || !cmd) return 0;
    memset(cmd, 0, sizeof(JsonCommand));

    p = line;
    while (*p == ' ' || *p == '\t') p++;

    /* JSON format */
    if (*p == '{') {
        if (json_extract_str(line, "cmd", tmp, sizeof(tmp))) {
            if (strcmp(tmp, "reset") == 0) {
                cmd->type = JSONCMD_RESET;
                return 1;
            }
            if (strcmp(tmp, "reset_th") == 0) {
                cmd->type = JSONCMD_RESET_TH;
                return 1;
            }
            if (strcmp(tmp, "mode") == 0) {
                cmd->type = JSONCMD_MODE;
                json_extract_str(line, "val", cmd->mode, sizeof(cmd->mode));
                return 1;
            }
            if (strcmp(tmp, "ctrl") == 0) {
                cmd->type = JSONCMD_CTRL;
                json_extract_str(line, "act", cmd->actuator, sizeof(cmd->actuator));
                if (json_extract_str(line, "val", tmp, sizeof(tmp))) {
                    memcpy(cmd->action, tmp, sizeof(cmd->action) - 1);
                } else if (json_extract_int(line, "val", &ival)) {
                    cmd->angle = (uint16_t)ival;
                    cmd->action[0] = '#';
                }
                return 1;
            }
            if (strcmp(tmp, "th") == 0) {
                cmd->type = JSONCMD_TH;
                json_extract_str(line, "key", cmd->th_key, sizeof(cmd->th_key));
                if (!json_extract_int(line, "val", &ival)) {
                    json_extract_int(line, "value", &ival);
                }
                cmd->th_val = (uint16_t)ival;
                return 1;
            }
        }
        /* code/ctl compat: {"code":4} */
        if (json_extract_int(line, "code", &ival) || json_extract_int(line, "ctl", &ival)) {
            if (ival >= 0 && ival <= 8) {
                cmd->type = JSONCMD_CTRL;
                switch (ival) {
                    case 0: memcpy(cmd->actuator, "buzzer", 7); memcpy(cmd->action, "off", 4); break;
                    case 1: memcpy(cmd->actuator, "buzzer", 7); memcpy(cmd->action, "on", 3); break;
                    case 2: memcpy(cmd->actuator, "servo", 6); cmd->angle = 90; cmd->action[0] = '#'; break;
                    case 3: memcpy(cmd->actuator, "servo", 6); cmd->angle = 0; cmd->action[0] = '#'; break;
                    case 4: memcpy(cmd->actuator, "fan", 4); memcpy(cmd->action, "on", 3); break;
                    case 5: memcpy(cmd->actuator, "fan", 4); memcpy(cmd->action, "off", 4); break;
                    case 6: memcpy(cmd->actuator, "led", 4); memcpy(cmd->action, "on", 3); break;
                    case 7: memcpy(cmd->actuator, "led", 4); memcpy(cmd->action, "auto", 5); break;
                    case 8: cmd->type = JSONCMD_RESET; break;
                }
                return 1;
            }
        }
        return 0;
    }

    /* Legacy: single digit 0-8 */
    if (*p >= '0' && *p <= '8' && (*(p + 1) == '\0' || *(p + 1) == ' ' || *(p + 1) == '\t' || *(p + 1) == '\r')) {
        cmd->type = JSONCMD_CTRL;
        switch (*p) {
            case '0': memcpy(cmd->actuator, "buzzer", 7); memcpy(cmd->action, "off", 4); break;
            case '1': memcpy(cmd->actuator, "buzzer", 7); memcpy(cmd->action, "on", 3); break;
            case '2': memcpy(cmd->actuator, "servo", 6); cmd->angle = 90; cmd->action[0] = '#'; break;
            case '3': memcpy(cmd->actuator, "servo", 6); cmd->angle = 0; cmd->action[0] = '#'; break;
            case '4': memcpy(cmd->actuator, "fan", 4); memcpy(cmd->action, "on", 3); break;
            case '5': memcpy(cmd->actuator, "fan", 4); memcpy(cmd->action, "off", 4); break;
            case '6': memcpy(cmd->actuator, "led", 4); memcpy(cmd->action, "on", 3); break;
            case '7': memcpy(cmd->actuator, "led", 4); memcpy(cmd->action, "auto", 5); break;
            case '8': cmd->type = JSONCMD_RESET; break;
        }
        return 1;
    }

    /* Legacy: kkVVV threshold */
    if (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) &&
        ((*(p+1) >= 'a' && *(p+1) <= 'z') || (*(p+1) >= 'A' && *(p+1) <= 'Z'))) {
        char key[4] = {0};
        uint16_t val = 0;
        key[0] = *p; key[1] = *(p+1);
        p += 2;
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (uint16_t)(*p - '0');
            p++;
        }
        cmd->type = JSONCMD_TH;
        memcpy(cmd->th_key, key, 3);
        cmd->th_val = val;
        return 1;
    }

    return 0;
}

/* ============ Legacy compat wrappers ============ */

void Serial_SendSensorToESP(uint8_t temp, uint8_t humi, uint32_t pm25_ugm3, uint32_t aq_ppm,
                            float flow_lpm, int32_t current_ma,
                            uint8_t battery_pct, uint8_t water_pct,
                            uint8_t link_level)
{
    UploadPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.temp = temp;
    pkt.humi = humi;
    pkt.pm25 = pm25_ugm3;
    pkt.aq   = aq_ppm;
    pkt.flow = flow_lpm;
    pkt.current_ma = current_ma;
    pkt.battery_pct = battery_pct;
    pkt.water_pct = water_pct;
    pkt.link_level = link_level;
    pkt.env_level = link_level;
    pkt.dht_ok = 1;
    Serial_SendUploadPacket(&pkt);
}

static void apply_control_digit(char ch,
                                uint8_t *buzzer_remote_mode, uint8_t *buzzer_remote_on,
                                uint8_t *fan_manual_mode, uint8_t *fan_manual_on,
                                uint8_t *servo_manual_mode, uint16_t *servo_manual_angle,
                                uint8_t *led_alarm_manual_mode)
{
    switch (ch) {
    case '1': *buzzer_remote_mode = 1; *buzzer_remote_on = 1; break;
    case '0': *buzzer_remote_mode = 1; *buzzer_remote_on = 0; break;
    case '2': *servo_manual_mode = 1; *servo_manual_angle = 90; break;
    case '3': *servo_manual_mode = 1; *servo_manual_angle = 0; break;
    case '4': *fan_manual_mode = 1; *fan_manual_on = 1; break;
    case '5': *fan_manual_mode = 1; *fan_manual_on = 0; break;
    case '6': *led_alarm_manual_mode = 1; break;
    case '7': *led_alarm_manual_mode = 0; break;
    case '8': *buzzer_remote_mode = 0; *buzzer_remote_on = 0;
              *fan_manual_mode = 0; *fan_manual_on = 0;
              *servo_manual_mode = 0; *servo_manual_angle = 0;
              *led_alarm_manual_mode = 0; break;
    default: break;
    }
}

void Serial_ProcessControlDownlink(const char *line,
                                   uint8_t *buzzer_remote_mode, uint8_t *buzzer_remote_on,
                                   uint8_t *fan_manual_mode, uint8_t *fan_manual_on,
                                   uint8_t *servo_manual_mode, uint16_t *servo_manual_angle,
                                   uint8_t *led_alarm_manual_mode)
{
    const char *p; char ch;
    if (!line || !buzzer_remote_mode || !buzzer_remote_on ||
        !fan_manual_mode || !fan_manual_on ||
        !servo_manual_mode || !servo_manual_angle || !led_alarm_manual_mode) return;

    p = line;
    while (*p == ' ' || *p == '\t') p++;
    ch = *p;
    if (ch < '0' || ch > '8') return;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (*p != '\0') return;
    apply_control_digit(ch, buzzer_remote_mode, buzzer_remote_on,
                        fan_manual_mode, fan_manual_on,
                        servo_manual_mode, servo_manual_angle,
                        led_alarm_manual_mode);
}

uint8_t Serial_ParseThresholdCmd(const char *line, char *out_key, uint16_t *out_value)
{
    const char *p; uint16_t val = 0; uint8_t digit_count = 0;
    if (!line || !out_key || !out_value) return 0;

    p = line;
    while (*p == ' ' || *p == '\t') p++;

    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) return 0;
    out_key[0] = *p; p++;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) return 0;
    out_key[1] = *p; p++;
    out_key[2] = '\0';

    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (uint16_t)(*p - '0');
        p++; digit_count++;
    }
    if (digit_count == 0 || digit_count > 5) return 0;

    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (*p != '\0') return 0;

    *out_value = val;
    return 1;
}
