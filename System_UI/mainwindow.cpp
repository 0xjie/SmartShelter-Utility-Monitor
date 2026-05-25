#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDate>
#include <QDialog>
#include <QDebug>
#include <QPalette>
#include <QDir>
#include <QFont>
#include <QFrame>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProcess>
#include <algorithm>
#include <QApplication>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QIcon>
#include <QSizePolicy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QLineEdit>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTemporaryDir>
#include <QTimer>
#include <QAbstractAnimation>
#include <QEvent>
#include <QEnterEvent>
#include <QEasingCurve>
#include <QUuid>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include <QStandardPaths>
#include <cmath>
#include <utility>

#include "databasemanager.h"
#include "dbconfig.h"
#include "mainwindow_utils.h"

MainWindow::MainWindow(const QString& userName,
                       const QString& userRole,
                       QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_userName(userName),
      m_userRole(userRole),
      m_dataTimer(nullptr),
      m_currentSeries(nullptr),
      m_flowSeries(nullptr),
      m_axisX(nullptr),
      m_axisY_Current(nullptr),
      m_axisY_Flow(nullptr),
      m_historyLineSeries(),
      m_historyLowerSeries(),
      m_historyChartViews(),
      m_historyCharts(),
      m_historyAxisXs(),
      m_historyAxisYs(),
      m_historyChartCards(),
      m_historyPowerStatsLabel(nullptr),
      m_historyWaterStatsLabel(nullptr),
      m_historyRefreshTimer(nullptr),
      m_timeLabel(nullptr),
      m_cardTempValue(nullptr),
      m_cardHumiValue(nullptr),
      m_cardCurrentValue(nullptr),
      m_cardFlowValue(nullptr),
      m_cardAirValue(nullptr),
      m_cardPmValue(nullptr),
      m_dotTemp(nullptr),
      m_dotHumi(nullptr),
      m_dotCurrent(nullptr),
      m_dotFlow(nullptr),
      m_dotAir(nullptr),
      m_dotPm(nullptr),
      m_realtimeStatusLabel(nullptr),
      m_windowIconLabel(nullptr),
      m_minimizeButton(nullptr),
      m_maximizeButton(nullptr),
      m_closeButton(nullptr),
      m_alarmInfoTable(nullptr),

      m_remoteDeviceCombo(nullptr),
      m_remoteCommandCombo(nullptr),
      m_logViewer(nullptr),
      m_loginTime(QDateTime::currentDateTime()) {
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, true);

    initUi();
    initConnections();
    initMqtt();
    updateTopBarTime();
    refreshHistoryPage();

    // Top bar clock and device status refresh timer.
    m_dataTimer = new QTimer(this);
    if (m_dataTimer != nullptr) {
        m_dataTimer->setInterval(1000);
        connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::updateTopBarTime);
        connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::onUpdateDashboardData);
        m_dataTimer->start();
    }

    // Move heavy IO/network init out of constructor so UI can always show first.
    QTimer::singleShot(0, this, [this]() {

        m_db = new DatabaseManager(this);
        QString dbErr;
        if (!m_db->openOrCreate(&dbErr)) {
            qDebug() << "[DB] open failed:" << dbErr;
        } else {
            qDebug() << "[DB] path =" << m_db->databasePath();
            exportDashboardHistoryData();
            refreshWaterPowerUsageSummary();
            loadRemoteExecLogTable();
            refreshAlarmInfoFromDatabase();
        }

    });
}

MainWindow::~MainWindow() {
    delete ui;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // 日志滚动区：悬停减速，移出恢复
    if (watched == m_remoteLogMarqueeView) {
        if (auto* t = m_remoteLogMarqueeView->findChild<QTimer*>(QStringLiteral("logScrollTimer"))) {
            if (event->type() == QEvent::Enter)
                t->setInterval(300);
            else if (event->type() == QEvent::Leave)
                t->setInterval(150);
        }
        return QMainWindow::eventFilter(watched, event);
    }

    // 历史数据图表：悬停加粗曲线
    if (m_historyChartViews.contains(static_cast<QChartView*>(watched))) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            const int idx = m_historyChartViews.indexOf(static_cast<QChartView*>(watched));
            if (idx >= 0 && idx < m_historyLineSeries.size()) {
                QLineSeries* series = m_historyLineSeries[idx];
                if (series != nullptr) {
                    QPen pen = series->pen();
                    pen.setWidthF(event->type() == QEvent::Enter ? 3.5 : 2.2);
                    series->setPen(pen);
                }
            }
            return QMainWindow::eventFilter(watched, event);
        }
    }

    // 仅处理 title bar 拖拽
    if (watched != ui->frameTopBar && watched != ui->labelSystemTitle && watched != m_windowIconLabel) {
        return QMainWindow::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonDblClick: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            toggleMaximizedState();
            return true;
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragOffset = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (m_dragging && (mouseEvent->buttons() & Qt::LeftButton)) {
            if (isMaximized()) {
                toggleMaximizedState();
                m_dragOffset = QPoint(width() / 2, 24);
            }
            move(mouseEvent->globalPosition().toPoint() - m_dragOffset);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_dragging = false;
            return true;
        }
        break;
    }
    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        updateTitleBarButtons();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::initUi() {
    setWindowTitle(QString());
    setWindowIcon(createEmergencyWindowIcon());
    ui->labelSystemTitle->setText("灾后临时安置点智慧管理系统");
    setStyleSheet(
        "QMainWindow{"
        "background-color:#071a36;"
        "}"
        "QWidget#centralwidget{"
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #071a36,stop:0.55 #0a2e5c,stop:1 #0f5fa8);"
        "}"
        "QFrame#frameTopBar{"
        "background:transparent;"
        "border:none;"
        "}"
        "QStackedWidget#stackedWidgetPages,"
        "QStackedWidget#stackedWidgetPages > QWidget{"
        "background:transparent;"
        "color:#e8f1ff;"
        "}"
        "QLabel{"
        "color:#dbeafe;"
        "}"
        "QLineEdit,QComboBox,QSpinBox,QPlainTextEdit{"
        "background:rgba(7, 26, 54, 0.88);"
        "color:#e8f1ff;"
        "border:1px solid rgba(96, 165, 250, 0.45);"
        "border-radius:10px;"
        "padding:6px 10px;"
        "selection-background-color:#2563eb;"
        "}"
        "QPushButton{"
        "background:rgba(18, 92, 178, 0.92);"
        "color:white;"
        "border:1px solid rgba(125, 211, 252, 0.35);"
        "border-radius:10px;"
        "padding:8px 14px;"
        "font-weight:700;"
        "}"
        "QPushButton:hover{background:rgba(26, 120, 226, 0.96);}"
        "QPushButton:pressed{background:rgba(14, 74, 147, 0.96);}"
        "QPushButton#titleBarButton,QPushButton#closeTitleBarButton{"
        "min-width:40px;"
        "max-width:40px;"
        "min-height:32px;"
        "max-height:32px;"
        "padding:0;"
        "border-radius:8px;"
        "background:rgba(255,255,255,0.08);"
        "border:1px solid rgba(125, 211, 252, 0.18);"
        "font-size:14px;"
        "font-weight:700;"
        "}"
        "QPushButton#titleBarButton:hover,QPushButton#closeTitleBarButton:hover{background:rgba(96, 165, 250, 0.28);}"
        "QPushButton#titleBarButton:pressed,QPushButton#closeTitleBarButton:pressed{background:rgba(37, 99, 235, 0.45);}"
        "QPushButton#closeTitleBarButton:hover{background:rgba(255,255,255,0.18);border-color:rgba(248, 113, 113, 0.45);color:#ef4444;}"
        "QPushButton#closeTitleBarButton:pressed{background:rgba(153, 27, 27, 0.95);}"
        "QTableWidget{"
        "background:rgba(7, 26, 54, 0.72);"
        "alternate-background-color:rgba(10, 37, 79, 0.9);"
        "color:#e8f1ff;"
        "gridline-color:rgba(125, 211, 252, 0.12);"
        "border:1px solid rgba(96, 165, 250, 0.30);"
        "border-radius:12px;"
        "selection-background-color:rgba(37, 99, 235, 0.65);"
        "selection-color:white;"
        "}"
        "QHeaderView::section{"
        "background:rgba(15, 95, 168, 0.92);"
        "color:white;"
        "border:none;"
        "padding:8px;"
        "}"
        "QGroupBox{"
        "background:rgba(9, 33, 71, 0.78);"
        "border:1px solid rgba(96, 165, 250, 0.30);"
        "border-radius:12px;"
        "margin-top:10px;"
        "padding-top:10px;"
        "}"
        "QGroupBox::title{"
        "subcontrol-origin: margin;"
        "left:12px;"
        "padding:0 4px;"
        "color:#bfdbfe;"
        "}");
    ui->frameTopBar->setStyleSheet("QFrame#frameTopBar{background:transparent;border:none;}");

    QString roleText = (m_userRole == "admin") ? "管理员" : "普通用户";
    if (m_timeLabel == nullptr) {
        m_timeLabel = new QLabel(this);
        m_timeLabel->setObjectName("labelCurrentTime");
        m_timeLabel->setStyleSheet("QLabel{color:#e9f4ff;font-size:14px;font-weight:600;}");
    }

    ui->labelCurrentUser->hide();

    ui->listWidgetNav->clear();
    ui->listWidgetNav->addItems({"实时监控", "水电数据", "历史数据", "报警管理", "设备管理", "系统设置"});
    ui->listWidgetNav->setMinimumWidth(188);
    ui->listWidgetNav->setMaximumWidth(200);
    ui->listWidgetNav->setSpacing(6);
    ui->listWidgetNav->setStyleSheet(
        "QListWidget{"
        "background:rgba(8, 28, 59, 0.82);"
        "border:1px solid rgba(96, 165, 250, 0.35);"
        "border-radius:16px;"
        "padding:12px 8px;"
        "font-size:15px;"
        "outline:0;"
        "}"
        "QListWidget::item{"
        "height:46px;"
        "margin:2px 4px;"
        "padding-left:16px;"
        "border-radius:10px;"
        "color:#dbeafe;"
        "font-weight:600;"
        "}"
        "QListWidget::item:hover{"
        "background:rgba(37, 99, 235, 0.35);"
        "color:white;"
        "}"
        "QListWidget::item:selected{"
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0f5fa8,stop:1 #28a6ff);"
        "color:white;"
        "}");

    while (QLayoutItem* item = ui->horizontalLayoutTop->takeAt(0)) {
        delete item;
    }

    auto* leftWrap = new QWidget(this);
    leftWrap->setFixedWidth(220);

    auto* centerWrap = new QWidget(this);
    centerWrap->setStyleSheet("QWidget{background:transparent;border:none;}");
    auto* centerLayout = new QHBoxLayout(centerWrap);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    auto* titleRow = new QWidget(centerWrap);
    titleRow->setStyleSheet("QWidget{background:transparent;border:none;}");
    auto* titleRowLayout = new QHBoxLayout(titleRow);
    titleRowLayout->setContentsMargins(0, 0, 0, 0);
    titleRowLayout->setSpacing(12);

    if (m_windowIconLabel == nullptr) {
        m_windowIconLabel = new QLabel(this);
        m_windowIconLabel->setObjectName("windowIconLabel");
        m_windowIconLabel->setFixedSize(28, 28);
    }
    m_windowIconLabel->setPixmap(windowIcon().pixmap(24, 24));
    ui->labelSystemTitle->setAlignment(Qt::AlignCenter);
    ui->labelSystemTitle->setStyleSheet("QLabel{color:white;font-size:22px;font-weight:800;}");
    m_timeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_timeLabel->setStyleSheet("QLabel{color:#dbeafe;font-size:17px;font-weight:800;padding-left:20px;}");

    titleRowLayout->addWidget(m_windowIconLabel, 0, Qt::AlignVCenter);
    titleRowLayout->addWidget(ui->labelSystemTitle, 0, Qt::AlignVCenter);
    titleRowLayout->addWidget(m_timeLabel, 0, Qt::AlignVCenter);
    centerLayout->addWidget(titleRow, 0, Qt::AlignCenter);

    // ===== 右侧区域：窗口按钮 =====
    auto* rightWrap = new QWidget(this);
    rightWrap->setFixedWidth(220);
    auto* rightLayout = new QVBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    auto* rightButtonsWrap = new QWidget(rightWrap);
    rightButtonsWrap->setStyleSheet("QWidget{background:transparent;border:none;}");
    auto* rightButtonsLayout = new QHBoxLayout(rightButtonsWrap);
    rightButtonsLayout->setContentsMargins(0, 0, 0, 0);
    rightButtonsLayout->setSpacing(8);

    if (m_minimizeButton == nullptr) {
        m_minimizeButton = new QPushButton("-", this);
        m_minimizeButton->setObjectName("titleBarButton");
        connect(m_minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    }
    if (m_maximizeButton == nullptr) {
        m_maximizeButton = new QPushButton("[]", this);
        m_maximizeButton->setObjectName("titleBarButton");
        connect(m_maximizeButton, &QPushButton::clicked, this, &MainWindow::toggleMaximizedState);
    }
    if (m_closeButton == nullptr) {
        m_closeButton = new QPushButton("X", this);
        m_closeButton->setObjectName("closeTitleBarButton");
        connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);
    }
    m_minimizeButton->setFixedSize(32, 32);
    m_maximizeButton->setFixedSize(32, 32);
    m_closeButton->setFixedSize(32, 32);
    m_minimizeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_maximizeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    rightButtonsLayout->addWidget(m_minimizeButton);
    rightButtonsLayout->addWidget(m_maximizeButton);
    rightButtonsLayout->addWidget(m_closeButton);
    rightLayout->addWidget(rightButtonsWrap, 0, Qt::AlignRight);

    ui->horizontalLayoutTop->addWidget(leftWrap, 0, Qt::AlignLeft | Qt::AlignVCenter);
    ui->horizontalLayoutTop->addStretch(1);
    ui->horizontalLayoutTop->addWidget(centerWrap, 0, Qt::AlignCenter);
    ui->horizontalLayoutTop->addStretch(1);
    ui->horizontalLayoutTop->addWidget(rightWrap, 0, Qt::AlignRight | Qt::AlignVCenter);

    ui->frameTopBar->installEventFilter(this);
    leftWrap->installEventFilter(this);
    centerWrap->installEventFilter(this);
    m_windowIconLabel->installEventFilter(this);
    ui->labelSystemTitle->installEventFilter(this);

    updateTitleBarButtons();

    buildMainPages();
    ui->listWidgetNav->setCurrentRow(0);
    ui->stackedWidgetPages->setCurrentIndex(0);
}

void MainWindow::initConnections() {
    connect(ui->listWidgetNav, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavCurrentRowChanged);
}

void MainWindow::toggleMaximizedState() {
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
    updateTitleBarButtons();
}

void MainWindow::updateTitleBarButtons() {
    if (m_maximizeButton != nullptr) {
        m_maximizeButton->setText(isMaximized() ? "o" : "[]");
        m_maximizeButton->setToolTip(isMaximized() ? "还原" : "最大化");
    }
    if (m_minimizeButton != nullptr) {
        m_minimizeButton->setToolTip("最小化");
    }
    if (m_closeButton != nullptr) {
        m_closeButton->setToolTip("关闭");
    }
}

