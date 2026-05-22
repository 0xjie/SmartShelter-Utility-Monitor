# Qt桌面端技术文档

## 概述

Qt桌面端是面向安置点工作人员的专业管理界面，基于 **Qt 6** 框架开发。提供实时监控、历史数据查询、报警管理、远程设备控制和系统设置等完整功能。采用**深色主题（暗蓝色调）**的无边框窗口设计，视觉效果专业。

### 基本信息

| 项目 | 说明 |
|------|------|
| 目标用户 | 安置点工作人员 |
| 开发框架 | Qt 6 (C++17) |
| 构建系统 | CMake |
| 数据库 | SQLite (QSQLITE驱动) |
| 通信协议 | MQTT (巴法云 bemfa.com:9501) |
| 编码风格 | 深色主题、无边框窗口、自定义标题栏 |

---

## 1. 文件结构

```
D:\AAA\System_UI\
├── main.cpp                     # 应用入口
├── mainwindow.h                 # 主窗口头文件（~340行）
├── mainwindow.cpp               # 主窗口实现（~4460行）
├── mainwindow.ui                # UI布局模板
├── mqtt.cpp / mqtt.h            # MQTT客户端封装（~90行）
├── databasemanager.cpp / .h     # SQLite数据库管理（~590行）
├── dbconfig.h                   # 数据库路径配置
├── authservice.cpp / .h         # 用户认证服务（~200行）
├── logindialog.cpp / .h         # 登录对话框（~310行）
├── registerdialog.cpp / .h      # 注册对话框（~380行）
├── securestore.cpp / .h         # 安全存储（~90行）
├── sensordata.h                 # 传感器数据结构
├── authdialog_styles.h          # 登录/注册对话框样式
└── style.qss                    # 全局样式表
```

---

## 2. 技术栈详情

| 技术 | 用途 |
|------|------|
| Qt 6 Core/Gui/Widgets | 基础框架 |
| QtCharts | 实时趋势图和历史折线图 |
| QMqttClient (Qt MQTT模块) | MQTT通信 |
| Qt SQL (QSQLITE驱动) | SQLite数据库操作 |
| QPainter | 自定义控件绘制（BatteryGauge/TankGauge） |
| QSettings | 记住密码配置 |
| QProcess + PowerShell | Excel导出（OOXML压缩打包） |
| QTemporaryDir | 临时文件管理 |

---

## 3. 架构设计

### 3.1 整体架构：Model-View-Controller

```
┌──────────────────────────────────────────────────────┐
│                     View (UI层)                        │
│  6个功能页面 + 自定义绘制控件 + 登录/注册对话框          │
├──────────────────────────────────────────────────────┤
│                  Controller (信号-槽机制)               │
│  MQTT消息解析 → 数据更新 → 数据库写入 → UI刷新          │
├──────────────────────────────────────────────────────┤
│                    Model (数据层)                       │
│  SensorData + DatabaseManager (SQLite CRUD)           │
│  + AuthService (用户认证) + SecureStore (安全存储)      │
└──────────────────────────────────────────────────────┘
```

### 3.2 应用启动流程

```
main()
  ├── QApplication初始化 + 加载style.qss
  ├── 创建AuthService（初始化数据库+默认用户）
  ├── 显示LoginDialog（无边框+背景图+拖拽）
  ├── 登录成功 → 创建MainWindow(userName, userRole)
  │   ├── initUi()              初始化界面（6页面+导航+标题栏）
  │   ├── initConnections()     信号槽连接
  │   ├── initMqtt()            MQTT连接（bemfa.com:9501）
  │   ├── updateTopBarTime()    启动顶部时钟
  │   ├── refreshHistoryPage()  加载历史数据
  │   └── refreshDeviceTable()  加载设备列表
  ├── MainWindow生命周期
  │   ├── switchAccountRequested → 销毁MainWindow → 重新显示登录
  │   └── 窗口关闭 → app.quit()
  └── app.exec() 事件循环
```

### 3.3 窗口特性

- **无边框窗口**：`setWindowFlags(Qt::Window | Qt::FramelessWindowHint)`
- **自定义标题栏**：三按钮（最小化`-`/最大化`[]`/关闭`X`）+ 系统标题 + 实时时钟
- **标题栏三区布局**：左侧占位(220px)、中间标题+时钟、右侧窗口按钮(220px)
- **拖拽移动**：标题栏区域安装eventFilter，鼠标按下记录偏移，移动时更新窗口位置
- **双击标题栏**：切换最大化/还原状态
- **窗口图标**：QPainter自绘盾牌+雷达波纹+医疗十字图标

