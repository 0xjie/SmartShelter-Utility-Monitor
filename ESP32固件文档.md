# ESP32 边缘网关固件技术文档

## 一、概述

ESP32固件是整个系统的边缘网关，负责STM32串口数据接收与转发、WiFi无线网络接入、MQTT协议桥接（连接巴法云平台）、下行控制指令解析与转发，以及阈值参数本地缓存。

- **文件位置**：`D:\AAA\esp32\stm32_to_esp32_uart_test\stm32_to_esp32_uart_test.ino`
- **开发环境**：Arduino IDE
- **编程语言**：C/C++
- **芯片**：ESP32-WROOM-32（双核240MHz, 520KB SRAM, 集成WiFi+蓝牙）
- **依赖库**：WiFi.h, PubSubClient.h, ArduinoJson.h

---

## 二、硬件连接

### 2.1 STM32 ↔ ESP32 UART

| 方向 | STM32引脚 | ESP32引脚 | 参数 |
|------|-----------|-----------|------|
| STM32→ESP32 | PA9 (USART1_TX) | GPIO16 (RX2) | 115200bps |
| ESP32→STM32 | PA10 (USART1_RX) | GPIO17 (TX2) | 8N1, 无流控 |

### 2.2 WiFi配置

```
SSID: "vivo"
密码: "12345678"
连接超时: 10秒（20次×500ms）
```

---

## 三、MQTT连接配置

| 参数 | 值 |
|------|-----|
| 服务器 | bemfa.com |
| 端口 | 9501（TCP） |
| 客户端ID | 6525cbc01d2d408eb1b28ca77a134ebc（设备唯一私钥） |
| 主题 | test001up（双向：既发布又订阅） |
| 缓冲区 | 1024字节（适应新JSON协议约400~500字节，默认128字节不够） |
| 上传间隔 | 2秒（UPLOAD_INTERVAL=2000ms） |

---

## 四、核心功能模块

### 4.1 UART数据接收（Serial2）

**逐行读取机制**：
- 以换行符`\n`为帧分隔符
- 收到完整行后trim处理，缓存到`stm32_last_json`字符串
- 设置`stm32_json_updated=true`标志通知上传模块
- 缓冲溢出保护：超过600字节自动丢弃当前帧并重置缓冲区

**轻量解析**（仅用于串口日志打印）：
- 使用ArduinoJson（StaticJsonDocument<256>）解析STM32上行JSON
- 提取关键字段：sen.t, sen.h, sen.pm, sen.aq, sen.f, sen.i（传感器值）
- 提取联动级别：lv.link
- 提取资源百分比：res.bp, res.wp
- 在串口打印格式：`T:26 H:60 PM:35 AQ:52 F:1.23 I:450 BP:85 WP:72 LV:0`

### 4.2 MQTT数据上传

- 每2秒定时检查`stm32_json_updated`标志
- 直接转发STM32的原始完整JSON（不做重新构建），保持数据完整性
- 上传成功：清除updated标志
- 上传失败：保留updated标志，下次循环重试（`client.publish()`返回false时恢复标志）
- 串口日志：`上传OK(字节数): {...}` 或 `上传失败(字节数, MQTT状态:rc)`

### 4.3 MQTT下行处理（callback函数）

**三层过滤机制**：

1. **自身上行数据过滤**：含`"sen"`字段的消息直接忽略（防止ESP32自己上传的数据被当作下行命令）
2. **阈值同步过滤**：含`"th_sync"`字段的消息忽略（防止阈值同步消息循环转发）
3. **空消息过滤**：`msg.length()==0`直接返回

**支持的四种下行格式**：

#### a) 单字符数字指令（'0'~'8'，旧工具兼容）
```
'0' → 蜂鸣器关    '1' → 蜂鸣器开
'2' → 窗户开90°   '3' → 窗户关0°
'4' → 风扇开      '5' → 风扇关
'6' → LED全亮     '7' → LED恢复自动
'8' → 全局复位
```
处理：直接通过`Serial2.println(msg)`转发至STM32。

#### b) JSON控制指令
```json
{"code": 4, "name": "风扇开启"}
```
- 支持三种键名：`code`、`cmd`(数字)、`ctl`
- 提取code值0~8，调用`forwardDigitToStm32()`转发
- 如果为JSON但无code字段，检查是否有`cmd`键（新协议），有则整行JSON直接转发

#### c) JSON阈值修改
```json
{"th": {"ta": 40, "pa": 160}}
```
- 遍历th对象中所有key-value对
- 每个键值对调用`forwardThresholdToStm32(key, value)`转发（格式：`ta40\n`）
- 同时更新ESP32本地阈值缓存
- 发布th_sync确认消息到MQTT（`"th"`替换为`"th_sync"`），供Web/Qt同步阈值，同时防止被自己过滤

#### d) JSON新协议命令（直通转发）
```json
{"cmd": "ctrl", "act": "fan", "val": "on"}
{"cmd": "mode", "val": "AUTO"}
{"cmd": "reset"}
{"cmd": "reset_th"}
```
- 直接整行JSON通过`Serial2.println(msg)`转发至STM32
- STM32端的`Serial_ParseCommand()`负责解析执行

