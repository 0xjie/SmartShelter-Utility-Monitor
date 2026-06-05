#define MQTT_MAX_PACKET_SIZE 1536

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

// ====== WiFi ======
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ====== MQTT ======
const char* MQTT_SERVER = "bemfa.com";
const int MQTT_PORT = 9501;
const char* MQTT_CLIENT_ID = "YOUR_BEMFA_CLIENT_ID";
const char* MQTT_TELEMETRY_TOPIC = "telemetry";
const char* MQTT_COMMAND_TOPIC = "command";

WiFiClient espClient;
PubSubClient client(espClient);

String serialBuffer = "";
const size_t OFFLINE_QUEUE_CAPACITY = 240;
// 离线数据先入内存队列，并同步写入 LittleFS。
// 网络恢复后按队列顺序补发。
const char* OFFLINE_QUEUE_FILE = "/offline_queue.jsonl";
const char* OFFLINE_QUEUE_TMP_FILE = "/offline_queue.tmp";
String offlineQueue[OFFLINE_QUEUE_CAPACITY];
size_t offlineQueueHead = 0;
size_t offlineQueueTail = 0;
size_t offlineQueueCount = 0;
bool offlineStorageReady = false;

int lv_cache = 0;
int bp_cache = -1;
int wp_cache = -1;

// ====== Threshold cache ======
// ESP32 本地缓存 STM32 当前生效的阈值（共16个）
// 用途：每次收到 STM32 遥测 JSON 中的 th 字段时更新，确保 Qt 查询时返回最新值
// 同时也用于 forwardThresholdToStm32 转发后本地同步
// 预警阈值（warn）：触发预警的条件值
// 告警阈值（alarm）：触发告警的条件值，告警阈值由Qt设置后自动推导预警阈值
static int th_tr = 32;
static int th_tc = 18;
static int th_hw = 70;
static int th_hd = 30;
static int th_ph = 75;
static int th_aq = 100;
static int th_ci = 800;
static int th_fl = 5;

static int th_ta = 38;
static int th_tb = 10;
static int th_ha = 85;
static int th_hb = 20;
static int th_pa = 150;
static int th_aa = 200;
static int th_ca = 1200;
static int th_fa = 10;

unsigned long lastUploadTime = 0;
const unsigned long UPLOAD_INTERVAL = 300;
unsigned long lastWifiAttemptTime = 0;
unsigned long lastMqttAttemptTime = 0;
const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long MQTT_RETRY_INTERVAL = 3000;
bool wifiConnectedLogged = false;
bool offlineUploadActiveLogged = false;

static bool persistOfflineQueue();

static bool enqueueTelemetry(const String& jsonLine, bool persist = true) {
  if (jsonLine.length() == 0) {
    return false;
  }

  // 队列满时丢弃最旧数据，优先保留最新数据。
  if (offlineQueueCount >= OFFLINE_QUEUE_CAPACITY) {
    offlineQueueHead = (offlineQueueHead + 1) % OFFLINE_QUEUE_CAPACITY;
    offlineQueueCount--;
    Serial.println("Queue full, dropped oldest frame");
  }

  offlineQueue[offlineQueueTail] = jsonLine;
  offlineQueueTail = (offlineQueueTail + 1) % OFFLINE_QUEUE_CAPACITY;
  offlineQueueCount++;

  if (persist) {
    persistOfflineQueue();
  }
  return true;
}

static bool dequeueTelemetry(String& outJsonLine, bool persist = true) {
  if (offlineQueueCount == 0) {
    return false;
  }

  outJsonLine = offlineQueue[offlineQueueHead];
  offlineQueue[offlineQueueHead] = "";
  offlineQueueHead = (offlineQueueHead + 1) % OFFLINE_QUEUE_CAPACITY;
  offlineQueueCount--;

  if (persist) {
    persistOfflineQueue();
  }
  return true;
}

static void logQueueSize(const char* tag) {
  Serial.printf("%s count=%u\n", tag, (unsigned int)offlineQueueCount);
}

static bool peekTelemetry(String& outJsonLine) {
  if (offlineQueueCount == 0) {
    return false;
  }

  outJsonLine = offlineQueue[offlineQueueHead];
  return true;
}

static bool mqttReadyForUpload() {
  return WiFi.status() == WL_CONNECTED && client.connected();
}