---

## 4. 用户认证系统

### 4.1 AuthService

- **数据库**：与主数据库共用 `D:/System_UI_Data/system_ui.sqlite`
- **表结构**：`users(id, username UNIQUE, password, role, created_at)`
- **连接管理**：独立连接名（UUID），避免与主数据库连接冲突

### 4.2 默认账号

| 账号 | 密码 | 角色 | 权限 |
|------|------|------|------|
| admin | Admin@123 | admin（管理员） | 全部权限：设备配置+远程控制+用户管理 |
| test | Test@123 | user（普通用户） | 仅数据查看和基本操作 |

### 4.3 登录对话框 (LoginDialog)

- **UI**：无边框+背景图(ISP0Y.jpg)、账号密码输入、记住密码复选框、密码显隐切换
- **拖拽**：鼠标按下/移动/释放事件实现窗口拖拽
- **记住密码**：通过SecureStore加密存储到QSettings
- **验证**：`AuthService::login()` → SQL查询密码比对
- **成功后**：保存账号+角色到成员变量，调用`accept()`关闭对话框

### 4.4 注册对话框 (RegisterDialog)

- **UI**：无边框+背景图、用户名/密码/确认密码三输入框
- **实时验证**：
  - 用户名：`onUsernameChanged()`实时检测4-20位字母数字下划线，检查是否已存在
  - 确认密码：`onConfirmChanged()`实时检测是否与密码一致
- **提交**：`validateAll()`全量验证 → `AuthService::registerUser()`

### 4.5 SecureStore

- **加密方案**：XOR + Base64编码
- **密钥**：`QSysInfo::machineUniqueId()` 的 SHA-256 哈希（绑定本机硬件）
- **存储位置**：QSettings（`CleanAuthDemo/SystemUI`）
- **容错**：解密失败时清除损坏数据，避免UI显示乱码

---

## 5. 六大功能页面

### 5.1 实时监控首页 (Dashboard)

**QStackedWidget索引**：0，对应 `pageDashboard`

#### 5.1.1 页面标题区
- 大标题"实时监控"(24px蓝色) + 副标题(13px)
- 系统运行状态标签：正常(绿色)/预警中(黄色)/告警关注中(红色)/离线(黄色)

#### 5.1.2 传感器卡片矩阵（3×2）
六个传感器卡片，每个包含：
- 传感器名称标签(13px)
- 大字体数值(28px) — 颜色根据级别自动变化：绿(正常)/黄(预警)/红(告警)
- 圆形状态指示灯(12px)
- 文字状态标签(12px)：正常/预警/告警

传感器排列（3列×2行）：
| 温度 | 湿度 | 水流 |
|------|------|------|
| PM2.5 | 空气指数 | 电流 |

级别数据来源：STM32上报的 `lv.d` 数组 `[tempLv, humiLv, pmLv, airLv, curLv, flwLv]`，Qt端不再本地计算。

#### 5.1.3 三线趋势图 (QtCharts)
- 三条折线：温度(红色)/湿度(蓝色)/PM2.5(黄色)
- X轴：QDateTimeAxis（格式 HH:mm:ss），自动滚动最近20个数据点
- Y轴：QValueAxis（范围0-100）
- 图表背景：深蓝半透明，无图例（上方有自定义色点图例行）

#### 5.1.4 累计用量卡片
三列布局：
- 系统累计能耗 (kWh)
- 累计电流 (Ah)
- 总流量 (L)

积分算法：`累积值 += 瞬时值 × Δt(秒) / 3600`，假定母线电压220V。

#### 5.1.5 模拟数据模式
当MQTT断开时（`m_useMqttRealtime == false`），`onUpdateDashboardData()` 每1秒生成随机模拟数据：
```cpp
temp = 23.0 + random(0~13)      // 温度 23~36℃
hum = 42.0 + random(0~30)       // 湿度 42~72%
current = 180 + random(0~420)   // 电流 180~600mA
flow = random(0~6.5)            // 水流 0~6.5 L/min
pm25 = 20 + random(0~110)       // PM2.5 20~130 μg/m³
```