void MainWindow::initMqtt() {
    const QString key = QStringLiteral("6525cbc01d2d408eb1b28ca77a134ebc");
    const QString telemetryTopic = QString::fromLatin1(kMqttTelemetryTopic);
    const QString commandTopic = QString::fromLatin1(kMqttCommandTopic);
    const QString helpTopic = QString::fromLatin1(kMqttHelpTopic);

    connect(&m_mqtt, &Mqtt::stateChanged, this, [this, telemetryTopic, commandTopic, helpTopic](int state) {
        if (state == 2) {  // QMqttClient::Connected
            m_useMqttRealtime = true;
            m_mqtt.subscribeTopic(telemetryTopic);
            m_mqtt.subscribeTopic(commandTopic);
            m_mqtt.subscribeTopic(helpTopic);
            if (m_realtimeStatusLabel != nullptr) {
                m_realtimeStatusLabel->setText("系统运行状态：MQTT 已连接");
                m_realtimeStatusLabel->setStyleSheet("QLabel{color:#059669;font-size:14px;font-weight:700;}");
            }
            qDebug() << "MQTT 已订阅主题:" << telemetryTopic << commandTopic << helpTopic;
        } else {
            m_useMqttRealtime = false;
            if (m_realtimeStatusLabel != nullptr) {
                m_realtimeStatusLabel->setText("系统运行状态：设备离线");
                m_realtimeStatusLabel->setStyleSheet("QLabel{color:#f59e0b;font-size:14px;font-weight:700;}");
            }
        }
    });

    connect(&m_mqtt, &Mqtt::textMessageReceived, this,
            [this, telemetryTopic](const QString& topic, const QString& payload) {
                if (topic != telemetryTopic) {
                    return;
                }

                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
                if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                    qDebug() << "MQTT JSON 解析错误:" << parseError.errorString() << payload;
                    return;
                }

                const QJsonObject wrappedObj = doc.object();
                const QJsonObject obj = unwrapWrappedPayload(wrappedObj, QStringLiteral("telemetry"));
                double temp = -1.0, humi = -1.0, pm25 = -1.0, airRaw = -1.0;
                double flow = -1.0, current = -1.0;
                double bp = -1.0, wp = -1.0;
                int usedPowerMAh = 0, usedWaterCL = 0;
                int batCapMAh = 10000, tankCapCL = 1000;  // 默认值兜底
                int batRemainMAh = 0, wtrRemainCL = 0, powerStatus = 0;
                int batRemainMin = 0, wtrRemainMin = 0;
                int linkLv = 0;
                QVector<int> sensorLvs(6, 0);  // t/h/pm/aq/cur/flw 级别
                QJsonArray almArr;
                QJsonArray actnArr;

                // 新协议：alm=[101,121] (int array), actn=bitmask (int)
                if (obj.contains("sen")) {
                    const QJsonObject sen = obj.value("sen").toObject();
                    temp = sen.value("t").toDouble(-1.0);
                    humi = sen.value("h").toDouble(-1.0);
                    pm25 = sen.value("pm").toDouble(-1.0);
                    airRaw = sen.value("aq").toDouble(-1.0);
                    flow = sen.value("f").toDouble(-1.0);
                    current = sen.value("i").toDouble(-1.0);

                    const QJsonObject res = obj.value("res").toObject();
                    bp = res.value("bp").toDouble(-1.0);
                    wp = res.value("wp").toDouble(-1.0);
                    usedPowerMAh = res.value("tu").toInt(0);
                    usedWaterCL  = res.value("wu").toInt(0);
                    batRemainMAh = res.value("br").toInt(0);
                    wtrRemainCL  = res.value("wr").toInt(0);
                    powerStatus  = res.value("ps").toInt(0);
                    batCapMAh    = res.value("bc").toInt(10000);
                    tankCapCL    = res.value("tc").toInt(1000);
                    batRemainMin = res.value("bt").toInt(0);
                    wtrRemainMin = res.value("wt").toInt(0);

                    const QJsonObject lv = obj.value("lv").toObject();
                    linkLv = lv.value("link").toInt(0);
                    const QJsonArray dArr = lv.value("d").toArray();
                    for (int i = 0; i < dArr.size() && i < 6; ++i)
                        sensorLvs[i] = dArr[i].toInt(0);

                    // alm 新格式: int array
                    const QJsonArray rawAlm = obj.value("alm").toArray();
                    for (const QJsonValue& v : rawAlm) {
                        QJsonObject ao;
                        if (v.isObject()) ao = v.toObject();
                        else { ao["c"] = v.toInt(0); ao["lv"] = 0; }
                        almArr.append(ao);
                    }
                    // actn 新格式: int bitmask → 转为字符串数组兼容旧逻辑
                    int actFlags = obj.value("actn").toInt(0);
                    if (actFlags & 0x01) actnArr.append(QStringLiteral("蜂鸣器已开启"));
                    if (actFlags & 0x02) actnArr.append(QStringLiteral("蜂鸣器已关闭"));
                    if (actFlags & 0x04) actnArr.append(QStringLiteral("风扇已开启"));
                    if (actFlags & 0x08) actnArr.append(QStringLiteral("风扇已关闭"));
                    if (actFlags & 0x10) actnArr.append(QStringLiteral("窗户已打开"));
                    if (actFlags & 0x20) actnArr.append(QStringLiteral("窗户已关闭"));
                }
                // 旧协议：{"t":..,"h":..,"p":..,"a":..,"f":..,"i":..}
                else if (obj.contains("t") && obj.contains("h")) {
                    temp = obj.value("t").toDouble(-1.0);
                    humi = obj.value("h").toDouble(-1.0);
                    pm25 = obj.value("p").toDouble(-1.0);
                    airRaw = obj.value("a").toDouble(-1.0);
                    flow = obj.value("f").toDouble(-1.0);
                    current = obj.value("i").toDouble(-1.0);
                    bp = obj.value("bp").toDouble(-1.0);
                    wp = obj.value("wp").toDouble(-1.0);
                    linkLv = obj.value("lv").toInt(0);
                    sensorLvs.fill(linkLv);
                }
                // 云端旧格式：{"params":{...}}
                else {
                    const QJsonObject params = obj.value("params").toObject();
                    if (params.isEmpty()) return;
                    temp = params.value("DHT11_T").toObject().value("value").toDouble(-1.0);
                    humi = params.value("DHT11_H").toObject().value("value").toDouble(-1.0);
                    pm25 = params.value("pm25").toObject().value("value").toDouble(-1.0);
                    airRaw = params.value("air_quality").toObject().value("value").toDouble(-1.0);
                    flow = params.value("waterflow").toObject().value("value").toDouble(-1.0);
                    current = params.value("elecFlow").toObject().value("value").toDouble(-1.0);
                }

                if (temp <= 0 || humi <= 0) return;
                if (pm25 < 0) pm25 = 0.0;
                if (airRaw < 0) airRaw = pm25;
                if (flow < 0) flow = 0.0;
                if (current < 0) current = 0.0;

                // 新函数：接收 STM32 判定的级别，不做本地计算
                updateDataWithLevels(temp, humi, current, flow, airRaw, pm25,
                                     linkLv, sensorLvs, almArr, actnArr);

                if (bp >= 0.0 || wp >= 0.0) {
                    updateResourcePct(bp >= 0.0 ? bp : m_batteryPct,
                                      wp >= 0.0 ? wp : m_waterPct,
                                      current, flow,
                                      usedPowerMAh, usedWaterCL,
                                      batRemainMAh, wtrRemainCL, powerStatus,
                                      batCapMAh, tankCapCL,
                                      batRemainMin, wtrRemainMin);

                    if (m_db != nullptr) {
                        QString dbErr;
                        if (!m_db->updateDailyResourceUsage(QDateTime::currentDateTime(),
                                                            usedPowerMAh,
                                                            usedWaterCL,
                                                            &dbErr)) {
                            qDebug() << "[DB] update daily resource usage failed:" << dbErr;
                        } else {
                            const QDateTime now = QDateTime::currentDateTime();
                            if (!m_lastDashboardHistoryExportAt.isValid()
                                || m_lastDashboardHistoryExportAt.secsTo(now) >= kDashboardHistoryExportIntervalSec) {
                                exportDashboardHistoryData();
                                m_lastDashboardHistoryExportAt = now;
                            }
                            refreshWaterPowerUsageSummary();
                        }
                    }
                }

                if (m_currentSeries != nullptr && m_flowSeries != nullptr && m_axisX != nullptr) {
                    qreal t = QDateTime::currentMSecsSinceEpoch();
                    m_currentSeries->append(t, current);
                    m_flowSeries->append(t, flow);

                    const int maxPoints = 20;
                    if (m_currentSeries->count() > maxPoints) {
                        m_currentSeries->removePoints(0, m_currentSeries->count() - maxPoints);
                        m_flowSeries->removePoints(0, m_flowSeries->count() - maxPoints);
                    }

                    // 动态调整Y轴范围
                    double curMax = 0, flowMax = 0;
                    for (const auto& pt : m_currentSeries->points())
                        curMax = qMax(curMax, pt.y());
                    for (const auto& pt : m_flowSeries->points())
                        flowMax = qMax(flowMax, pt.y());
                    if (curMax < 100) curMax = 100;
                    if (flowMax < 1) flowMax = 1;
                    m_axisY_Current->setRange(0, curMax * 1.3);
                    m_axisY_Flow->setRange(0, flowMax * 1.3);

                    if (m_currentSeries->count() >= 2) {
                        auto pts = m_currentSeries->points();
                        m_axisX->setRange(QDateTime::fromMSecsSinceEpoch((qint64)pts.first().x()),
                                           QDateTime::fromMSecsSinceEpoch((qint64)pts.last().x()));
                    }
                }
            });

    // 订阅 Web 一键求助通知
    connect(&m_mqtt, &Mqtt::textMessageReceived, this,
            [this, helpTopic](const QString& topic, const QString& payload) {
                if (topic != helpTopic) return;

                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
                if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                    return;
                }

                const QJsonObject wrappedObj = doc.object();
                const QJsonObject obj = unwrapWrappedPayload(wrappedObj, QStringLiteral("help"));
                const QString type = obj.value("type").toString();
                const QString label = obj.value("label").toString();
                const QString site = obj.value("site").toString();
                const QString time = obj.value("time").toString();

                // 求助类型中文映射
                QMap<QString, QString> typeMap;
                typeMap["medical"] = "身体不适";
                typeMap["water"]   = "生活缺水";
                typeMap["power"]   = "电力故障";
                typeMap["other"]   = "其他求助";
                const QString typeCN = typeMap.value(type, type);

                // 写入报警数据库
                if (m_db != nullptr) {
                    const QString alarmTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    m_db->insertAlarmInfo(alarmTime, "求助:" + typeCN,
                                          QString("%1 — %2").arg(label, site), "求助");
                    refreshAlarmInfoFromDatabase();
                }

                // 弹窗通知工作人员（红白紧急风格）
                {
                    QDialog dlg;
                    dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);
                    dlg.setModal(true);
                    dlg.setFixedSize(440, 280);
                    dlg.setStyleSheet("QDialog{background:#ffffff;border:3px solid #dc2626;border-radius:16px;}");

                    auto* lay = new QVBoxLayout(&dlg);
                    lay->setSpacing(0);
                    lay->setContentsMargins(0, 0, 0, 0);

                    // 红色标题栏
                    auto* titleBar = new QWidget(&dlg);
                    titleBar->setStyleSheet("background:#dc2626;border-radius:12px 12px 0 0;");
                    auto* titleLay = new QHBoxLayout(titleBar);
                    titleLay->setContentsMargins(20, 14, 12, 14);
                    auto* icoLabel = new QLabel("🚨", titleBar);
                    icoLabel->setStyleSheet("QLabel{font-size:22px;}");
                    auto* titleLabel = new QLabel("紧急求助", titleBar);
                    titleLabel->setStyleSheet("QLabel{color:#ffffff;font-size:18px;font-weight:800;}");
                    titleLay->addWidget(icoLabel);
                    titleLay->addWidget(titleLabel);
                    titleLay->addStretch();
                    auto* closeBtn = new QPushButton("✕", titleBar);
                    closeBtn->setFixedSize(30, 30);
                    closeBtn->setStyleSheet(
                        "QPushButton{background:rgba(255,255,255,0.15);color:white;border:none;"
                        "border-radius:15px;font-size:16px;font-weight:700;}"
                        "QPushButton:hover{background:rgba(255,255,255,0.3);}");
                    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
                    titleLay->addWidget(closeBtn);
                    lay->addWidget(titleBar);

                    // 白色内容区
                    auto* body = new QWidget(&dlg);
                    body->setStyleSheet("background:#ffffff;");
                    auto* bodyLay = new QVBoxLayout(body);
                    bodyLay->setContentsMargins(24, 20, 24, 16);
                    bodyLay->setSpacing(8);

                    auto* typeLabel = new QLabel(QString("【%1】").arg(typeCN), body);
                    typeLabel->setStyleSheet("QLabel{color:#dc2626;font-size:20px;font-weight:800;}");
                    auto* descLabel = new QLabel(label, body);
                    descLabel->setStyleSheet("QLabel{color:#1f2937;font-size:15px;font-weight:600;}");
                    descLabel->setWordWrap(true);
                    auto* infoLabel = new QLabel(
                        QString("来源站点：%1　　时间：%2")
                            .arg(site.isEmpty() ? "未知" : site,
                                 time.isEmpty() ? QDateTime::currentDateTime().toString("HH:mm:ss") : time),
                        body);
                    infoLabel->setStyleSheet("QLabel{color:#6b7280;font-size:12px;margin-top:4px;}");

                    bodyLay->addWidget(typeLabel);
                    bodyLay->addWidget(descLabel);
                    bodyLay->addWidget(infoLabel);
                    bodyLay->addStretch();
                    lay->addWidget(body, 1);

                    // 红色按钮行
                    auto* btnBar = new QWidget(&dlg);
                    btnBar->setStyleSheet("background:#fef2f2;border-radius:0 0 12px 12px;");
                    auto* btnLay = new QHBoxLayout(btnBar);
                    btnLay->setContentsMargins(16, 10, 16, 14);
                    btnLay->addStretch();
                    auto* ackBtn = new QPushButton("已知晓，立即处理", btnBar);
                    ackBtn->setFixedHeight(38);
                    ackBtn->setStyleSheet(
                        "QPushButton{background:#dc2626;color:white;border:none;"
                        "border-radius:10px;padding:8px 28px;font-size:15px;font-weight:700;}"
                        "QPushButton:hover{background:#b91c1c;}"
                        "QPushButton:pressed{background:#991b1b;}");
                    QObject::connect(ackBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
                    btnLay->addWidget(ackBtn);
                    btnLay->addStretch();
                    lay->addWidget(btnBar);

                    dlg.exec();
                }
            });

    m_mqtt.connectWithKey(QStringLiteral("bemfa.com"), 9501, key);
}


void MainWindow::updateTopBarTime() {
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    if (m_timeLabel != nullptr) {
        m_timeLabel->setText(now);
    }
    const QDateTime nowDt = QDateTime::currentDateTime();
    const bool deviceOffline =
        (!m_lastRealtimeDataAt.isValid() || m_lastRealtimeDataAt.secsTo(nowDt) >= 8);

    if (deviceOffline) {
        m_isDeviceOffline = true;
        if (!m_offlineStartAt.isValid()) {
            m_offlineStartAt = nowDt;
            m_lastOfflineDurationSec = -1;
        }
        if (m_realtimeStatusLabel != nullptr) {
            m_realtimeStatusLabel->setText("系统运行状态：异常");
            m_realtimeStatusLabel->setStyleSheet("QLabel{color:#f59e0b;font-size:14px;font-weight:700;}");
        }
        updateRealtimeCardOfflineState(true);
    } else {
        m_isDeviceOffline = false;
        m_offlineStartAt = QDateTime();
        m_lastOfflineDurationSec = -1;
    }
}

void MainWindow::onNavCurrentRowChanged(int row) {
    // 懒加载：页面首次访问时才构建
    if (!m_builtPages.contains(row)) {
        switch (row) {
        case 0: buildRealtimePage(); break;
        case 1: buildWaterPowerPage(); break;
        case 2: buildHistoryPage(); break;
        case 3: buildAlarmPage(); break;
        case 4: buildDevicePage(); break;
        case 5: buildSettingsPage(); break;
        }
        m_builtPages.insert(row);
    }

    switch (row) {
    case 0:
        ui->stackedWidgetPages->setCurrentWidget(ui->pageDashboard);
        break;
    case 1:
        ui->stackedWidgetPages->setCurrentWidget(ui->pageRemote);  // 水电管理
        break;
    case 2:
        ui->stackedWidgetPages->setCurrentWidget(ui->pageEnvironment);
        break;
    case 3:
        ui->stackedWidgetPages->setCurrentWidget(ui->pageAlarm);
        refreshAlarmInfoFromDatabase();
        break;
    case 4:
        ui->stackedWidgetPages->setCurrentWidget(ui->pageWaterPower);
        break;
    case 5:
        ui->stackedWidgetPages->setCurrentWidget(ui->pageSetting);
        break;
    default:
        break;
    }
}