static bool isValidTelemetryJson(const String& jsonLine) {
  if (jsonLine.length() == 0) {
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, jsonLine);
  if (err) {
    return false;
  }

  JsonObjectConst sen = doc["sen"];
  JsonObjectConst res = doc["res"];
  JsonObjectConst lv = doc["lv"];
  return !sen.isNull() && !res.isNull() && !lv.isNull();
}

static bool persistOfflineQueue() {
  if (!offlineStorageReady) {
    return false;
  }

  // 先写 tmp 再替换正式文件，降低掉电损坏风险。
  LittleFS.remove(OFFLINE_QUEUE_TMP_FILE);

  if (offlineQueueCount == 0) {
    LittleFS.remove(OFFLINE_QUEUE_FILE);
    return true;
  }

  File file = LittleFS.open(OFFLINE_QUEUE_TMP_FILE, "w");
  if (!file) {
    Serial.println("Offline queue save failed: open temp file");
    return false;
  }

  for (size_t i = 0; i < offlineQueueCount; i++) {
    const size_t idx = (offlineQueueHead + i) % OFFLINE_QUEUE_CAPACITY;
    file.println(offlineQueue[idx]);
  }
  file.close();

  LittleFS.remove(OFFLINE_QUEUE_FILE);
  if (!LittleFS.rename(OFFLINE_QUEUE_TMP_FILE, OFFLINE_QUEUE_FILE)) {
    Serial.println("Offline queue save failed: rename temp file");
    return false;
  }

  return true;
}

static void loadOfflineQueueFromDisk() {
  if (!offlineStorageReady || !LittleFS.exists(OFFLINE_QUEUE_FILE)) {
    return;
  }

  File file = LittleFS.open(OFFLINE_QUEUE_FILE, "r");
  if (!file) {
    Serial.println("Offline queue load failed: open file");
    return;
  }

  size_t restored = 0;
  size_t droppedInvalid = 0;
  size_t droppedOverflow = 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) {
      continue;
    }

    if (!isValidTelemetryJson(line)) {
      droppedInvalid++;
      continue;
    }

    // 恢复时仍走统一入队逻辑，保持队列状态一致。
    if (offlineQueueCount >= OFFLINE_QUEUE_CAPACITY) {
      droppedOverflow++;
    }
    if (enqueueTelemetry(line, false)) {
      restored++;
    }
  }
  file.close();

  if (droppedInvalid > 0 || droppedOverflow > 0) {
    persistOfflineQueue();
  }

  Serial.printf("Offline queue restored: active=%u, loaded=%u, invalid=%u, overflow=%u\n",
                (unsigned int)offlineQueueCount,
                (unsigned int)restored,
                (unsigned int)droppedInvalid,
                (unsigned int)droppedOverflow);
}

static void setupOfflineStorage() {
  offlineStorageReady = LittleFS.begin(true);
  if (!offlineStorageReady) {
    Serial.println("LittleFS mount failed, offline queue is RAM-only");
    return;
  }

  // 若上次停在 tmp 阶段，这里优先尝试恢复。
  if (!LittleFS.exists(OFFLINE_QUEUE_FILE) && LittleFS.exists(OFFLINE_QUEUE_TMP_FILE)) {
    LittleFS.rename(OFFLINE_QUEUE_TMP_FILE, OFFLINE_QUEUE_FILE);
  }

  loadOfflineQueueFromDisk();
}

static bool publishTelemetryFrame(const String& stm32JsonLine, bool fromQueue = false) {
  if (stm32JsonLine.length() == 0) {
    return false;
  }

  StaticJsonDocument<1536> rootDoc;
  StaticJsonDocument<1024> payloadDoc;
  rootDoc["type"] = "telemetry";
  rootDoc["source"] = "esp32";

  // STM32 原始 JSON 作为 payload 上传，便于上位机统一解析。
  DeserializationError payloadErr = deserializeJson(payloadDoc, stm32JsonLine);
  if (payloadErr) {
    Serial.print("Telemetry wrap failed: ");
    Serial.println(payloadErr.c_str());
    return false;
  }

  rootDoc["payload"] = payloadDoc.as<JsonObject>();
  String uploadJson;
  serializeJson(rootDoc, uploadJson);

  bool ok = client.publish(MQTT_TELEMETRY_TOPIC, uploadJson.c_str());
  if (ok) {
    if (fromQueue) {
      Serial.printf("📤 [SEND] cached upload OK (%d bytes), queued=%u: %s\n",
                    uploadJson.length(), (unsigned int)offlineQueueCount, uploadJson.c_str());
    } else {
      Serial.printf("⬆️ [SEND] realtime upload OK (%d bytes), queued=%u: %s\n",
                    uploadJson.length(), (unsigned int)offlineQueueCount, uploadJson.c_str());
    }
  } else {
    Serial.printf("[SEND] upload FAILED (%d bytes, mqtt_state=%d)\n",
                  uploadJson.length(), client.state());
  }
  return ok;
}