### 5.2 水电管理页面 (WaterPower)

**QStackedWidget索引**：1，对应 `pageRemote`

#### 5.2.1 水电剩余资源卡片
左右两栏布局（中间竖线分隔）：

**左栏 - 电量：**
- BatteryGauge自定义控件（76×160px）
  - 电池外壳+正极触点
  - 填充高度随百分比变化
  - 颜色分段：绿(>60%) / 黄(>30%) / 红(≤30%)
  - 中心显示百分比数字
- 信息标签：剩余mAh / 已用mAh / 容量mAh / 预估可用时间(分/时/天)

**右栏 - 水量：**
- TankGauge自定义控件（76×160px）
  - 梯形水箱形状（上宽下窄）
  - 填充高度+颜色分段同电池
  - 中心显示百分比数字
- 信息标签：剩余L / 已用L / 容量L / 预估可用时间

#### 5.2.2 实时负载卡片
左右两栏：

**左 - 实时负载大字：**
- 42px大字显示当前电流(A)
- 颜色根据STM32的powerStatus(ps)变化：绿(正常)/黄(偏高)/红(过载)
- 负载状态文字：无负载/负载正常/负载偏高/负载过载

**右 - 今日已用：**
- 今日用电 (Ah) — 黄色18px
- 今日用水 (L) — 蓝色18px

#### 5.2.3 水电趋势图 (双Y轴)
- **电流折线**：红色，左Y轴(0-15A)
- **水流折线**：绿色，右Y轴(0-10 L/min)
- **X轴**：QDateTimeAxis（格式 HH:mm:ss），最近60点
- 图表有图例，显示在右上角

### 5.3 历史数据页面

**QStackedWidget索引**：2，对应 `pageEnvironment`

#### 5.3.1 工具栏
**第一行：** 起始QDateTimeEdit ~ 结束QDateTimeEdit + 查询按钮 + 导出Excel按钮
- 起始默认：当前时间-1天
- 结束默认：当前时间
- 查询按钮触发 `refreshHistoryPage()`

**第二行：** 6个传感器QCheckBox（温度/湿度/PM2.5/空气指数/电流/水流），默认全部勾选

#### 5.3.2 动态折线图网格
6个预创建的QChartView（每个用QAreaSeries填充面积），根据选中传感器数量动态排列：

| 选中数 | 布局 |
|--------|------|
| 1 | 1×1 |
| 2 | 2×1 |
| 3-4 | 2×2 |
| 5-6 | 3×2 |

每个图表特性：
- X轴：QDateTimeAxis，自适应格式（跨度>1天显示MM-dd，否则HH:mm），4个刻度
- Y轴：QValueAxis，自动缩放（数据范围+15%边距）
- 曲线：5px粗线+半透明填充面积，颜色各异
- **hover悬停**：线条加粗至8px（eventFilter + Enter/Leave事件）
- **鼠标追踪**：enabled

#### 5.3.3 统计信息区
- **统计信息卡片**：左列（温度/湿度/空气指数均值）+ 右列（水流/电流/PM2.5均值）
- **趋势摘要卡片**：时间范围 + 记录总数 + 说明文字

#### 5.3.4 Excel导出
点击"导出 Excel"按钮 → 选择保存路径 → `exportHistoryAsXlsx()`：

**技术方案**：手动构建OOXML格式XML文件 → QTemporaryDir → PowerShell `Compress-Archive` → 重命名为.xlsx

**XLSX内容结构**：
1. 标题行：系统名+报表名（合并单元格A1:E1，深蓝背景白色大字）
2. 元数据：导出时间、时间范围
3. 统计信息：各传感器均值
4. 趋势摘要
5. 数据表：序号/时间点/温度(℃)/湿度(%)/电流(A)

**样式**：自定义字体(Microsoft YaHei)、填充色、边框、合并单元格、列宽自适应

### 5.4 报警管理页面

**QStackedWidget索引**：3，对应 `pageAlarm`

#### 5.4.1 统计概览卡片
5个彩色计数器（横排）：
- 今日报警（白色）
- 严重（红色 #ef4444）
- 预警（黄色 #f59e0b）
- 求助（黄色/紫色）
- 已解决（绿色 #22c55e）

统计范围：仅限今日（alarmTime以当天日期开头）

