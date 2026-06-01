#include "mainwindow_utils.h"
#include <QDialog>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtMath>


const char kMqttTelemetryTopic[] = "telemetry";
const char kMqttCommandTopic[] = "command";
const char kMqttCommandPublishTopic[] = "command";
const char kMqttHelpTopic[] = "help";

QString buildWrappedMqttJson(const QString& type, const QString& source, const QJsonObject& payload) {
    QJsonObject root;
    root.insert(QStringLiteral("type"), type);
    root.insert(QStringLiteral("source"), source);
    root.insert(QStringLiteral("payload"), payload);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QJsonObject unwrapWrappedPayload(const QJsonObject& obj, const QString& expectedType) {
    if (obj.value(QStringLiteral("type")).toString() == expectedType &&
        obj.value(QStringLiteral("payload")).isObject()) {
        return obj.value(QStringLiteral("payload")).toObject();
    }
    return obj;
}

QString buildThresholdMqttJson(const QJsonObject& values) {
    QJsonObject payload;
    payload.insert(QStringLiteral("kind"), QStringLiteral("threshold"));
    payload.insert(QStringLiteral("values"), values);
    return buildWrappedMqttJson(QStringLiteral("command"), QStringLiteral("qt"), payload);
}

QString buildSingleThresholdMqttJson(const QString& key, int value) {
    QJsonObject values;
    values.insert(key, value);
    return buildThresholdMqttJson(values);
}

QString buildResetThresholdMqttJson() {
    QJsonObject payload;
    payload.insert(QStringLiteral("kind"), QStringLiteral("reset_threshold"));

    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("command"));
    root.insert(QStringLiteral("source"), QStringLiteral("qt"));
    root.insert(QStringLiteral("payload"), payload);
    root.insert(QStringLiteral("cmd"), QStringLiteral("reset_th"));
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

// 级别色直接从 STM32 上报的 lv.d 数组读取，不再本地计算

void applySensorLevelStyle(QLabel* valueLabel, QLabel* dotLabel, QLabel* stateLabel, int lv) {
    if (valueLabel != nullptr) {
        QString css;
        if (lv >= 2)      css = QStringLiteral("QLabel{color:#ef4444;font-size:28px;font-weight:800;}");
        else if (lv == 1) css = QStringLiteral("QLabel{color:#f59e0b;font-size:28px;font-weight:800;}");
        else              css = QStringLiteral("QLabel{color:#f8fbff;font-size:28px;font-weight:800;}");
        valueLabel->setStyleSheet(css);
    }
    if (dotLabel != nullptr) {
        QColor c;
        if (lv >= 2)      c = QColor(239, 68, 68);
        else if (lv == 1) c = QColor(245, 158, 11);
        else              c = QColor(34, 197, 94);
        dotLabel->setStyleSheet(QStringLiteral("QLabel{background:%1;border-radius:6px;}").arg(c.name()));
    }
    if (stateLabel != nullptr) {
        QString text, css;
        if (lv >= 2)      { text = QStringLiteral("警告"); css = QStringLiteral("QLabel{color:#ef4444;font-size:12px;font-weight:600;}"); }
        else if (lv == 1) { text = QStringLiteral("预警"); css = QStringLiteral("QLabel{color:#f59e0b;font-size:12px;font-weight:600;}"); }
        else              { text = QStringLiteral("正常"); css = QStringLiteral("QLabel{color:#22c55e;font-size:12px;font-weight:600;}"); }
        stateLabel->setText(text);
        stateLabel->setStyleSheet(css);
    }
}

QString lvStatusZh(int lv) {
    if (lv >= 2) return QStringLiteral("警告");
    if (lv == 1) return QStringLiteral("预警");
    return QStringLiteral("正常");
}

// ====== 自定义弹窗（无原生边框，主题风格统一）======

/** 确认对话框（返回 true=是/确认） */
bool customConfirm(QWidget* parent, const QString& title, const QString& text) {
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg.setModal(true);
    dlg.setFixedSize(400, 200);
    dlg.setStyleSheet(
        "QDialog{background:#0a1628;border:1px solid #1e3a5f;border-radius:12px;}");

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    // 标题栏
    auto* titleBar = new QWidget(&dlg);
    titleBar->setStyleSheet("background:#0f1f3a;border-radius:12px 12px 0 0;");
    auto* titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(16, 10, 8, 10);
    auto* titleLabel = new QLabel(title, titleBar);
    titleLabel->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
    titleLay->addWidget(titleLabel);
    titleLay->addStretch();
    auto* closeBtn = new QPushButton("✕", titleBar);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#7a8fb8;border:none;"
        "border-radius:14px;font-size:14px;font-weight:700;}"
        "QPushButton:hover{background:#ef4444;color:white;}");
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    titleLay->addWidget(closeBtn);
    layout->addWidget(titleBar);

    // 内容
    auto* body = new QWidget(&dlg);
    body->setStyleSheet("background:transparent;");
    auto* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(24, 20, 24, 16);
    auto* msgLabel = new QLabel(text, body);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("QLabel{color:#e8f0ff;font-size:14px;line-height:1.5;}");
    bodyLay->addWidget(msgLabel);
    bodyLay->addStretch();
    layout->addWidget(body, 1);

    // 按钮行
    auto* btnBar = new QWidget(&dlg);
    btnBar->setStyleSheet("background:#0a1628;border-radius:0 0 12px 12px;");
    auto* btnLay = new QHBoxLayout(btnBar);
    btnLay->setContentsMargins(16, 8, 16, 14);
    btnLay->addStretch();
    auto* noBtn = new QPushButton("取消", btnBar);
    noBtn->setFixedHeight(34);
    noBtn->setStyleSheet(
        "QPushButton{background:#1e293b;color:#94a3b8;border:1px solid #334155;"
        "border-radius:8px;padding:6px 24px;font-size:13px;font-weight:600;}"
        "QPushButton:hover{background:#334155;color:#cbd5e1;}");
    auto* yesBtn = new QPushButton("确认", btnBar);
    yesBtn->setFixedHeight(34);
    yesBtn->setStyleSheet(
        "QPushButton{background:#0f5fa8;color:white;border:1px solid #2f7fca;"
        "border-radius:8px;padding:6px 24px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#1a7ad4;}");
    QObject::connect(noBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(yesBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLay->addWidget(noBtn);
    btnLay->addWidget(yesBtn);
    layout->addWidget(btnBar);

    return dlg.exec() == QDialog::Accepted;
}

/** 提示/警告对话框 */
void customMessage(QWidget* parent, const QString& title, const QString& text,
                          bool isWarning) {
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg.setModal(true);
    dlg.setFixedSize(400, 200);
    dlg.setStyleSheet(
        "QDialog{background:#0a1628;border:1px solid #1e3a5f;border-radius:12px;}");

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* titleBar = new QWidget(&dlg);
    titleBar->setStyleSheet("background:#0f1f3a;border-radius:12px 12px 0 0;");
    auto* titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(16, 10, 8, 10);
    auto* titleLabel = new QLabel(title, titleBar);
    titleLabel->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
    titleLay->addWidget(titleLabel);
    titleLay->addStretch();
    auto* closeBtn = new QPushButton("✕", titleBar);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#7a8fb8;border:none;"
        "border-radius:14px;font-size:14px;font-weight:700;}"
        "QPushButton:hover{background:#ef4444;color:white;}");
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    titleLay->addWidget(closeBtn);
    layout->addWidget(titleBar);

    auto* body = new QWidget(&dlg);
    body->setStyleSheet("background:transparent;");
    auto* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(24, 20, 24, 16);
    auto* msgLabel = new QLabel(text, body);
    msgLabel->setWordWrap(true);
    QString msgColor = isWarning ? "#fbbf24" : "#e8f0ff";
    msgLabel->setStyleSheet(QString("QLabel{color:%1;font-size:14px;line-height:1.5;}").arg(msgColor));
    bodyLay->addWidget(msgLabel);
    bodyLay->addStretch();
    layout->addWidget(body, 1);

    auto* btnBar = new QWidget(&dlg);
    btnBar->setStyleSheet("background:#0a1628;border-radius:0 0 12px 12px;");
    auto* btnLay = new QHBoxLayout(btnBar);
    btnLay->setContentsMargins(16, 8, 16, 14);
    btnLay->addStretch();
    auto* okBtn = new QPushButton("确定", btnBar);
    okBtn->setFixedHeight(34);
    okBtn->setStyleSheet(
        "QPushButton{background:#0f5fa8;color:white;border:1px solid #2f7fca;"
        "border-radius:8px;padding:6px 24px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#1a7ad4;}");
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLay->addWidget(okBtn);
    layout->addWidget(btnBar);

    dlg.exec();
}

void showExportSuccessDialog(QWidget* parent, const QString& nativePath) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("导出成功"));
    dlg.setModal(true);
    dlg.resize(540, 240);
    dlg.setStyleSheet(
        QStringLiteral(
            "QDialog{background-color:#f8fafc;}"
            "QLabel{color:#0f172a;}"
            "QPushButton{background-color:#2563eb;color:#ffffff;padding:8px 22px;"
            "border-radius:8px;font-weight:600;border:none;min-width:88px;}"
            "QPushButton:hover{background-color:#1d4ed8;}"
            "QPushButton:pressed{background-color:#1e40af;}"));

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 20, 24, 20);

    auto* iconTitleRow = new QHBoxLayout();
    auto* okIcon = new QLabel(&dlg);
    okIcon->setFixedSize(44, 44);
    okIcon->setAlignment(Qt::AlignCenter);
    okIcon->setStyleSheet(QStringLiteral(
        "QLabel{background-color:#22c55e;border-radius:22px;color:#ffffff;"
        "font-size:20px;font-weight:800;}"));
    okIcon->setText(QStringLiteral("✓"));

    auto* title = new QLabel(QStringLiteral("导出成功"), &dlg);
    title->setStyleSheet(QStringLiteral("font-size:20px;font-weight:800;color:#0f172a;"));

    iconTitleRow->addWidget(okIcon);
    iconTitleRow->addSpacing(12);
    iconTitleRow->addWidget(title);
    iconTitleRow->addStretch();

    auto* hint =
        new QLabel(QStringLiteral("文件已保存到下列路径，可直接全选复制："), &dlg);
    hint->setStyleSheet(QStringLiteral("font-size:13px;color:#334155;font-weight:600;"));

    auto* pathEdit = new QLineEdit(nativePath, &dlg);
    pathEdit->setReadOnly(true);
    pathEdit->setStyleSheet(
        QStringLiteral(
            "QLineEdit{padding:10px 12px;font-size:13px;color:#0f172a;"
            "background-color:#ffffff;border:2px solid #94a3b8;border-radius:8px;"
            "selection-background-color:#2563eb;selection-color:#ffffff;}"));

    auto* foot = new QLabel(
        QStringLiteral("格式：原生 XLSX（含样式、合并单元格与列宽）"), &dlg);
    foot->setStyleSheet(QStringLiteral("font-size:12px;color:#64748b;"));
    foot->setWordWrap(true);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* okBtn = new QPushButton(QStringLiteral("确定"), &dlg);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(okBtn);

    layout->addLayout(iconTitleRow);
    layout->addWidget(hint);
    layout->addWidget(pathEdit);
    layout->addWidget(foot);
    layout->addSpacing(8);
    layout->addLayout(btnRow);

    pathEdit->setFocus();
    pathEdit->selectAll();

    dlg.exec();
}