// ============================================================
// 下行命令转发函数（ESP32 → STM32 UART）
// ============================================================
// 这两个函数是 ESP32 桥接角色的核心：
//   MQTT 收到 Qt/Web 下行命令 → 解析 kind 字段 → 调用对应转发函数
//   → Serial2 (UART2, TX=GPIO17) → STM32 PA10(USART1 RX)
// UART 参数：115200 8N1

// ---- 数字控制码转发 ----
// Qt 远程控制按钮按下 → MQTT {"kind":"control","code":1}
// → ESP32 调用 forwardDigitToStm32(1) → Serial2.println("1")
// → STM32 Serial_ParseCommand() 解析单字符 → JSONCMD_CTRL → exec_control_cmd()
// 控制码 0~7 含义（与 STM32 serial.c:370-379 的 switch 对应）：
//   0=蜂鸣器关 1=蜂鸣器开 2=窗户开(90°) 3=窗户关(0°)
//   4=风扇开   5=风扇关   6=LED手动开  7=LED恢复自动
static void forwardDigitToStm32(int digit) {
  if (digit < 0 || digit > 8) {
    return;
  }

  Serial2.println(String(digit));       // UART发送单个数字 + \n
  Serial.print("🔻 [STM32 CMD] ");
  Serial.println(digit);
}

// ---- 阈值修改命令转发 ----
// Qt 一键下发 → MQTT {"kind":"threshold","values":{"ta":38,...}}
// → ESP32 遍历 values，每个键值对调用一次本函数
// → Serial2.printf("{\"cmd\":\"th\",\"key\":\"ta\",\"val\":38}\n")
// → STM32 Serial_ParseCommand() 解析 → JSONCMD_TH → 修改阈值变量
// 同时更新 ESP32 本地缓存（16个阈值全部缓存，供遥测 th 字段上报）
static void forwardThresholdToStm32(const char* key, int value) {
  // 向 STM32 发送 JSON 格式的阈值命令
  Serial2.printf("{\"cmd\":\"th\",\"key\":\"%s\",\"val\":%d}\n", key, value);
  Serial2.flush();
  delay(20);  // 等待 STM32 接收处理完成
  Serial.printf("🔻 [STM32 TH] %s%d\n", key, value);

  // 同步更新 ESP32 本地阈值缓存
  if (strcmp(key, "tr") == 0) th_tr = value;
  else if (strcmp(key, "tc") == 0) th_tc = value;
  else if (strcmp(key, "ta") == 0) th_ta = value;
  else if (strcmp(key, "tb") == 0) th_tb = value;
  else if (strcmp(key, "hw") == 0) th_hw = value;
  else if (strcmp(key, "hd") == 0) th_hd = value;
  else if (strcmp(key, "ha") == 0) th_ha = value;
  else if (strcmp(key, "hb") == 0) th_hb = value;
  else if (strcmp(key, "ph") == 0) th_ph = value;
  else if (strcmp(key, "pa") == 0) th_pa = value;
  else if (strcmp(key, "aq") == 0) th_aq = value;
  else if (strcmp(key, "aa") == 0) th_aa = value;
  else if (strcmp(key, "ci") == 0) th_ci = value;
  else if (strcmp(key, "ca") == 0) th_ca = value;
  else if (strcmp(key, "fl") == 0) th_fl = value;
  else if (strcmp(key, "fa") == 0) th_fa = value;
}