#### 5.4.2 筛选器
QComboBox下拉选择：全部 / 严重 / 预警 / 求助

#### 5.4.3 报警表格（7列）

| 列名 | 说明 |
|------|------|
| 起始时间 | alarm_time |
| 结束时间 | end_time（未结束显示"—"） |
| 传感器 | sensor_name |
| 内容 | alarm_content |
| 级别 | 严重(红)/预警(黄)/求助(紫) |
| 状态 | 报警中(红)/求助中(紫)/已解决(绿) |
| 操作 | 求助类→"解决"按钮 / 传感器类→"自动"标签 / 已解决→"✓" |

#### 5.4.4 告警生命周期

```
STM32上报alm数组 → MainWindow对比prevAlmCodes
  ├── 新增报警码 → addAlarmRecord() → 写入alarm_info表(active)
  │   └── 弹窗通知（30秒防抖）
  ├── 消失的报警码 → updateAlarmResolved() → 标记resolved + 记录end_time
  └── 手动处理 → markAlarmHandled() → status='resolved'
```

**告警码→中文映射表**（20+条）：

| 码 | 含义 |
|----|------|
| 101/102 | 温度偏高/过高告警 |
| 103/104 | 温度偏低/过低告警 |
| 111/112 | 湿度偏高/过高告警 |
| 113/114 | 湿度偏低/过低告警 |
| 121/122 | PM2.5偏高/超标告警 |
| 131/132 | 空气质量下降/恶化告警 |
| 141/142 | 电流偏高/过载告警 |
| 151/152 | 水流偏高/异常告警 |
| 200 | 温度+PM2.5组合告警（火灾风险） |
| 201 | PM2.5+AQ组合告警（烟雾污染） |
| 202 | 水流+电流组合告警（管道异常） |
| 901-905 | 各传感器离线告警 |

### 5.5 设备管理页面

**QStackedWidget索引**：4，对应 `pageWaterPower`

#### 5.5.1 阈值远程设置卡片

**设计理念**：Qt只配置告警值，STM32自动推导预警值
- 预警=告警-6℃(温度) / -15%(湿度) / div2(PM/AQ/水流) / ×2div3(电流)

**8个阈值QSpinBox**（2列×4行网格）：

| 键 | 标签 | 默认值 | 单位 |
|----|------|--------|------|
| ta | 温度告警上限 | 38 | ℃ |
| tb | 温度告警下限 | 10 | ℃ |
| ha | 湿度告警上限 | 85 | % |
| hb | 湿度告警下限 | 20 | % |
| pa | PM2.5告警 | 150 | μg/m³ |
| aa | AQ告警 | 200 | |
| ca | 电流告警 | 15 | A |
| fa | 水流告警 | 10 | L/min |

SpinBox样式：深蓝背景+蓝色边框+居中对齐，0~5000范围

**按钮：**
- **"恢复默认"**：重置8个SpinBox为默认值 + 发送 `{"cmd":"reset_th"}` 到STM32
- **"一键下发"**：确认后构建 `{"th":{"ta":38,...}}` → 通过MQTT发布到test001up

#### 5.5.2 设备控制卡片

4行按钮对，每行包含标签 + ON按钮 + OFF按钮：

| 标签 | ON | OFF | 码值 |
|------|-----|-----|------|
| 警报 | 开启(1) | 关闭(0) | 蜂鸣器 |
| 风扇 | 开启(4) | 关闭(5) | |
| 窗户 | 打开(2) | 关闭(3) | 舵机 |
| 警报灯 | 开启(6) | 关闭(7) | LED |

- ON按钮按下时高亮（深蓝边框+背景）
- 每次点击通过MQTT发送 `{"code":N,"name":"..."}` 到test001up
- 同时记录远程操作日志

#### 5.5.3 远程控制日志

**跑马灯样式QPlainTextEdit**：
- 只读、等宽字体(TypeWriter)
- 自动滚动：QTimer每150ms触发 `scrollBar->setValue(value+1)`，回绕到顶部
- **悬停减速**：eventFilter检测Enter/Leave事件 → 间隔150ms变为300ms
- 固定表头显示列名：时间 | 设备 | 指令 | 详情
- 显示最近20条：`queryRemoteExecLogs(20)`

日志格式（等宽对齐）：
```
2026-05-19 14:30:22  | SYSTEM     | 风扇开启           | 已发送到 test001up
```