QString stripRemoteLogCodeSuffix(QString text) {
    const QRegularExpression re(QStringLiteral(R"((（码|\(码)\s*\d+(）|\)))"));
    text.remove(re);
    return text.trimmed();
}

QString formatRemoteLogTableLine(const QString& executeTime,
                                 const QString& commandText,
                                 const QString& resultText) {
    const QString res = stripRemoteLogCodeSuffix(resultText);
    return QStringLiteral("%1 | %2 | %3")
        .arg(executeTime, -19, QLatin1Char(' '))
        .arg(commandText, -18, QLatin1Char(' '))
        .arg(res);
}

void clearLayout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->layout() != nullptr) {
            clearLayout(item->layout());
        }
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void applyShadow(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }

    auto* effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(24);
    effect->setOffset(0, 8);
    effect->setColor(QColor(15, 23, 42, 35));
    widget->setGraphicsEffect(effect);
}

QFrame* createPanelCard(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName("panelCard");
    frame->setFrameShape(QFrame::NoFrame);
    frame->setStyleSheet(
        "QFrame#panelCard{"
        "background:rgba(9, 33, 71, 218);"
        "border:1px solid rgba(80, 166, 255, 0.35);"
        "border-radius:18px;"
        "}");
    applyShadow(frame);
    return frame;
}