// ---- 旧协议兼容解析器 ----
// 兼容三种旧格式：
//   1. {"code":4} 或 {"cmd":4} 或 {"ctl":4} → 数字码直接转发
//   2. {"th":{"ta":38,"tb":10}} → 遍历阈值键值对逐个转发
//   3. {"cmd":"th","key":"ta","val":38} → 单键值转发
//   4. 其他 JSON → 整包当作原始命令通过 Serial2.println 发送给 STM32
static bool parseLegacyCommandJson(const JsonObjectConst& payloadObj) {
  int ncmd = -1;
  bool haveNum = false;

  if (payloadObj.containsKey("code")) {
    ncmd = payloadObj["code"].as<int>();
    haveNum = true;
  } else if (payloadObj.containsKey("cmd") && payloadObj["cmd"].is<int>()) {
    ncmd = payloadObj["cmd"].as<int>();
    haveNum = true;
  } else if (payloadObj.containsKey("ctl") && payloadObj["ctl"].is<int>()) {
    ncmd = payloadObj["ctl"].as<int>();
    haveNum = true;
  }

  if (haveNum && ncmd >= 0 && ncmd <= 8) {
    forwardDigitToStm32(ncmd);
    return true;
  }

  if (payloadObj.containsKey("th") && payloadObj["th"].is<JsonObject>()) {
    JsonObjectConst thObj = payloadObj["th"].as<JsonObjectConst>();
    for (JsonPairConst kv : thObj) {
      forwardThresholdToStm32(kv.key().c_str(), kv.value().as<int>());
    }
    return true;
  }

  if (payloadObj.containsKey("cmd") && payloadObj["cmd"].is<const char*>() &&
      strcmp(payloadObj["cmd"] | "", "th") == 0) {
    const char* key = payloadObj["key"] | "";
    int value = payloadObj["val"] | (payloadObj["value"] | -1);
    if (key[0] != '\0' && value >= 0) {
      forwardThresholdToStm32(key, value);
      return true;
    }
  }

  if (payloadObj.containsKey("cmd")) {
    String raw;
    serializeJson(payloadObj, raw);
    Serial2.println(raw);
    Serial.print("🔻 [STM32 JSON] ");
    Serial.println(raw);
    return true;
  }

  return false;
}

// ============================================================
// 下行命令路由（ESP32 核心分发逻辑）
// ============================================================
// 接收 MQTT 消息的内层 payload，根据 kind 字段分发到不同的转发函数。
// 支持的 kind 类型：
//   "control"          → forwardDigitToStm32(code)        控制码0~7
//   "threshold"        → 遍历values → forwardThresholdToStm32()  阈值修改
//   "reset_threshold"  → Serial2.println({"cmd":"reset_th"})      阈值恢复默认
//   "json"             → Serial2.println(原始JSON)                透传
//   其他/无kind        → 交给 parseLegacyCommandJson() 旧协议兼容
static bool handleCommandPayload(const JsonObjectConst& payloadObj) {
  if (payloadObj.containsKey("th_sync")) {
    Serial.println("Threshold sync message ignored");
    return true;
  }

  if (payloadObj.containsKey("kind")) {
    const char* kind = payloadObj["kind"] | "";

    if (strcmp(kind, "control") == 0) {
      int code = payloadObj["code"] | -1;
      if (code >= 0 && code <= 8) {
        forwardDigitToStm32(code);
        return true;
      }
    } else if (strcmp(kind, "threshold") == 0) {
      // 直接 as<JsonObjectConst>() 而非 is<JsonObject>()，避免 JsonVariantConst 的 const 兼容问题
      JsonObjectConst values;
      if (payloadObj.containsKey("values")) {
        values = payloadObj["values"].as<JsonObjectConst>();
      }
      if (values.isNull() && payloadObj.containsKey("th")) {
        values = payloadObj["th"].as<JsonObjectConst>();
      }

      if (!values.isNull()) {
        for (JsonPairConst kv : values) {
          forwardThresholdToStm32(kv.key().c_str(), kv.value().as<int>());
        }
        return true;
      }
    } else if (strcmp(kind, "reset_threshold") == 0) {
      Serial2.println("{\"cmd\":\"reset_th\"}");
      Serial.println("🔻 [STM32 RESET] reset_th");
      return true;
    } else if (strcmp(kind, "json") == 0) {
      if (payloadObj.containsKey("data") && payloadObj["data"].is<JsonObject>()) {
        String raw;
        serializeJson(payloadObj["data"], raw);
        Serial2.println(raw);
        Serial.print("🔻 [STM32 RAW] ");
        Serial.println(raw);
        return true;
      }
    }
  }

  return parseLegacyCommandJson(payloadObj);
}