### 5.6 系统设置页面

**QStackedWidget索引**：5，对应 `pageSetting`

#### 5.6.1 账号信息卡片
网格布局显示：
- 当前账号 / 角色(管理员/普通用户) / 登录时间

两个按钮：
- **"修改信息"**：弹出对话框 → 修改用户名和密码 → `updateCurrentUserBasicInfo()`
- **"切换账号"**：确认 → `emit switchAccountRequested()` → 关闭MainWindow → 回到登录界面

修改密码流程：
1. `loadCurrentUserBasicInfo()` 读取当前密码
2. 弹出对话框输入新用户名/密码（留空保留原值）
3. 检查用户名是否已被其他用户占用
4. `UPDATE users SET username=?, password=?`

#### 5.6.2 系统状态卡片（双列布局）

**左 - MQTT连接：**
- 状态：已连接(绿)/未连接(红)
- Broker：bemfa.com:9501
- Topic：test001up / WebQT1

**右 - 数据库信息：**
- 路径：D:\System_UI_Data\system_ui.sqlite
- 大小：xx KB
- 数据库：已连接/未连接
- 最后更新时间

#### 5.6.3 系统日志
QPlainTextEdit只读区域，显示登录时间戳。

---

## 6. MQTT通信模块

### 6.1 Mqtt类封装

```cpp
class Mqtt : public QObject {
    QMqttClient* m_client;
    QString m_host, m_key;
    quint16 m_port;
    QStringList m_topics;
    QTimer* m_reconnectTimer;
    int m_reconnectDelaySec = 3;    // 初始重连间隔3秒
};
```

### 6.2 连接参数

| 参数 | 值 |
|------|-----|
| Broker | bemfa.com |
| 端口 | 9501 |
| Client ID | 6525cbc01d2d408eb1b28ca77a134ebc（巴法云设备私钥） |
| 用户名/密码 | 空 |
| KeepAlive | 30秒 |

### 6.3 订阅主题

| 主题 | 用途 |
|------|------|
| test001up | STM32传感器数据上行 + 控制指令下行（双向） |
| WebQT1 | Web看板一键求助通知 |

### 6.4 自动重连机制

- **指数退避**：起始3秒，每次翻倍，上限60秒
- Disconnected状态 → `m_reconnectTimer->start(delay * 1000)` → `tryReconnect()`
- Connected后 → 重置delay为3秒 + 重新订阅所有已记录主题

### 6.5 数据协议处理

MainWindow中 `textMessageReceived` 信号处理三种JSON格式：

#### 新协议（当前使用）
```json
{
  "sen": {"t": 26, "h": 60, "pm": 35, "aq": 52, "f": 0.00, "i": 0},
  "res": {"bp": 85, "wp": 72, "tu": 0, "wu": 0, "br": 0, "wr": 0,
          "ps": 0, "bc": 10000, "tc": 1000, "bt": 0, "wt": 0},
  "lv": {"link": 0, "d": [0,0,0,0,0,0]},
  "alm": [{"c": 101, "lv": 1}],
  "actn": 0
}
```
- `sen`：传感器原始值（t/h/pm/aq/f/i）
- `res`：水电资源数据（bp/wp=百分比，tu/wu=已用，br/wr=剩余，ps=负载状态，bc/tc=容量，bt/wt=预估剩余分钟）
- `lv.d`：6传感器级别数组 [tempLv, humiLv, pmLv, airLv, curLv, flwLv]
- `alm`：报警码数组（int array 或 object array）
- `actn`：执行器位掩码（0x01蜂鸣器开/0x02关/0x04风扇开/0x08关/0x10窗户开/0x20关）

#### 旧协议（兼容）
```json
{"t":26,"h":60,"p":35,"a":52,"f":0.00,"i":0,"bp":85,"wp":72,"lv":0}
```

#### 云平台旧格式（兼容）
```json
{"params":{"DHT11_T":{"value":26},"DHT11_H":{"value":60},...}}
```

### 6.6 下行协议（Qt→MQTT→ESP32→STM32）

**控制指令**：
```json
{"code":4,"name":"风扇开启"}
```
code值：0蜂关/1蜂开/2窗开/3窗关/4扇开/5扇关/6灯开/7灯关