void MainWindow::setCardStateDot(QLabel* dot, const QColor& color) {
    if (dot == nullptr) {
        return;
    }
    dot->setStyleSheet(QString("QLabel{background:%1;border-radius:6px;}").arg(color.name()));
}

void MainWindow::updateRealtimeCardOfflineState(bool offline) {
    if (!offline) {
        return;
    }

    const QString stateText = QStringLiteral("离线");
    const QString stateStyle =
        QStringLiteral("QLabel{color:#e2e8f0;font-size:12px;font-weight:700;}");
    const QList<QLabel*> stateLabels = {
        m_stateTempLabel, m_stateHumiLabel, m_stateFlowLabel,
        m_stateCurrentLabel, m_stateAirLabel, m_statePmLabel
    };
    for (QLabel* label : stateLabels) {
        if (label == nullptr) {
            continue;
        }
        label->setText(stateText);
        label->setStyleSheet(stateStyle);
    }

    const QList<QLabel*> dots = {m_dotTemp, m_dotHumi, m_dotFlow, m_dotCurrent, m_dotAir, m_dotPm};
    for (QLabel* dot : dots) {
        setCardStateDot(dot, QColor(0xff, 0xff, 0xff));
    }
}

void MainWindow::updateData(double temp, double hum, double current,
                            double flow, double airIndex, double pm25) {
    // 旧接口直接调用新接口，级别由调用方自行计算后传入
    QVector<int> lvs(6, 0);
    QJsonArray emptyAlm, emptyActn;
    updateDataWithLevels(temp, hum, current, flow, airIndex, pm25, 0, lvs, emptyAlm, emptyActn);
}

void MainWindow::updateDataWithLevels(double temp, double hum, double current,
                                       double flow, double airIndex, double pm25,
                                       int linkLv, const QVector<int>& sensorLvs,
                                       const QJsonArray& almArr, const QJsonArray& actnArr) {
    if (m_cardTempValue == nullptr) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();

    m_lastRealtimeDataAt = now;
    m_isDeviceOffline = false;
    m_offlineStartAt = QDateTime();
    m_lastOfflineDurationSec = -1;

    m_cardTempValue->setText(QString("%1 ℃").arg(QString::number(temp, 'f', 1)));
    m_cardHumiValue->setText(QString("%1 %").arg(QString::number(hum, 'f', 1)));
    m_cardCurrentValue->setText(QString("%1 A").arg(QString::number(current / 1000.0, 'f', 1)));
    m_cardFlowValue->setText(QString("%1 L/min").arg(QString::number(flow, 'f', 2)));

    // 从 STM32 lv.d 数组获取各传感器级别（不再本地计算）
    int tempLv    = sensorLvs.value(0, 0);
    int humiLv    = sensorLvs.value(1, 0);
    int pmLv      = sensorLvs.value(2, 0);
    int airLv     = sensorLvs.value(3, 0);
    int currentLv = sensorLvs.value(4, 0);
    int flowLv    = sensorLvs.value(5, 0);

    // 空气质量文字（Web用"优/良/差"）
    QString airGrade = QStringLiteral("优");
    if (airLv >= 2) airGrade = QStringLiteral("差");
    else if (airLv == 1) airGrade = QStringLiteral("良");
    m_cardAirValue->setText(QString("%1 (%2)").arg(QString::number(airIndex, 'f', 0), airGrade));
    m_cardPmValue->setText(QString("%1 ug/m3").arg(QString::number(pm25, 'f', 1)));

    applySensorLevelStyle(m_cardTempValue, m_dotTemp, m_stateTempLabel, tempLv);
    applySensorLevelStyle(m_cardHumiValue, m_dotHumi, m_stateHumiLabel, humiLv);
    applySensorLevelStyle(m_cardFlowValue, m_dotFlow, m_stateFlowLabel, flowLv);
    applySensorLevelStyle(m_cardAirValue, m_dotAir, m_stateAirLabel, airLv);
    applySensorLevelStyle(m_cardPmValue, m_dotPm, m_statePmLabel, pmLv);
    applySensorLevelStyle(m_cardCurrentValue, m_dotCurrent, m_stateCurrentLabel, currentLv);

    // 系统状态直接从 linkLv 判断（不再本地计算）
    if (m_realtimeStatusLabel != nullptr) {
        if (linkLv >= 2) {
            m_realtimeStatusLabel->setText(QStringLiteral("系统运行状态：告警关注中"));
            m_realtimeStatusLabel->setStyleSheet(QStringLiteral("QLabel{color:#dc2626;font-size:14px;font-weight:700;}"));
        } else if (linkLv == 1) {
            m_realtimeStatusLabel->setText(QStringLiteral("系统运行状态：预警提示中"));
            m_realtimeStatusLabel->setStyleSheet(QStringLiteral("QLabel{color:#f59e0b;font-size:14px;font-weight:700;}"));
        } else {
            m_realtimeStatusLabel->setText(QStringLiteral("系统运行状态：正常"));
            m_realtimeStatusLabel->setStyleSheet(QStringLiteral("QLabel{color:#059669;font-size:14px;font-weight:700;}"));
        }
    }

    // 告警码→中文
    static const QMap<int, QString> almMsg = {
        {101,QStringLiteral("温度偏高")},{102,QStringLiteral("温度过高告警")},
        {103,QStringLiteral("温度偏低")},{104,QStringLiteral("温度过低告警")},
        {111,QStringLiteral("湿度偏高")},{112,QStringLiteral("湿度过高告警")},
        {113,QStringLiteral("湿度偏低")},{114,QStringLiteral("湿度过低告警")},
        {121,QStringLiteral("PM2.5偏高")},{122,QStringLiteral("PM2.5超标告警")},
        {131,QStringLiteral("空气质量下降")},{132,QStringLiteral("空气质量恶化告警")},
        {141,QStringLiteral("电流偏高")},{142,QStringLiteral("电流过载告警")},
        {151,QStringLiteral("水流偏高")},{152,QStringLiteral("水流异常告警")},
        {200,QStringLiteral("温度+PM2.5组合告警(火灾风险)")},
        {201,QStringLiteral("PM2.5+AQ组合告警(烟雾污染)")},
        {202,QStringLiteral("水流+电流组合告警(管道异常)")},
        {901,QStringLiteral("DHT11离线")},{902,QStringLiteral("MQ135离线")},
        {903,QStringLiteral("GP2Y10离线")},{904,QStringLiteral("ACS712离线")},
        {905,QStringLiteral("水流传感器离线")}
    };

    // ===== 活跃报警追踪：检测新增和消失的报警 =====
    QSet<int> curAlmCodes;
    for (const QJsonValue& v : almArr) {
        const QJsonObject a = v.toObject();
        int c = a.value("c").toInt(0);
        if (c > 0) curAlmCodes.insert(c);
    }
    const QString timeStr = now.toString("yyyy-MM-dd HH:mm:ss");

    // 新增的报警（当前有、之前没有）
    QSet<int> newCodes = curAlmCodes - m_prevAlmCodes;
    for (int code : newCodes) {
        int lv = (code % 10 == 2 || code >= 200) ? 2 : 1;
        QString level = (lv >= 2) ? QStringLiteral("严重") : QStringLiteral("预警");
        QString msg = almMsg.value(code, QString("Code %1").arg(code));
        addAlarmRecord(timeStr, msg, msg, level, code);
    }

    // 消失的报警（之前有、当前没有）→ 记录结束时间
    QSet<int> goneCodes = m_prevAlmCodes - curAlmCodes;
    bool anyGone = false;
    for (int code : goneCodes) {
        if (m_db) {
            QString dbErr;
            if (m_db->updateAlarmResolved(code, timeStr, &dbErr)) {
                anyGone = true;
            }
        }
    }
    // 只在有报警消失时刷新一次（避免多次重建表格）
    if (anyGone) refreshAlarmInfoFromDatabase();
    m_prevAlmCodes = curAlmCodes;

    // 告警弹窗（仅当有新报警时）
    if (!newCodes.isEmpty() && (!m_lastLocalAlarmAt.isValid() || m_lastLocalAlarmAt.secsTo(now) >= 30)) {
        QStringList alarmMessages;
        for (int code : newCodes) {
            QString msg = almMsg.value(code, QString("Code %1").arg(code));
            QString line = QString("%1 (码:%2)").arg(msg).arg(code);
            alarmMessages << line;
        }
        QStringList actMsgs;
        for (const QJsonValue& v : actnArr) actMsgs << v.toString();
        if (!actMsgs.isEmpty())
            alarmMessages << QStringLiteral("\n已采取措施：") + actMsgs.join(" / ");
        m_lastLocalAlarmAt = now;
        showRealtimeAlarmDialog(this, alarmMessages.join("\n"));
    }

    if (m_db != nullptr) {
        SensorData sample;
        sample.ts = now;
        sample.tempC = temp;
        sample.humiPercent = hum;
        sample.pm25UgM3 = pm25;
        sample.airIndex = airIndex;
        sample.currentA = current;
        sample.flowLMin = flow;
        sample.airQuality = airGrade;
        QString dbErr;
        if (!m_db->insertSensorSample(sample, &dbErr)) {
            qDebug() << "[DB] insert sample failed:" << dbErr;
        }
    }

}

void MainWindow::updateResourcePct(double batteryPct, double waterPct,
                                    double currentMA, double flowLMin,
                                    int usedPowerMAh, int usedWaterCL,
                                    int batRemainMAh, int wtrRemainCL, int powerStatus,
                                    int batCapMAh, int tankCapCL,
                                    int batRemainMin, int wtrRemainMin) {
    m_batteryPct = qBound(0.0, batteryPct, 100.0);
    m_waterPct   = qBound(0.0, waterPct,   100.0);
    m_lastUsedPowerMAh = qMax(0, usedPowerMAh);
    m_lastUsedWaterCL = qMax(0, usedWaterCL);

    // 实时负载大字 + 状态（使用STM32 powerStatus）
    if (m_wpLoadValueLabel != nullptr) {
        const double curA = currentMA / 1000.0;
        m_wpLoadValueLabel->setText(QString("%1 A").arg(QString::number(curA, 'f', 1)));
        QColor loadColor;
        if (powerStatus >= 2)        loadColor = QColor(239, 68, 68);
        else if (powerStatus >= 1)   loadColor = QColor(245, 158, 11);
        else                         loadColor = QColor(34, 197, 94);
        m_wpLoadValueLabel->setStyleSheet(
            QString("QLabel{color:%1;font-size:42px;font-weight:800;}").arg(loadColor.name()));
    }
    if (m_wpLoadStatusLabel != nullptr) {
        QString st;
        if (powerStatus >= 2)            st = QStringLiteral("⚠ 负载过载");
        else if (powerStatus >= 1)       st = QStringLiteral("⚡ 负载偏高");
        else if (currentMA > 10.0)       st = QStringLiteral("✓ 负载正常");
        else                             st = QStringLiteral("— 无负载");
        m_wpLoadStatusLabel->setText(st);
    }

    // 今日已用标签（直接显示STM32上传值）
    if (m_wpTodayPowerLabel != nullptr)
        m_wpTodayPowerLabel->setText(QStringLiteral("用电: %1 Ah").arg(QString::number(usedPowerMAh / 1000.0, 'f', 1)));
    if (m_wpTodayWaterLabel != nullptr)
        m_wpTodayWaterLabel->setText(QStringLiteral("用水: %1 L").arg(QString::number(usedWaterCL / 100.0, 'f', 1)));

    // ===== 电量：更新电池图标 + 信息（全部值直接来自STM32） =====
    {
        const double remainMAh = (double)batRemainMAh;

        if (auto* g = static_cast<BatteryGauge*>(m_batteryGauge))
            g->setPct(m_batteryPct);

        if (m_batteryInfoLabel != nullptr) {
            QString info = QStringLiteral(
                "剩余: %1 mAh\n"
                "已用: %2 mAh\n"
                "今日用电: %4 Ah\n"
                "容量: %3 mAh")
                .arg(QString::number(remainMAh, 'f', 0),
                     QString::number(usedPowerMAh),
                     QString::number(batCapMAh),
                     QString::number(m_lastUsedPowerMAh / 1000.0, 'f', 1));
            if (batRemainMin > 0) {
                if (batRemainMin >= 1440)
                    info += QStringLiteral("\n预估: 约 %1 天").arg(QString::number(batRemainMin / 1440.0, 'f', 1));
                else if (batRemainMin >= 60)
                    info += QStringLiteral("\n预估: 约 %1 小时").arg(QString::number(batRemainMin / 60));
                else
                    info += QStringLiteral("\n预估: 约 %1 分钟").arg(QString::number(batRemainMin));
            }
            m_batteryInfoLabel->setText(info);
        }
    }

    // ===== 水量：更新水箱图标 + 信息 =====
    {
        const double remainL = wtrRemainCL / 100.0;  // cL to L (STM32)
        const double usedL   = usedWaterCL / 100.0;

        if (auto* g = static_cast<TankGauge*>(m_tankGauge))
            g->setPct(m_waterPct);

        if (m_tankInfoLabel != nullptr) {
            QString info = QStringLiteral(
                "剩余: %1 L\n"
                "已用: %2 L\n"
                "今日用水: %4 L\n"
                "容量: %3 L")
                .arg(QString::number(remainL, 'f', 1),
                     QString::number(usedL, 'f', 1),
                     QString::number(tankCapCL / 100.0, 'f', 1),
                     QString::number(m_lastUsedWaterCL / 100.0, 'f', 1));
            if (wtrRemainMin > 0) {
                if (wtrRemainMin >= 1440)
                    info += QStringLiteral("\n预估: 约 %1 天").arg(QString::number(wtrRemainMin / 1440.0, 'f', 1));
                else if (wtrRemainMin >= 60)
                    info += QStringLiteral("\n预估: 约 %1 小时").arg(QString::number(wtrRemainMin / 60));
                else
                    info += QStringLiteral("\n预估: 约 %1 分钟").arg(QString::number(wtrRemainMin));
            } else {
                info += QStringLiteral("\n当前无用水");
            }
            m_tankInfoLabel->setText(info);
        }
    }

    refreshWaterPowerUsageSummary();
}

QString MainWindow::resolveDashboardHistoryDataPath() const {
    const QDir workspaceRoot(QStringLiteral("D:/AAA"));
    return workspaceRoot.filePath(QStringLiteral("dashboard/js/history_data.js"));
}

void MainWindow::refreshWaterPowerUsageSummary() {
    if (m_wpTodayPowerLabel) {
        m_wpTodayPowerLabel->setText(
            QStringLiteral("用电: %1 Ah").arg(QString::number(m_lastUsedPowerMAh / 1000.0, 'f', 1)));
    }
    if (m_wpTodayWaterLabel) {
        m_wpTodayWaterLabel->setText(
            QStringLiteral("用水: %1 L").arg(QString::number(m_lastUsedWaterCL / 100.0, 'f', 1)));
    }

    if (!m_wpRecentRangeLabel || !m_wpRecentPowerLabel || !m_wpRecentWaterLabel) {
        return;
    }

    const QDate endDay = QDate::currentDate().addDays(-1);
    const QDate startDay = endDay.addDays(-6);
    int totalPowerMAh = 0;
    int totalWaterCL = 0;

    if (m_db != nullptr) {
        QString queryErr;
        const QList<DatabaseManager::DailyResourceUsageEntry> rows =
            m_db->queryDailyResourceUsage(startDay, endDay, &queryErr);
        if (!queryErr.isEmpty()) {
            qDebug() << "[DB] query daily summary failed:" << queryErr;
        } else {
            for (const auto& row : rows) {
                totalPowerMAh += qMax(0, row.powerMAh);
                totalWaterCL += qMax(0, row.waterCL);
            }
        }
    }

    m_wpRecentRangeLabel->setText(
        QStringLiteral("统计区间: %1 ~ %2")
            .arg(startDay.toString(QStringLiteral("MM-dd")),
                 endDay.toString(QStringLiteral("MM-dd"))));
    m_wpRecentPowerLabel->setText(
        QStringLiteral("用电: %1 Ah").arg(QString::number(totalPowerMAh / 1000.0, 'f', 1)));
    m_wpRecentWaterLabel->setText(
        QStringLiteral("用水: %1 L").arg(QString::number(totalWaterCL / 100.0, 'f', 1)));
}