QIcon createEmergencyWindowIcon() {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, 64, 64);
    bg.setColorAt(0.0, QColor(0x0a, 0x2f, 0x63));
    bg.setColorAt(1.0, QColor(0x0f, 0x7a, 0xe5));
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 16, 16);

    QPainterPath shield;
    shield.moveTo(32, 12);
    shield.lineTo(47, 18);
    shield.lineTo(45, 34);
    shield.quadTo(42, 46, 32, 52);
    shield.quadTo(22, 46, 19, 34);
    shield.lineTo(17, 18);
    shield.closeSubpath();
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawPath(shield);

    painter.setBrush(QColor(0x0f, 0x5f, 0xa8));
    painter.drawRoundedRect(QRectF(28, 21, 8, 18), 2, 2);
    painter.drawRoundedRect(QRectF(23, 26, 18, 8), 2, 2);

    QPen radarPen(QColor(120, 219, 255, 210), 2.2, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(radarPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(10, 10, 44, 44), 30 * 16, 56 * 16);
    painter.drawArc(QRectF(15, 15, 34, 34), 32 * 16, 48 * 16);

    return QIcon(pixmap);
}

QPixmap createAlarmIconPixmap(const QSize& size) {
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(2, 2, size.width() - 4, size.height() - 4);
    QPainterPath path;
    path.moveTo(bounds.center().x(), bounds.top());
    path.lineTo(bounds.right(), bounds.bottom());
    path.lineTo(bounds.left(), bounds.bottom());
    path.closeSubpath();

    painter.fillPath(path, QColor(0xdc, 0x26, 0x26));
    painter.setPen(QPen(QColor(0x7f, 0x1d, 0x1d), 1.5));
    painter.drawPath(path);

    painter.setPen(QPen(Qt::white, qMax(2, size.width() / 10), Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(bounds.center().x(), bounds.top() + bounds.height() * 0.26),
                     QPointF(bounds.center().x(), bounds.top() + bounds.height() * 0.66));
    painter.drawPoint(QPointF(bounds.center().x(), bounds.top() + bounds.height() * 0.82));

    return pixmap;
}

// 跟踪当前活跃的告警弹窗，新弹窗出现时自动关闭旧的（避免堆叠）
static QPointer<QDialog> s_activeAlertDialog;

bool isAlertDialogActive() {
    return !s_activeAlertDialog.isNull();
}

static void showRealtimeAlertDialog(QWidget* parent, const QString& detailText,
                                    bool isAlarm, const QString& title) {
    // 关闭当前活跃弹窗（ALARM替换WARN，或同级别更新）
    if (s_activeAlertDialog) {
        s_activeAlertDialog->close();
        s_activeAlertDialog = nullptr;
    }

    // 堆分配 + open() 非阻塞模态（不阻塞MQTT事件循环）
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    dialog->setModal(true);
    dialog->setObjectName(isAlarm ? "realtimeAlarmDialog" : "realtimeWarnDialog");

    const QString borderColor  = isAlarm ? "#dc2626" : "#f59e0b";
    const QString bgColor      = isAlarm ? "#fff5f5" : "#fffbeb";
    const QString titleColor   = isAlarm ? "#b91c1c" : "#b45309";
    const QString detailColor  = isAlarm ? "#7f1d1d" : "#78350f";
    const QString btnBg        = isAlarm ? "#dc2626" : "#f59e0b";
    const QString btnHover     = isAlarm ? "#b91c1c" : "#d97706";
    const QString btnPress     = isAlarm ? "#991b1b" : "#b45309";
    const QString closeColor   = isAlarm ? "#b91c1c" : "#b45309";
    const QString closeHover   = isAlarm ? "#7f1d1d" : "#78350f";
    const QString alarmLevel   = isAlarm ? "紧急" : "";

    dialog->setStyleSheet(
        QString("QDialog#%1{"
        "background:%2;"
        "border:3px solid %3;"
        "border-radius:16px;"
        "}"
        "QLabel#alertTitle{"
        "color:%4;"
        "font-size:24px;"
        "font-weight:800;"
        "}"
        "QLabel#alertDetail{"
        "color:%5;"
        "font-size:17px;"
        "font-weight:600;"
        "line-height:1.6;"
        "}"
        "QPushButton#alertOkButton{"
        "min-width:130px;"
        "min-height:42px;"
        "border-radius:12px;"
        "border:1px solid %3;"
        "background:%6;"
        "color:white;"
        "font-size:16px;"
        "font-weight:800;"
        "padding:0 22px;"
        "}"
        "QPushButton#alertOkButton:hover{background:%7;}"
        "QPushButton#alertOkButton:pressed{background:%8;}"
        "QPushButton#alertCloseButton{"
        "border:none;"
        "background:transparent;"
        "color:%9;"
        "font-size:22px;"
        "font-weight:800;"
        "min-width:28px;"
        "min-height:28px;"
        "}"
        "QPushButton#alertCloseButton:hover{color:%10;}")
        .arg(dialog->objectName(), bgColor, borderColor,
             titleColor, detailColor,
             btnBg, btnHover, btnPress,
             closeColor, closeHover));

    auto* rootLayout = new QVBoxLayout(dialog);
    rootLayout->setContentsMargins(18, 14, 18, 18);
    rootLayout->setSpacing(10);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    auto* levelBadge = new QLabel(alarmLevel, dialog);
    if (!alarmLevel.isEmpty()) {
        levelBadge->setStyleSheet(QString(
            "QLabel{background:%1;color:white;font-size:13px;font-weight:800;"
            "border-radius:6px;padding:3px 10px;}").arg(borderColor));
    }
    topRow->addWidget(levelBadge);
    topRow->addStretch();
    auto* closeButton = new QPushButton(QString(QChar(0x00D7)), dialog);
    closeButton->setObjectName("alertCloseButton");
    topRow->addWidget(closeButton, 0, Qt::AlignRight);
    rootLayout->addLayout(topRow);

    auto* contentRow = new QHBoxLayout();
    contentRow->setSpacing(16);

    auto* iconLabel = new QLabel(dialog);
    iconLabel->setPixmap(createAlarmIconPixmap(QSize(72, 72)));
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    contentRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(10);
    auto* titleLabel = new QLabel(title, dialog);
    titleLabel->setObjectName("alertTitle");
    auto* detailLabel = new QLabel(detailText, dialog);
    detailLabel->setObjectName("alertDetail");
    detailLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(detailLabel);
    textLayout->addStretch();
    contentRow->addLayout(textLayout, 1);

    rootLayout->addLayout(contentRow);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* okButton = new QPushButton(isAlarm ? "知道了" : "我知道了", dialog);
    okButton->setObjectName("alertOkButton");
    buttonRow->addWidget(okButton);
    rootLayout->addLayout(buttonRow);

    QObject::connect(closeButton, &QPushButton::clicked, dialog, &QDialog::reject);
    QObject::connect(okButton, &QPushButton::clicked, dialog, &QDialog::accept);
    // 弹窗关闭时自动清空跟踪指针
    QObject::connect(dialog, &QDialog::finished, [](int) {
        if (s_activeAlertDialog) s_activeAlertDialog = nullptr;
    });

    dialog->resize(520, 220);
    s_activeAlertDialog = dialog;
    dialog->open();  // 非阻塞窗口模态：不阻塞MQTT事件循环
}

void showRealtimeAlarmDialog(QWidget* parent, const QString& detailText) {
    showRealtimeAlertDialog(parent, detailText, true, QStringLiteral("🚨 设备告警"));
}

void showRealtimeWarnDialog(QWidget* parent, const QString& detailText) {
    showRealtimeAlertDialog(parent, detailText, false, QStringLiteral("⚠ 设备预警"));
}

/* ---- BatteryGauge ---- */
BatteryGauge::BatteryGauge(QWidget* parent) : QWidget(parent) { setFixedSize(76, 160); }

void BatteryGauge::setPct(double p) { m_pct = qBound(0.0, p, 100.0); update(); }

void BatteryGauge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int w = width(), h = height();
    const int nubW = 20, nubH = 7, shellTop = nubH + 2, bodyH = h - shellTop - 4;
    const QRect body(4, shellTop, w - 8, bodyH);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(200,210,220));
    p.drawRoundedRect(QRect((w - nubW) / 2, 2, nubW, nubH), 3, 3);
    p.setBrush(QColor(20, 40, 70, 220));
    p.setPen(QPen(QColor(120,160,200,120), 1.5));
    p.drawRoundedRect(body, 8, 8);
    QColor fillC;
    if (m_pct > 60)      fillC = QColor(34, 197, 94);
    else if (m_pct > 30) fillC = QColor(245, 158, 11);
    else                 fillC = QColor(239, 68, 68);
    int fillH = qMax(4, (int)(bodyH * m_pct / 100.0));
    QRect fillR(body.x() + 2, body.y() + bodyH - fillH, body.width() - 4, fillH - 1);
    p.setPen(Qt::NoPen);
    p.setBrush(fillC);
    p.drawRoundedRect(fillR, 5, 5);
    p.setPen(QColor(255,255,255));
    QFont f = p.font(); f.setPixelSize(16); f.setBold(true); p.setFont(f);
    p.drawText(body, Qt::AlignCenter, QString("%1%").arg((int)m_pct));
}