**阈值下发**：
```json
{"th":{"ta":38,"tb":10,"ha":85,"hb":20,"pa":150,"aa":200,"ca":15,"fa":10}}
```

**恢复默认**：
```json
{"cmd":"reset_th"}
```

### 6.7 Web求助处理

监听 `WebQT1` 主题，解析求助JSON：
```json
{"type":"medical","label":"有人发烧需要医疗","site":"A区2号帐篷","time":"14:30:00"}
```
- 写入alarm_info表（级别=求助）
- 红白紧急风格弹窗通知工作人员（`WindowStaysOnTopHint`）

求助类型映射：
| type | 中文 |
|------|------|
| medical | 身体不适 |
| water | 生活缺水 |
| power | 电力故障 |
| other | 其他求助 |

---

## 7. 数据库设计 (SQLite)

### 7.1 数据库位置
- 路径：`D:/System_UI_Data/system_ui.sqlite`
- 自动创建目录：`QDir().mkpath()`
- 连接名：UUID唯一标识，避免多实例冲突

### 7.2 表结构

#### data（历史查询主表）
```sql
CREATE TABLE data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT NOT NULL,
    temperature REAL, humidity REAL, pm25 REAL,
    air_index REAL, current_a REAL, flow_l_min REAL
);
CREATE INDEX idx_data_ts ON data(ts);
```

#### alarm_info（告警记录）
```sql
CREATE TABLE alarm_info (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    alarm_time TEXT NOT NULL,     -- 告警开始时间
    sensor_name TEXT NOT NULL,    -- 传感器名称
    alarm_content TEXT NOT NULL,  -- 告警内容
    end_time TEXT,                -- 告警结束时间
    level TEXT DEFAULT '预警',    -- 严重/预警/求助
    status TEXT DEFAULT 'active', -- active/resolved
    alarm_code INTEGER DEFAULT 0  -- 告警码（101-905）
);
CREATE INDEX idx_alarm_info_time ON alarm_info(alarm_time);
```

#### device_info（设备元数据）
```sql
CREATE TABLE device_info (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL UNIQUE,
    device_name TEXT NOT NULL,
    device_type TEXT NOT NULL,
    location TEXT
);
```

#### device_abnormal_records（设备离线记录）
```sql
CREATE TABLE device_abnormal_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sensor_name TEXT NOT NULL,
    offline_time TEXT NOT NULL,
    offline_duration_sec INTEGER NOT NULL DEFAULT 0,
    UNIQUE(sensor_name, offline_time)
);
```
- 离线判定：最近数据时间 > 8秒 → 设备离线
- 每秒更新离线时长（UPSERT合并）

#### remote_exec_logs（远程控制操作日志）
```sql
CREATE TABLE remote_exec_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    execute_time TEXT NOT NULL,
    device_id TEXT NOT NULL,
    command_text TEXT NOT NULL,
    result_text TEXT
);
CREATE INDEX idx_remote_exec_time ON remote_exec_logs(execute_time);
```

#### users（用户账号，由AuthService管理）
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'user',
    created_at TEXT NOT NULL
);
```

### 7.3 关键操作API

| 方法 | 说明 |
|------|------|
| `openOrCreate()` | 打开/创建数据库+自动建表 |
| `insertSensorSample()` | 写入data表 |
| `queryRecentData(limit, since)` | 查询data表（DESC，最多10000条） |
| `queryAverageSince()` | 聚合查询（COUNT + AVG of 6 fields） |
| `insertAlarmInfo()` | 插入告警（status='active'） |
| `updateAlarmResolved(code, endTime)` | 按alarm_code标记已解决 |
| `markAlarmHandled(id)` | 手动标记已处理（求助类） |
| `upsertDeviceInfo()` | UPSERT设备信息 |
| `upsertDeviceOfflineRecord()` | UPSERT离线记录 |
| `insertRemoteExecLog/queryRemoteExecLogs()` | 远程操作日志 |

### 7.4 数据库连接管理

- 每个DatabaseManager/AuthService实例使用独立的SQLite连接名（UUID）
- 临时查询（如修改密码）创建短生命周期的独立连接，用完立即关闭+移除
- 析构时：`m_db.close()` → `m_db = QSqlDatabase()` → `removeDatabase(connName)`

---

## 8. 数据流与信号槽

### 8.1 MQTT接收数据流

```
MQTT Broker → QMqttClient::messageReceived
  → Mqtt::textMessageReceived(topic, payload)
    → MainWindow lambda [1]: 解析JSON → updateDataWithLevels()
    → MainWindow lambda [2]: 解析WebQT1 → 求助弹窗
