/**
 * ESP32：订阅巴法云 MQTT，解析 Qt 下发的 {"code":0~8,"name":"..."}，
 * 仅将 0~8 数字经 Serial2 转发给 STM32（一行数字 + 换行）。
 *
 * 约定：0 蜂鸣关 / 1 蜂鸣开 / 2 窗开 / 3 窗关 / 4 扇开 / 5 扇关 / 6 灯开 / 7 灯关
 */

#define MQTT_MAX_PACKET_SIZE 1024
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ====== WiFi ======
const char* WIFI_SSID = "vivo";
const char* WIFI_PASS = "12345678";

// ====== MQTT (巴法云) ======
const char* MQTT_SERVER = "bemfa.com";
const int MQTT_PORT = 9501;
const char* MQTT_CLIENT_ID = "6525cbc01d2d408eb1b28ca77a134ebc";  // 私钥
const char* MQTT_TOPIC = "test001up";

// ====== 对象 ======
WiFiClient espClient;
PubSubClient client(espClient);

// ====== 串口缓冲 ======
String serialBuffer = "";

// ====== 数据缓存（缓存 STM32 上行完整 JSON，定时转发到 MQTT）======
String stm32_last_json = "";       // STM32 上行完整 JSON 字符串
bool  stm32_json_updated = false;  // 有新数据待上传
int lv_cache = 0;   // 联动级别（用于日志打印）
int bp_cache = -1;
int wp_cache = -1;

// ====== 阈值缓存（MQTT 远程设置后缓存，上传时附带到云端）======
// 预警阈值
static int th_tr = 32;   // 温度预警上限
static int th_tc = 18;   // 温度预警下限
static int th_hw = 70;   // 湿度预警上限
static int th_hd = 30;   // 湿度预警下限
static int th_ph = 75;   // PM2.5 预警
static int th_aq = 100;  // 空气质量预警
static int th_ci = 800;  // 电流预警 (mA)
static int th_fl = 5;    // 水流预警 (L/min)
// 告警阈值
static int th_ta = 38;   // 温度告警上限
static int th_tb = 10;   // 温度告警下限
static int th_ha = 85;   // 湿度告警上限
static int th_hb = 20;   // 湿度告警下限
static int th_pa = 150;  // PM2.5 告警
static int th_aa = 200;  // 空气质量告警
static int th_ca = 1200; // 电流告警 (mA)
static int th_fa = 10;   // 水流告警 (L/min)

unsigned long lastUploadTime = 0;
const unsigned long UPLOAD_INTERVAL = 2000;  // 2秒上传，降低延迟

// 将 0~8 转发到 STM32（单行数字 + 换行）
static void forwardDigitToStm32(int digit) {
  if (digit < 0 || digit > 8) {
    return;
  }
  Serial2.println(String(digit));
  Serial.print(">>> 转发 STM32: ");
  Serial.print(digit);
  Serial.println(" (0蜂关 1蜂开 2窗开 3窗关 4扇开 5扇关 6灯开 7灯关)");
}

// ====== 阈值命令：转发到 STM32 并更新本地缓存 ======
static void forwardThresholdToStm32(const char* key, int value) {
  Serial2.print(key);
  Serial2.println(value);
  Serial.printf(">>> 阈值 STM32: %s%d\n", key, value);

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

// ====== MQTT 回调（Qt 下行 -> 转发 STM32）======
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();

  Serial.print("MQTT消息[");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (msg.length() == 0) {
    return;
  }

  // 过滤自己上传的传感器数据（含"sen"字段），不是下行命令
  if (msg.indexOf("\"sen\"") >= 0) {
    return;
  }

  /* 单字符：'0'~'7'（与其它工具兼容） */
  if (msg.length() == 1) {
    const char c = msg[0];
    if (c >= '0' && c <= '8') {
      Serial2.println(msg);
      Serial.print(">>> 转发 STM32: ");
      Serial.print(msg);
      Serial.println(" (0蜂关 1蜂开 2窗开 3窗关 4扇开 5扇关 6灯开 7灯关)");
    } else {
      Serial.println("未识别的单字符指令");
    }
    return;
  }

  /* JSON：优先 {"code":4,"name":"风扇开启"} */
  if (msg.startsWith("{")) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (!err) {
      /* 阈值修改：{"th":{"tr":35,"tf":30}} */
      if (doc.containsKey("th") && doc["th"].is<JsonObject>()) {
        JsonObject thObj = doc["th"];
        for (JsonPair kv : thObj) {
          const char* key = kv.key().c_str();
          int val = kv.value().as<int>();
          forwardThresholdToStm32(key, val);
        }
        // 回传云端（改为 th_sync 防循环），供 Web 同步阈值
        String syncMsg = msg;
        syncMsg.replace("\"th\"", "\"th_sync\"");
        client.publish(MQTT_TOPIC, syncMsg.c_str());
        return;
      }

      // 忽略云端回传的阈值同步消息，避免循环
      if (doc.containsKey("th_sync")) {
        return;
      }

      int ncmd = -1;
      bool haveNum = false;
      if (doc.containsKey("code")) {
        ncmd = doc["code"].as<int>();
        haveNum = true;
      } else if (doc.containsKey("cmd") && doc["cmd"].is<int>()) {
        ncmd = doc["cmd"].as<int>();
        haveNum = true;
      } else if (doc.containsKey("ctl") && doc["ctl"].is<int>()) {
        ncmd = doc["ctl"].as<int>();
        haveNum = true;
      }
      if (haveNum && ncmd >= 0 && ncmd <= 8) {
        forwardDigitToStm32(ncmd);
        return;
      }

      // 新协议命令（ctrl/mode/th/reset）：直接转发给 STM32
      if (doc.containsKey("cmd")) {
        Serial2.println(msg);
        Serial.print(">>> 转发STM32(新协议): ");
        Serial.println(msg);
        return;
      }
    }
  }

  Serial.println("未识别的下行消息，已忽略");
}