### 4.4 指令转发函数

**forwardDigitToStm32(digit)**
- 将0~8数字转为字符串 + 换行符发送
- `Serial2.println(String(digit))`

**forwardThresholdToStm32(key, value)**
- 将两字母key + 数值 + 换行符发送（如`tr35\n`）
- `Serial2.print(key); Serial2.println(value)`
- 同时更新ESP32本地对应缓存变量

### 4.5 阈值缓存

ESP32本地缓存16个阈值参数：

| 类型 | 变量 | 默认值 | 含义 |
|------|------|--------|------|
| 预警 | th_tr | 32 | 温度预警上限 |
| 预警 | th_tc | 18 | 温度预警下限 |
| 预警 | th_hw | 70 | 湿度预警上限 |
| 预警 | th_hd | 30 | 湿度预警下限 |
| 预警 | th_ph | 75 | PM2.5预警 |
| 预警 | th_aq | 100 | 空气质量预警 |
| 预警 | th_ci | 800 | 电流预警(mA) |
| 预警 | th_fl | 5 | 水流预警(L/min) |
| 告警 | th_ta | 38 | 温度告警上限 |
| 告警 | th_tb | 10 | 温度告警下限 |
| 告警 | th_ha | 85 | 湿度告警上限 |
| 告警 | th_hb | 20 | 湿度告警下限 |
| 告警 | th_pa | 150 | PM2.5告警 |
| 告警 | th_aa | 200 | 空气质量告警 |
| 告警 | th_ca | 1200 | 电流告警(mA) |
| 告警 | th_fa | 10 | 水流告警(L/min) |

注意：ESP32缓存仅供参考/日志打印，实际阈值控制权在STM32端。阈值通过MQTT远程设置后同步更新缓存。

---

## 五、WiFi与MQTT重连机制

### 5.1 初始化流程

```
Serial.begin(115200)
Serial2.begin(115200, SERIAL_8N1, 16, 17)
setup_wifi() → 连接WiFi（最多重试10秒）
client.setServer("bemfa.com", 9501)
client.setCallback(callback)
client.setBufferSize(1024)
```

### 5.2 级联重连（reconnect函数）

主循环中`client.loop()`后检查连接状态：
1. MQTT未连接 → 进入reconnect()
2. reconnect()中先检查WiFi状态：
   - WiFi.status() != WL_CONNECTED → 先调用`setup_wifi()`重连WiFi
   - WiFi已连接 → 直接尝试MQTT连接
3. MQTT连接成功 → 自动重新订阅test001up主题
4. MQTT连接失败 → 打印错误码，delay(3000)后重试

**特点**：级联恢复链路（WiFi断→先连WiFi→再连MQTT），避免因WiFi问题导致MQTT反复重连失败。

---

## 六、防循环机制

当Qt或Web下发阈值修改命令`{"th":{"ta":40}}`时存在循环风险：
- ESP32收到→转发STM32→同时发布th_sync确认→巴法云广播→ESP32再次收到→又转发...
- 如果th_sync被当作新阈值命令处理会导致无限循环

**三层防护**：

1. **sen过滤**：含`"sen"`字段的消息直接忽略（上行数据不会被当作下行）
2. **th_sync过滤**：含`"th_sync"`字段的消息忽略（阈值同步确认不会触发二次处理）
3. **th→th_sync替换**：ESP32在发布确认消息时，将原JSON中的`"th"`替换为`"th_sync"`再发布

---

## 七、主循环流程

```
loop():
  1. if (!client.connected()) → reconnect()
  2. client.loop()  // 处理MQTT消息收发
  3. // 串口接收处理
     while (Serial2.available()):
       read char → 遇到\n则处理完整行
         缓存到stm32_last_json
         轻量解析打印日志
  4. // 定时上传
     if (millis()-lastUploadTime > 2000 && stm32_json_updated):
        client.publish("test001up", stm32_last_json)
        成功→清除标志, 失败→保留标志重试
```

---

## 八、数据流总结

### 上行路径
```
STM32(1s采样) → UART TX → ESP32 Serial2 RX → 行缓冲 → stm32_last_json
→ 2s定时器触发 → MQTT publish("test001up") → 巴法云 → Qt/Web订阅
```

### 下行路径
```
Qt/Web publish → 巴法云 → ESP32 MQTT订阅 → callback解析
→ 判断消息类型 → Serial2转发 → STM32 UART RX → Serial_ParseCommand → 执行
```

---

## 九、部署说明

1. 使用Arduino IDE打开`stm32_to_esp32_uart_test.ino`
2. 安装依赖库：PubSubClient（by Nick O'Leary）、ArduinoJson（by Benoit Blanchon）
3. 修改WiFi SSID和密码为实际网络
4. 选择开发板：ESP32 Dev Module
5. 编译上传

**注意事项**：
- MQTT_CLIENT_ID为设备唯一标识，多设备部署时需区分
- MQTT主题需与Qt/Web端config.js中的topic配置一致
- 确保ESP32与STM32的UART波特率一致（115200）