```

### 8.2 updateDataWithLevels() 核心流程

```
1. 更新时间戳、计算dtSec（相邻采样间隔）
2. 更新6个传感器卡片数值+级别样式
3. 更新系统状态标签（linkLv）
4. 同步设备状态数组（m_devices）
5. 对比almArr与prevAlmCodes → 检测新增/消失告警
   ├── 新增 → addAlarmRecord() → 弹窗(30秒防抖)
   └── 消失 → updateAlarmResolved() → refreshAlarmInfoFromDatabase()
6. 积分累计量（电流→Ah, 功率→kWh, 水流→L）
7. 写入数据库（data表）
8. 追加水电页折线图数据点（最近60点）
```

### 8.3 updateResourcePct() 水电数据流

```
1. 更新m_batteryPct / m_waterPct
2. 更新实时负载大字+状态（根据powerStatus）
3. 更新今日已用标签（直接显示STM32值）
4. 更新BatteryGauge自定义绘制
5. 更新电池信息标签（剩余/已用/容量/预估）
6. 更新TankGauge自定义绘制
7. 更新水箱信息标签（剩余/已用/容量/预估）
```

### 8.4 设备离线检测

```
updateTopBarTime() 每秒触发:
  lastRealtimeDataAt.secsTo(now) >= 8
    → m_isDeviceOffline = true
    → 状态标签"异常"（黄色）
    → 传感器卡片显示"离线"状态
    → syncDeviceOfflineRecords() 写入离线时长
```

---

## 9. 自定义UI组件

### 9.1 BatteryGauge（电池电量指示器）
- 继承QWidget，固定尺寸76×160px
- QPainter自定义绘制：
  - 顶部正极触点（圆角矩形）
  - 电池外壳（深蓝半透明+浅蓝边框）
  - 填充区域：高度=m_pct/100×bodyH
  - 颜色：绿(>60%) / 黄(>30%) / 红(≤30%)
  - 中心百分比数字

### 9.2 TankGauge（水箱水量指示器）
- 继承QWidget，固定尺寸76×160px
- 梯形水箱形状（上宽下窄，通过QPainterPath实现）
- 顶部盖子（半透明矩形）
- 填充+颜色分段同BatteryGauge
- 中心百分比数字

### 9.3 PanelCard（通用面板卡片）
```cpp
QFrame* createPanelCard(QWidget* parent)
  // 样式：深蓝半透明背景 + 蓝色发光边框 + 18px圆角 + 投影
  // 效果：QGraphicsDropShadowEffect(blur=24, offset(0,8))
```

### 9.4 DeviceHoverCard（悬停放大卡片）
- 继承QFrame，设置 `WA_Hover` 属性
- QVariantAnimation：Enter→放大至108%，Leave→恢复100%
- 动画时长170ms，OutCubic缓动

### 9.5 自定义对话框（无原生边框）
`customConfirm()` / `customMessage()`：
- FramelessWindowHint + Dialog
- 深蓝背景(#0a1628) + 边框(#1e3a5f) + 12px圆角
- 标题栏：深蓝(#0f1f3a) + 圆角顶部
- 关闭按钮：hover变红
- 确认/取消按钮：蓝色/灰色主题

### 9.6 告警弹窗
`showRealtimeAlarmDialog()`：
- 红白紧急风格：白色背景+红色边框+红色标题
- 三角警示图标（QPainter自绘）
- OK按钮：红色hover/pressed变深

### 9.7 导出成功对话框
`showExportSuccessDialog()`：
- 白色背景+绿色勾号图标
- 只读路径输入框（自动全选，方便复制）

---

## 10. 模拟数据与离线模式

### 10.1 实时监控模拟数据
当MQTT未连接时（`m_useMqttRealtime == false`），每秒生成覆盖阈值区间的随机数据：
- 温度 23~36℃，湿度 42~72%
- 电流 180~600mA（覆盖低负载到告警阈值）
- PM2.5 20~130（覆盖正常到接近告警）
- 水流量 0~6.5 L/min

### 10.2 设备管理模拟
`onUpdateWaterPowerData()`每秒随机：
- 随机设备电池-1~2%
- 10%概率切换状态（运行中↔维护中）
- 随机波动读数±6.0
- 超阈值→状态变为"波动"

### 10.3 离线状态处理
- 8秒无MQTT数据→标记离线
- 所有传感器卡片显示"离线"
- 离线记录写入device_abnormal_records（每秒更新时长）
- 恢复后清除离线状态

---

## 11. 其他关键实现

### 11.1 标题栏按钮
- `toggleMaximizedState()`：`isMaximized() ? showNormal() : showMaximized()`
- `updateTitleBarButtons()`：最大化时显示"o"(还原)，否则"[]"(最大化)
- `changeEvent()`监听WindowStateChange更新按钮状态

### 11.2 导航切换
```cpp
onNavCurrentRowChanged(int row):
  0→Dashboard  1→水电  2→历史  3→报警  4→设备  5→设置