void MainWindow::refreshWaterPowerAnalysisPage() {
    if (m_db == nullptr || m_wpUsagePowerSet == nullptr || m_wpUsageWaterSet == nullptr ||
        m_wpTrendPowerSet == nullptr || m_wpTrendWaterSet == nullptr) {
        return;
    }

    const QDate selectedDay = m_wpAnalysisDateEdit ? m_wpAnalysisDateEdit->date() : QDate::currentDate().addDays(-1);
    const int bucketMinutes = 60;
    const int trendDays = (m_wpTrendDaysCombo && m_wpTrendDaysCombo->currentIndex() == 1) ? 15
                        : (m_wpTrendDaysCombo && m_wpTrendDaysCombo->currentIndex() == 2) ? 30
                        : 7;

    const QDateTime dayStart(selectedDay, QTime(0, 0, 0));
    const QDateTime dayEnd(selectedDay, QTime(23, 59, 59));

    QString err;
    const QList<SensorData> points = m_db->queryDataRange(dayStart, dayEnd, &err);
    if (!err.isEmpty()) {
        qDebug() << "[DB] query water/power day points failed:" << err;
    }

    struct BucketUsage {
        double powerMAh = 0.0;
        double waterL = 0.0;
    };

    QMap<int, BucketUsage> bucketMap;
    for (int i = 0; i < points.size(); ++i) {
        double dtSec = 1.0;
        if (i + 1 < points.size()) {
            dtSec = qBound(1.0, static_cast<double>(points[i].ts.secsTo(points[i + 1].ts)), 3600.0);
        }
        const int bucketIdx = qBound(0, points[i].ts.time().msecsSinceStartOfDay() / (bucketMinutes * 60 * 1000),
                                     (24 * 60 / bucketMinutes) - 1);
        BucketUsage& bucket = bucketMap[bucketIdx];
        bucket.powerMAh += qMax(0.0, points[i].currentA * dtSec / 3600.0);
        bucket.waterL += qMax(0.0, points[i].flowLMin * dtSec / 60.0);
    }

    m_wpUsagePowerSet->remove(0, m_wpUsagePowerSet->count());
    m_wpUsageWaterSet->remove(0, m_wpUsageWaterSet->count());
    QStringList bucketLabels;

    double totalPowerMAh = 0.0;
    double totalWaterL = 0.0;
    double peakPowerMAh = -1.0;
    double peakWaterL = -1.0;
    QString peakPowerTime;
    QString peakWaterTime;

    const int bucketCount = 24 * 60 / bucketMinutes;
    for (int i = 0; i < bucketCount; ++i) {
        const QDateTime bucketTime = dayStart.addSecs(i * bucketMinutes * 60);
        const BucketUsage bucket = bucketMap.value(i);
        *m_wpUsagePowerSet << qRound(bucket.powerMAh * 10.0) / 10.0;
        *m_wpUsageWaterSet << qRound(bucket.waterL * 10.0) / 10.0;
        bucketLabels << QString::number(i);
        totalPowerMAh += bucket.powerMAh;
        totalWaterL += bucket.waterL;
        if (bucket.powerMAh > peakPowerMAh) {
            peakPowerMAh = bucket.powerMAh;
            peakPowerTime = bucketTime.toString(QStringLiteral("HH:mm"));
        }
        if (bucket.waterL > peakWaterL) {
            peakWaterL = bucket.waterL;
            peakWaterTime = bucketTime.toString(QStringLiteral("HH:mm"));
        }
    }

    if (m_wpUsageAxisX_Power) { m_wpUsageAxisX_Power->clear(); m_wpUsageAxisX_Power->setCategories(bucketLabels); }
    if (m_wpUsageAxisX_Water) { m_wpUsageAxisX_Water->clear(); m_wpUsageAxisX_Water->setCategories(bucketLabels); }
    if (m_wpUsageAxisY_Power) {
        m_wpUsageAxisY_Power->setRange(0.0, qMax(10.0, peakPowerMAh * 1.25));
    }
    if (m_wpUsageAxisY_Water) {
        m_wpUsageAxisY_Water->setRange(0.0, qMax(1.0, peakWaterL * 1.25));
    }
    if (m_wpUsageStatsLabel) {
        const double avgPower = bucketCount > 0 ? totalPowerMAh / bucketCount : 0.0;
        const double avgWater = bucketCount > 0 ? totalWaterL / bucketCount : 0.0;
        int activePowerBuckets = 0;
        int activeWaterBuckets = 0;
        for (auto it = bucketMap.constBegin(); it != bucketMap.constEnd(); ++it) {
            if (it.value().powerMAh > 0.01) activePowerBuckets++;
            if (it.value().waterL > 0.001) activeWaterBuckets++;
        }
        m_wpUsageStatsLabel->setText(
            QStringLiteral(
                "日期：%1\n"
                "用电总量：%2 mAh，均值：%3 mAh/时段，峰值：%4 mAh（%5），活跃时长：约 %6 小时\n"
                "用水总量：%7 L，均值：%8 L/时段，峰值：%9 L（%10），活跃时长：约 %11 小时")
                .arg(selectedDay.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(QString::number(totalPowerMAh, 'f', 0))
                .arg(QString::number(avgPower, 'f', 1))
                .arg(QString::number(qMax(0.0, peakPowerMAh), 'f', 0))
                .arg(peakPowerTime.isEmpty() ? QStringLiteral("--") : peakPowerTime)
                .arg(QString::number(activePowerBuckets * bucketMinutes / 60.0, 'f', 1))
                .arg(QString::number(totalWaterL, 'f', 1))
                .arg(QString::number(avgWater, 'f', 2))
                .arg(QString::number(qMax(0.0, peakWaterL), 'f', 2))
                .arg(peakWaterTime.isEmpty() ? QStringLiteral("--") : peakWaterTime)
                .arg(QString::number(activeWaterBuckets * bucketMinutes / 60.0, 'f', 1)));
    }

    const QDate trendEnd = QDate::currentDate().addDays(-1);
    const QDate trendStart = trendEnd.addDays(-(trendDays - 1));
    const QList<DatabaseManager::DailyResourceUsageEntry> trendRows =
        m_db->queryDailyResourceUsage(trendStart, trendEnd, &err);
    if (!err.isEmpty()) {
        qDebug() << "[DB] query water/power trend rows failed:" << err;
    }
    QMap<QString, DatabaseManager::DailyResourceUsageEntry> trendMap;
    for (const auto& row : trendRows) trendMap.insert(row.day, row);

    m_wpTrendPowerSet->remove(0, m_wpTrendPowerSet->count());
    m_wpTrendWaterSet->remove(0, m_wpTrendWaterSet->count());
    QStringList trendLabels;
    double maxTrendPower = 0.0;
    double maxTrendWater = 0.0;
    double totalTrendPower = 0.0;
    double totalTrendWater = 0.0;
    QString peakPowerDay;
    QString peakWaterDay;
    const bool sameMonth = (trendStart.month() == trendEnd.month() && trendStart.year() == trendEnd.year());
    int lastMonth = -1;
    for (int i = 0; i < trendDays; ++i) {
        const QDate day = trendStart.addDays(i);
        const QString key = day.toString(Qt::ISODate);
        const auto row = trendMap.value(key, DatabaseManager::DailyResourceUsageEntry{});
        const double power = row.powerMAh;
        const double water = row.waterCL / 100.0;
        *m_wpTrendPowerSet << qRound(power * 10.0) / 10.0;
        *m_wpTrendWaterSet << qRound(water * 10.0) / 10.0;
        if (sameMonth) {
            trendLabels << day.toString(QStringLiteral("d"));
        } else if (day.month() != lastMonth) {
            trendLabels << day.toString(QStringLiteral("M.d"));
            lastMonth = day.month();
        } else {
            trendLabels << day.toString(QStringLiteral("d"));
        }
        totalTrendPower += power;
        totalTrendWater += water;
        if (power > maxTrendPower) {
            maxTrendPower = power;
            peakPowerDay = day.toString(QStringLiteral("MM-dd"));
        }
        if (water > maxTrendWater) {
            maxTrendWater = water;
            peakWaterDay = day.toString(QStringLiteral("MM-dd"));
        }
    }

    if (m_wpTrendAxisX_Power) { m_wpTrendAxisX_Power->clear(); m_wpTrendAxisX_Power->setCategories(trendLabels); }
    if (m_wpTrendAxisX_Water) { m_wpTrendAxisX_Water->clear(); m_wpTrendAxisX_Water->setCategories(trendLabels); }
    if (m_wpTrendAxisY_Power) {
        m_wpTrendAxisY_Power->setRange(0.0, qMax(10.0, maxTrendPower * 1.25));
    }
    if (m_wpTrendAxisY_Water) {
        m_wpTrendAxisY_Water->setRange(0.0, qMax(1.0, maxTrendWater * 1.25));
    }
    if (m_wpTrendStatsLabel) {
        m_wpTrendStatsLabel->setText(
            QStringLiteral(
                "区间：%1 ~ %2\n"
                "总用电：%3 mAh，日均：%4 mAh，峰值日：%5（%6 mAh）\n"
                "总用水：%7 L，日均：%8 L，峰值日：%9（%10 L）")
                .arg(trendStart.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(trendEnd.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(QString::number(totalTrendPower, 'f', 0))
                .arg(QString::number(trendDays > 0 ? totalTrendPower / trendDays : 0.0, 'f', 1))
                .arg(peakPowerDay.isEmpty() ? QStringLiteral("--") : peakPowerDay)
                .arg(QString::number(maxTrendPower, 'f', 0))
                .arg(QString::number(totalTrendWater, 'f', 1))
                .arg(QString::number(trendDays > 0 ? totalTrendWater / trendDays : 0.0, 'f', 1))
                .arg(peakWaterDay.isEmpty() ? QStringLiteral("--") : peakWaterDay)
                .arg(QString::number(maxTrendWater, 'f', 1)));
    }
}

void MainWindow::exportDashboardHistoryData() {
    if (m_db == nullptr) {
        return;
    }

    const QDate endDay = QDate::currentDate().addDays(-1);
    const QDate startDay = endDay.addDays(-6);
    QString queryErr;
    const QList<DatabaseManager::DailyResourceUsageEntry> usageRows =
        m_db->queryDailyResourceUsage(QDate(), QDate(), &queryErr);
    if (!queryErr.isEmpty()) {
        qDebug() << "[DB] query daily resource usage failed:" << queryErr;
        return;
    }

    QMap<QString, DatabaseManager::DailyResourceUsageEntry> usageByDay;
    for (const auto& row : usageRows) {
        usageByDay.insert(row.day, row);
    }

    auto buildJsonDay = [](const QString& day,
                           const DatabaseManager::DailyResourceUsageEntry* row) -> QJsonObject {
        QJsonObject obj;
        obj.insert(QStringLiteral("date"), day);
        obj.insert(QStringLiteral("samples"), 0);
        obj.insert(QStringLiteral("avgCurrentMA"), 0);
        obj.insert(QStringLiteral("avgFlowLMin"), 0);
        obj.insert(QStringLiteral("powerMAh"), row ? row->powerMAh : 0);
        obj.insert(QStringLiteral("waterL"), row ? (static_cast<double>(row->waterCL) / 100.0) : 0.0);
        return obj;
    };

    QJsonArray allDays;
    for (const auto& row : usageRows) {
        allDays.append(buildJsonDay(row.day, &row));
    }

    QJsonArray recent7;
    for (int i = 0; i < 7; ++i) {
        const QString day = startDay.addDays(i).toString(Qt::ISODate);
        const auto it = usageByDay.constFind(day);
        const DatabaseManager::DailyResourceUsageEntry* row = (it != usageByDay.constEnd()) ? &it.value() : nullptr;
        recent7.append(buildJsonDay(day, row));
    }

    QJsonArray recent10;
    const QDate recent10Start = endDay.addDays(-9);
    for (int i = 0; i < 10; ++i) {
        const QString day = recent10Start.addDays(i).toString(Qt::ISODate);
        const auto it = usageByDay.constFind(day);
        const DatabaseManager::DailyResourceUsageEntry* row = (it != usageByDay.constEnd()) ? &it.value() : nullptr;
        recent10.append(buildJsonDay(day, row));
    }

    QJsonObject root;
    root.insert(QStringLiteral("allDays"), allDays);
    root.insert(QStringLiteral("recent10"), recent10);
    root.insert(QStringLiteral("recent7"), recent7);

    const QString outPath = resolveDashboardHistoryDataPath();
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "[Web] open history_data.js failed:" << outPath << file.errorString();
        return;
    }

    QString content;
    content += QStringLiteral("// 由 System_UI 自动生成，数据来源 D:/System_UI_Data/system_ui.sqlite\n");
    content += QStringLiteral("// 生成时间: %1\n")
                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    content += QStringLiteral("window.HISTORY_DATA = ");
    content += QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
    content += QStringLiteral(";\n");
    file.write(content.toUtf8());
    file.close();
}

void MainWindow::onUpdateDashboardData() {
    if (m_useMqttRealtime) {
        return;
    }

    const double temp = 23.0 + QRandomGenerator::global()->bounded(130) / 10.0;
    const double hum = 42.0 + QRandomGenerator::global()->bounded(300) / 10.0;
    const double current =
        180.0 + static_cast<double>(QRandomGenerator::global()->bounded(420));  // mA，覆盖阈值区间
    const double flow = QRandomGenerator::global()->bounded(65) / 10.0;
    const double pm25 = 20.0 + QRandomGenerator::global()->bounded(1100) / 10.0;

    const double airIndex = pm25;
    updateData(temp, hum, current, flow, airIndex, pm25);

    if (m_currentSeries == nullptr || m_flowSeries == nullptr || m_axisX == nullptr) {
        return;
    }

    qreal t = QDateTime::currentMSecsSinceEpoch();
    m_currentSeries->append(t, current);
    m_flowSeries->append(t, flow);

    const int maxPoints = 20;
    if (m_currentSeries->count() > maxPoints) {
        m_currentSeries->removePoints(0, m_currentSeries->count() - maxPoints);
        m_flowSeries->removePoints(0, m_flowSeries->count() - maxPoints);
    }

    double curMax = 0, flowMax = 0;
    for (const auto& pt : m_currentSeries->points())
        curMax = qMax(curMax, pt.y());
    for (const auto& pt : m_flowSeries->points())
        flowMax = qMax(flowMax, pt.y());
    if (curMax < 100) curMax = 100;
    if (flowMax < 1) flowMax = 1;
    m_axisY_Current->setRange(0, curMax * 1.3);
    m_axisY_Flow->setRange(0, flowMax * 1.3);

    if (m_currentSeries->count() >= 2) {
        auto pts = m_currentSeries->points();
        m_axisX->setRange(QDateTime::fromMSecsSinceEpoch((qint64)pts.first().x()),
                           QDateTime::fromMSecsSinceEpoch((qint64)pts.last().x()));
    }
}

void MainWindow::refreshHistoryPage() {
    constexpr int kHistoryChartMaxPoints = 720;

    bool anyBuilt = false;
    for (int i = 0; i < m_historyCharts.size(); ++i) {
        if (m_historyCharts[i] != nullptr) { anyBuilt = true; break; }
    }
    if (m_db == nullptr || !anyBuilt) {
        return;
    }

    const QDateTime rangeStart = m_historyStartCombo
                                     ? m_historyStartCombo->dateTime()
                                     : QDateTime::currentDateTime().addDays(-1);
    const QDateTime rangeEnd = m_historyEndCombo
                                   ? m_historyEndCombo->dateTime()
                                   : QDateTime::currentDateTime();

    if (rangeStart >= rangeEnd) {
        for (int i = 0; i < m_historyStatLabels.size(); ++i) {
            if (m_historyStatLabels[i]) m_historyStatLabels[i]->setVisible(false);
        }
        return;
    }

    QVector<int> selectedMetrics;
    for (int i = 0; i < m_historyMetricChecks.size(); ++i) {
        if (m_historyMetricChecks[i] != nullptr && m_historyMetricChecks[i]->isChecked()) {
            selectedMetrics.append(i);
        }
    }
    if (selectedMetrics.isEmpty()) {
        for (int i = 0; i < m_historyLineSeries.size(); ++i) {
            if (m_historyLineSeries[i] != nullptr) {
                m_historyLineSeries[i]->clear();
            }
        }
        for (int i = 0; i < m_historyStatLabels.size(); ++i) {
            if (m_historyStatLabels[i]) m_historyStatLabels[i]->setVisible(false);
        }
        return;
    }

    QString sampleErr;
    int totalCount = 0;
    const QList<SensorData> points =
        m_db->queryDataRangeSampled(rangeStart, rangeEnd, kHistoryChartMaxPoints, &totalCount, &sampleErr);
    if (!sampleErr.isEmpty()) {
        qDebug() << "[DB] query sampled history data failed:" << sampleErr;
    }

    QString statsErr;
    QVector<DatabaseManager::HistoryMetricStats> stats;
    if (!m_db->queryHistoryMetricStats(rangeStart, rangeEnd, &stats, &statsErr) && !statsErr.isEmpty()) {
        qDebug() << "[DB] query history stats failed:" << statsErr;
    }

    if (points.isEmpty() || stats.isEmpty() || totalCount <= 0) {
        for (int i = 0; i < m_historyLineSeries.size(); ++i) {
            if (m_historyLineSeries[i] != nullptr) {
                m_historyLineSeries[i]->clear();
            }
        }
        for (int i = 0; i < m_historyStatLabels.size(); ++i) {
            if (m_historyStatLabels[i]) m_historyStatLabels[i]->setVisible(false);
        }
        return;
    }

    const QStringList metricNames = {
        QStringLiteral("温度"),
        QStringLiteral("湿度"),
        QStringLiteral("PM2.5"),
        QStringLiteral("空气指数")
    };
    const QStringList metricUnits = {
        QStringLiteral("℃"),
        QStringLiteral("%"),
        QStringLiteral("ug/m3"),
        QString()
    };
    const QList<QColor> metricColors = {
        QColor(248, 113, 113),
        QColor(56, 189, 248),
        QColor(251, 191, 36),
        QColor(34, 197, 94)
    };

    for (int metricIdx = 0; metricIdx < m_historyLineSeries.size(); ++metricIdx) {
        QLineSeries* series = m_historyLineSeries[metricIdx];
        if (series == nullptr) continue;
        series->clear();
        QPen pen(metricColors.value(metricIdx, QColor(125, 211, 252)));
        pen.setWidthF(2.2);
        series->setPen(pen);
        series->setName(metricNames.value(metricIdx));
    }

    QVector<QVector<QPointF>> sampledSeriesPoints(4);
    for (int metricIdx : selectedMetrics) {
        if (m_historyCharts.value(metricIdx) == nullptr) continue;
        sampledSeriesPoints[metricIdx].reserve(points.size());
    }

    for (const auto& pt : points) {
        const QVector<double> values = {
            pt.tempC,
            pt.humiPercent,
            pt.pm25UgM3,
            pt.airIndex
        };
        for (int metricIdx : selectedMetrics) {
            if (m_historyCharts.value(metricIdx) == nullptr) continue;
            const double value = values.value(metricIdx);
            sampledSeriesPoints[metricIdx].append(QPointF(pt.ts.toMSecsSinceEpoch(), value));
        }
    }

    for (int metricIdx : selectedMetrics) {
        QLineSeries* series = m_historyLineSeries.value(metricIdx, nullptr);
        if (series == nullptr) continue;
        series->replace(sampledSeriesPoints[metricIdx]);
    }

    for (int metricIdx : selectedMetrics) {
        if (m_historyCharts.value(metricIdx) == nullptr) continue;
        QDateTimeAxis* axisX = m_historyAxisXs.value(metricIdx);
        QValueAxis* axisY = m_historyAxisYs.value(metricIdx);
        if (!axisX || !axisY) continue;
        axisX->setRange(rangeStart, rangeEnd);
        qint64 span = rangeStart.secsTo(rangeEnd);
        axisX->setFormat(span > 86400 ? "MM-dd" : "HH:mm");
        const DatabaseManager::HistoryMetricStats& s = stats[metricIdx];
        if (s.count > 0 && s.maxVal > s.minVal) {
            double m = (s.maxVal - s.minVal) * 0.15;
            if (m < 0.01) m = 1.0;
            axisY->setRange(qMax(0.0, s.minVal - m), s.maxVal + m);
        } else {
            axisY->setRange(0, 100);
        }
    }

    for (int metricIdx = 0; metricIdx < 4; ++metricIdx) {
        if (m_historyStatLabels.value(metricIdx) == nullptr) continue;
        const bool selected = selectedMetrics.contains(metricIdx)
                              && m_historyCharts.value(metricIdx) != nullptr;
        m_historyStatLabels[metricIdx]->setVisible(selected);
        if (!selected) continue;
        const DatabaseManager::HistoryMetricStats& s = stats[metricIdx];
        if (s.count <= 0) {
            m_historyStatLabels[metricIdx]->setVisible(false);
            continue;
        }
        const QString unit = metricUnits.value(metricIdx);
        m_historyStatLabels[metricIdx]->setText(
            QStringLiteral("%1\n最小 %2%6  最大 %3%6\n均值 %4%6  最新 %5%6\n共 %7 条")
                .arg(metricNames.value(metricIdx),
                     QString::number(s.minVal, 'f', 1),
                     QString::number(s.maxVal, 'f', 1),
                     QString::number(s.avgVal, 'f', 1),
                     QString::number(s.latestVal, 'f', 1),
                     unit)
                .arg(s.count));
    }
}

void MainWindow::ensureChartBuilt(int metricIdx) {
    if (metricIdx < 0 || metricIdx >= 4) return;
    if (m_historyCharts.value(metricIdx) != nullptr) return;

    QWidget* card = m_historyChartCards.value(metricIdx);
    if (!card) return;

    QVBoxLayout* cardLayout = qobject_cast<QVBoxLayout*>(card->layout());
    if (!cardLayout) return;

    QLayoutItem* item = cardLayout->itemAt(1);
    if (item && item->widget()) {
        cardLayout->removeWidget(item->widget());
        delete item->widget();
    }

    const QList<QColor> metricColors = {
        QColor(248, 113, 113),
        QColor(56, 189, 248),
        QColor(251, 191, 36),
        QColor(34, 197, 94)
    };

    auto* chart = new QChart();
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
    chart->legend()->setVisible(false);

    auto* axisX = new QDateTimeAxis(this);
    axisX->setFormat(QStringLiteral("MM-dd HH:mm"));
    axisX->setLabelsColor(QColor(0x9a, 0xba, 0xda));
    chart->addAxis(axisX, Qt::AlignBottom);

    auto* axisY = new QValueAxis(this);
    axisY->setLabelsColor(QColor(0x9a, 0xba, 0xda));
    axisY->setGridLineColor(QColor(125, 211, 252, 35));
    chart->addAxis(axisY, Qt::AlignLeft);

    auto* series = new QLineSeries(this);
    QPen pen(metricColors.value(metricIdx, QColor(125, 211, 252)));
    pen.setWidthF(2.2);
    series->setPen(pen);
    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    auto* chartView = new QChartView(chart, card);
    chartView->setRenderHint(QPainter::Antialiasing, false);
    chartView->setMinimumHeight(240);
    chartView->setStyleSheet("background:transparent;border:none;");
    chartView->installEventFilter(this);

    cardLayout->addWidget(chartView);

    m_historyCharts[metricIdx] = chart;
    m_historyAxisXs[metricIdx] = axisX;
    m_historyAxisYs[metricIdx] = axisY;
    m_historyLineSeries[metricIdx] = series;
    m_historyChartViews[metricIdx] = chartView;
}


void MainWindow::onExportHistoryClicked() {
    if (m_db == nullptr) {
        customMessage(this, "导出失败", "数据库未初始化。", true);
        return;
    }

    const QDateTime rangeStart = m_historyStartCombo
                                     ? m_historyStartCombo->dateTime()
                                     : QDateTime::currentDateTime().addDays(-1);
    const QDateTime rangeEnd = m_historyEndCombo
                                   ? m_historyEndCombo->dateTime()
                                   : QDateTime::currentDateTime();

    if (rangeStart >= rangeEnd) {
        customMessage(this, "导出失败", "起始时间必须早于结束时间，请重新选择。", true);
        return;
    }

    bool hasCheckedMetric = false;
    for (QCheckBox* check : std::as_const(m_historyMetricChecks)) {
        if (check != nullptr && check->isChecked()) {
            hasCheckedMetric = true;
            break;
        }
    }
    if (!hasCheckedMetric) {
        customMessage(this, "导出失败", "请至少勾选一个指标。", true);
        return;
    }

    QString rowsErr;
    QList<SensorData> exportRows = m_db->queryDataRange(rangeStart, rangeEnd, &rowsErr);
    if (!rowsErr.isEmpty()) {
        customMessage(this, "导出失败", QString("读取历史数据失败：%1").arg(rowsErr), true);
        return;
    }
    if (exportRows.isEmpty()) {
        customMessage(this, "导出失败", "当前没有可导出的历史数据。", true);
        return;
    }

    m_historyPoints.clear();
    m_historyPoints.reserve(exportRows.size());
    for (const SensorData& row : exportRows) {
        m_historyPoints.append({row.ts.toString("MM-dd HH:mm:ss"),
                                row.tempC,
                                row.humiPercent,
                                row.pm25UgM3,
                                row.airIndex,
                                row.currentA,
                                row.flowLMin,
                                row.ts});
    }
    QString exportDir = "D:/SystemData";
    QDir dRoot("D:/");
    if (dRoot.exists()) {
        const QStringList dirFilters = {"SystemData*"};
        const QFileInfoList candidates = dRoot.entryInfoList(
            dirFilters,
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name);
        if (!candidates.isEmpty()) {
            exportDir = candidates.first().absoluteFilePath();
        } else if (!QDir(exportDir).exists()) {
            dRoot.mkpath("SystemData");
        }
    }

    const QString exportRangeText = QString("%1~%2")
        .arg(rangeStart.toString("yyyyMMdd-HHmm"), rangeEnd.toString("yyyyMMdd-HHmm"));
    const QString defaultFileName =
        QString("历史数据报表_%1_%2.xlsx")
            .arg(exportRangeText, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString initialPath =
        QDir::toNativeSeparators(exportDir + "/" + defaultFileName);

    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        "导出 Excel 报表",
        initialPath,
        "Excel 工作簿 (*.xlsx)");

    if (selectedPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!exportHistoryAsXlsx(selectedPath, &errorMessage)) {
        customMessage(this, "导出失败",
            QString("无法生成 XLSX 文件：%1").arg(errorMessage), true);
        return;
    }

    showExportSuccessDialog(this, QDir::toNativeSeparators(selectedPath));
}

void MainWindow::exportWaterPowerAnalysis(bool includeSingleDay, bool includeTrend) {
    QString exportDir = QStringLiteral("D:/SystemData/WaterElec");
    QDir().mkpath(exportDir);

    const QString defaultFileName =
        QStringLiteral("水电分析_%1.xlsx")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString initialPath = QDir::toNativeSeparators(exportDir + "/" + defaultFileName);

    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出当前分析"),
        initialPath,
        QStringLiteral("Excel 工作簿 (*.xlsx)"));

    if (selectedPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!exportWaterPowerAnalysisAsXlsx(selectedPath, includeSingleDay, includeTrend, &errorMessage)) {
        customMessage(this, QStringLiteral("导出失败"),
                      QStringLiteral("无法生成 XLSX 文件：%1").arg(errorMessage), true);
        return;
    }

    showExportSuccessDialog(this, QDir::toNativeSeparators(selectedPath));
}

bool MainWindow::exportHistoryAsXlsx(const QString& filePath, QString* errorMessage) {
    auto setError = [&](const QString& text) {
        if (errorMessage != nullptr) *errorMessage = text;
        return false;
    };

    auto escapeXml = [](QString text) {
        text.replace('&', "&amp;");
        text.replace('<', "&lt;");
        text.replace('>', "&gt;");
        text.replace('"', "&quot;");
        text.replace('\'', "&apos;");
        return text;
    };

    auto writeUtf8File = [&](const QString& path, const QString& content) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        const QByteArray bytes = content.toUtf8();
        const qint64 written = f.write(bytes);
        f.close();
        return written == bytes.size();
    };

    QVector<int> selectedMetrics;
    for (int i = 0; i < m_historyMetricChecks.size(); ++i) {
        if (m_historyMetricChecks[i] != nullptr && m_historyMetricChecks[i]->isChecked()) {
            selectedMetrics.append(i);
        }
    }
    if (selectedMetrics.isEmpty()) {
        return setError(QStringLiteral("请至少勾选一个指标"));
    }

    const QStringList metricNames = {
        QStringLiteral("温度"),
        QStringLiteral("湿度"),
        QStringLiteral("PM2.5"),
        QStringLiteral("空气指数")
    };
    const QStringList metricUnits = {
        QStringLiteral("℃"),
        QStringLiteral("%"),
        QStringLiteral("ug/m3"),
        QString()
    };

    const QDateTime rangeStart = m_historyStartCombo ? m_historyStartCombo->dateTime()
                                                     : QDateTime::currentDateTime().addDays(-1);
    const QDateTime rangeEnd = m_historyEndCombo ? m_historyEndCombo->dateTime()
                                                 : QDateTime::currentDateTime();
    const QString exportTime = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString rangeText = QStringLiteral("%1 ~ %2")
                                  .arg(rangeStart.toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                                       rangeEnd.toString(QStringLiteral("yyyy-MM-dd HH:mm")));

    struct MetricSheet {
        QString name;
        QString unit;
        QList<double> values;
        double minVal = 0.0;
        double maxVal = 0.0;
        double avgVal = 0.0;
        double latestVal = 0.0;
        int count = 0;
    };

    QList<MetricSheet> sheetInfos;
    for (int metricIdx : selectedMetrics) {
        MetricSheet info;
        info.name = metricNames.value(metricIdx);
        info.unit = metricUnits.value(metricIdx);
        double sum = 0.0;
        double minVal = 1e18;
        double maxVal = -1e18;
        for (const auto& point : std::as_const(m_historyPoints)) {
            double value = 0.0;
            switch (metricIdx) {
            case 0: value = point.temp; break;
            case 1: value = point.humi; break;
            case 2: value = point.pm25; break;
            case 3: value = point.airIndex; break;
            default: break;
            }
            info.values.append(value);
            sum += value;
            minVal = qMin(minVal, value);
            maxVal = qMax(maxVal, value);
            info.latestVal = value;
            info.count++;
        }
        if (info.count > 0) {
            info.minVal = minVal;
            info.maxVal = maxVal;
            info.avgVal = sum / info.count;
        }
        sheetInfos.append(info);
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return setError(QStringLiteral("无法创建临时目录"));
    }

    QDir root(tempDir.path());
    root.mkpath(QStringLiteral("_rels"));
    root.mkpath(QStringLiteral("docProps"));
    root.mkpath(QStringLiteral("xl/_rels"));
    root.mkpath(QStringLiteral("xl/worksheets"));

    QString contentTypes =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                       "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                       "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                       "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>");
    for (int i = 0; i < sheetInfos.size(); ++i) {
        contentTypes += QStringLiteral("<Override PartName=\"/xl/worksheets/sheet%1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>")
                            .arg(i + 1);
    }
    contentTypes += QStringLiteral(
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
        "</Types>");

    const QString rels =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                       "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
                       "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
                       "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
                       "</Relationships>");

    QString workbook =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                       "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>");
    QString workbookRels =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
    for (int i = 0; i < sheetInfos.size(); ++i) {
        workbook += QStringLiteral("<sheet name=\"%1\" sheetId=\"%2\" r:id=\"rId%2\"/>")
                        .arg(escapeXml(sheetInfos[i].name))
                        .arg(i + 1);
        workbookRels += QStringLiteral("<Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%1.xml\"/>")
                            .arg(i + 1);
    }
    workbook += QStringLiteral("</sheets></workbook>");
    workbookRels += QStringLiteral("<Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>")
                        .arg(sheetInfos.size() + 1);
    workbookRels += QStringLiteral("</Relationships>");

    const QString appXml =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
                       "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
                       "<Application>System_UI</Application></Properties>");

    const QString coreXml =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
                       "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                       "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
                       "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
                       "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
                       "<dc:title>历史数据报表</dc:title><dc:creator>System_UI</dc:creator>"
                       "<cp:lastModifiedBy>System_UI</cp:lastModifiedBy>"
                       "<dcterms:created xsi:type=\"dcterms:W3CDTF\">%1</dcterms:created>"
                       "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">%1</dcterms:modified>"
                       "</cp:coreProperties>")
            .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ")));

    const QString stylesXml =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                       "<fonts count=\"3\">"
                       "<font><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>"
                       "<font><b/><sz val=\"16\"/><color rgb=\"FFFFFFFF\"/><name val=\"Microsoft YaHei\"/></font>"
                       "<font><b/><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>"
                       "</fonts>"
                       "<fills count=\"4\">"
                       "<fill><patternFill patternType=\"none\"/></fill>"
                       "<fill><patternFill patternType=\"gray125\"/></fill>"
                       "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF1F4E78\"/><bgColor indexed=\"64\"/></patternFill></fill>"
                       "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFDCE6F2\"/><bgColor indexed=\"64\"/></patternFill></fill>"
                       "</fills>"
                       "<borders count=\"2\">"
                       "<border><left/><right/><top/><bottom/><diagonal/></border>"
                       "<border><left style=\"thin\"/><right style=\"thin\"/><top style=\"thin\"/><bottom style=\"thin\"/><diagonal/></border>"
                       "</borders>"
                       "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
                       "<cellXfs count=\"4\">"
                       "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
                       "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
                       "<xf numFmtId=\"0\" fontId=\"2\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
                       "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"left\" vertical=\"center\"/></xf>"
                       "</cellXfs>"
                       "</styleSheet>");

    auto inlineCell = [&](const QString& ref, const QString& text, int style) {
        return QStringLiteral("<c r=\"%1\" t=\"inlineStr\" s=\"%2\"><is><t>%3</t></is></c>")
            .arg(ref)
            .arg(style)
            .arg(escapeXml(text));
    };
    auto numberCell = [&](const QString& ref, double value, int style) {
        return QStringLiteral("<c r=\"%1\" s=\"%2\"><v>%3</v></c>")
            .arg(ref)
            .arg(style)
            .arg(QString::number(value, 'f', 1));
    };

    QStringList sheetXmls;
    for (const MetricSheet& info : std::as_const(sheetInfos)) {
        QString sheetXml =
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                           "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                           "<cols>"
                           "<col min=\"1\" max=\"1\" width=\"10\" customWidth=\"1\"/>"
                           "<col min=\"2\" max=\"2\" width=\"24\" customWidth=\"1\"/>"
                           "<col min=\"3\" max=\"3\" width=\"18\" customWidth=\"1\"/>"
                           "</cols><sheetData>");

        int row = 1;
        sheetXml += QStringLiteral("<row r=\"%1\" ht=\"30\" customHeight=\"1\">").arg(row);
        sheetXml += inlineCell(QStringLiteral("A1"), QStringLiteral("%1历史数据报表").arg(info.name), 1);
        sheetXml += QStringLiteral("</row>");
        ++row;

        auto addInfoRow = [&](const QString& key, const QString& value) {
            sheetXml += QStringLiteral("<row r=\"%1\">").arg(row);
            sheetXml += inlineCell(QStringLiteral("A%1").arg(row), key, 3);
            sheetXml += inlineCell(QStringLiteral("B%1").arg(row), value, 3);
            sheetXml += QStringLiteral("</row>");
            ++row;
        };
        addInfoRow(QStringLiteral("导出时间"), exportTime);
        addInfoRow(QStringLiteral("时间范围"), rangeText);
        addInfoRow(QStringLiteral("记录数"), QString::number(info.count));
        addInfoRow(QStringLiteral("最小值"), QStringLiteral("%1 %2").arg(QString::number(info.minVal, 'f', 1), info.unit));
        addInfoRow(QStringLiteral("最大值"), QStringLiteral("%1 %2").arg(QString::number(info.maxVal, 'f', 1), info.unit));
        addInfoRow(QStringLiteral("均值"), QStringLiteral("%1 %2").arg(QString::number(info.avgVal, 'f', 1), info.unit));
        addInfoRow(QStringLiteral("最新值"), QStringLiteral("%1 %2").arg(QString::number(info.latestVal, 'f', 1), info.unit));

        ++row;
        sheetXml += QStringLiteral("<row r=\"%1\">").arg(row);
        sheetXml += inlineCell(QStringLiteral("A%1").arg(row), QStringLiteral("序号"), 2);
        sheetXml += inlineCell(QStringLiteral("B%1").arg(row), QStringLiteral("时间点"), 2);
        sheetXml += inlineCell(QStringLiteral("C%1").arg(row), QStringLiteral("%1(%2)").arg(info.name, info.unit), 2);
        sheetXml += QStringLiteral("</row>");
        ++row;

        for (int i = 0; i < m_historyPoints.size() && i < info.values.size(); ++i) {
            sheetXml += QStringLiteral("<row r=\"%1\">").arg(row);
            sheetXml += QStringLiteral("<c r=\"A%1\" s=\"3\"><v>%2</v></c>").arg(QString::number(row), QString::number(i + 1));
            sheetXml += inlineCell(QStringLiteral("B%1").arg(row), m_historyPoints[i].timeLabel, 3);
            sheetXml += numberCell(QStringLiteral("C%1").arg(row), info.values[i], 3);
            sheetXml += QStringLiteral("</row>");
            ++row;
        }

        sheetXml += QStringLiteral("</sheetData><mergeCells count=\"1\"><mergeCell ref=\"A1:C1\"/></mergeCells></worksheet>");
        sheetXmls.append(sheetXml);
    }

    if (!writeUtf8File(root.filePath(QStringLiteral("[Content_Types].xml")), contentTypes) ||
        !writeUtf8File(root.filePath(QStringLiteral("_rels/.rels")), rels) ||
        !writeUtf8File(root.filePath(QStringLiteral("docProps/app.xml")), appXml) ||
        !writeUtf8File(root.filePath(QStringLiteral("docProps/core.xml")), coreXml) ||
        !writeUtf8File(root.filePath(QStringLiteral("xl/workbook.xml")), workbook) ||
        !writeUtf8File(root.filePath(QStringLiteral("xl/_rels/workbook.xml.rels")), workbookRels) ||
        !writeUtf8File(root.filePath(QStringLiteral("xl/styles.xml")), stylesXml)) {
        return setError(QStringLiteral("临时文件写入失败"));
    }
    for (int i = 0; i < sheetXmls.size(); ++i) {
        if (!writeUtf8File(root.filePath(QStringLiteral("xl/worksheets/sheet%1.xml").arg(i + 1)), sheetXmls[i])) {
            return setError(QStringLiteral("临时文件写入失败"));
        }
    }

    QFile::remove(filePath);
    const QString zipPath = QFileInfo(filePath).absolutePath() + QStringLiteral("/.__tmp_history_export__.zip");
    QFile::remove(zipPath);
    QProcess zipProcess;
    QStringList args;
    args << QStringLiteral("-NoProfile")
         << QStringLiteral("-Command")
         << QStringLiteral("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
                .arg(QDir::toNativeSeparators(tempDir.path()).replace('\'', QStringLiteral("''")),
                     QDir::toNativeSeparators(zipPath).replace('\'', QStringLiteral("''")));
    zipProcess.start(QStringLiteral("powershell"), args);
    if (!zipProcess.waitForFinished(20000)) {
        zipProcess.kill();
        return setError(QStringLiteral("打包超时"));
    }
    if (zipProcess.exitStatus() != QProcess::NormalExit || zipProcess.exitCode() != 0) {
        return setError(QString::fromLocal8Bit(zipProcess.readAllStandardError()));
    }
    if (!QFile::exists(zipPath)) {
        return setError(QStringLiteral("未生成 ZIP 临时文件"));
    }
    if (!QFile::rename(zipPath, filePath)) {
        QFile::remove(filePath);
        if (!QFile::rename(zipPath, filePath)) {
            return setError(QStringLiteral("ZIP 重命名为 XLSX 失败"));
        }
    }
    if (!QFile::exists(filePath)) {
        return setError(QStringLiteral("未生成目标 XLSX 文件"));
    }
    return true;
}