/* ---- TankGauge ---- */
TankGauge::TankGauge(QWidget* parent) : QWidget(parent) { setFixedSize(76, 160); }

void TankGauge::setPct(double p) { m_pct = qBound(0.0, p, 100.0); update(); }

void TankGauge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int w = width(), h = height();
    const int pad = 3, bodyH = h - 14;
    const QRect body(pad, 0, w - pad * 2, bodyH);
    QPainterPath shellPath;
    int topW = body.width();
    int botW = body.width() - 10;
    shellPath.addRoundedRect(QRect(body.x() + (topW - botW) / 2, body.y(), botW, body.height()), 10, 10);
    p.setBrush(QColor(20, 40, 70, 220));
    p.setPen(QPen(QColor(120,160,200,120), 1.5));
    p.drawPath(shellPath);
    QColor fillC;
    if (m_pct > 60)      fillC = QColor(59, 130, 246);
    else if (m_pct > 30) fillC = QColor(245, 158, 11);
    else                 fillC = QColor(239, 68, 68);
    int fillH = qMax(3, (int)(bodyH * m_pct / 100.0));
    QRect fillR(body.x() + (topW - botW) / 2 + 2, body.y() + bodyH - fillH, botW - 4, fillH - 2);
    p.setPen(Qt::NoPen);
    p.setBrush(fillC);
    p.drawRoundedRect(fillR, 7, 7);
    p.setBrush(QColor(120,160,200,80));
    p.drawRoundedRect(QRect(body.x() + 6, body.y() + 2, topW - 12, 6), 2, 2);
    p.setPen(QColor(255,255,255));
    QFont f = p.font(); f.setPixelSize(16); f.setBold(true); p.setFont(f);
    p.drawText(body, Qt::AlignCenter, QString("%1%").arg((int)m_pct));
}