```
- 报警页切换时自动刷新数据
- 设置页每次重建（buildSettingsPage()）以显示最新状态

### 11.3 实时负载大字
独立于updateDataWithLevels，在updateResourcePct中更新（使用STM32的powerStatus字段）：
- powerStatus=0（无负载）：灰色"-- mA"
- powerStatus=1（偏高）：黄色
- powerStatus=2+（过载）：红色

### 11.4 传感器级别统一
Qt端不再本地计算阈值判断。所有级别数据从STM32的`lv.d`数组读取：
```cpp
int tempLv = sensorLvs.value(0, 0);   // 温度级别
int humiLv = sensorLvs.value(1, 0);   // 湿度级别
int pmLv   = sensorLvs.value(2, 0);   // PM2.5级别
int airLv  = sensorLvs.value(3, 0);   // 空气质量级别
int curLv  = sensorLvs.value(4, 0);   // 电流级别
int flwLv  = sensorLvs.value(5, 0);   // 水流级别
```

### 11.5 告警弹窗防抖
```cpp
// 仅当有新报警 且 距上次弹窗≥30秒时弹出
if (!newCodes.isEmpty() && (!m_lastLocalAlarmAt.isValid() || m_lastLocalAlarmAt.secsTo(now) >= 30))
```

### 11.6 累计量积分
使用相邻两次MQTT上报的时间间隔Δt计算：
```cpp
m_totalCurrent += iAmpere * (dtSec / 3600.0);  // Ah
m_totalPower += powerKw * (dtSec / 3600.0);     // kWh
m_totalWater += (flow / 60.0) * dtSec;           // L
```
dtSec上限3600秒（防止长时间断连后异常积分）

### 11.7 水电页折线图
在`updateDataWithLevels()`末尾追加数据点（电流A、水流L/min），保持最近60点。

### 11.8 远程日志跑马灯
- QPlainTextEdit无滚动条
- QTimer(150ms)：`scrollBar->setValue(bar->value()+1)`
- 到达底部回绕到顶部
- eventFilter：Enter→150→300ms，Leave→恢复150ms

---

## 附录A：MQTT主题流

```
        STM32 ──UART──> ESP32 ──MQTT──> 巴法云
                                           │
                      ┌────────────────────┤
                      ▼                    ▼
               test001up订阅          WebQT1订阅
                 (Qt桌面端)          (Qt桌面端 + Web看板)
                      │                    │
                      ▼                    ▼
              传感器数据+阈值       Web求助消息
```

## 附录B：用户界面截图要点

- **主题色**：深蓝渐变背景(#071a36 → #0a2e5c → #0f5fa8)
- **卡片**：半透明深蓝(rgba(9,33,71,218)) + 蓝色发光边框 + 18px圆角 + 投影
- **文字**：主要白色(#f8fbff)、蓝色调(#7dd3fc / #bfdcff / #dbeafe)
- **按钮**：蓝色主色调(rgba(18,92,178))、hover变亮、pressed变深
- **表格**：深色背景+交替行色、蓝色表头、网格线半透明

## 附录C：编译与运行

- **构建系统**：CMake (CMakeLists.txt)
- **Qt版本**：Qt 6.x
- **Qt模块依赖**：Core, Gui, Widgets, Charts, Mqtt, Sql
- **数据库目录**：运行时自动创建 `D:\System_UI_Data\`
- **资源文件**：`ISP0Y.jpg`（登录/注册背景图）通过Qt资源系统嵌入