bool MainWindow::exportWaterPowerAnalysisAsXlsx(const QString& filePath,
                                                bool includeSingleDay,
                                                bool includeTrend,
                                                QString* errorMessage) {
    auto setError = [&](const QString& text) {
        if (errorMessage != nullptr) *errorMessage = text;
        return false;
    };

    auto escapeXml = [](QString text) {
        text.replace('&', "&amp;");
        text.replace('<', "&lt;");
        text.replace('>', "&gt;");
        text.replace('"', "&quot;");
        text.replace('\'', "&apos;");
        return text;
    };

    auto writeUtf8File = [&](const QString& path, const QString& content) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const qint64 written = f.write(content.toUtf8());
        f.close();
        return written == content.toUtf8().size();
    };

    auto colName = [](int index) {
        QString name;
        int n = index;
        while (n > 0) {
            const int rem = (n - 1) % 26;
            name.prepend(QChar('A' + rem));
            n = (n - 1) / 26;
        }
        return name;
    };

    const QDate selectedDay = m_wpAnalysisDateEdit ? m_wpAnalysisDateEdit->date() : QDate::currentDate().addDays(-1);
    const int bucketMinutes = 60;
    const int trendDays = (m_wpTrendDaysCombo && m_wpTrendDaysCombo->currentIndex() == 1) ? 15
                        : (m_wpTrendDaysCombo && m_wpTrendDaysCombo->currentIndex() == 2) ? 30
                        : 7;

    struct SingleRow {
        QString startTime;
        QString endTime;
        double powerMAh = 0.0;
        double waterL = 0.0;
        bool powerPeak = false;
        bool waterPeak = false;
    };
    QList<SingleRow> singleRows;
    double totalPowerMAh = 0.0, totalWaterL = 0.0, avgPower = 0.0, avgWater = 0.0, peakPower = 0.0, peakWater = 0.0;
    QString peakPowerTime, peakWaterTime;
    double activePowerHours = 0.0, activeWaterHours = 0.0;

    if (includeSingleDay) {
        const QDateTime dayStart(selectedDay, QTime(0, 0, 0));
        const QDateTime dayEnd(selectedDay, QTime(23, 59, 59));
        QString err;
        const QList<SensorData> points = m_db ? m_db->queryDataRange(dayStart, dayEnd, &err) : QList<SensorData>{};
        if (!err.isEmpty()) qDebug() << "[DB] export single-day query failed:" << err;

        struct BucketUsage { double powerMAh = 0.0; double waterL = 0.0; };
        QMap<int, BucketUsage> bucketMap;
        for (int i = 0; i < points.size(); ++i) {
            double dtSec = 1.0;
            if (i + 1 < points.size()) {
                dtSec = qBound(1.0, static_cast<double>(points[i].ts.secsTo(points[i + 1].ts)), 3600.0);
            }
            const int bucketIdx = qBound(0, points[i].ts.time().msecsSinceStartOfDay() / (bucketMinutes * 60 * 1000),
                                         (24 * 60 / bucketMinutes) - 1);
            BucketUsage& bucket = bucketMap[bucketIdx];
            bucket.powerMAh += qMax(0.0, points[i].currentA * dtSec / 3600.0);
            bucket.waterL += qMax(0.0, points[i].flowLMin * dtSec / 60.0);
        }

        const int bucketCount = 24 * 60 / bucketMinutes;
        for (int i = 0; i < bucketCount; ++i) {
            const QDateTime start = dayStart.addSecs(i * bucketMinutes * 60);
            const QDateTime end = start.addSecs(bucketMinutes * 60);
            const auto bucket = bucketMap.value(i);
            totalPowerMAh += bucket.powerMAh;
            totalWaterL += bucket.waterL;
            peakPower = qMax(peakPower, bucket.powerMAh);
            peakWater = qMax(peakWater, bucket.waterL);
            singleRows.append({start.toString("HH:mm"),
                               end.toString("HH:mm"),
                               bucket.powerMAh,
                               bucket.waterL,
                               false,
                               false});
        }
        if (bucketCount > 0) {
            avgPower = totalPowerMAh / bucketCount;
            avgWater = totalWaterL / bucketCount;
        }
        for (auto& row : singleRows) {
            if (qFuzzyCompare(row.powerMAh + 1.0, peakPower + 1.0) && peakPower > 0.0) {
                row.powerPeak = true;
                peakPowerTime = row.startTime;
            }
            if (qFuzzyCompare(row.waterL + 1.0, peakWater + 1.0) && peakWater > 0.0) {
                row.waterPeak = true;
                peakWaterTime = row.startTime;
            }
            if (row.powerMAh > 0.01) activePowerHours += bucketMinutes / 60.0;
            if (row.waterL > 0.001) activeWaterHours += bucketMinutes / 60.0;
        }
    }

    struct TrendRow {
        QString day;
        double powerMAh = 0.0;
        double waterL = 0.0;
        bool powerPeak = false;
        bool waterPeak = false;
    };
    QList<TrendRow> trendRows;
    double trendTotalPower = 0.0, trendTotalWater = 0.0, trendAvgPower = 0.0, trendAvgWater = 0.0;
    double trendPeakPower = 0.0, trendPeakWater = 0.0;
    QString trendPeakPowerDay, trendPeakWaterDay;

    if (includeTrend) {
        const QDate trendEnd = QDate::currentDate().addDays(-1);
        const QDate trendStart = trendEnd.addDays(-(trendDays - 1));
        QString err;
        const QList<DatabaseManager::DailyResourceUsageEntry> rows =
            m_db ? m_db->queryDailyResourceUsage(trendStart, trendEnd, &err) : QList<DatabaseManager::DailyResourceUsageEntry>{};
        if (!err.isEmpty()) qDebug() << "[DB] export trend query failed:" << err;
        QMap<QString, DatabaseManager::DailyResourceUsageEntry> trendMap;
        for (const auto& row : rows) trendMap.insert(row.day, row);
        for (int i = 0; i < trendDays; ++i) {
            const QDate day = trendStart.addDays(i);
            const auto row = trendMap.value(day.toString(Qt::ISODate), DatabaseManager::DailyResourceUsageEntry{});
            TrendRow tr;
            tr.day = day.toString("yyyy-MM-dd");
            tr.powerMAh = row.powerMAh;
            tr.waterL = row.waterCL / 100.0;
            trendRows.append(tr);
            trendTotalPower += tr.powerMAh;
            trendTotalWater += tr.waterL;
            trendPeakPower = qMax(trendPeakPower, tr.powerMAh);
            trendPeakWater = qMax(trendPeakWater, tr.waterL);
        }
        if (trendDays > 0) {
            trendAvgPower = trendTotalPower / trendDays;
            trendAvgWater = trendTotalWater / trendDays;
        }
        for (auto& row : trendRows) {
            if (qFuzzyCompare(row.powerMAh + 1.0, trendPeakPower + 1.0) && trendPeakPower > 0.0) {
                row.powerPeak = true;
                trendPeakPowerDay = row.day;
            }
            if (qFuzzyCompare(row.waterL + 1.0, trendPeakWater + 1.0) && trendPeakWater > 0.0) {
                row.waterPeak = true;
                trendPeakWaterDay = row.day;
            }
        }
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return setError(QStringLiteral("无法创建临时目录"));
    QDir root(tempDir.path());
    root.mkpath("_rels");
    root.mkpath("docProps");
    root.mkpath("xl/_rels");
    root.mkpath("xl/worksheets");

    const QString contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet3.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
        "</Types>";

    const QString rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
        "</Relationships>";

    const QString workbook =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets>"
        "<sheet name=\"导出说明\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"单日分时\" sheetId=\"2\" r:id=\"rId2\"/>"
        "<sheet name=\"多日趋势\" sheetId=\"3\" r:id=\"rId3\"/>"
        "</sheets>"
        "</workbook>";

    const QString workbookRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet3.xml\"/>"
        "<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";

    const QString appXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
        "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
        "<Application>OpenAI Codex</Application>"
        "</Properties>";

    const QString coreXml =
        QString("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
                "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
                "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
                "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
                "<dc:title>%1</dc:title>"
                "<dc:creator>OpenAI Codex</dc:creator>"
                "<cp:lastModifiedBy>OpenAI Codex</cp:lastModifiedBy>"
                "<dcterms:created xsi:type=\"dcterms:W3CDTF\">%2</dcterms:created>"
                "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">%2</dcterms:modified>"
                "</cp:coreProperties>")
            .arg(escapeXml(QStringLiteral("水电分析导出")),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    const QString stylesXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"3\">"
        "<font><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>"
        "<font><b/><sz val=\"16\"/><color rgb=\"FFFFFFFF\"/><name val=\"Microsoft YaHei\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>"
        "</fonts>"
        "<fills count=\"4\">"
        "<fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF0F5FA8\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFDDEBFF\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "</fills>"
        "<borders count=\"2\">"
        "<border><left/><right/><top/><bottom/><diagonal/></border>"
        "<border><left style=\"thin\"/><right style=\"thin\"/><top style=\"thin\"/><bottom style=\"thin\"/><diagonal/></border>"
        "</borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"6\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"2\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"left\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"left\" vertical=\"top\" wrapText=\"1\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"center\" vertical=\"center\"/></xf>"
        "</cellXfs>"
        "</styleSheet>";

    auto inlineCell = [&](const QString& ref, const QString& text, int style) {
        return QString("<c r=\"%1\" t=\"inlineStr\" s=\"%2\"><is><t>%3</t></is></c>")
            .arg(ref, QString::number(style), escapeXml(text));
    };
    auto numberCell = [&](const QString& ref, double value, int style) {
        return QString("<c r=\"%1\" s=\"%2\"><v>%3</v></c>")
            .arg(ref, QString::number(style), QString::number(value, 'f', 3));
    };

    QString sheet1 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData>";
    int row = 1;
    auto addInfo = [&](const QString& key, const QString& value) {
        sheet1 += QString("<row r=\"%1\">").arg(row);
        sheet1 += inlineCell(QString("A%1").arg(row), key, 3);
        sheet1 += inlineCell(QString("B%1").arg(row), value, 3);
        sheet1 += "</row>";
        row++;
    };
    sheet1 += QString("<row r=\"1\" ht=\"28\" customHeight=\"1\">%1</row>").arg(inlineCell("A1", QStringLiteral("水电分析导出说明"), 1));
    row = 2;
    addInfo(QStringLiteral("导出时间"), QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    addInfo(QStringLiteral("选定日期"), selectedDay.toString("yyyy-MM-dd"));
    addInfo(QStringLiteral("分时粒度"), QString::number(bucketMinutes) + QStringLiteral(" 分钟"));
    addInfo(QStringLiteral("趋势天数"), QString::number(trendDays) + QStringLiteral(" 天"));
    sheet1 += "</sheetData></worksheet>";

    QString sheet2 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData>";
    row = 1;
    sheet2 += QString("<row r=\"1\">%1%2%3%4%5%6%7</row>")
                  .arg(inlineCell("A1", QStringLiteral("序号"), 2),
                       inlineCell("B1", QStringLiteral("开始时间"), 2),
                       inlineCell("C1", QStringLiteral("结束时间"), 2),
                       inlineCell("D1", QStringLiteral("用电(mAh)"), 2),
                       inlineCell("E1", QStringLiteral("用水(L)"), 2),
                       inlineCell("F1", QStringLiteral("是否用电峰值"), 2),
                       inlineCell("G1", QStringLiteral("是否用水峰值"), 2));
    row = 2;
    if (includeSingleDay) {
        for (int i = 0; i < singleRows.size(); ++i, ++row) {
            const auto& r = singleRows[i];
            sheet2 += QString("<row r=\"%1\">").arg(row);
            sheet2 += numberCell(QString("A%1").arg(row), i + 1, 5);
            sheet2 += inlineCell(QString("B%1").arg(row), r.startTime, 3);
            sheet2 += inlineCell(QString("C%1").arg(row), r.endTime, 3);
            sheet2 += numberCell(QString("D%1").arg(row), r.powerMAh, 3);
            sheet2 += numberCell(QString("E%1").arg(row), r.waterL, 3);
            sheet2 += inlineCell(QString("F%1").arg(row), r.powerPeak ? QStringLiteral("是") : QStringLiteral(""), 5);
            sheet2 += inlineCell(QString("G%1").arg(row), r.waterPeak ? QStringLiteral("是") : QStringLiteral(""), 5);
            sheet2 += "</row>";
        }
        row++;
        auto addSummary2 = [&](const QString& key, const QString& value) {
            sheet2 += QString("<row r=\"%1\">").arg(row);
            sheet2 += inlineCell(QString("A%1").arg(row), key, 3);
            sheet2 += inlineCell(QString("B%1").arg(row), value, 3);
            sheet2 += "</row>";
            row++;
        };
        addSummary2(QStringLiteral("单日总用电"), QString::number(totalPowerMAh, 'f', 0) + QStringLiteral(" mAh"));
        addSummary2(QStringLiteral("单日总用水"), QString::number(totalWaterL, 'f', 1) + QStringLiteral(" L"));
        addSummary2(QStringLiteral("平均每时段用电"), QString::number(avgPower, 'f', 1) + QStringLiteral(" mAh"));
        addSummary2(QStringLiteral("平均每时段用水"), QString::number(avgWater, 'f', 2) + QStringLiteral(" L"));
        addSummary2(QStringLiteral("用电峰值时段"), peakPowerTime.isEmpty() ? QStringLiteral("--") : peakPowerTime);
        addSummary2(QStringLiteral("用水峰值时段"), peakWaterTime.isEmpty() ? QStringLiteral("--") : peakWaterTime);
        addSummary2(QStringLiteral("用电活跃时长"), QString::number(activePowerHours, 'f', 1) + QStringLiteral(" 小时"));
        addSummary2(QStringLiteral("用水活跃时长"), QString::number(activeWaterHours, 'f', 1) + QStringLiteral(" 小时"));
    }
    sheet2 += "</sheetData></worksheet>";

    QString sheet3 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData>";
    row = 1;
    sheet3 += QString("<row r=\"1\">%1%2%3%4%5%6</row>")
                  .arg(inlineCell("A1", QStringLiteral("序号"), 2),
                       inlineCell("B1", QStringLiteral("日期"), 2),
                       inlineCell("C1", QStringLiteral("日用电(mAh)"), 2),
                       inlineCell("D1", QStringLiteral("日用水(L)"), 2),
                       inlineCell("E1", QStringLiteral("是否用电峰值日"), 2),
                       inlineCell("F1", QStringLiteral("是否用水峰值日"), 2));
    row = 2;
    if (includeTrend) {
        for (int i = 0; i < trendRows.size(); ++i, ++row) {
            const auto& r = trendRows[i];
            sheet3 += QString("<row r=\"%1\">").arg(row);
            sheet3 += numberCell(QString("A%1").arg(row), i + 1, 5);
            sheet3 += inlineCell(QString("B%1").arg(row), r.day, 3);
            sheet3 += numberCell(QString("C%1").arg(row), r.powerMAh, 3);
            sheet3 += numberCell(QString("D%1").arg(row), r.waterL, 3);
            sheet3 += inlineCell(QString("E%1").arg(row), r.powerPeak ? QStringLiteral("是") : QStringLiteral(""), 5);
            sheet3 += inlineCell(QString("F%1").arg(row), r.waterPeak ? QStringLiteral("是") : QStringLiteral(""), 5);
            sheet3 += "</row>";
        }
        row++;
        auto addSummary3 = [&](const QString& key, const QString& value) {
            sheet3 += QString("<row r=\"%1\">").arg(row);
            sheet3 += inlineCell(QString("A%1").arg(row), key, 3);
            sheet3 += inlineCell(QString("B%1").arg(row), value, 3);
            sheet3 += "</row>";
            row++;
        };
        addSummary3(QStringLiteral("区间总用电"), QString::number(trendTotalPower, 'f', 0) + QStringLiteral(" mAh"));
        addSummary3(QStringLiteral("区间总用水"), QString::number(trendTotalWater, 'f', 1) + QStringLiteral(" L"));
        addSummary3(QStringLiteral("日均用电"), QString::number(trendAvgPower, 'f', 1) + QStringLiteral(" mAh"));
        addSummary3(QStringLiteral("日均用水"), QString::number(trendAvgWater, 'f', 1) + QStringLiteral(" L"));
        addSummary3(QStringLiteral("用电峰值日"), trendPeakPowerDay.isEmpty() ? QStringLiteral("--") : trendPeakPowerDay);
        addSummary3(QStringLiteral("用水峰值日"), trendPeakWaterDay.isEmpty() ? QStringLiteral("--") : trendPeakWaterDay);
    }
    sheet3 += "</sheetData></worksheet>";

    if (!writeUtf8File(root.filePath("[Content_Types].xml"), contentTypes) ||
        !writeUtf8File(root.filePath("_rels/.rels"), rels) ||
        !writeUtf8File(root.filePath("docProps/app.xml"), appXml) ||
        !writeUtf8File(root.filePath("docProps/core.xml"), coreXml) ||
        !writeUtf8File(root.filePath("xl/workbook.xml"), workbook) ||
        !writeUtf8File(root.filePath("xl/_rels/workbook.xml.rels"), workbookRels) ||
        !writeUtf8File(root.filePath("xl/styles.xml"), stylesXml) ||
        !writeUtf8File(root.filePath("xl/worksheets/sheet1.xml"), sheet1) ||
        !writeUtf8File(root.filePath("xl/worksheets/sheet2.xml"), sheet2) ||
        !writeUtf8File(root.filePath("xl/worksheets/sheet3.xml"), sheet3)) {
        return setError(QStringLiteral("临时文件写入失败"));
    }

    QFile::remove(filePath);
    const QString zipPath = QFileInfo(filePath).absolutePath() + "/.__tmp_water_elec_export__.zip";
    QFile::remove(zipPath);
    QProcess zipProcess;
    QStringList args;
    args << "-NoProfile"
         << "-Command"
         << QString("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
                .arg(QDir::toNativeSeparators(tempDir.path()).replace('\'', "''"),
                     QDir::toNativeSeparators(zipPath).replace('\'', "''"));
    zipProcess.start("powershell", args);
    if (!zipProcess.waitForFinished(20000)) {
        zipProcess.kill();
        return setError(QStringLiteral("打包超时"));
    }
    if (zipProcess.exitStatus() != QProcess::NormalExit || zipProcess.exitCode() != 0) {
        return setError(QString::fromLocal8Bit(zipProcess.readAllStandardError()));
    }
    if (!QFile::exists(zipPath)) {
        return setError(QStringLiteral("未生成 ZIP 临时文件"));
    }
    if (!QFile::rename(zipPath, filePath)) {
        QFile::remove(filePath);
        if (!QFile::rename(zipPath, filePath)) {
            return setError(QStringLiteral("ZIP 重命名为 XLSX 失败"));
        }
    }
    return true;
}

void MainWindow::addAlarmRecord(const QString& timeText,
                                const QString& sensorText,
                                const QString& contentText,
                                const QString& level,
                                int alarmCode) {
    if (m_alarmInfoTable == nullptr) {
        return;
    }

    if (m_db != nullptr) {
        QString insErr;
        if (!m_db->insertAlarmInfo(timeText, sensorText, contentText, level, alarmCode, &insErr)) {
            qDebug() << "[DB] insert alarm_info failed:" << insErr;
        }
    }

    // 直接刷新避免手动拼接表格
    refreshAlarmInfoFromDatabase();
}

void MainWindow::refreshAlarmInfoFromDatabase() {
    if (m_alarmInfoTable == nullptr || m_db == nullptr) {
        return;
    }

    // 防止重建过程中界面闪烁/卡顿
    m_alarmInfoTable->setUpdatesEnabled(false);

    QString qErr;
    const QList<DatabaseManager::AlarmInfoEntry> rows = m_db->queryAlarmInfos(200, &qErr);
    if (!qErr.isEmpty()) {
        qDebug() << "[DB] query alarm_info failed:" << qErr;
        return;
    }

    // 读取筛选
    QString filterLevel;
    auto* levelFilter = ui->pageAlarm->findChild<QComboBox*>("alarmLevelFilter");
    if (levelFilter) {
        filterLevel = levelFilter->currentText();
    }

    int totalCount = 0, dangerCount = 0, warnCount = 0, helpCount = 0, doneCount = 0;
    const QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    m_alarmInfoTable->setRowCount(0);
    int rowIdx = 0;
    for (const auto& r : rows) {
        // 级别（直接从DB读取，不再推导）
        QString levelText = r.level;
        if (levelText.isEmpty()) levelText = QStringLiteral("预警");
        if (levelText == QStringLiteral("求助")) levelText = QStringLiteral("求助");
        if (levelText == QStringLiteral("严重")) levelText = QStringLiteral("严重");
        if (levelText == QStringLiteral("预警")) levelText = QStringLiteral("预警");

        QColor levelColor;
        if (levelText == QStringLiteral("严重"))      levelColor = QColor(239, 68, 68);
        else if (levelText == QStringLiteral("预警"))  levelColor = QColor(245, 158, 11);
        else if (levelText == QStringLiteral("求助"))  levelColor = QColor(168, 85, 247);  // purple
        else                                          levelColor = QColor(148, 163, 184);

        // 统计今日
        if (r.alarmTime.startsWith(today)) {
            totalCount++;
            if (levelText == QStringLiteral("严重"))      dangerCount++;
            else if (levelText == QStringLiteral("预警"))  warnCount++;
            else if (levelText == QStringLiteral("求助"))  helpCount++;
            if (r.status == QStringLiteral("resolved"))   doneCount++;
        }

        // 筛选
        if (!filterLevel.isEmpty() && filterLevel != QStringLiteral("全部") && filterLevel != levelText) {
            continue;
        }

        m_alarmInfoTable->insertRow(rowIdx);
        m_alarmInfoTable->setItem(rowIdx, 0, new QTableWidgetItem(r.alarmTime));                    // 起始
        m_alarmInfoTable->setItem(rowIdx, 1, new QTableWidgetItem(r.endTime.isEmpty() ? "—" : r.endTime)); // 结束
        m_alarmInfoTable->setItem(rowIdx, 2, new QTableWidgetItem(r.sensorName));                   // 传感器
        m_alarmInfoTable->setItem(rowIdx, 3, new QTableWidgetItem(r.alarmContent));                 // 内容
        auto* levelItem = new QTableWidgetItem(levelText);
        levelItem->setForeground(levelColor);
        levelItem->setData(Qt::TextAlignmentRole, Qt::AlignCenter);
        m_alarmInfoTable->setItem(rowIdx, 4, levelItem);  // 级别

        bool isResolved = (r.status == QStringLiteral("resolved"));
        bool isHelp = (levelText == QStringLiteral("求助"));

        // 状态：传感器=报警中/已处理(自动)，求助=求助中/已处理
        QString statusText;
        QColor statusColor;
        if (isHelp) {
            statusText = isResolved ? QStringLiteral("已解决") : QStringLiteral("求助中");
            statusColor = isResolved ? QColor(34,197,94) : QColor(168,85,247);
        } else {
            bool done = isResolved || !r.endTime.isEmpty();
            statusText = done ? QStringLiteral("已解决") : QStringLiteral("报警中");
            statusColor = done ? QColor(34,197,94) : QColor(239,68,68);
        }
        auto* statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(statusColor);
        statusItem->setData(Qt::TextAlignmentRole, Qt::AlignCenter);
        m_alarmInfoTable->setItem(rowIdx, 5, statusItem);

        // 操作：求助未处理→按钮，求助已处理→✓，传感器→自动
        if (isHelp && !isResolved) {
            auto* btn = new QPushButton(QStringLiteral("解决"));
            btn->setStyleSheet(
                "QPushButton{background:rgba(34,197,94,0.2);color:#22c55e;border:1px solid rgba(34,197,94,0.4);"
                "border-radius:4px;padding:3px 10px;font-size:11px;}"
                "QPushButton:hover{background:rgba(34,197,94,0.35);}");
            int alarmId = r.id;
            connect(btn, &QPushButton::clicked, this, [this, alarmId]() {
                onAlarmHandledClicked(alarmId);
            });
            m_alarmInfoTable->setCellWidget(rowIdx, 6, btn);
        } else if (isResolved) {
            auto* doneLabel = new QLabel(QStringLiteral("✓"));
            doneLabel->setStyleSheet("QLabel{color:#22c55e;font-size:13px;font-weight:800;}");
            doneLabel->setAlignment(Qt::AlignCenter);
            m_alarmInfoTable->setCellWidget(rowIdx, 6, doneLabel);
        } else {
            auto* autoLabel = new QLabel(QStringLiteral("自动"));
            autoLabel->setStyleSheet("QLabel{color:#64748b;font-size:11px;}");
            autoLabel->setAlignment(Qt::AlignCenter);
            m_alarmInfoTable->setCellWidget(rowIdx, 6, autoLabel);
        }
        rowIdx++;
    }

    // 更新统计数字
    auto updateStat = [&](const QString& objName, int count) {
        auto* label = ui->pageAlarm->findChild<QLabel*>(objName);
        if (label) label->setText(QString::number(count));
    };
    updateStat("alarmStatTotal", totalCount);
    updateStat("alarmStatDanger", dangerCount);
    updateStat("alarmStatWarn", warnCount);
    updateStat("alarmStatHelp", helpCount);
    updateStat("alarmStatDone", doneCount);

    m_alarmInfoTable->setUpdatesEnabled(true);
}

void MainWindow::onAlarmHandledClicked(int alarmId) {
    if (m_db == nullptr) return;
    QString err;
    if (m_db->markAlarmHandled(alarmId, &err)) {
        refreshAlarmInfoFromDatabase();
    } else {
        qDebug() << "[DB] markAlarmHandled failed:" << err;
    }
}

void MainWindow::appendRemoteControlLog(const QString& deviceId,
                                        const QString& command,
                                        const QString& result) {
    if (m_db == nullptr) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString dbErr;
    if (!m_db->insertRemoteExecLog(deviceId, command, result, now, &dbErr)) {
        qDebug() << "[DB] insert remote exec log failed:" << dbErr;
        return;
    }
    loadRemoteExecLogTable();
}

void MainWindow::loadRemoteExecLogTable() {
    if (m_remoteLogMarqueeView == nullptr || m_db == nullptr) {
        return;
    }
    QString err;
    const QList<DatabaseManager::RemoteExecLogEntry> rows = m_db->queryRemoteExecLogs(20, &err);
    if (!err.isEmpty()) {
        qDebug() << "[DB] query remote logs failed:" << err;
    }
    QStringList lines;
    if (!rows.isEmpty()) {
        for (const auto& row : rows) {
            lines.append(formatRemoteLogTableLine(row.executeTime,
                                                  row.deviceId,
                                                  row.commandText,
                                                  row.resultText));
        }
    } else {
        lines.append(QStringLiteral("暂无执行日志"));
    }
    m_remoteLogMarqueeView->setPlainText(lines.join(QLatin1Char('\n')));
    if (QScrollBar* bar = m_remoteLogMarqueeView->verticalScrollBar()) {
        bar->setValue(0);
    }
}

void MainWindow::onAddAccountClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("添加账户"));
    dialog.setModal(true);
    dialog.resize(460, 360);

    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e293b; }"
        "QLabel { color: #e2e8f0; font-size: 13px; }"
        "QLabel#addAccountTitle {"
        "  color: #f8fafc; font-size: 18px; font-weight: 800; padding-bottom: 4px;"
        "}"
        "QLabel#addAccountTip {"
        "  color: #94a3b8; font-size: 12px; padding-bottom: 12px;"
        "}"
        "QLineEdit {"
        "  background-color: #334155;"
        "  border: 1px solid #475569;"
        "  border-radius: 10px;"
        "  padding: 10px 14px;"
        "  color: #f8fafc;"
        "  font-size: 13px;"
        "  selection-background-color: #3b82f6;"
        "  selection-color: #ffffff;"
        "}"
        "QLineEdit:focus { border: 1px solid #60a5fa; background-color: #3d4f63; }"
        "QPushButton#addAccountOkBtn {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 10px;"
        "  padding: 10px 22px;"
        "  font-weight: 700;"
        "  min-width: 96px;"
        "}"
        "QPushButton#addAccountOkBtn:hover { background-color: #1d4ed8; }"
        "QPushButton#addAccountOkBtn:pressed { background-color: #1e40af; }"
        "QPushButton#addAccountCancelBtn {"
        "  background-color: transparent;"
        "  color: #cbd5e1;"
        "  border: 1px solid #475569;"
        "  border-radius: 10px;"
        "  padding: 10px 22px;"
        "  font-weight: 600;"
        "  min-width: 96px;"
        "}"
        "QPushButton#addAccountCancelBtn:hover {"
        "  background-color: #334155;"
        "  border-color: #64748b;"
        "}"
        "QLabel#addAccountErr {"
        "  color: #ef4444; font-size: 12px; padding-top: 4px;"
        "}"));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(QStringLiteral("添加账户"), &dialog);
    titleLabel->setObjectName(QStringLiteral("addAccountTitle"));
    layout->addWidget(titleLabel);

    auto* tipLabel = new QLabel(QStringLiteral("新账户将写入系统用户表，默认角色为管理员。"), &dialog);
    tipLabel->setObjectName(QStringLiteral("addAccountTip"));
    tipLabel->setWordWrap(true);
    layout->addWidget(tipLabel);

    auto* form = new QFormLayout();
    form->setSpacing(12);
    form->setContentsMargins(0, 8, 0, 8);

    auto* editUser = new QLineEdit(&dialog);
    editUser->setPlaceholderText(QStringLiteral("4-20位字母、数字或下划线"));
    form->addRow(QStringLiteral("用户名："), editUser);

    auto* editPwd = new QLineEdit(&dialog);
    editPwd->setEchoMode(QLineEdit::Password);
    editPwd->setPlaceholderText(QStringLiteral("至少6位"));
    form->addRow(QStringLiteral("密码："), editPwd);

    auto* editPwd2 = new QLineEdit(&dialog);
    editPwd2->setEchoMode(QLineEdit::Password);
    editPwd2->setPlaceholderText(QStringLiteral("再次输入密码"));
    form->addRow(QStringLiteral("确认密码："), editPwd2);

    layout->addLayout(form);

    auto* errLabel = new QLabel(&dialog);
    errLabel->setObjectName(QStringLiteral("addAccountErr"));
    errLabel->setVisible(false);
    layout->addWidget(errLabel);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    cancelBtn->setObjectName(QStringLiteral("addAccountCancelBtn"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto* okBtn = new QPushButton(QStringLiteral("确定"), &dialog);
    okBtn->setObjectName(QStringLiteral("addAccountOkBtn"));
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, [&]() {
        const QString username = editUser->text().trimmed();
        const QString password = editPwd->text();
        const QString password2 = editPwd2->text();

        if (username.isEmpty()) {
            errLabel->setText(QStringLiteral("用户名不能为空"));
            errLabel->setVisible(true);
            return;
        }
        QRegularExpression nameRx(QStringLiteral("^[A-Za-z0-9_]{4,20}$"));
        if (!nameRx.match(username).hasMatch()) {
            errLabel->setText(QStringLiteral("用户名需为4-20位字母、数字或下划线"));
            errLabel->setVisible(true);
            return;
        }
        if (password.size() < 6) {
            errLabel->setText(QStringLiteral("密码长度至少6位"));
            errLabel->setVisible(true);
            return;
        }
        if (password != password2) {
            errLabel->setText(QStringLiteral("两次密码输入不一致"));
            errLabel->setVisible(true);
            return;
        }

        const QString connectionName =
            QString("system_ui_add_account_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(QDir(DbConfig::kDbDir).filePath(DbConfig::kDbFileName));
            if (!db.open()) {
                errLabel->setText(QStringLiteral("数据库打开失败：%1").arg(db.lastError().text()));
                errLabel->setVisible(true);
                QSqlDatabase::removeDatabase(connectionName);
                return;
            }

            QSqlQuery q(db);
            q.prepare("SELECT 1 FROM users WHERE lower(username)=lower(?) LIMIT 1;");
            q.addBindValue(username);
            if (!q.exec()) {
                errLabel->setText(QStringLiteral("查询失败：%1").arg(q.lastError().text()));
                errLabel->setVisible(true);
                db.close();
                QSqlDatabase::removeDatabase(connectionName);
                return;
            }
            if (q.next()) {
                errLabel->setText(QStringLiteral("用户名已存在"));
                errLabel->setVisible(true);
                db.close();
                QSqlDatabase::removeDatabase(connectionName);
                return;
            }

            q.prepare(
                "INSERT INTO users(username,password,role,created_at) VALUES(?,?,?,?);");
            q.addBindValue(username);
            q.addBindValue(password);
            q.addBindValue(QStringLiteral("admin"));
            q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
            if (!q.exec()) {
                errLabel->setText(QStringLiteral("添加失败：%1").arg(q.lastError().text()));
                errLabel->setVisible(true);
                db.close();
                QSqlDatabase::removeDatabase(connectionName);
                return;
            }

            db.close();
            QSqlDatabase::removeDatabase(connectionName);
        }

        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        customMessage(this, QStringLiteral("添加成功"), QStringLiteral("新账户已创建。"));
    }
}

void MainWindow::onSwitchAccountClicked() {
    if (!customConfirm(this, "切换账号",
            "确定退出当前账号并返回登录页面吗？")) {
        return;
    }

    emit switchAccountRequested();
    close();
}

void MainWindow::onEditAccountInfoClicked() {
    QString currentPassword;
    QString errMsg;
    if (!loadCurrentUserBasicInfo(&currentPassword, &errMsg)) {
        customMessage(this, "读取失败", QString("读取用户信息失败：%1").arg(errMsg), true);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("修改用户信息"));
    dialog.setModal(true);
    dialog.resize(460, 320);

    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e293b; }"
        "QLabel { color: #e2e8f0; font-size: 13px; }"
        "QLabel#editAccountTitle {"
        "  color: #f8fafc; font-size: 18px; font-weight: 800; padding-bottom: 4px;"
        "}"
        "QLabel#editAccountTip {"
        "  color: #94a3b8; font-size: 12px; padding-bottom: 12px;"
        "}"
        "QLineEdit {"
        "  background-color: #334155;"
        "  border: 1px solid #475569;"
        "  border-radius: 10px;"
        "  padding: 10px 14px;"
        "  color: #f8fafc;"
        "  font-size: 13px;"
        "  selection-background-color: #3b82f6;"
        "  selection-color: #ffffff;"
        "}"
        "QLineEdit:focus { border: 1px solid #60a5fa; background-color: #3d4f63; }"
        "QPushButton#editAccountOkBtn {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 10px;"
        "  padding: 10px 22px;"
        "  font-weight: 700;"
        "  min-width: 96px;"
        "}"
        "QPushButton#editAccountOkBtn:hover { background-color: #1d4ed8; }"
        "QPushButton#editAccountOkBtn:pressed { background-color: #1e40af; }"
        "QPushButton#editAccountCancelBtn {"
        "  background-color: transparent;"
        "  color: #cbd5e1;"
        "  border: 1px solid #475569;"
        "  border-radius: 10px;"
        "  padding: 10px 22px;"
        "  font-weight: 600;"
        "  min-width: 96px;"
        "}"
        "QPushButton#editAccountCancelBtn:hover {"
        "  background-color: #334155;"
        "  border-color: #64748b;"
        "}"));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(QStringLiteral("修改用户信息"), &dialog);
    titleLabel->setObjectName(QStringLiteral("editAccountTitle"));
    layout->addWidget(titleLabel);

    auto* tipLabel =
        new QLabel(QStringLiteral("留空表示保留原值。"), &dialog);
    tipLabel->setObjectName(QStringLiteral("editAccountTip"));
    tipLabel->setWordWrap(true);
    layout->addWidget(tipLabel);

    auto* form = new QFormLayout();
    form->setSpacing(12);
    form->setContentsMargins(0, 8, 0, 8);

    auto* userLabel = new QLabel(QStringLiteral("当前账号："), &dialog);
    auto* userVal = new QLabel(m_userName, &dialog);
    userVal->setStyleSheet("font-weight:700;");
    form->addRow(userLabel, userVal);

    auto* editUser = new QLineEdit(&dialog);
    editUser->setPlaceholderText(QStringLiteral("输入新账号"));
    form->addRow(QStringLiteral("新账号："), editUser);

    auto* editPwd = new QLineEdit(&dialog);
    editPwd->setEchoMode(QLineEdit::Password);
    editPwd->setPlaceholderText(QStringLiteral("输入新密码（至少6位）"));
    form->addRow(QStringLiteral("新密码："), editPwd);

    layout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    cancelBtn->setObjectName(QStringLiteral("editAccountCancelBtn"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto* okBtn = new QPushButton(QStringLiteral("确定"), &dialog);
    okBtn->setObjectName(QStringLiteral("editAccountOkBtn"));
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString newUsername = editUser->text().trimmed();
    if (newUsername.isEmpty()) {
        newUsername = m_userName;
    }
    QString newPassword = editPwd->text();
    if (newPassword.isEmpty()) {
        newPassword = currentPassword;
    } else if (newPassword.size() < 6) {
        customMessage(this, QStringLiteral("输入无效"), QStringLiteral("密码长度至少为 6 位。"), true);
        return;
    }

    QString updateErr;
    if (!updateCurrentUserBasicInfo(newUsername, newPassword, &updateErr)) {
        customMessage(this, QStringLiteral("修改失败"), updateErr, true);
        return;
    }

    m_userName = newUsername;
    buildSettingsPage();
    m_builtPages.insert(5);
    customMessage(this, QStringLiteral("修改完成"), QStringLiteral("账号信息已更新。"));
}

bool MainWindow::loadCurrentUserBasicInfo(QString* password, QString* errMsg) {
    const QString connectionName =
        QString("system_ui_main_load_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(QDir(DbConfig::kDbDir).filePath(DbConfig::kDbFileName));
    if (!db.open()) {
        if (errMsg != nullptr) {
            *errMsg = db.lastError().text();
        }
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    {
        QSqlQuery query(db);
        query.prepare("SELECT password FROM users WHERE lower(username)=lower(?) LIMIT 1;");
        query.addBindValue(m_userName);
        if (!query.exec()) {
            if (errMsg != nullptr) {
                *errMsg = query.lastError().text();
            }
            db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }
        if (!query.next()) {
            if (errMsg != nullptr) {
                *errMsg = "未找到当前用户信息";
            }
            db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        if (password != nullptr) {
            *password = query.value(0).toString();
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
    return true;
}

bool MainWindow::updateCurrentUserBasicInfo(const QString& newUsername,
                                            const QString& newPassword,
                                            QString* errMsg) {
    const QString connectionName =
        QString("system_ui_main_update_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(QDir(DbConfig::kDbDir).filePath(DbConfig::kDbFileName));
    if (!db.open()) {
        if (errMsg != nullptr) {
            *errMsg = db.lastError().text();
        }
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    {
        QSqlQuery query(db);
        query.prepare("SELECT 1 FROM users WHERE lower(username)=lower(?) AND lower(username)<>lower(?) LIMIT 1;");
        query.addBindValue(newUsername);
        query.addBindValue(m_userName);
        if (!query.exec()) {
            if (errMsg != nullptr) {
                *errMsg = query.lastError().text();
            }
            db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }
        if (query.next()) {
            if (errMsg != nullptr) {
                *errMsg = "账号已存在，请更换账号名。";
            }
            db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        query.prepare(
            "UPDATE users "
            "SET username=?, password=? "
            "WHERE lower(username)=lower(?);");
        query.addBindValue(newUsername);
        query.addBindValue(newPassword);
        query.addBindValue(m_userName);
        if (!query.exec()) {
            if (errMsg != nullptr) {
                *errMsg = query.lastError().text();
            }
            db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
    return true;
}