// ====== WiFi连接 ======
void setup_wifi() {
  Serial.println("连接WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry++;
    if (retry > 20) {
      Serial.println("\nWiFi连接失败");
      return;
    }
  }
  Serial.println("\nWiFi已连接");
  Serial.println(WiFi.localIP());
}

// ====== MQTT重连（含WiFi检测）======
void reconnect() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi已断开，重新连接...");
      setup_wifi();
    }
    Serial.print("连接MQTT...");
    if (client.connect(MQTT_CLIENT_ID)) {
      Serial.println("成功");
      client.subscribe(MQTT_TOPIC);
      Serial.print("已订阅主题: ");
      Serial.println(MQTT_TOPIC);
    } else {
      Serial.print("失败 rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

// ====== 初始化 ======
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX2=16, TX2=17

  delay(500);
  Serial.println("ESP32启动");

  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setBufferSize(1024);  // 新JSON约350字节，默认128不够
}

// ====== 主循环 ======
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ====== 串口接收（来自 STM32）======
  while (Serial2.available()) {
    char c = (char)Serial2.read();

    if (c == '\n') {
      serialBuffer.trim();

      if (serialBuffer.length() > 0) {
        Serial.println("串口数据: " + serialBuffer);

        // 缓存完整 JSON（直接转发，不解析重建）
        stm32_last_json = serialBuffer;
        stm32_json_updated = true;

        // 轻量解析关键字段用于日志和缓存
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, serialBuffer);
        if (!error) {
          JsonObject sen = doc["sen"];
          if (sen) {
            lv_cache = doc["lv"]["link"] | 0;
            bp_cache = doc["res"]["bp"] | -1;
            wp_cache = doc["res"]["wp"] | -1;
            Serial.printf("T:%d H:%d PM:%d AQ:%d F:%.2f I:%d BP:%d WP:%d LV:%d\n",
              sen["t"].as<int>(), sen["h"].as<int>(),
              sen["pm"].as<int>(), sen["aq"].as<int>(),
              sen["f"].as<float>(), sen["i"].as<int>(),
              bp_cache, wp_cache, lv_cache);
          }
        }
      }

      serialBuffer = "";
    } else {
      serialBuffer += c;
      if (serialBuffer.length() > 600) {
        serialBuffer = "";
        Serial.println("串口缓冲溢出，丢弃当前帧");
      }
    }
  }

  // ====== 定时上传（直接转发 STM32 完整 JSON）======
  if (millis() - lastUploadTime > UPLOAD_INTERVAL && stm32_json_updated) {
    lastUploadTime = millis();
    stm32_json_updated = false;

    if (stm32_last_json.length() > 0) {
      if (client.publish(MQTT_TOPIC, stm32_last_json.c_str())) {
        Serial.printf("上传OK(%d字节): %s\n", stm32_last_json.length(), stm32_last_json.c_str());
      } else {
        Serial.printf("上传失败(%d字节, MQTT状态:%d)\n", stm32_last_json.length(), client.state());
        stm32_json_updated = true; // 重试
      }
    }
  }
}