// ============================================================
// MQTT 回调函数 — 下行命令入口
// ============================================================
// 巴法云收到 Qt/Web 发布的 command topic 消息后推送给 ESP32
// 本函数处理流程：
//   ① 只处理 "command" topic，其他忽略
//   ② 单字符消息（如 "4"）→ 直接 forwardDigitToStm32(c-'0')
//   ③ JSON 格式消息 → 解析后提取 payload 交给 handleCommandPayload()
//   ④ 支持带 type/source 包装的标准格式：
//      {"type":"command","source":"qt","payload":{...}}
//      也兼容不带包装的原始格式：{"kind":"control","code":1}
//   ⑤ th_sync 消息是阈值同步通知，仅忽略（不转发给 STM32）
void callback(char* topic, byte* payload, unsigned int length) {
  // ① 仅处理下行命令 topic
  if (strcmp(topic, MQTT_COMMAND_TOPIC) != 0) {
    return;
  }

  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();

  Serial.print("MQTT message[");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (msg.length() == 0) {
    return;
  }

  // ② 单字符数字码：直接转发（兼容最简协议，如 Web 直接发 "4"）
  if (msg.length() == 1) {
    const char c = msg[0];
    if (c >= '0' && c <= '8') {
      forwardDigitToStm32(c - '0');
    }
    return;
  }

  // ③ 非 JSON 消息直接丢弃
  if (!msg.startsWith("{")) {
    Serial.println("Unknown command payload, ignored");
    return;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.print("Command JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  // ④ th_sync 是 ESP32 内部阈值同步消息，不转发给 STM32
  if (doc.containsKey("th_sync")) {
    Serial.println("Threshold sync message ignored");
    return;
  }

  // ⑤ 带 type/source 包装的标准格式：提取 payload 层
  //    {"type":"command","source":"qt","payload":{...实际命令...}}
  if (doc.containsKey("type") && String((const char*)doc["type"]) == "command" &&
      doc.containsKey("payload") && doc["payload"].is<JsonObject>()) {
    if (handleCommandPayload(doc["payload"].as<JsonObjectConst>())) {
      return;
    }
  }

  // ⑥ 无包装的原始 JSON：整包当作 payload 处理（兼容旧协议）
  if (doc.is<JsonObject>()) {
    handleCommandPayload(doc.as<JsonObjectConst>());
    return;
  }

  Serial.println("Unsupported command JSON, ignored");
}

void setup_wifi() {
  Serial.println("WiFi connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiAttemptTime = millis();
}

static void serviceWiFi() {
  wl_status_t wifiStatus = WiFi.status();

  if (wifiStatus == WL_CONNECTED) {
    if (!wifiConnectedLogged) {
      Serial.println("📶 [WIFI] connected");
      Serial.println(WiFi.localIP());
      wifiConnectedLogged = true;
    }
    return;
  }

  if (wifiConnectedLogged) {
    Serial.println("📴 [WIFI] disconnected");
    wifiConnectedLogged = false;
    offlineUploadActiveLogged = false;
    lastWifiAttemptTime = millis() - WIFI_RETRY_INTERVAL;
  }

  if (millis() - lastWifiAttemptTime >= WIFI_RETRY_INTERVAL) {
    Serial.println("🔄 [WIFI] reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastWifiAttemptTime = millis();
  }
}

static void serviceMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (client.connected()) {
    return;
  }

  if (millis() - lastMqttAttemptTime < MQTT_RETRY_INTERVAL) {
    return;
  }

  lastMqttAttemptTime = millis();
  Serial.print("MQTT connecting...");
  if (client.connect(MQTT_CLIENT_ID)) {
    Serial.println(" connected");
    client.subscribe(MQTT_COMMAND_TOPIC);
    Serial.print("Subscribed topic: ");
    Serial.println(MQTT_COMMAND_TOPIC);
  } else {
    Serial.print(" failed rc=");
    Serial.println(client.state());
  }
}

// ============================================================
// 初始化
// ============================================================
// Serial:  调试串口 (USB, 115200)
// Serial2: STM32通信串口 (UART2, TX=GPIO17, RX=GPIO16, 115200 8N1)
//          TX(GPIO17) → STM32 PA10(USART1 RX)
//          RX(GPIO16) ← STM32 PA9 (USART1 TX)
// WiFi:    连接路由器
// MQTT:    连接巴法云 bemfa.com:9501，订阅 command topic（下行命令）
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);   // UART2 与 STM32 通信

  delay(500);
  Serial.println("ESP32 started");

  setupOfflineStorage();
  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(1536);
}

// ============================================================
// 主循环
// ============================================================
// 三大任务：
//   1. WiFi/MQTT 保活：serviceWiFi() + serviceMqtt()
//   2. STM32 数据接收：UART2 读取 → 解析 JSON → MQTT 上传遥测
//   3. 离线队列回放：断网期间缓存的数据在网络恢复后逐条上传
void loop() {
  // ---- 网络保活 ----
  serviceWiFi();      // WiFi 断线自动重连
  serviceMqtt();      // MQTT 断线自动重连 + 订阅
  if (client.connected()) {
    client.loop();    // MQTT 消息循环（触发 callback）
  }

  // ---- STM32 UART 数据接收与上传 ----
  // 数据流：STM32 PA9(TX) → GPIO16(RX) → Serial2 → JSON解析 → MQTT发布
  // 行分隔符：\n
  // 遥测JSON格式（STM32 每秒发送一次）：
  //   {"sen":{...},"res":{...},"lv":{...},"act":{...},"th":{...},"alm":[...]}
  // 如果收到 {"ack":...} 则跳过（STM32 的确认回执，不上传）
  while (Serial2.available()) {
    char c = (char)Serial2.read();

    if (c == '\n') {
      serialBuffer.trim();

      if (serialBuffer.length() > 0) {
        Serial.println("[UART] data: " + serialBuffer);
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, serialBuffer);
        if (!error) {
          if (doc.containsKey("ack")) {
            Serial.print("✅ [STM32 ACK] ");
            Serial.println(serialBuffer);
            serialBuffer = "";
            continue;
          }

          JsonObjectConst sen = doc["sen"];
          JsonObjectConst res = doc["res"];
          JsonObjectConst lv = doc["lv"];
          const bool hasBacklog = offlineQueueCount > 0;
          if (!sen.isNull() && !res.isNull() && !lv.isNull()) {
            // 只有没有历史积压时才实时直传。
            // 只要已有积压，新帧也继续入队，保证顺序。
            if (!hasBacklog && mqttReadyForUpload() && publishTelemetryFrame(serialBuffer, false)) {
              Serial.println("[SEND=] direct realtime frame");
            } else {
              enqueueTelemetry(serialBuffer);
              logQueueSize("💾 [QUEUE+]");
            }
            lv_cache = doc["lv"]["link"] | 0;
            bp_cache = doc["res"]["bp"] | -1;
            wp_cache = doc["res"]["wp"] | -1;
            Serial.printf("T:%d H:%d PM:%d AQ:%d F:%.2f I:%d BP:%d WP:%d LV:%d\n",
              sen["t"].as<int>(), sen["h"].as<int>(),
              sen["pm"].as<int>(), sen["aq"].as<int>(),
              sen["f"].as<float>(), sen["i"].as<int>(),
              bp_cache, wp_cache, lv_cache);
          } else {
            Serial.println("Drop invalid telemetry frame");
          }
        } else {
          Serial.println("Drop invalid telemetry frame");
        }
      }

      serialBuffer = "";
    } else {
      serialBuffer += c;
      if (serialBuffer.length() > 900) {
        serialBuffer = "";
        Serial.println("UART buffer overflow, drop frame");
      }
    }
  }

  if (millis() - lastUploadTime > UPLOAD_INTERVAL && offlineQueueCount > 0) {
    lastUploadTime = millis();

    if (mqttReadyForUpload()) {
      if (!offlineUploadActiveLogged) {
        Serial.printf("🚀 [QUEUE SEND] start cached upload, count=%u\n", (unsigned int)offlineQueueCount);
        offlineUploadActiveLogged = true;
      }
      String pendingJson;
      if (peekTelemetry(pendingJson)) {
        // 先看队头，发布成功后再出队。
        // 失败时保留原帧，等待下次重试。
        if (!isValidTelemetryJson(pendingJson)) {
          Serial.println("Drop invalid queued frame");
          dequeueTelemetry(pendingJson);
          logQueueSize("🗑️ [QUEUE-INVALID]");
        } else if (publishTelemetryFrame(pendingJson, true)) {
          dequeueTelemetry(pendingJson);
          logQueueSize("📦 [QUEUE-]");
          if (offlineQueueCount == 0) {
            Serial.println("🏁 [QUEUE SEND] cached upload complete, back to realtime");
            offlineUploadActiveLogged = false;
          }
        }
      }
    } else {
      Serial.printf("[QUEUE WAIT] count=%u\n", (unsigned int)offlineQueueCount);
    }
  }
}
