# SmartShelter-Utility-Monitor

> 灾后临时安置点智慧水电管理与环境监测系统
>
> Smart Water-Electricity Management & Environmental Monitoring System for Post-Disaster Temporary Shelters

[![Platform](https://img.shields.io/badge/MCU-STM32F103C8T6-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html)
[![ESP32](https://img.shields.io/badge/WiFi-ESP32-green)](https://www.espressif.com/en/products/socs/esp32)
[![Qt](https://img.shields.io/badge/Qt-6.11-brightgreen)](https://www.qt.io/)
[![MQTT](https://img.shields.io/badge/protocol-MQTT-orange)](https://mqtt.org/)

## 📖 项目简介

本系统面向灾后临时安置场景，基于 STM32F103C8T6 微控制器，集成 6 类环境传感器与 4 类执行器，通过 ESP32 WiFi 模块经 MQTT 协议上云，配套 Qt 桌面管理端与 Web 看板，实现对安置点水电资源与环境参数的**实时监测、三级联动预警、远程控制**。

### 核心特性

- 🔌 **6路传感器采集** — 温湿度(DHT11)、空气质量(MQ135)、PM2.5(GP2Y10)、电流(ACS712)、水流(YF-S401)
- 🚨 **三级联动告警** — 正常🟢 → 预警🟡 → 告警🔴，自动驱动风扇/蜂鸣器/舵机/LED
- 💧⚡ **水电剩余模拟** — 积分衰减算法估算剩余电量/水量，支持远程校准
- 🌐 **MQTT 云通信** — STM32 ↔ ESP32 UART ↔ WiFi ↔ 巴法云 ↔ Qt/Web 双端
- 🖥️ **双端界面** — Qt 桌面端（工作人员：详细数据+历史曲线+远程控制）+ Web 看板（灾民：大字直观展示）

---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    STM32F103C8T6 (面包板)                     │
│  ┌──────────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────────┐  │
│  │ DHT11    │  │MQ135 │  │GP2Y10│  │ACS712│  │ YF-S401  │  │
│  │ 温湿度   │  │空气质量│  │PM2.5 │  │ 电流 │  │ 水流     │  │
│  └────┬─────┘  └──┬───┘  └──┬───┘  └──┬───┘  └────┬─────┘  │
│       └───────────┴─────────┴─────────┴───────────┘          │
│                          │ UART(115200 8N1)                  │
│                    ┌─────┴─────┐                             │
│                    │   ESP32   │                             │
│                    │ WiFi/MQTT │                             │
│                    └─────┬─────┘                             │
└──────────────────────────┼──────────────────────────────────┘
                           │ MQTT (bemfa.com:9501)
              ┌────────────┼────────────┐
              ▼            │            ▼
      ┌──────────┐         │    ┌──────────────┐
      │ Qt 桌面端│◄────────┘    │  Web 看板     │
      │(工作人员)│              │  (灾民)       │
      └──────────┘              └──────────────┘
```

---

## 📂 目录结构

```
SmartShelter-Utility-Monitor/
├── FinalSystem/              # STM32 固件 (Keil MDK v5)
│   ├── Hardware/             #   外设驱动 (DHT11/MQ135/GP2Y10/ACS712/水流/OLED/舵机/蜂鸣器/LED)
│   ├── User/                 #   main.c — 主循环、传感器采集、阈值判断
│   ├── System/               #   延时、ADC扩展
│   ├── Start/                #   启动文件
│   └── Library/              #   STM32 标准外设库
├── esp32/                    # ESP32 固件 (Arduino)
│   └── stm32_to_esp32_uart_test/
│       └── *.ino             #   UART↔MQTT 桥接程序
├── dashboard/                # Web 看板 (HTML/CSS/JS)
│   ├── index.html            #   主页面
│   ├── js/
│   │   ├── app.js            #   数据渲染、图表、告警弹窗
│   │   ├── config.js         #   MQTT 连接配置
│   │   └── history_data.js   #   历史数据查询
│   └── css/
│       └── dashboard.css     #   样式
└── System_UI/                # Qt 桌面端 (Qt 6.11 + CMake)
    ├── mainwindow.cpp/h      #   主窗口
    ├── mainwindow_pages.cpp  #   各页面实现
    ├── mainwindow_utils.cpp  #   工具函数
    ├── mqtt.cpp/h            #   MQTT 客户端
    ├── databasemanager.cpp/h #   SQLite 数据管理
    ├── authservice.cpp/h     #   用户认证
    └── ...
```

---

## 🔧 硬件组成

### MCU 与通信

| 模块 | 型号 | 说明 |
|------|------|------|
| 主控 | STM32F103C8T6 | ARM Cortex-M3, 64KB Flash, 20KB RAM |
| WiFi | ESP32-WROOM | 双核 240MHz, WiFi 802.11 b/g/n |

### 传感器

| 模块 | 接口 | 测量范围 | 精度 |
|------|------|----------|------|
| DHT11 | GPIO (PB13) | 温度 0~50°C, 湿度 20~90%RH | ±2°C / ±5%RH |
| MQ135 | ADC1_IN1 (PA1) | 空气质量 (NH3/苯/烟雾) | — |
| GP2Y10 | ADC1_IN2 (PA2) | PM2.5 粉尘 0~500 µg/m³ | — |
| ACS712-30A | ADC1_IN0 (PA0) | 电流 0~30A | 66mV/A |
| YF-S401 | EXTI5 (PA5) | 水流 0.3~6 L/min | 98脉冲/L |

### 执行器

| 模块 | 接口 | 功能 |
|------|------|------|
| SG90 舵机 | TIM3_CH1 (PA6) | 模拟窗户开/关 (0°~90°) |
| 风扇 | GPIO (PB6) | 通风散热 |
| 蜂鸣器 | GPIO (PB14) | 声音报警 |
| LED ×3 | PA15/PA11/PA12 | 红/绿/黄 状态指示 |

### 显示

| 模块 | 接口 | 说明 |
|------|------|------|
| SSD1306 OLED 0.96" | I²C (PB8/PB9) | 双页：传感器值 / 水电剩余 |

---

## ⚙️ 三级联动告警

系统取 6 路传感器中**最差级别**作为整体状态，自动控制执行器：

| 级别 | LED | 风扇 | 蜂鸣器 | 舵机(窗户) | 触发条件示例 |
|------|-----|------|--------|------------|------------|
| 🟢 正常(0) | 绿灯 | 停 | 停 | 关 0° | 所有参数正常 |
| 🟡 预警(1) | 黄灯 | 转 | 慢响(2s/滴) | 关 0° | 温度 >32°C / PM2.5 >75 |
| 🔴 警报(2) | 红灯 | 转 | 常响 | 开 90° | 温度 >38°C / PM2.5 >150 |

> 所有阈值可通过远程命令动态修改，支持手动/自动模式切换。

---

## 📡 通信协议

### 上行 — STM32 → ESP32 → 云端 (1s周期)

```json
{
  "t": 26,      // 温度 (°C)
  "h": 60,      // 湿度 (%)
  "pm": 35,     // PM2.5 (µg/m³)
  "aq": 52,     // 空气质量 (ppm)
  "f": 0.00,    // 瞬时水流 (L/min)
  "i": 0,       // 瞬时电流 (mA)
  "bp": 85,     // 剩余电量 (%)
  "wp": 72,     // 剩余水量 (%)
  "lv": 0       // 联动级别 (0=正常 1=预警 2=告警)
}
```

### 下行 — 云端 → ESP32 → STM32

| 类型 | 格式 | 示例 |
|------|------|------|
| 控制指令 | JSON `{"code": N}` | `{"code": 4}` 风扇开 |
| 阈值修改 | `{"th": {"tr": 35}}` | 设置温度预警上限为35°C |
| 水电校准 | `bc10000` / `wc2000` / `bp80` / `wp50` | 电池容量/水量/百分比校准 |

---

## 🚀 快速开始

### 前置要求

- **Keil MDK v5** — 编译 STM32 固件
- **Arduino IDE** — 烧录 ESP32 固件
- **Qt 6.11+** — 编译桌面端 (MinGW 64-bit)
- **巴法云账号** — MQTT 云服务 (bemfa.com)

### 1. 配置凭据

修改以下文件中的占位符为你的实际值：

**ESP32** (`esp32/stm32_to_esp32_uart_test/*.ino`):
```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";        // ← 改为你的 WiFi 名
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";     // ← 改为你的 WiFi 密码
const char* MQTT_CLIENT_ID = "YOUR_BEMFA_CLIENT_ID";  // ← 改为巴法云私钥
```

**Web 看板** (`dashboard/js/config.js`):
```js
clientId: 'YOUR_BEMFA_CLIENT_ID',  // ← 改为巴法云私钥
```

**Qt 桌面端** (`System_UI/mainwindow.cpp`):
```cpp
const QString key = QStringLiteral("YOUR_BEMFA_CLIENT_ID");  // ← 改为巴法云私钥
```

### 2. 编译与烧录

```bash
# STM32 — 用 Keil MDK 打开 FinalSystem/Project.uvprojx，编译并烧录
# ESP32 — 用 Arduino IDE 打开 esp32 目录下 .ino 文件，选择 ESP32 Dev Module，上传
```

### 3. 启动 Qt 桌面端

```bash
cd System_UI/build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug
cmake --build . -j8
./System_UI
```

### 4. 部署 Web 看板

将 `dashboard/` 目录部署到任意静态服务器，或直接用浏览器打开 `index.html`。

---

## 🎯 演示场景

系统设计了完整的水电演示流程：

1. **正常状态** — 绿灯亮，OLED 循环显示传感器值
2. **预警触发** — 温度超32°C → 黄灯 + 风扇转 + 蜂鸣器间歇响
3. **告警触发** — PM2.5超150 → 红灯 + 常响 + 舵机开窗90°
4. **远程干预** — Qt/Web 下发指令，强制开关执行器
5. **水电消耗** — 积分算法估算剩余，Web/Qt 大字进度条展示

---

## 📝 开发说明

- **编译器**: Keil MDK v5 (ARMCC v5.06) — 注意中文字符在 `/* */` 注释中可能导致编译错误，统一使用 `//` 行注释
- **ADC**: C8T6 仅有一个 ADC1，多通道需在每次读取前显式 `ADC_RegularChannelConfig()` 切换
- **PA15**: 默认用作 JTDI，需 `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` 才能正常驱动 LED
- **ACS712 空载漂移**: 600次采样覆盖2个50Hz工频周期，DC blocker 基线追踪(α=0.05)，15mA死区+3秒归零

---

## 📄 License

MIT License — 详见 [LICENSE](LICENSE)

---

*🏫 应急管理大学 · 物联网工程 · 2026 届本科毕业设计*
