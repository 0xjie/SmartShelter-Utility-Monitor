#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
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

namespace {

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

static QString lvStatusZh(int lv) {
    if (lv >= 2) return QStringLiteral("警告");
    if (lv == 1) return QStringLiteral("预警");
    return QStringLiteral("正常");
}

static QString lvDeviceStatus(int lv) {
    if (lv >= 2) return QStringLiteral("告警");
    if (lv == 1) return QStringLiteral("关注");
    return QStringLiteral("运行中");
}

// ====== 自定义弹窗（无原生边框，主题风格统一）======

/** 确认对话框（返回 true=是/确认） */
static bool customConfirm(QWidget* parent, const QString& title, const QString& text) {
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
static void customMessage(QWidget* parent, const QString& title, const QString& text,
                          bool isWarning = false) {
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
    static const QRegularExpression re(QStringLiteral(R"((（码|\(码)\s*\d+(）|\)))"));
    text.remove(re);
    return text.trimmed();
}

QString formatRemoteLogTableLine(const QString& executeTime,
                                 const QString& deviceId,
                                 const QString& commandText,
                                 const QString& resultText) {
    const QString res = stripRemoteLogCodeSuffix(resultText);
    return QStringLiteral("%1 | %2 | %3 | %4")
        .arg(executeTime, -19, QLatin1Char(' '))
        .arg(deviceId, -10, QLatin1Char(' '))
        .arg(commandText, -18, QLatin1Char(' '))
        .arg(res);
}

/** 设备管理页：悬停轻微放大，移出复原 */
class DeviceHoverCard final : public QFrame {
public:
    explicit DeviceHoverCard(QWidget* parent = nullptr)
        : QFrame(parent) {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setFrameShape(QFrame::NoFrame);
        setObjectName(QStringLiteral("deviceInfoCard"));
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(170);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            const QSize sz = v.toSize();
            if (sz.isValid() && !sz.isEmpty()) {
                setFixedSize(sz);
            }
        });
    }

    void setBaseSize(int w, int h) {
        m_baseW = qMax(48, w);
        m_baseH = qMax(48, h);
        setFixedSize(m_baseW, m_baseH);
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        QFrame::enterEvent(event);
        raise();
        animateToHover(true);
    }

    void leaveEvent(QEvent* event) override {
        QFrame::leaveEvent(event);
        animateToHover(false);
    }

private:
    void animateToHover(bool hovered) {
        if (m_anim->state() == QAbstractAnimation::Running) {
            m_anim->stop();
        }
        const QSize target =
            hovered ? QSize(static_cast<int>(m_baseW * 1.08), static_cast<int>(m_baseH * 1.08))
                    : QSize(m_baseW, m_baseH);
        m_anim->setStartValue(size());
        m_anim->setEndValue(target);
        m_anim->start();
    }

    int m_baseW = 200;
    int m_baseH = 110;
    QVariantAnimation* m_anim = nullptr;
};

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

QString sensorTypeShortLabel(const QString& type) {
    if (type.contains("Temperature", Qt::CaseInsensitive) || type.contains("温度")) {
        return "TEMP";
    }
    if (type.contains("Humidity", Qt::CaseInsensitive) || type.contains("湿度")) {
        return "HUMI";
    }
    if (type.contains("Current", Qt::CaseInsensitive) || type.contains("电流")) {
        return "CURR";
    }
    if (type.contains("PM2.5", Qt::CaseInsensitive) || type.contains("PM25", Qt::CaseInsensitive)) {
        return "PM25";
    }
    return "SENSOR";
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

void showRealtimeAlarmDialog(QWidget* parent, const QString& detailText) {
    QDialog dialog(parent);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setModal(true);
    dialog.setObjectName("realtimeAlarmDialog");
    dialog.setStyleSheet(
        "QDialog#realtimeAlarmDialog{"
        "background:#fff5f5;"
        "border:3px solid #dc2626;"
        "border-radius:16px;"
        "}"
        "QLabel#alarmTitle{"
        "color:#b91c1c;"
        "font-size:26px;"
        "font-weight:800;"
        "}"
        "QLabel#alarmDetail{"
        "color:#7f1d1d;"
        "font-size:18px;"
        "font-weight:600;"
        "line-height:1.6;"
        "}"
        "QPushButton#alarmOkButton{"
        "min-width:130px;"
        "min-height:42px;"
        "border-radius:12px;"
        "border:1px solid #dc2626;"
        "background:#ff1010;"
        "color:white;"
        "font-size:16px;"
        "font-weight:800;"
        "padding:0 22px;"
        "}"
        "QPushButton#alarmOkButton:hover{background:#dc2626;}"
        "QPushButton#alarmOkButton:pressed{background:#991b1b;}"
        "QPushButton#alarmCloseButton{"
        "border:none;"
        "background:transparent;"
        "color:#b91c1c;"
        "font-size:22px;"
        "font-weight:800;"
        "min-width:28px;"
        "min-height:28px;"
        "}"
        "QPushButton#alarmCloseButton:hover{color:#7f1d1d;}");

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(18, 14, 18, 18);
    rootLayout->setSpacing(10);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addStretch();
    auto* closeButton = new QPushButton(QString(QChar(0x00D7)), &dialog);
    closeButton->setObjectName("alarmCloseButton");
    topRow->addWidget(closeButton, 0, Qt::AlignRight);
    rootLayout->addLayout(topRow);

    auto* contentRow = new QHBoxLayout();
    contentRow->setSpacing(16);

    auto* iconLabel = new QLabel(&dialog);
    iconLabel->setPixmap(createAlarmIconPixmap(QSize(72, 72)));
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    contentRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(10);
    auto* titleLabel = new QLabel("设备告警", &dialog);
    titleLabel->setObjectName("alarmTitle");
    auto* detailLabel = new QLabel(detailText, &dialog);
    detailLabel->setObjectName("alarmDetail");
    detailLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(detailLabel);
    textLayout->addStretch();
    contentRow->addLayout(textLayout, 1);

    rootLayout->addLayout(contentRow);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* okButton = new QPushButton("OK", &dialog);
    okButton->setObjectName("alarmOkButton");
    buttonRow->addWidget(okButton);
    rootLayout->addLayout(buttonRow);

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.resize(520, 220);
    dialog.exec();
}
}  // namespace

/* ---- 水电页面自定义绘制组件 ---- */

class BatteryGauge : public QWidget {
    double m_pct = 100;
public:
    explicit BatteryGauge(QWidget* parent = nullptr) : QWidget(parent) { setFixedSize(76, 160); }
    void setPct(double p) { m_pct = qBound(0.0, p, 100.0); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width(), h = height();
        const int shellPad = 3, nubW = 20, nubH = 7, shellTop = nubH + 2, bodyH = h - shellTop - 4;
        const QRect body(4, shellTop, w - 8, bodyH);
        // nub
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(200,210,220));
        p.drawRoundedRect(QRect((w - nubW) / 2, 2, nubW, nubH), 3, 3);
        // shell
        p.setBrush(QColor(20, 40, 70, 220));
        p.setPen(QPen(QColor(120,160,200,120), 1.5));
        p.drawRoundedRect(body, 8, 8);
        // fill color
        QColor fillC;
        if (m_pct > 60)      fillC = QColor(34, 197, 94);
        else if (m_pct > 30) fillC = QColor(245, 158, 11);
        else                 fillC = QColor(239, 68, 68);
        // fill
        int fillH = qMax(4, (int)(bodyH * m_pct / 100.0));
        QRect fillR(body.x() + 2, body.y() + bodyH - fillH, body.width() - 4, fillH - 1);
        p.setPen(Qt::NoPen);
        p.setBrush(fillC);
        p.drawRoundedRect(fillR, 5, 5);
        // pct text
        p.setPen(QColor(255,255,255));
        QFont f = p.font(); f.setPixelSize(16); f.setBold(true); p.setFont(f);
        p.drawText(body, Qt::AlignCenter, QString("%1%").arg((int)m_pct));
    }
};

class TankGauge : public QWidget {
    double m_pct = 100;
public:
    explicit TankGauge(QWidget* parent = nullptr) : QWidget(parent) { setFixedSize(76, 160); }
    void setPct(double p) { m_pct = qBound(0.0, p, 100.0); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width(), h = height();
        const int pad = 3, bodyH = h - 14;
        const QRect body(pad, 0, w - pad * 2, bodyH);
        // shell - trapezoid approximation for tank shape (wider top)
        QPainterPath shellPath;
        int topW = body.width();
        int botW = body.width() - 10;
        shellPath.addRoundedRect(QRect(body.x() + (topW - botW) / 2, body.y(), botW, body.height()), 10, 10);
        p.setBrush(QColor(20, 40, 70, 220));
        p.setPen(QPen(QColor(120,160,200,120), 1.5));
        p.drawPath(shellPath);
        // fill color
        QColor fillC;
        if (m_pct > 60)      fillC = QColor(59, 130, 246);
        else if (m_pct > 30) fillC = QColor(245, 158, 11);
        else                 fillC = QColor(239, 68, 68);
        // fill
        int fillH = qMax(3, (int)(bodyH * m_pct / 100.0));
        QRect fillR(body.x() + (topW - botW) / 2 + 2, body.y() + bodyH - fillH, botW - 4, fillH - 2);
        p.setPen(Qt::NoPen);
        p.setBrush(fillC);
        p.drawRoundedRect(fillR, 7, 7);
        // lid
        p.setBrush(QColor(120,160,200,80));
        p.drawRoundedRect(QRect(body.x() + 6, body.y() + 2, topW - 12, 6), 2, 2);
        // pct text
        p.setPen(QColor(255,255,255));
        QFont f = p.font(); f.setPixelSize(16); f.setBold(true); p.setFont(f);
        p.drawText(body, Qt::AlignCenter, QString("%1%").arg((int)m_pct));
    }
};

MainWindow::MainWindow(const QString& userName,
                       const QString& userRole,
                       QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_userName(userName),
      m_userRole(userRole),
      m_dataTimer(nullptr),
      m_tempSeries(nullptr),
      m_humiSeries(nullptr),
      m_pmSeries(nullptr),
      m_axisX(nullptr),
      m_axisY(nullptr),
      m_dashboardTempSeries(nullptr),
      m_dashboardHumiSeries(nullptr),
      m_dashboardPmSeries(nullptr),
      m_historyLineSeries(),
      m_historyChartViews(),
      m_dashboardAxisX1(nullptr),
      m_dashboardAxisY1(nullptr),
      m_dashboardAxisX2(nullptr),
      m_dashboardAxisY2(nullptr),
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
      m_totalCurrentLabel(nullptr),
      m_totalPowerLabel(nullptr),
      m_totalWaterLabel(nullptr),
      m_realtimeStatusLabel(nullptr),
      m_realtimeTimestampLabel(nullptr),
      m_windowIconLabel(nullptr),
      m_minimizeButton(nullptr),
      m_maximizeButton(nullptr),
      m_closeButton(nullptr),
      m_dashboardStep(0),
      m_chartStep(0),
      m_totalCurrent(0.0),
      m_totalWater(0.0),
      m_totalPower(0.0),
      m_historyStatsLabel(nullptr),
      m_historyStatsLeftLabel(nullptr),
      m_historyStatsRightLabel(nullptr),
      m_historySummaryLabel(nullptr),
      m_historyDataSourceLabel(nullptr),
      m_realtimeSensorTable(nullptr),
      m_alarmInfoTable(nullptr),
      m_deviceTable(nullptr),
      m_deviceSearchEdit(nullptr),
      m_deviceStatusFilterCombo(nullptr),
      m_deviceTypeFilterCombo(nullptr),
      m_remoteDeviceCombo(nullptr),
      m_remoteCommandCombo(nullptr),
      m_remoteIntervalSpin(nullptr),
      m_logViewer(nullptr),
      m_remoteControlLogTable(nullptr),
      m_loginTime(QDateTime::currentDateTime()) {
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, true);

    m_devices = {
        {"TEMP-001", QStringLiteral("北区温度传感器"), QStringLiteral("温度传感器"), QStringLiteral("A区机房"), "运行中", 93, "v1.2.0", true, 5, 26.0, "℃", 35.0, 0.0, true, true, false},
        {"HUMI-002", QStringLiteral("仓储区湿度传感器"), QStringLiteral("湿度传感器"), QStringLiteral("B区仓储"), "运行中", 88, "v1.1.4", true, 5, 55.0, "%", 75.0, 0.0, false, true, false},
        {"FLOW-003", QStringLiteral("管网主线流量计"), QStringLiteral("水流传感器"), QStringLiteral("管网主线"), "运行中", 80, "v1.0.7", true, 5, 2.5, "L/min", 5.0, 0.0, true, false, false},
        {"CURR-004", QStringLiteral("配电室电流传感器"), QStringLiteral("电流传感器"), QStringLiteral("配电室"), "运行中", 76, "v1.0.9", true, 3, 220.0, "A", 50.0, 0.0, true, false, true},
        {"AIR-005", QStringLiteral("主通道空气质量传感器"), QStringLiteral("空气质量传感器"), QStringLiteral("主通道"), "运行中", 74, "v1.2.6", true, 5, 38.0, "", 90.0, 0.0, true, true, false},
        {"PM25-006", QStringLiteral("主通道 PM2.5 传感器"), QStringLiteral("PM2.5 传感器"), QStringLiteral("主通道"), "运行中", 68, "v1.3.1", true, 10, 42.0, "ug/m3", 90.0, 0.0, true, true, false}
    };

    initUi();
    initConnections();
    initMqtt();
    updateTopBarTime();
    refreshHistoryPage();
    refreshDeviceTable();

    // Top bar clock and device status refresh timer.
    m_dataTimer = new QTimer(this);
    if (m_dataTimer != nullptr) {
        m_dataTimer->setInterval(1000);
        connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::updateTopBarTime);
        connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::onUpdateDashboardData);
        connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::onUpdateWaterPowerData);
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
            syncDeviceInfoToDatabase();
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
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        auto* chartView = qobject_cast<QChartView*>(watched);
        if (chartView && m_historyChartViews.contains(chartView)) {
            bool ok = false;
            int idx = chartView->property("sensorIndex").toInt(&ok);
            if (ok && idx >= 0 && idx < m_historyLineSeries.size()) {
                QLineSeries* series = m_historyLineSeries[idx];
                if (series) {
                    QPen pen = series->pen();
                    pen.setWidthF(event->type() == QEvent::Enter ? 8.0 : 5.0);
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
    const QString topic = QStringLiteral("test001up");
    const QString helpTopic = QStringLiteral("WebQT1");

    connect(&m_mqtt, &Mqtt::stateChanged, this, [this, topic, helpTopic](int state) {
        if (state == 2) {  // QMqttClient::Connected
            m_useMqttRealtime = true;
            m_mqtt.subscribeTopic(topic);
            m_mqtt.subscribeTopic(helpTopic);
            if (m_realtimeStatusLabel != nullptr) {
                m_realtimeStatusLabel->setText("系统运行状态：MQTT 已连接");
                m_realtimeStatusLabel->setStyleSheet("QLabel{color:#059669;font-size:14px;font-weight:700;}");
            }
            qDebug() << "MQTT 已订阅主题:" << topic << helpTopic;
        } else {
            m_useMqttRealtime = false;
            if (m_realtimeStatusLabel != nullptr) {
                m_realtimeStatusLabel->setText("系统运行状态：设备离线");
                m_realtimeStatusLabel->setStyleSheet("QLabel{color:#f59e0b;font-size:14px;font-weight:700;}");
            }
        }
    });

    connect(&m_mqtt, &Mqtt::textMessageReceived, this,
            [this](const QString&, const QString& payload) {
                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
                if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                    qDebug() << "MQTT JSON 解析错误:" << parseError.errorString() << payload;
                    return;
                }

                const QJsonObject obj = doc.object();
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
                }

                if (m_tempSeries != nullptr && m_humiSeries != nullptr && m_pmSeries != nullptr && m_axisX != nullptr) {
                    qreal t = QDateTime::currentMSecsSinceEpoch();
                    m_tempSeries->append(t, temp);
                    m_humiSeries->append(t, humi);
                    m_pmSeries->append(t, pm25);

                    const int maxPoints = 20;
                    if (m_tempSeries->count() > maxPoints) {
                        m_tempSeries->removePoints(0, m_tempSeries->count() - maxPoints);
                        m_humiSeries->removePoints(0, m_humiSeries->count() - maxPoints);
                        m_pmSeries->removePoints(0, m_pmSeries->count() - maxPoints);
                    }

                    if (m_tempSeries->count() >= 2) {
                        auto pts = m_tempSeries->points();
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

                const QJsonObject obj = doc.object();
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

void MainWindow::initEnvironmentChart() {
}

void MainWindow::buildMainPages() {
    buildRealtimePage();
    buildWaterPowerPage();
    buildHistoryPage();
    buildAlarmPage();
    buildDevicePage();
    buildSettingsPage();
}

void MainWindow::buildRealtimePage() {
    auto* rootLayout = ui->verticalLayoutDashboard;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    auto* headerCard = createPanelCard(ui->pageDashboard);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);

    auto* titleWrap = new QVBoxLayout();
    auto* titleLabel = new QLabel("实时监控", headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel("集中查看关键指标实时数据与曲线。", headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(subLabel);

    auto* rightWrap = new QVBoxLayout();
    m_realtimeStatusLabel = new QLabel("系统运行状态：正常", headerCard);
    m_realtimeStatusLabel->setStyleSheet("QLabel{color:#059669;font-size:14px;font-weight:700;}");
    rightWrap->addWidget(m_realtimeStatusLabel, 0, Qt::AlignRight);

    headerLayout->addLayout(titleWrap);
    headerLayout->addStretch();
    headerLayout->addLayout(rightWrap);
    rootLayout->addWidget(headerCard);

    auto createCard = [&](const QString& title, QLabel*& valueLabel, QLabel*& dotLabel, QLabel*& stateLabel) -> QFrame* {
        auto* card = new QFrame(ui->pageDashboard);
        card->setObjectName("statusCard");
        applyShadow(card);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(8);

        auto* titleLabelLocal = new QLabel(title, card);
        titleLabelLocal->setStyleSheet("QLabel{color:#9cc7ff;font-size:13px;font-weight:600;}");
        valueLabel = new QLabel("--", card);
        valueLabel->setStyleSheet("QLabel{color:#f8fbff;font-size:28px;font-weight:800;}");
        dotLabel = new QLabel(card);
        dotLabel->setFixedSize(12, 12);
        dotLabel->setStyleSheet("QLabel{background:#22c55e;border-radius:6px;}");
        stateLabel = new QLabel("正常", card);
        stateLabel->setStyleSheet("QLabel{color:#22c55e;font-size:12px;font-weight:600;}");
        auto* stateRow = new QHBoxLayout();
        stateRow->addWidget(dotLabel);
        stateRow->addWidget(stateLabel);
        stateRow->addStretch();

        layout->addWidget(titleLabelLocal);
        layout->addWidget(valueLabel);
        layout->addLayout(stateRow);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->addWidget(createCard("温度", m_cardTempValue, m_dotTemp, m_stateTempLabel), 0, 0);
    grid->addWidget(createCard("湿度", m_cardHumiValue, m_dotHumi, m_stateHumiLabel), 0, 1);
    // 将「水流/电流/PM2.5」三项在网格中换换位
    grid->addWidget(createCard("水流", m_cardFlowValue, m_dotFlow, m_stateFlowLabel), 0, 2);
    grid->addWidget(createCard("空气指数", m_cardAirValue, m_dotAir, m_stateAirLabel), 1, 1);
    grid->addWidget(createCard("PM2.5", m_cardPmValue, m_dotPm, m_statePmLabel), 1, 0);
    grid->addWidget(createCard("电流", m_cardCurrentValue, m_dotCurrent, m_stateCurrentLabel), 1, 2);
    rootLayout->addLayout(grid);

    // 实时曲线：温度 / 湿度 / PM2.5
    m_tempSeries = new QLineSeries(this);
    m_humiSeries = new QLineSeries(this);
    m_pmSeries = new QLineSeries(this);

    auto* chart = new QChart();
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
    chart->legend()->setVisible(false);  // 不显示图例
    chart->addSeries(m_tempSeries);
    chart->addSeries(m_humiSeries);
    chart->addSeries(m_pmSeries);

    m_tempSeries->setColor(QColor(248, 113, 113));
    m_humiSeries->setColor(QColor(56, 189, 248));
    m_pmSeries->setColor(QColor(251, 191, 36));

    // X轴改为时间
    m_axisX = new QDateTimeAxis(this);
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
    m_axisX->setGridLineColor(QColor(125, 211, 252, 35));
    m_axisX->setLinePenColor(QColor(0x60, 0xa5, 0xfa));
    chart->addAxis(m_axisX, Qt::AlignBottom);
    m_tempSeries->attachAxis(m_axisX);
    m_humiSeries->attachAxis(m_axisX);
    m_pmSeries->attachAxis(m_axisX);

    m_axisY = new QValueAxis(this);
    m_axisY->setRange(0, 100);
    m_axisY->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
    m_axisY->setGridLineColor(QColor(125, 211, 252, 35));
    m_axisY->setLinePenColor(QColor(0x60, 0xa5, 0xfa));
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_tempSeries->attachAxis(m_axisY);
    m_humiSeries->attachAxis(m_axisY);
    m_pmSeries->attachAxis(m_axisY);

    auto* chartCard = createPanelCard(ui->pageDashboard);
    auto* chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(12, 12, 12, 12);

    // 折线图上方说明标签
    auto* legendRow = new QHBoxLayout();
    auto makeDot = [&](const QString& text, const QColor& color) {
        auto* row = new QHBoxLayout();
        auto* dot = new QLabel(chartCard);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString("QLabel{background:%1;border-radius:5px;}").arg(color.name()));
        auto* lbl = new QLabel(text, chartCard);
        lbl->setStyleSheet("QLabel{color:#cbd5e1;font-size:12px;}");
        row->addWidget(dot);
        row->addWidget(lbl);
        return row;
    };
    legendRow->addStretch();
    legendRow->addLayout(makeDot(QStringLiteral("温度(℃)"), QColor(248,113,113)));
    legendRow->addSpacing(16);
    legendRow->addLayout(makeDot(QStringLiteral("湿度(%)"), QColor(56,189,248)));
    legendRow->addSpacing(16);
    legendRow->addLayout(makeDot(QStringLiteral("PM2.5(μg/m³)"), QColor(251,191,36)));
    legendRow->addStretch();
    chartLayout->addLayout(legendRow);

    auto* chartView = new QChartView(chart, chartCard);
    chartView->setRenderHint(QPainter::Antialiasing, true);
    chartView->setStyleSheet("background:transparent;border:none;");
    chartLayout->addWidget(chartView);
    rootLayout->addWidget(chartCard, 1);

    auto* totalCard = createPanelCard(ui->pageDashboard);
    auto* totalLayout = new QHBoxLayout(totalCard);
    totalLayout->setContentsMargins(18, 14, 18, 14);
    auto* powerTitle = new QLabel("系统累计能耗", totalCard);
    powerTitle->setStyleSheet("QLabel{color:#9cc7ff;font-size:13px;font-weight:600;}");
    m_totalPowerLabel = new QLabel("0.000 kWh", totalCard);
    m_totalPowerLabel->setStyleSheet("QLabel{font-size:26px;font-weight:800;color:#f8fbff;}");
    auto* currentTitle = new QLabel("累计电流", totalCard);
    currentTitle->setStyleSheet("QLabel{color:#9cc7ff;font-size:13px;font-weight:600;}");
    m_totalCurrentLabel = new QLabel("0.000 Ah", totalCard);
    m_totalCurrentLabel->setStyleSheet("QLabel{font-size:26px;font-weight:800;color:#f8fbff;}");
    auto* waterTitle = new QLabel("总流量", totalCard);
    waterTitle->setStyleSheet("QLabel{color:#9cc7ff;font-size:13px;font-weight:600;}");
    m_totalWaterLabel = new QLabel("0.0 L", totalCard);
    m_totalWaterLabel->setStyleSheet("QLabel{font-size:26px;font-weight:800;color:#f8fbff;}");
    auto* leftCol = new QVBoxLayout();
    leftCol->addWidget(powerTitle);
    leftCol->addWidget(m_totalPowerLabel);
    auto* centerCol = new QVBoxLayout();
    centerCol->addWidget(currentTitle);
    centerCol->addWidget(m_totalCurrentLabel);
    auto* rightCol = new QVBoxLayout();
    rightCol->addWidget(waterTitle);
    rightCol->addWidget(m_totalWaterLabel);
    totalLayout->addLayout(leftCol);
    totalLayout->addStretch();
    totalLayout->addLayout(centerCol);
    totalLayout->addStretch();
    totalLayout->addLayout(rightCol);
    rootLayout->addWidget(totalCard);
}

void MainWindow::buildWaterPowerPage() {
    auto* rootLayout = ui->verticalLayoutRemote;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    // ===== 页头卡片（标题+副标题） =====
    {
        auto* headerCard = createPanelCard(ui->pageRemote);
        auto* headerLayout = new QHBoxLayout(headerCard);
        headerLayout->setContentsMargins(20, 16, 20, 16);

        auto* titleWrap = new QVBoxLayout();
        auto* titleLabel = new QLabel(QStringLiteral("水电数据"), headerCard);
        titleLabel->setStyleSheet("QLabel{color:#f8fbff;font-size:20px;font-weight:800;}");
        auto* subtitleLabel = new QLabel(QStringLiteral("实时监控电力与水资源状态，掌握消耗趋势"), headerCard);
        subtitleLabel->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;margin-top:2px;}");
        titleWrap->addWidget(titleLabel);
        titleWrap->addWidget(subtitleLabel);
        headerLayout->addLayout(titleWrap);
        headerLayout->addStretch();
        rootLayout->addWidget(headerCard);
    }

    // ===== 水电剩余资源卡片（左电右水，宽松排列） =====
    {
        auto* resCard = createPanelCard(ui->pageRemote);
        auto* resLayout = new QVBoxLayout(resCard);
        resLayout->setContentsMargins(24, 16, 24, 16);
        resLayout->setSpacing(14);

        auto* resTitle = new QLabel(QStringLiteral("水电剩余资源"), resCard);
        resTitle->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
        resLayout->addWidget(resTitle);

        // 主布局：左右两栏
        auto* mainRow = new QHBoxLayout();
        mainRow->setSpacing(30);

        // ---- 左栏：电量（图标在左，信息在右） ----
        {
            auto* leftSide = new QHBoxLayout();
            leftSide->setSpacing(14);

            // 左侧：电池图标
            auto* gaugeCol = new QVBoxLayout();
            auto* elecDot = new QLabel(resCard);
            elecDot->setFixedSize(10, 10);
            elecDot->setStyleSheet("QLabel{background:#22c55e;border-radius:5px;}");
            auto* elecLabel = new QLabel(QStringLiteral("电力"), resCard);
            elecLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:12px;font-weight:700;}");
            auto* elecHeader = new QHBoxLayout();
            elecHeader->addWidget(elecDot);
            elecHeader->addWidget(elecLabel);
            elecHeader->addStretch();
            gaugeCol->addLayout(elecHeader);
            m_batteryGauge = new BatteryGauge(resCard);
            gaugeCol->addWidget(m_batteryGauge, 0, Qt::AlignCenter);
            leftSide->addLayout(gaugeCol);

            // 右侧：信息文字
            m_batteryInfoLabel = new QLabel(resCard);
            m_batteryInfoLabel->setStyleSheet("QLabel{color:#cbd5e1;font-size:12px;line-height:1.6;}");
            m_batteryInfoLabel->setWordWrap(true);
            m_batteryInfoLabel->setMinimumWidth(130);
            leftSide->addWidget(m_batteryInfoLabel);
            leftSide->addStretch();

            mainRow->addLayout(leftSide);
        }

        // 分隔线
        auto* sep = new QFrame(resCard);
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("QFrame{color:rgba(255,255,255,0.08);}");
        mainRow->addWidget(sep);

        // ---- 右栏：水量（图标在左，信息在右） ----
        {
            auto* rightSide = new QHBoxLayout();
            rightSide->setSpacing(14);

            // 左侧：水箱图标
            auto* gaugeCol = new QVBoxLayout();
            auto* waterDot = new QLabel(resCard);
            waterDot->setFixedSize(10, 10);
            waterDot->setStyleSheet("QLabel{background:#3b82f6;border-radius:5px;}");
            auto* waterLabel = new QLabel(QStringLiteral("供水"), resCard);
            waterLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:12px;font-weight:700;}");
            auto* waterHeader = new QHBoxLayout();
            waterHeader->addWidget(waterDot);
            waterHeader->addWidget(waterLabel);
            waterHeader->addStretch();
            gaugeCol->addLayout(waterHeader);
            m_tankGauge = new TankGauge(resCard);
            gaugeCol->addWidget(m_tankGauge, 0, Qt::AlignCenter);
            rightSide->addLayout(gaugeCol);

            // 右侧：信息文字
            m_tankInfoLabel = new QLabel(resCard);
            m_tankInfoLabel->setStyleSheet("QLabel{color:#cbd5e1;font-size:12px;line-height:1.6;}");
            m_tankInfoLabel->setWordWrap(true);
            m_tankInfoLabel->setMinimumWidth(130);
            rightSide->addWidget(m_tankInfoLabel);
            rightSide->addStretch();

            mainRow->addLayout(rightSide);
        }

        resLayout->addLayout(mainRow);
        rootLayout->addWidget(resCard);
    }

    // ===== 今日消耗 + 实时负载 =====
    {
        auto* loadCard = createPanelCard(ui->pageRemote);
        auto* loadLayout = new QHBoxLayout(loadCard);
        loadLayout->setContentsMargins(24, 16, 24, 16);
        loadLayout->setSpacing(20);

        // -- 左：实时负载大字 --
        auto* loadLeft = new QVBoxLayout();
        auto* loadTitle = new QLabel(QStringLiteral("实时负载"), loadCard);
        loadTitle->setStyleSheet("QLabel{color:#94a3b8;font-size:11px;font-weight:600;}");
        loadLeft->addWidget(loadTitle, 0, Qt::AlignLeft);
        m_wpLoadValueLabel = new QLabel(QStringLiteral("-- mA"), loadCard);
        m_wpLoadValueLabel->setStyleSheet("QLabel{color:#22c55e;font-size:42px;font-weight:800;}");
        loadLeft->addWidget(m_wpLoadValueLabel);
        m_wpLoadStatusLabel = new QLabel(QStringLiteral("等待数据..."), loadCard);
        m_wpLoadStatusLabel->setStyleSheet("QLabel{color:#64748b;font-size:12px;}");
        loadLeft->addWidget(m_wpLoadStatusLabel);
        loadLayout->addLayout(loadLeft);

        // 分隔线
        auto* sep = new QFrame(loadCard);
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("QFrame{color:rgba(255,255,255,0.08);}");
        loadLayout->addWidget(sep);

        // -- 右：今日消耗 --
        auto* todayRight = new QVBoxLayout();
        todayRight->setSpacing(10);
        auto* todayTitle = new QLabel(QStringLiteral("今日已用"), loadCard);
        todayTitle->setStyleSheet("QLabel{color:#94a3b8;font-size:11px;font-weight:600;}");
        todayRight->addWidget(todayTitle, 0, Qt::AlignLeft);
        m_wpTodayPowerLabel = new QLabel(QStringLiteral("用电: -- mAh"), loadCard);
        m_wpTodayPowerLabel->setStyleSheet("QLabel{color:#fbbf24;font-size:18px;font-weight:700;}");
        todayRight->addWidget(m_wpTodayPowerLabel);
        m_wpTodayWaterLabel = new QLabel(QStringLiteral("用水: -- L"), loadCard);
        m_wpTodayWaterLabel->setStyleSheet("QLabel{color:#60a5fa;font-size:18px;font-weight:700;}");
        todayRight->addWidget(m_wpTodayWaterLabel);
        loadLayout->addLayout(todayRight);

        rootLayout->addWidget(loadCard);
    }

    // ===== 实时电流/水流趋势折线图 =====
    {
        auto* chartCard = createPanelCard(ui->pageRemote);
        auto* chartLayout = new QVBoxLayout(chartCard);
        chartLayout->setContentsMargins(12, 10, 12, 10);

        auto* chartTitle = new QLabel(QStringLiteral("实时电流 / 水流趋势"), chartCard);
        chartTitle->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
        chartLayout->addWidget(chartTitle);

        // 电流=红色，水流=绿色
        m_wpCurrentSeries = new QLineSeries(this);
        m_wpCurrentSeries->setName(QStringLiteral("电流(A)"));
        m_wpCurrentSeries->setColor(QColor(239, 68, 68));   // red
        m_wpFlowSeries = new QLineSeries(this);
        m_wpFlowSeries->setName(QStringLiteral("水流(L/min)"));
        m_wpFlowSeries->setColor(QColor(16, 185, 129));     // green

        m_wpChart = new QChart();
        m_wpChart->setBackgroundVisible(false);
        m_wpChart->setPlotAreaBackgroundVisible(true);
        m_wpChart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
        m_wpChart->legend()->setVisible(true);
        m_wpChart->legend()->setLabelColor(QColor(0xdb, 0xea, 0xfe));
        m_wpChart->addSeries(m_wpCurrentSeries);
        m_wpChart->addSeries(m_wpFlowSeries);

        // X 轴：时间
        m_wpAxisX = new QDateTimeAxis(this);
        m_wpAxisX->setFormat("HH:mm:ss");
        m_wpAxisX->setLabelsColor(QColor(0x94, 0xa3, 0xb8));
        m_wpChart->addAxis(m_wpAxisX, Qt::AlignBottom);
        m_wpCurrentSeries->attachAxis(m_wpAxisX);
        m_wpFlowSeries->attachAxis(m_wpAxisX);

        // 左 Y 轴：电流 (A) 红色
        m_wpAxisY_Cur = new QValueAxis(this);
        m_wpAxisY_Cur->setTitleText(QStringLiteral("电流(A)"));
        m_wpAxisY_Cur->setLabelsColor(QColor(239, 68, 68));
        m_wpAxisY_Cur->setTitleBrush(QBrush(QColor(239, 68, 68)));
        m_wpAxisY_Cur->setRange(0, 15);
        m_wpChart->addAxis(m_wpAxisY_Cur, Qt::AlignLeft);
        m_wpCurrentSeries->attachAxis(m_wpAxisY_Cur);

        // 右 Y 轴：水流 (L/min) 绿色
        m_wpAxisY_Flow = new QValueAxis(this);
        m_wpAxisY_Flow->setTitleText(QStringLiteral("水流(L/min)"));
        m_wpAxisY_Flow->setLabelsColor(QColor(16, 185, 129));
        m_wpAxisY_Flow->setTitleBrush(QBrush(QColor(16, 185, 129)));
        m_wpAxisY_Flow->setRange(0, 10);
        m_wpChart->addAxis(m_wpAxisY_Flow, Qt::AlignRight);
        m_wpFlowSeries->attachAxis(m_wpAxisY_Flow);

        m_wpChartView = new QChartView(m_wpChart, chartCard);
        m_wpChartView->setRenderHint(QPainter::Antialiasing, true);
        m_wpChartView->setMinimumHeight(280);
        m_wpChartView->setStyleSheet("background:transparent;");

        chartLayout->addWidget(m_wpChartView);
        rootLayout->addWidget(chartCard);
    }
}

void MainWindow::buildHistoryPage() {
    auto* rootLayout = ui->verticalLayoutEnvironment;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    // ===== Header =====
    auto* headerCard = createPanelCard(ui->pageEnvironment);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);
    auto* titleWrap = new QVBoxLayout();
    auto* titleLabel = new QLabel("历史数据", headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel(
        "查看趋势分析。选择时间范围与数据类型，点击确认后展示折线图。",
        headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(subLabel);
    headerLayout->addLayout(titleWrap);
    headerLayout->addStretch();
    auto* exportButton = new QPushButton("导出 Excel", headerCard);
    exportButton->setStyleSheet(
        "QPushButton{background:rgba(18,92,178,0.92);color:white;border:1px solid rgba(125,211,252,0.35);"
        "border-radius:10px;padding:8px 18px;font-weight:700;}"
        "QPushButton:hover{background:rgba(26,120,226,0.96);}");
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportHistoryClicked);
    headerLayout->addWidget(exportButton);
    rootLayout->addWidget(headerCard);

    // ===== Toolbar: 时间范围 + 数据类型 =====
    auto* toolCard = createPanelCard(ui->pageEnvironment);
    auto* toolLayout = new QVBoxLayout(toolCard);
    toolLayout->setContentsMargins(16, 10, 16, 10);
    toolLayout->setSpacing(8);

    const QString confirmStyle =
        "QPushButton{background:#0f5fa8;color:white;border:1px solid #2f7fca;"
        "border-radius:8px;padding:6px 24px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#1a7ad4;}";
    const QString cbStyle =
        "QCheckBox{color:#dbeafe;font-size:13px;spacing:4px;}"
        "QCheckBox::indicator{width:16px;height:16px;border:2px solid #5a7ba8;border-radius:3px;background:transparent;}"
        "QCheckBox::indicator:checked{background:#0f5fa8;border-color:#2f7fca;}";

    // 第一行：起始时间 ~ 结束时间 + 查询
    auto* topRow = new QHBoxLayout();
    const QString dtStyle =
        "QDateTimeEdit{background:rgba(7,26,54,0.88);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);"
        "border-radius:8px;padding:6px 10px;font-size:13px;}"
        "QDateTimeEdit:hover{border-color:rgba(96,165,250,0.7);}"
        "QDateTimeEdit:focus{border-color:#38bdf8;}"
        "QDateTimeEdit::up-button{width:16px;}"
        "QDateTimeEdit::down-button{width:16px;}";

    const QDateTime now = QDateTime::currentDateTime();

    auto* startLabel = new QLabel("起始：", toolCard);
    startLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    auto* startEdit = new QDateTimeEdit(now.addDays(-1), toolCard);
    startEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    startEdit->setMaximumDateTime(now);
    startEdit->setStyleSheet(dtStyle);
    startEdit->setObjectName("historyStartTime");

    auto* endLabel = new QLabel("结束：", toolCard);
    endLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    auto* endEdit = new QDateTimeEdit(now, toolCard);
    endEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    endEdit->setMaximumDateTime(now);
    endEdit->setStyleSheet(dtStyle);
    endEdit->setObjectName("historyEndTime");

    m_historyConfirmBtn = new QPushButton("查询", toolCard);
    m_historyConfirmBtn->setStyleSheet(confirmStyle);

    topRow->addWidget(startLabel);
    topRow->addWidget(startEdit);
    topRow->addSpacing(8);
    topRow->addWidget(endLabel);
    topRow->addWidget(endEdit);
    topRow->addSpacing(12);
    topRow->addWidget(m_historyConfirmBtn);
    topRow->addStretch();
    toolLayout->addLayout(topRow);

    // 第二行：数据类型 checkboxes
    auto* sensorRow = new QHBoxLayout();
    auto* sensorLabel = new QLabel("数据类型：", toolCard);
    sensorLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    sensorRow->addWidget(sensorLabel);

    const QStringList sensorNames = {"温度", "湿度", "PM2.5", "空气指数", "电流", "水流"};
    m_historySensorCheckBoxes.clear();
    for (int i = 0; i < sensorNames.size(); ++i) {
        auto* cb = new QCheckBox(sensorNames[i], toolCard);
        cb->setChecked(true);
        cb->setStyleSheet(cbStyle);
        sensorRow->addWidget(cb);
        m_historySensorCheckBoxes.append(cb);
    }
    sensorRow->addStretch();
    toolLayout->addLayout(sensorRow);

    rootLayout->addWidget(toolCard);

    // ===== 折线图容器（动态网格）=====
    m_historyChartContainer = new QWidget(ui->pageEnvironment);
    m_historyChartContainer->setStyleSheet("background:transparent;");
    m_historyGridLayout = new QGridLayout(m_historyChartContainer);
    m_historyGridLayout->setSpacing(10);
    m_historyGridLayout->setContentsMargins(0, 0, 0, 0);

    const QList<QColor> colors = {
        QColor(248, 113, 113),  // 温度 珊瑚红
        QColor(251, 191, 36),   // 湿度 琥珀
        QColor(251, 146, 60),   // PM2.5 橙
        QColor(163, 230, 53),   // 空气指数 青柠
        QColor(196, 132, 252),  // 电流 浅紫
        QColor(244, 114, 182)   // 水流 粉红
    };
    const QStringList yTitles = {"温度 (℃)", "湿度 (%)", "PM2.5 (μg/m³)", "空气指数", "电流 (A)", "水流 (L/min)"};

    const QString chartCardStyle =
        "QFrame{background:rgba(8,27,58,0.5);border:1px solid rgba(96,165,250,0.2);"
        "border-radius:8px;}";

    for (int i = 0; i < 6; ++i) {
        // 先建对象，再入图，最后设属性填数据
        auto* upper = new QLineSeries(this);
        auto* lower = new QLineSeries(this);
        auto* area  = new QAreaSeries(upper, lower);

        auto* chart = new QChart();
        chart->setTitle(sensorNames[i]);
        chart->setTitleBrush(QBrush(QColor(0xe8, 0xf1, 0xff)));
        chart->setBackgroundVisible(false);
        chart->setPlotAreaBackgroundVisible(true);
        chart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
        chart->legend()->setVisible(false);
        chart->setMargins(QMargins(6, 8, 6, 4));
        chart->setAnimationOptions(QChart::SeriesAnimations);

        // series 入图后再设属性
        chart->addSeries(area);
        m_historyLineSeries.append(upper);
        m_historyLowerSeries.append(lower);
        upper->setPen(QPen(QBrush(colors[i]), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        upper->setPointsVisible(false);
        lower->setPen(QPen(Qt::transparent));
        area->setBorderColor(Qt::transparent);
        QColor areaColor = colors[i];
        areaColor.setAlpha(58);
        area->setColor(areaColor);
        area->setBrush(QBrush(areaColor));

        lower->append(0, 0);
        lower->append(1, 0);

        auto* axisX = new QDateTimeAxis();
        axisX->setFormat("HH:mm");
        axisX->setLabelsColor(QColor(0x9a, 0xba, 0xda));
        axisX->setGridLineColor(QColor(255, 255, 255, 14));
        axisX->setGridLineVisible(true);
        axisX->setTickCount(4);
        axisX->setLabelsFont(QFont("sans", 8));
        chart->addAxis(axisX, Qt::AlignBottom);
        area->attachAxis(axisX);
        upper->attachAxis(axisX);

        auto* axisY = new QValueAxis();
        axisY->setLabelsColor(QColor(0x9a, 0xba, 0xda));
        axisY->setGridLineColor(QColor(255, 255, 255, 14));
        axisY->setTitleText(yTitles[i]);
        axisY->setTitleBrush(QBrush(QColor(0xbf, 0xdb, 0xfe)));
        axisY->setLabelFormat("%.1f");
        axisY->setLabelsFont(QFont("sans", 8));
        chart->addAxis(axisY, Qt::AlignLeft);
        area->attachAxis(axisY);
        upper->attachAxis(axisY);

        auto* chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing, true);
        chartView->setStyleSheet("background:transparent;border:none;");
        chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        chartView->setMouseTracking(true);
        chartView->setAttribute(Qt::WA_Hover, true);
        chartView->installEventFilter(this);

        auto* card = new QFrame(m_historyChartContainer);
        card->setStyleSheet(chartCardStyle);
        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(6, 4, 6, 4);
        cardLay->addWidget(chartView);
        m_historyChartViews.append(chartView);
        chartView->setProperty("sensorIndex", i);
    }

    rootLayout->addWidget(m_historyChartContainer, 1);

    // ===== 统计信息 + 趋势摘要 =====
    auto* infoRow = new QHBoxLayout();
    infoRow->setSpacing(16);

    auto* statsCard = createPanelCard(ui->pageEnvironment);
    auto* statsLayout = new QVBoxLayout(statsCard);
    statsLayout->setContentsMargins(18, 16, 18, 16);
    auto* statsTitle = new QLabel("统计信息", statsCard);
    statsTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    m_historyStatsLabel = new QLabel("--", statsCard);
    m_historyStatsLabel->setWordWrap(true);
    m_historyStatsLabel->setVisible(false);
    m_historyStatsLabel->setStyleSheet("QLabel{color:#e8f1ff;line-height:1.6;}");

    auto* statsColumns = new QHBoxLayout();
    statsColumns->setContentsMargins(0, 8, 0, 0);
    statsColumns->setSpacing(18);

    m_historyStatsLeftLabel = new QLabel("--", statsCard);
    m_historyStatsLeftLabel->setWordWrap(true);
    m_historyStatsLeftLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_historyStatsLeftLabel->setStyleSheet("QLabel{color:#e8f1ff;line-height:1.6;}");

    m_historyStatsRightLabel = new QLabel("--", statsCard);
    m_historyStatsRightLabel->setWordWrap(true);
    m_historyStatsRightLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_historyStatsRightLabel->setStyleSheet("QLabel{color:#e8f1ff;line-height:1.6;}");

    statsColumns->addWidget(m_historyStatsLeftLabel, 1);
    statsColumns->addWidget(m_historyStatsRightLabel, 1);
    statsLayout->addLayout(statsColumns);

    auto* summaryCard = createPanelCard(ui->pageEnvironment);
    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    auto* summaryTitle = new QLabel("趋势摘要", summaryCard);
    summaryTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    m_historySummaryLabel = new QLabel("--", summaryCard);
    m_historySummaryLabel->setWordWrap(true);
    m_historySummaryLabel->setStyleSheet("QLabel{color:#e8f1ff;line-height:1.7;}");
    summaryLayout->addWidget(summaryTitle);
    summaryLayout->addWidget(m_historySummaryLabel);

    infoRow->addWidget(statsCard, 1);
    infoRow->addWidget(summaryCard, 1);
    rootLayout->addLayout(infoRow);

    // ===== 连接信号 =====
    connect(m_historyConfirmBtn, &QPushButton::clicked, this, [this]() {
        refreshHistoryPage();
    });

    // 初始加载
    refreshHistoryPage();
}

void MainWindow::buildAlarmPage() {
    auto* rootLayout = ui->verticalLayoutAlarm;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    auto* headerCard = createPanelCard(ui->pageAlarm);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);

    auto* titleWrap = new QVBoxLayout();
    auto* titleLabel = new QLabel("报警管理", headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel(
        "查看与管理历史报警记录，支持按级别筛选。",
        headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(subLabel);

    headerLayout->addLayout(titleWrap);
    headerLayout->addStretch();
    rootLayout->addWidget(headerCard);

    // ===== 统计概览卡片 =====
    auto* statsCard = createPanelCard(ui->pageAlarm);
    auto* statsLayout = new QHBoxLayout(statsCard);
    statsLayout->setContentsMargins(20, 14, 20, 14);
    statsLayout->setSpacing(24);

    auto makeStat = [&](const QString& label, const QString& color, const QString& objName) {
        auto* wrap = new QWidget(statsCard);
        wrap->setStyleSheet("background:transparent;");
        auto* lay = new QVBoxLayout(wrap);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        auto* val = new QLabel("0", wrap);
        val->setStyleSheet(QString("QLabel{font-size:28px;font-weight:800;color:%1;}").arg(color));
        val->setAlignment(Qt::AlignCenter);
        val->setObjectName(objName);
        auto* lbl = new QLabel(label, wrap);
        lbl->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
        lbl->setAlignment(Qt::AlignCenter);
        lay->addWidget(val);
        lay->addWidget(lbl);
        statsLayout->addWidget(wrap);
    };

    makeStat("今日报警", "#f8fbff", "alarmStatTotal");
    makeStat("严重", "#ef4444", "alarmStatDanger");
    makeStat("预警", "#f59e0b", "alarmStatWarn");
    makeStat("求助", "#f59e0b", "alarmStatHelp");
    makeStat("已解决", "#22c55e", "alarmStatDone");
    rootLayout->addWidget(statsCard);

    // ===== 筛选栏 =====
    auto* filterCard = createPanelCard(ui->pageAlarm);
    auto* filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(16, 10, 16, 10);
    filterLayout->setSpacing(12);

    auto* filterLabel = new QLabel("级别筛选：", filterCard);
    filterLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    auto* levelFilter = new QComboBox(filterCard);
    levelFilter->addItems({"全部", "严重", "预警", "求助"});
    levelFilter->setStyleSheet(
        "QComboBox{background:rgba(7,26,54,0.88);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);"
        "border-radius:8px;padding:6px 12px;font-size:13px;min-width:120px;}"
        "QComboBox:hover{border-color:rgba(96,165,250,0.7);}"
        "QComboBox QAbstractItemView{background:#0a1628;color:#e8f1ff;border:1px solid #1e3a5f;"
        "selection-background-color:#0f5fa8;outline:none;}");
    levelFilter->setObjectName("alarmLevelFilter");
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(levelFilter);
    filterLayout->addStretch();
    rootLayout->addWidget(filterCard);

    // ===== 报警表格（8列：始/终/传感器/内容/级别/状态/操作）=====
    m_alarmInfoTable = new QTableWidget(0, 7, ui->pageAlarm);
    m_alarmInfoTable->setHorizontalHeaderLabels({"起始时间", "结束时间", "传感器", "内容", "级别", "状态", "操作"});
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_alarmInfoTable->verticalHeader()->setVisible(false);
    m_alarmInfoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmInfoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmInfoTable->setAlternatingRowColors(true);

    auto* infoCard = createPanelCard(ui->pageAlarm);
    auto* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(12, 12, 12, 12);
    auto* infoTitle = new QLabel("报警记录", infoCard);
    infoTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    infoLayout->addWidget(infoTitle);
    infoLayout->addWidget(m_alarmInfoTable);
    rootLayout->addWidget(infoCard, 1);

    connect(levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { refreshAlarmInfoFromDatabase(); });
}

void MainWindow::buildDevicePage() {
    auto* rootLayout = ui->verticalLayoutWP;
    clearLayout(rootLayout);
    rootLayout->setSpacing(16);

    // ====== Header ======
    auto* headerCard = createPanelCard(ui->pageWaterPower);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(24, 18, 24, 14);
    headerLayout->setSpacing(6);
    auto* titleLabel = new QLabel("设备管理", headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    headerLayout->addWidget(titleLabel);
    auto* hintLabel = new QLabel("远程设置 STM32 传感器阈值，以及控制蜂鸣器、风扇、窗户、警报灯等执行器的开关。阈值下发后设备立即生效，重启恢复默认。",
                                headerCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
    headerLayout->addWidget(hintLabel);
    rootLayout->addWidget(headerCard);

    // ====== 阈值远程设置 ======
    {
        auto* thCard = createPanelCard(ui->pageWaterPower);
        auto* thLayout = new QVBoxLayout(thCard);
        thLayout->setContentsMargins(20, 16, 20, 16);
        thLayout->setSpacing(12);

        // 标题行
        auto* thHeader = new QHBoxLayout();
        auto* thTitle = new QLabel("⚙ 阈值远程设置", thCard);
        thTitle->setStyleSheet("QLabel{font-size:20px;font-weight:800;color:#7dd3fc;}");
        thHeader->addWidget(thTitle);
        thHeader->addStretch();
        thLayout->addLayout(thHeader);

        // 阈值：温湿度上下限 + PM/电流/水流/空气质量上限
        struct ThDef { const char* key; const char* label; const char* unit; int defaultVal; };
        // Qt只配告警值，下发后STM32自动推导预警值
        auto* deriveHint = new QLabel(QStringLiteral(
            "配置告警阈值，STM32自动推算预警值\n"
            "预警=告警-6℃(温)/-15%(湿)/÷2(PM/AQ/水流)/×2/3(电流)"),
            thCard);
        deriveHint->setStyleSheet("QLabel{color:#94a3b8;font-size:11px;}");
        deriveHint->setWordWrap(true);
        thLayout->addWidget(deriveHint);

        ThDef thDefs[] = {
            {"ta", "温度告警上限", "℃", 38},
            {"tb", "温度告警下限", "℃", 10},
            {"ha", "湿度告警上限", "%", 85},
            {"hb", "湿度告警下限", "%", 20},
            {"pa", "PM2.5告警", "μg/m³", 150},
            {"aa", "AQ告警", "", 200},
            {"ca", "电流告警", "A", 15},
            {"fa", "水流告警", "L/min", 10},
        };
        QVector<QSpinBox*> spinPtrs;

        auto* thGrid = new QGridLayout();
        thGrid->setHorizontalSpacing(20);
        thGrid->setVerticalSpacing(8);

        auto makeSpin = [&](int val) -> QSpinBox* {
            auto* s = new QSpinBox(thCard);
            s->setRange(0, 5000);
            s->setValue(val);
            s->setFixedSize(90, 32);
            s->setAlignment(Qt::AlignCenter);
            s->setStyleSheet(
                "QSpinBox{background:#0f1f3a;color:#7dd3fc;border:1px solid #2f7fca;"
                "border-radius:6px;font-size:14px;font-weight:700;}"
                "QSpinBox:hover{border-color:#5ba0e8;}"
                "QSpinBox:focus{border-color:#38bdf8;background:#0a1630;}"
                "QSpinBox::up-button{width:20px;border-radius:3px;}"
                "QSpinBox::down-button{width:20px;border-radius:3px;}");
            return s;
        };

        for (int i = 0; i < 8; i++) {
            auto& d = thDefs[i];
            auto* w = new QWidget(thCard);
            w->setStyleSheet("background:transparent;");
            auto* hl = new QHBoxLayout(w);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(6);
            auto* lb = new QLabel(d.label, w);
            lb->setStyleSheet("QLabel{color:#e8f0ff;font-size:13px;font-weight:600;}");
            auto* sp = makeSpin(d.defaultVal);
            spinPtrs.append(sp);
            auto* un = new QLabel(d.unit, w);
            un->setStyleSheet("QLabel{color:#7a8fb8;font-size:11px;}");
            hl->addWidget(lb);
            hl->addStretch();
            hl->addWidget(sp);
            hl->addWidget(un);
            thGrid->addWidget(w, i / 2, i % 2);  // 2 列布局
        }

        thLayout->addLayout(thGrid);

        // 按钮行
        auto* thBtnRow = new QHBoxLayout();
        thBtnRow->setSpacing(16);
        auto* thResetBtn = new QPushButton("恢复默认", thCard);
        thResetBtn->setMinimumHeight(38);
        thResetBtn->setStyleSheet(
            "QPushButton{background:#1e293b;color:#94a3b8;border:1px solid #334155;"
            "border-radius:8px;padding:6px 20px;font-size:13px;font-weight:600;}"
            "QPushButton:hover{background:#334155;color:#cbd5e1;}");
        auto* thSendBtn = new QPushButton("一键下发", thCard);
        thSendBtn->setMinimumHeight(38);
        thSendBtn->setStyleSheet(
            "QPushButton{background:#0f5fa8;color:white;border:1px solid #2f7fca;"
            "border-radius:8px;padding:6px 28px;font-size:14px;font-weight:700;}"
            "QPushButton:hover{background:#1a7ad4;}"
            "QPushButton:pressed{background:#0a4a8a;}");
        thBtnRow->addStretch();
        thBtnRow->addWidget(thResetBtn);
        thBtnRow->addWidget(thSendBtn);
        thBtnRow->addStretch();
        thLayout->addLayout(thBtnRow);

        connect(thResetBtn, &QPushButton::clicked, this, [this, spinPtrs, thDefs]() {
            for (int i = 0; i < spinPtrs.size(); i++)
                spinPtrs[i]->setValue(thDefs[i].defaultVal);
            // 同时下发恢复默认命令给STM32
            m_mqtt.publishText("test001up", "{\"cmd\":\"reset_th\"}");
        });

        connect(thSendBtn, &QPushButton::clicked, this, [this, spinPtrs, thDefs]() {
            if (!customConfirm(this, "确认下发",
                    "将阈值下发至 STM32，设备将立即按新阈值运行。\n是否确认？"))
                return;

            QJsonObject thObj;
            for (int i = 0; i < spinPtrs.size(); i++)
                thObj[thDefs[i].key] = spinPtrs[i]->value();

            QJsonObject cmd;
            cmd["th"] = thObj;
            m_mqtt.publishText("test001up",
                QString::fromUtf8(QJsonDocument(cmd).toJson(QJsonDocument::Compact)));

            customMessage(this, "已发送",
                "阈值指令已通过 MQTT 下发。\nESP32 将自动转发至 STM32。");
        });

        rootLayout->addWidget(thCard);
    }

    auto* controlCard = createPanelCard(ui->pageWaterPower);
    auto* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlLayout->setSpacing(10);
    auto* controlTitle = new QLabel("设备控制", controlCard);
    controlTitle->setStyleSheet("QLabel{font-size:18px;font-weight:800;color:#7dd3fc;}");
    controlLayout->addWidget(controlTitle);

    auto publishDeviceCode = [this](int code, const QString& name) {
        const QString topic = QStringLiteral("test001up");
        QJsonObject cmd;
        cmd["code"] = code;
        cmd["name"] = name;
        m_mqtt.publishText(topic, QString::fromUtf8(QJsonDocument(cmd).toJson(QJsonDocument::Compact)));
        appendRemoteControlLog("SYSTEM", name, QStringLiteral("已发送到 %1").arg(topic));
    };

    const QString ctrlBtnNormal =
        "QPushButton{background:rgba(37,99,235,0.45);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);"
        "border-radius:8px;padding:6px 18px;font-weight:600;font-size:13px;}"
        "QPushButton:hover{background:rgba(59,130,246,0.65);}";
    const QString ctrlOnActive =
        "QPushButton{background:#075985;color:#f0f9ff;border:2px solid #38bdf8;border-radius:8px;"
        "padding:5px 16px;font-weight:800;font-size:13px;}"
        "QPushButton:hover{background:#0c4a6e;}";

    auto createCodeControlRow = [&](const QString& title,
                                    const QString& onBtnText,
                                    const QString& offBtnText,
                                    int onCode,
                                    int offCode,
                                    const QString& onLogName,
                                    const QString& offLogName) {
        auto* row = new QHBoxLayout();
        row->setSpacing(12);
        auto* label = new QLabel(title, controlCard);
        label->setMinimumWidth(72);
        label->setStyleSheet("QLabel{font-size:14px;color:#dbeafe;font-weight:700;}");
        auto* onButton = new QPushButton(onBtnText, controlCard);
        auto* offButton = new QPushButton(offBtnText, controlCard);
        onButton->setMinimumHeight(36);
        offButton->setMinimumHeight(36);
        onButton->setStyleSheet(ctrlBtnNormal);
        offButton->setStyleSheet(ctrlBtnNormal);
        connect(onButton, &QPushButton::clicked, this, [publishDeviceCode, onCode, onLogName, onButton, ctrlOnActive]() {
            publishDeviceCode(onCode, onLogName);
            onButton->setStyleSheet(ctrlOnActive);
        });
        connect(offButton, &QPushButton::clicked, this, [publishDeviceCode, offCode, offLogName, onButton, ctrlBtnNormal]() {
            publishDeviceCode(offCode, offLogName);
            onButton->setStyleSheet(ctrlBtnNormal);
        });
        row->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);
        row->addStretch();
        row->addWidget(onButton, 0, Qt::AlignRight);
        row->addWidget(offButton, 0, Qt::AlignRight);
        controlLayout->addLayout(row);
    };

    createCodeControlRow("警报", "开启", "关闭", 1, 0, "警报开启", "警报关闭");
    createCodeControlRow("风扇", "开启", "关闭", 4, 5, "风扇开启", "风扇关闭");
    createCodeControlRow("窗户", "打开", "关闭", 2, 3, "窗户打开", "窗户关闭");
    createCodeControlRow("警报灯", "开启", "关闭", 6, 7, "警报灯开启", "警报灯关闭");
    controlLayout->addSpacing(4);
    rootLayout->addWidget(controlCard);

    m_remoteControlLogTable = nullptr;
    m_remoteLogMarqueeView = new QPlainTextEdit(ui->pageWaterPower);
    m_remoteLogMarqueeView->setReadOnly(true);
    m_remoteLogMarqueeView->setMinimumHeight(200);
    m_remoteLogMarqueeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_remoteLogMarqueeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_remoteLogMarqueeView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_remoteLogMarqueeView->setMouseTracking(true);
    {
        QFont mono;
        mono.setStyleHint(QFont::TypeWriter, QFont::PreferQuality);
        mono.setFixedPitch(true);
        mono.setPointSize(10);
        m_remoteLogMarqueeView->setFont(mono);
    }
    m_remoteLogMarqueeView->setStyleSheet(
        "QPlainTextEdit{background:rgba(7,26,54,0.86);border:1px solid rgba(96,165,250,0.30);"
        "border-bottom-left-radius:8px;border-bottom-right-radius:8px;"
        "border-top-left-radius:0;border-top-right-radius:0;"
        "color:#dbeafe;font-size:13px;line-height:1.45;padding:8px 10px;}");

    // ==== 日志缓慢上移滚动 ====
    auto* logTimer = new QTimer(m_remoteLogMarqueeView);
    logTimer->setObjectName(QStringLiteral("logScrollTimer"));
    connect(logTimer, &QTimer::timeout, this, [this]() {
        if (!m_remoteLogMarqueeView || !m_remoteLogMarqueeView->isVisible())
            return;
        QScrollBar* bar = m_remoteLogMarqueeView->verticalScrollBar();
        if (!bar || bar->maximum() <= 0)
            return;
        int val = bar->value() + 1;
        if (val >= bar->maximum())
            val = 0;
        bar->setValue(val);
    });
    logTimer->start(150);

    // 鼠标悬停大幅减慢，移出恢复
    m_remoteLogMarqueeView->installEventFilter(this);

    auto* logCard = createPanelCard(ui->pageWaterPower);
    auto* logLayout = new QVBoxLayout(logCard);
    logLayout->setContentsMargins(10, 8, 10, 10);
    auto* logTitle = new QLabel("远程控制执行日志（最近 20 条）", logCard);
    logTitle->setStyleSheet("QLabel{font-size:13px;font-weight:600;color:#7dd3fc;}");
    logLayout->addWidget(logTitle);

    // 表头（固定不滚动）
    auto* logHeader = new QLabel(logCard);
    logHeader->setStyleSheet(
        "QLabel{background:rgba(15,95,168,0.5);border:1px solid rgba(96,165,250,0.30);"
        "border-bottom:none;border-top-left-radius:8px;border-top-right-radius:8px;"
        "color:#7dd3fc;font-size:13px;font-weight:700;padding:6px 12px;}");
    logHeader->setText(formatRemoteLogTableLine(QStringLiteral("时间"),
                                                QStringLiteral("设备"),
                                                QStringLiteral("指令"),
                                                QStringLiteral("详情")));
    logLayout->addWidget(logHeader);
    logLayout->addWidget(m_remoteLogMarqueeView);
    rootLayout->addWidget(logCard, 1);

    m_remoteCommandCombo = nullptr;
    m_remoteIntervalSpin = nullptr;
    refreshDeviceTable();
    loadRemoteExecLogTable();
}

void MainWindow::buildSettingsPage() {
    auto* rootLayout = ui->verticalLayoutSetting;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    auto* headerCard = createPanelCard(ui->pageSetting);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);
    auto* titleLabel = new QLabel("系统设置", headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel("账号信息、连接状态与系统日志。", headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subLabel);
    rootLayout->addWidget(headerCard);

    // ====== 账号信息卡片 ======
    auto* accountCard = createPanelCard(ui->pageSetting);
    auto* accountLayout = new QGridLayout(accountCard);
    accountLayout->setContentsMargins(18, 16, 18, 16);
    accountLayout->setHorizontalSpacing(12);
    accountLayout->setVerticalSpacing(10);
    auto* accountTitle = new QLabel("当前账号信息", accountCard);
    accountTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    accountLayout->addWidget(accountTitle, 0, 0, 1, 2);

    const QString roleText = (m_userRole == "admin") ? "管理员" : "普通用户";
    accountLayout->addWidget(new QLabel("账号：", accountCard), 1, 0);
    accountLayout->addWidget(new QLabel(m_userName, accountCard), 1, 1);
    accountLayout->addWidget(new QLabel("角色：", accountCard), 2, 0);
    accountLayout->addWidget(new QLabel(roleText, accountCard), 2, 1);
    accountLayout->addWidget(new QLabel("登录时间：", accountCard), 3, 0);
    accountLayout->addWidget(new QLabel(m_loginTime.toString("yyyy-MM-dd HH:mm:ss"), accountCard), 3, 1);

    auto* editAccountButton = new QPushButton("修改信息", accountCard);
    editAccountButton->setMinimumHeight(34);
    connect(editAccountButton, &QPushButton::clicked, this, &MainWindow::onEditAccountInfoClicked);
    accountLayout->addWidget(editAccountButton, 5, 0, 1, 1, Qt::AlignLeft);

    auto* switchAccountButton = new QPushButton("切换账号", accountCard);
    switchAccountButton->setMinimumHeight(34);
    connect(switchAccountButton, &QPushButton::clicked, this, &MainWindow::onSwitchAccountClicked);
    accountLayout->addWidget(switchAccountButton, 5, 1, 1, 1, Qt::AlignRight);
    rootLayout->addWidget(accountCard);

    // ====== MQTT + DB 状态（双列）======
    auto* statusCard = createPanelCard(ui->pageSetting);
    auto* statusRow = new QHBoxLayout(statusCard);
    statusRow->setContentsMargins(18, 16, 18, 16);
    statusRow->setSpacing(16);

    // MQTT 状态
    auto* mqttBox = new QWidget(statusCard);
    mqttBox->setStyleSheet("background:transparent;");
    auto* mqttLay = new QVBoxLayout(mqttBox);
    mqttLay->setContentsMargins(0, 0, 0, 0);
    mqttLay->setSpacing(8);
    auto* mqttTitle = new QLabel("MQTT 连接", mqttBox);
    mqttTitle->setStyleSheet("QLabel{font-size:15px;font-weight:700;color:#7dd3fc;}");
    mqttLay->addWidget(mqttTitle);

    auto addInfoRow = [](QVBoxLayout* lay, const QString& label, const QString& value, const QString& color = "#e8f1ff") {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
        auto* val = new QLabel(value);
        val->setStyleSheet(QString("QLabel{color:%1;font-size:12px;font-weight:600;}").arg(color));
        val->setWordWrap(true);
        row->addWidget(lbl);
        row->addWidget(val, 1);
        lay->addLayout(row);
    };
    const bool mqttConnected = m_mqtt.isConnected();
    addInfoRow(mqttLay, "状态：", mqttConnected ? "已连接" : "未连接",
               mqttConnected ? "#22c55e" : "#ef4444");
    addInfoRow(mqttLay, "Broker：", "bemfa.com:9501");
    addInfoRow(mqttLay, "Topic：", "test001up / WebQT1");
    mqttLay->addStretch();

    // DB 状态
    auto* dbBox = new QWidget(statusCard);
    dbBox->setStyleSheet("background:transparent;");
    auto* dbLay = new QVBoxLayout(dbBox);
    dbLay->setContentsMargins(0, 0, 0, 0);
    dbLay->setSpacing(8);
    auto* dbTitle = new QLabel("数据库信息", dbBox);
    dbTitle->setStyleSheet("QLabel{font-size:15px;font-weight:700;color:#7dd3fc;}");
    dbLay->addWidget(dbTitle);

    QString dbPath = QDir(DbConfig::kDbDir).filePath(DbConfig::kDbFileName);
    QFileInfo dbFi(dbPath);
    addInfoRow(dbLay, "路径：", QDir::toNativeSeparators(dbPath));
    addInfoRow(dbLay, "大小：", dbFi.exists()
        ? QString("%1 KB").arg(dbFi.size() / 1024) : "尚未创建");
    addInfoRow(dbLay, "数据库：", m_db ? "已连接" : "未连接",
               m_db ? "#22c55e" : "#f59e0b");
    addInfoRow(dbLay, "最后更新：", dbFi.exists()
        ? dbFi.lastModified().toString("yyyy-MM-dd HH:mm:ss") : "--");
    dbLay->addStretch();

    statusRow->addWidget(mqttBox);
    statusRow->addWidget(dbBox);
    rootLayout->addWidget(statusCard);

    // ====== 系统日志 ======
    m_logViewer = new QPlainTextEdit(ui->pageSetting);
    m_logViewer->setReadOnly(true);
    m_logViewer->setPlainText(
        QStringLiteral("%1 %2 登入")
            .arg(m_loginTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), m_userName));

    auto* logCard = createPanelCard(ui->pageSetting);
    auto* logLayout = new QVBoxLayout(logCard);
    logLayout->setContentsMargins(12, 12, 12, 12);
    auto* logTitle = new QLabel("系统日志", logCard);
    logTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    logLayout->addWidget(logTitle);
    logLayout->addWidget(m_logViewer);
    rootLayout->addWidget(logCard, 1);
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
        syncDeviceOfflineRecords(nowDt);
    } else {
        m_isDeviceOffline = false;
        m_offlineStartAt = QDateTime();
        m_lastOfflineDurationSec = -1;
    }
}

void MainWindow::onNavCurrentRowChanged(int row) {
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
        buildSettingsPage();
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
    if (m_realtimeSensorTable != nullptr) {
        const QList<QStringList> rows = {
            {"temperature", "离线", "--", "A区机柜"},
            {"humidity", "离线", "--", "B区仓储"},
            {"water_flow", "离线", "--", "管网主线"},
            {"current", "离线", "--", "供电柜"},
            {"air_quality", "离线", "--", "主通道"},
            {"pm25", "离线", "--", "主通道"}
        };
        for (int row = 0; row < rows.size(); ++row) {
            for (int col = 0; col < rows[row].size(); ++col) {
                m_realtimeSensorTable->setItem(row, col, new QTableWidgetItem(rows[row][col]));
            }
        }
    }
}

void MainWindow::syncDeviceOfflineRecords(const QDateTime& now) {
    if (m_db == nullptr || !m_offlineStartAt.isValid()) {
        return;
    }
    const qint64 durationSec = qMax<qint64>(0, m_offlineStartAt.secsTo(now));
    if (durationSec == m_lastOfflineDurationSec) {
        return;
    }
    m_lastOfflineDurationSec = durationSec;

    const QStringList sensorFields = {
        "temperature", "humidity", "water_flow", "current", "air_quality", "pm25"
    };
    for (const QString& sensor : sensorFields) {
        QString err;
        if (!m_db->upsertDeviceOfflineRecord(sensor, m_offlineStartAt, durationSec, &err)) {
            qDebug() << "[DB] upsert offline record failed:" << sensor << err;
        }
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
    if (m_cardTempValue == nullptr || m_totalPowerLabel == nullptr) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    double dtSec = 0.0;
    if (m_lastCumulativeSampleAt.isValid()) {
        const qint64 dtMs = m_lastCumulativeSampleAt.msecsTo(now);
        dtSec = qBound(0.0, static_cast<double>(dtMs) / 1000.0, 3600.0);
    }
    m_lastCumulativeSampleAt = now;

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

    for (auto& device : m_devices) {
        if (device.id == "TEMP-001")      { device.latestValue = temp; device.status = lvDeviceStatus(tempLv); }
        else if (device.id == "HUMI-002") { device.latestValue = hum; device.status = lvDeviceStatus(humiLv); }
        else if (device.id == "FLOW-003") { device.latestValue = flow; device.status = lvDeviceStatus(flowLv); }
        else if (device.id == "CURR-004") { device.latestValue = current; device.status = lvDeviceStatus(currentLv); }
        else if (device.id == "AIR-005")  { device.latestValue = airIndex; device.status = lvDeviceStatus(airLv); }
        else if (device.id == "PM25-006") { device.latestValue = pm25; device.status = lvDeviceStatus(pmLv); }
    }

    if (m_realtimeSensorTable != nullptr) {
        const QList<QStringList> rows = {
            {"temperature", lvStatusZh(tempLv), QString("%1 ℃").arg(QString::number(temp, 'f', 1)), "A区机柜"},
            {"humidity", lvStatusZh(humiLv), QString("%1 %").arg(QString::number(hum, 'f', 1)), "B区仓储"},
            {"water_flow", lvStatusZh(flowLv), QString("%1 L/min").arg(QString::number(flow, 'f', 2)), "管网主线"},
            {"current", lvStatusZh(currentLv), QString("%1 A").arg(QString::number(current / 1000.0, 'f', 1)), "供电柜"},
            {"air_quality", lvStatusZh(airLv), QString("%1 (%2)").arg(QString::number(airIndex, 'f', 0), airGrade), "主通道"},
            {"pm25", lvStatusZh(pmLv), QString("%1 ug/m3").arg(QString::number(pm25, 'f', 1)), "主通道"}
        };
        for (int row = 0; row < rows.size(); ++row) {
            for (int col = 0; col < rows[row].size(); ++col) {
                m_realtimeSensorTable->setItem(row, col, new QTableWidgetItem(rows[row][col]));
            }
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

    // 累计量：按相邻两次上报间隔 Δt（秒）积分。电流 mA→A；假定母线电压 220V；水流为 L/min。
    if (dtSec > 0.0) {
        constexpr double kLineVoltageV = 220.0;
        const double iAmpere = current / 1000.0;
        m_totalCurrent += iAmpere * (dtSec / 3600.0);
        const double powerKw = iAmpere * kLineVoltageV / 1000.0;
        m_totalPower += powerKw * (dtSec / 3600.0);
        m_totalWater += (flow / 60.0) * dtSec;
    }
    if (m_totalCurrentLabel) m_totalCurrentLabel->setText(QString("%1 Ah").arg(QString::number(m_totalCurrent, 'f', 3)));
    if (m_totalPowerLabel) m_totalPowerLabel->setText(QString("%1 kWh").arg(QString::number(m_totalPower, 'f', 3)));
    if (m_totalWaterLabel) m_totalWaterLabel->setText(QString("%1 L").arg(QString::number(m_totalWater, 'f', 1)));

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

    // 实时负载大字（此部分已移至updateResourcePct）

    // ===== 水电页实时电流/水流折线图追加 =====
    if (m_wpCurrentSeries != nullptr && m_wpFlowSeries != nullptr) {
        m_wpCurrentSeries->append(now.toMSecsSinceEpoch(), current / 1000.0);
        m_wpFlowSeries->append(now.toMSecsSinceEpoch(), flow);

        const int maxPoints = 60;
        if (m_wpCurrentSeries->count() > maxPoints) {
            m_wpCurrentSeries->removePoints(0, m_wpCurrentSeries->count() - maxPoints);
            m_wpFlowSeries->removePoints(0, m_wpFlowSeries->count() - maxPoints);
        }

        // X 轴：最近数据点的前后时间范围
        if (m_wpAxisX != nullptr && m_wpCurrentSeries->count() >= 2) {
            const auto pts = m_wpCurrentSeries->points();
            const qreal tMin = pts.first().x();
            const qreal tMax = pts.last().x();
            m_wpAxisX->setRange(QDateTime::fromMSecsSinceEpoch((qint64)tMin),
                                QDateTime::fromMSecsSinceEpoch((qint64)tMax));
        }
    }

    refreshHistoryPage();
}

void MainWindow::updateResourcePct(double batteryPct, double waterPct,
                                    double currentMA, double flowLMin,
                                    int usedPowerMAh, int usedWaterCL,
                                    int batRemainMAh, int wtrRemainCL, int powerStatus,
                                    int batCapMAh, int tankCapCL,
                                    int batRemainMin, int wtrRemainMin) {
    m_batteryPct = qBound(0.0, batteryPct, 100.0);
    m_waterPct   = qBound(0.0, waterPct,   100.0);

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
                "容量: %3 mAh")
                .arg(QString::number(remainMAh, 'f', 0),
                     QString::number(usedPowerMAh),
                     QString::number(batCapMAh));
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
                "容量: %3 L")
                .arg(QString::number(remainL, 'f', 1),
                     QString::number(usedL, 'f', 1),
                     QString::number(tankCapCL / 100.0, 'f', 1));
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

    if (m_tempSeries == nullptr || m_humiSeries == nullptr || m_pmSeries == nullptr || m_axisX == nullptr) {
        return;
    }

    qreal t = QDateTime::currentMSecsSinceEpoch();
    m_tempSeries->append(t, temp);
    m_humiSeries->append(t, hum);
    m_pmSeries->append(t, pm25);

    const int maxPoints = 20;
    if (m_tempSeries->count() > maxPoints) {
        m_tempSeries->removePoints(0, m_tempSeries->count() - maxPoints);
        m_humiSeries->removePoints(0, m_humiSeries->count() - maxPoints);
        m_pmSeries->removePoints(0, m_pmSeries->count() - maxPoints);
    }

    if (m_tempSeries->count() >= 2) {
        auto pts = m_tempSeries->points();
        m_axisX->setRange(QDateTime::fromMSecsSinceEpoch((qint64)pts.first().x()),
                           QDateTime::fromMSecsSinceEpoch((qint64)pts.last().x()));
    }
}

void MainWindow::onUpdateEnvironmentData() {
    // 预留空实现
}

void MainWindow::refreshHistoryPage() {
    if (m_db == nullptr || m_historyLineSeries.size() != 6 || m_historyChartViews.size() != 6) {
        return;
    }

    auto* startEdit = ui->pageEnvironment->findChild<QDateTimeEdit*>("historyStartTime");
    auto* endEdit   = ui->pageEnvironment->findChild<QDateTimeEdit*>("historyEndTime");
    const QDateTime rangeStart = startEdit ? startEdit->dateTime() : QDateTime::currentDateTime().addDays(-1);
    const QDateTime rangeEnd   = endEdit   ? endEdit->dateTime()   : QDateTime::currentDateTime();

    if (rangeStart >= rangeEnd) {
        for (auto* v : m_historyChartViews) {
            if (auto* card = v->parentWidget()) card->hide();
        }
        if (m_historyStatsLeftLabel) m_historyStatsLeftLabel->setText("时间范围有误");
        if (m_historySummaryLabel) m_historySummaryLabel->setText("起始时间必须早于结束时间，请重新选择。");
        return;
    }
    if (rangeEnd > QDateTime::currentDateTime()) {
        for (auto* v : m_historyChartViews) {
            if (auto* card = v->parentWidget()) card->hide();
        }
        if (m_historyStatsLeftLabel) m_historyStatsLeftLabel->setText("时间超出范围");
        if (m_historySummaryLabel) m_historySummaryLabel->setText("结束时间不能超过当前时间，请重新选择。");
        return;
    }

    // 查询完整时间范围内的数据点，避免被固定 LIMIT 截断。
    QString err;
    QList<SensorData> points = m_db->queryDataRange(rangeStart, rangeEnd, &err);
    if (!err.isEmpty()) {
        qDebug() << "[DB] query recent data failed:" << err;
    }

    // 计算哪些传感器被选中
    QList<int> selectedIndices;
    for (int i = 0; i < m_historySensorCheckBoxes.size(); ++i) {
        if (m_historySensorCheckBoxes[i]->isChecked()) {
            selectedIndices.append(i);
        }
    }

    const int totalSelected = selectedIndices.size();
    if (totalSelected == 0) {
        for (auto* v : m_historyChartViews) {
            if (auto* card = v->parentWidget()) card->hide();
        }
        if (m_historyStatsLeftLabel) m_historyStatsLeftLabel->setText("请至少选择一项数据类型。");
        if (m_historyStatsRightLabel) m_historyStatsRightLabel->setText(QString());
        if (m_historySummaryLabel) m_historySummaryLabel->setText("请在工具栏中选择数据类型后点击「确定」。");
        return;
    }

    // 计算行列数
    int cols = 0, rows = 0;
    if (totalSelected == 1)       { cols = 1; rows = 1; }
    else if (totalSelected == 2)  { cols = 2; rows = 1; }
    else if (totalSelected <= 4)  { cols = 2; rows = 2; }
    else                          { cols = 3; rows = (totalSelected + 2) / 3; }

    // 清空网格（只清布局项，不 delete widget）
    while (m_historyGridLayout->count() > 0) {
        auto* item = m_historyGridLayout->takeAt(0);
        item->widget()->setParent(nullptr);
        delete item;
    }
    // 重置行列拉伸
    for (int c = 0; c < m_historyGridLayout->columnCount(); ++c)
        m_historyGridLayout->setColumnStretch(c, 0);
    for (int r = 0; r < m_historyGridLayout->rowCount(); ++r)
        m_historyGridLayout->setRowStretch(r, 0);

    // 重排
    int gridIdx = 0;
    for (int si : selectedIndices) {
        QChartView* view = m_historyChartViews[si];
        QLineSeries* series = m_historyLineSeries[si];
        QLineSeries* lowerSeries = (si < m_historyLowerSeries.size()) ? m_historyLowerSeries[si] : nullptr;
        QChart* chart = view->chart();
        auto* card = view->parentWidget();

        // 清空旧数据
        series->clear();

        // 更新下边界基线为当前时间范围
        if (lowerSeries) {
            lowerSeries->clear();
            lowerSeries->append(rangeStart.toMSecsSinceEpoch(), 0);
            lowerSeries->append(rangeEnd.toMSecsSinceEpoch(), 0);
        }

        // 填入数据点
        qreal minVal = 1e18, maxVal = -1e18;
        for (const auto& pt : points) {
            double val = 0.0;
            switch (si) {
            case 0: val = pt.tempC; break;
            case 1: val = pt.humiPercent; break;
            case 2: val = pt.pm25UgM3; break;
            case 3: val = pt.airIndex; break;
            case 4: val = pt.currentA / 1000.0; break;  // mA→A
            case 5: val = pt.flowLMin; break;
            }
            series->append(pt.ts.toMSecsSinceEpoch(), val);
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
        }

        // 设置轴范围
        auto axes = chart->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            auto* axisX = qobject_cast<QDateTimeAxis*>(axes.first());
            if (axisX) {
                axisX->setRange(rangeStart, rangeEnd);
                qint64 spanSecs = rangeStart.secsTo(rangeEnd);
                axisX->setFormat(spanSecs > 86400 ? "MM-dd" : "HH:mm");
            }
        }
        axes = chart->axes(Qt::Vertical);
        if (!axes.isEmpty()) {
            auto* axisY = qobject_cast<QValueAxis*>(axes.first());
            if (axisY && maxVal > minVal) {
                double margin = (maxVal - minVal) * 0.15;
                if (margin < 0.01) margin = 1.0;
                axisY->setRange(qMax(0.0, minVal - margin), maxVal + margin);
            } else if (axisY) {
                axisY->setRange(0, 100);
            }
        }

        int r = gridIdx / cols;
        int c = gridIdx % cols;
        m_historyGridLayout->addWidget(card, r, c);
        card->show();
        gridIdx++;
    }

    // 等比例拉伸
    for (int c = 0; c < cols; ++c)
        m_historyGridLayout->setColumnStretch(c, 1);
    for (int r = 0; r < rows; ++r)
        m_historyGridLayout->setRowStretch(r, 1);

    // 更新统计信息
    DatabaseManager::DataAverages avg;
    QString avgErr;
    if (!m_db->queryAverageSince(rangeStart, &avg, &avgErr)) {
        qDebug() << "[DB] query averages failed:" << avgErr;
    }

    if (avg.sampleCount > 0) {
        const QString leftText =
            QString("温度均值：%1 ℃\n湿度均值：%2 %\n空气指数均值：%3")
                .arg(avg.temperature, 0, 'f', 1)
                .arg(avg.humidity, 0, 'f', 1)
                .arg(avg.airIndex, 0, 'f', 0);
        const QString rightText =
            QString("水流均值：%1 L/min\n电流均值：%2 A\nPM2.5 均值：%3 μg/m³")
                .arg(avg.flowLMin, 0, 'f', 2)
                .arg(avg.currentA / 1000.0, 0, 'f', 1)
                .arg(avg.pm25, 0, 'f', 1);

        if (m_historyStatsLeftLabel) m_historyStatsLeftLabel->setText(leftText);
        if (m_historyStatsRightLabel) m_historyStatsRightLabel->setText(rightText);

        if (m_historyStatsLabel) {
            m_historyStatsLabel->setText(
                QString("温度均值：%1 ℃\n湿度均值：%2 %\nPM2.5 均值：%3 μg/m³\n"
                        "空气指数均值：%4\n电流均值：%5 A\n水流均值：%6 L/min")
                    .arg(avg.temperature, 0, 'f', 1)
                    .arg(avg.humidity, 0, 'f', 1)
                    .arg(avg.pm25, 0, 'f', 1)
                    .arg(avg.airIndex, 0, 'f', 0)
                    .arg(avg.currentA / 1000.0, 0, 'f', 1)
                    .arg(avg.flowLMin, 0, 'f', 2));
        }

        if (m_historySummaryLabel) {
            const QString rangeStr = QString("%1 ~ %2")
                .arg(rangeStart.toString("yyyy-MM-dd HH:mm"), rangeEnd.toString("yyyy-MM-dd HH:mm"));
            m_historySummaryLabel->setText(
                QStringLiteral("%1 内共 %2 条记录。折线图展示各传感器数值随时间的变化趋势，"
                               "纵轴自动适配数据范围。可调整时间与数据类型后点击「查询」刷新。")
                    .arg(rangeStr)
                    .arg(avg.sampleCount));
        }
    } else {
        if (m_historyStatsLeftLabel) m_historyStatsLeftLabel->setText("暂无数据");
        if (m_historyStatsRightLabel) m_historyStatsRightLabel->setText(QString());
        if (m_historySummaryLabel) m_historySummaryLabel->setText("当前时间范围内没有可展示的数据。");
    }
}


void MainWindow::onExportHistoryClicked() {
    if (m_db == nullptr) {
        customMessage(this, "导出失败", "数据库未初始化。", true);
        return;
    }

    auto* startEdit = ui->pageEnvironment->findChild<QDateTimeEdit*>("historyStartTime");
    auto* endEdit   = ui->pageEnvironment->findChild<QDateTimeEdit*>("historyEndTime");
    const QDateTime rangeStart = startEdit ? startEdit->dateTime() : QDateTime::currentDateTime().addDays(-1);
    const QDateTime rangeEnd   = endEdit   ? endEdit->dateTime()   : QDateTime::currentDateTime();

    if (rangeStart >= rangeEnd) {
        customMessage(this, "导出失败", "起始时间必须早于结束时间，请重新选择。", true);
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

bool MainWindow::exportHistoryAsXlsx(const QString& filePath, QString* errorMessage) {
    auto setError = [&](const QString& text) {
        if (errorMessage != nullptr) {
            *errorMessage = text;
        }
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

    QStringList statLines;
    if (m_historyStatsLabel != nullptr) {
        statLines = m_historyStatsLabel->text().split('\n', Qt::SkipEmptyParts);
    }
    const QString summary = (m_historySummaryLabel != nullptr)
                                ? m_historySummaryLabel->text()
                                : "趋势整体平稳。";
    const QString exportTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    auto* es = ui->pageEnvironment->findChild<QDateTimeEdit*>("historyStartTime");
    auto* ee = ui->pageEnvironment->findChild<QDateTimeEdit*>("historyEndTime");
    const QDateTime esStart = es ? es->dateTime() : QDateTime::currentDateTime().addDays(-1);
    const QDateTime esEnd   = ee ? ee->dateTime()   : QDateTime::currentDateTime();
    const QString rangeText = QString("%1 ~ %2")
        .arg(esStart.toString("yyyyMMdd-HHmm"), esEnd.toString("yyyyMMdd-HHmm"));

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return setError("无法创建临时目录");
    }

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
        "<Override PartName=\"/xl/workbook.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" "
        "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
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
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"历史数据报表\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
        "</workbook>";

    const QString workbookRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";

    const QString appXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
        "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
        "<Application>System_UI</Application>"
        "</Properties>";

    const QString coreXml =
        QString("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
                "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
                "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
                "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
                "<dc:title>历史数据报表</dc:title>"
                "<dc:creator>System_UI</dc:creator>"
                "<cp:lastModifiedBy>System_UI</cp:lastModifiedBy>"
                "<dcterms:created xsi:type=\"dcterms:W3CDTF\">%1</dcterms:created>"
                "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">%1</dcterms:modified>"
                "</cp:coreProperties>")
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ"));

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
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF1F4E78\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFDCE6F2\"/><bgColor indexed=\"64\"/></patternFill></fill>"
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
        "<xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyAlignment=\"1\"><alignment horizontal=\"left\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\"><alignment horizontal=\"left\" vertical=\"top\" wrapText=\"1\"/></xf>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>";

    QString sheetXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";

    // 计算列宽（近似自动列宽）
    QVector<int> maxChars = {4, 8, 8, 8, 8};
    for (int i = 0; i < m_historyPoints.size(); ++i) {
        const auto& point = m_historyPoints[i];
        maxChars[0] = qMax(maxChars[0], QString::number(i + 1).size());
        maxChars[1] = qMax(maxChars[1], point.timeLabel.size());
        maxChars[2] = qMax(maxChars[2], QString::number(point.temp, 'f', 1).size() + 2);
        maxChars[3] = qMax(maxChars[3], QString::number(point.humi, 'f', 1).size() + 2);
        maxChars[4] = qMax(maxChars[4], QString::number(point.current, 'f', 1).size() + 2);
    }
    maxChars[1] = qMax(maxChars[1], summary.size());

    sheetXml += "<cols>";
    for (int i = 0; i < maxChars.size(); ++i) {
        const double width = qBound(10.0, maxChars[i] * 1.25 + 2.0, 52.0);
        sheetXml += QString("<col min=\"%1\" max=\"%1\" width=\"%2\" customWidth=\"1\"/>")
                        .arg(i + 1)
                        .arg(QString::number(width, 'f', 2));
    }
    sheetXml += "</cols>";

    auto inlineCell = [&](const QString& ref, const QString& text, int style) {
        return QString("<c r=\"%1\" t=\"inlineStr\" s=\"%2\"><is><t>%3</t></is></c>")
            .arg(ref)
            .arg(style)
            .arg(escapeXml(text));
    };
    auto numberCell = [&](const QString& ref, double value, int style) {
        return QString("<c r=\"%1\" s=\"%2\"><v>%3</v></c>")
            .arg(ref)
            .arg(style)
            .arg(QString::number(value, 'f', 1));
    };

    int row = 1;
    sheetXml += "<sheetData>";

    sheetXml += QString("<row r=\"%1\" ht=\"30\" customHeight=\"1\">").arg(row);
    sheetXml += inlineCell("A1", "灾后临时安置点智慧管理系统 历史数据报表", 1);
    sheetXml += "</row>";
    ++row;

    sheetXml += QString("<row r=\"%1\">").arg(row);
    sheetXml += inlineCell(QString("A%1").arg(row), "导出时间", 4);
    sheetXml += inlineCell(QString("B%1").arg(row), exportTime, 3);
    sheetXml += "</row>";
    ++row;

    sheetXml += QString("<row r=\"%1\">").arg(row);
    sheetXml += inlineCell(QString("A%1").arg(row), "时间范围", 4);
    sheetXml += inlineCell(QString("B%1").arg(row), rangeText, 3);
    sheetXml += "</row>";
    ++row;

    ++row;  // 绌鸿

    sheetXml += QString("<row r=\"%1\">").arg(row);
    sheetXml += inlineCell(QString("A%1").arg(row), "统计信息", 4);
    sheetXml += "</row>";
    ++row;

    for (const QString& line : std::as_const(statLines)) {
        const QStringList kv = line.split(QStringLiteral("："));
        const QString key = kv.value(0);
        const QString value = (kv.size() > 1) ? kv.mid(1).join("：") : "";
        sheetXml += QString("<row r=\"%1\">").arg(row);
        sheetXml += inlineCell(QString("A%1").arg(row), key, 3);
        sheetXml += inlineCell(QString("B%1").arg(row), value, 3);
        sheetXml += "</row>";
        ++row;
    }

    ++row;  // 绌鸿

    sheetXml += QString("<row r=\"%1\">").arg(row);
    sheetXml += inlineCell(QString("A%1").arg(row), "趋势摘要", 4);
    sheetXml += "</row>";
    ++row;

    sheetXml += QString("<row r=\"%1\" ht=\"72\" customHeight=\"1\">").arg(row);
    sheetXml += inlineCell(QString("A%1").arg(row), summary, 5);
    sheetXml += "</row>";
    ++row;

    ++row;  // 绌鸿

    const QStringList headers = {"序号", "时间点", "温度(℃)", "湿度(%)", "电流(A)"};
    sheetXml += QString("<row r=\"%1\" ht=\"22\" customHeight=\"1\">").arg(row);
    for (int i = 0; i < headers.size(); ++i) {
        const QString ref = QString("%1%2").arg(colName(i + 1), QString::number(row));
        sheetXml += inlineCell(ref, headers[i], 2);
    }
    sheetXml += "</row>";
    ++row;

    for (int i = 0; i < m_historyPoints.size(); ++i) {
        const auto& point = m_historyPoints[i];
        sheetXml += QString("<row r=\"%1\">").arg(row);
        sheetXml += QString("<c r=\"A%1\" s=\"3\"><v>%2</v></c>").arg(QString::number(row), QString::number(i + 1));
        sheetXml += inlineCell(QString("B%1").arg(row), point.timeLabel, 3);
        sheetXml += numberCell(QString("C%1").arg(row), point.temp, 3);
        sheetXml += numberCell(QString("D%1").arg(row), point.humi, 3);
        sheetXml += numberCell(QString("E%1").arg(row), point.current, 3);
        sheetXml += "</row>";
        ++row;
    }

    sheetXml += "</sheetData>";

    // 合并单元格：主标题、统计标题、趋势标题、趋势内容
    const int statsTitleRow = 5;
    const int summaryTitleRow = statsTitleRow + statLines.size() + 2;
    const int summaryValueRow = summaryTitleRow + 1;
    sheetXml += "<mergeCells count=\"4\">";
    sheetXml += "<mergeCell ref=\"A1:E1\"/>";
    sheetXml += QString("<mergeCell ref=\"A%1:E%1\"/>").arg(statsTitleRow);
    sheetXml += QString("<mergeCell ref=\"A%1:E%1\"/>").arg(summaryTitleRow);
    sheetXml += QString("<mergeCell ref=\"A%1:E%1\"/>").arg(summaryValueRow);
    sheetXml += "</mergeCells>";

    sheetXml += "</worksheet>";

    if (!writeUtf8File(root.filePath("[Content_Types].xml"), contentTypes) ||
        !writeUtf8File(root.filePath("_rels/.rels"), rels) ||
        !writeUtf8File(root.filePath("docProps/app.xml"), appXml) ||
        !writeUtf8File(root.filePath("docProps/core.xml"), coreXml) ||
        !writeUtf8File(root.filePath("xl/workbook.xml"), workbook) ||
        !writeUtf8File(root.filePath("xl/_rels/workbook.xml.rels"), workbookRels) ||
        !writeUtf8File(root.filePath("xl/styles.xml"), stylesXml) ||
        !writeUtf8File(root.filePath("xl/worksheets/sheet1.xml"), sheetXml)) {
        return setError("临时文件写入失败");
    }

    QFile::remove(filePath);
    const QString zipPath = QFileInfo(filePath).absolutePath() + "/.__tmp_history_export__.zip";
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
        return setError("打包超时");
    }
    if (zipProcess.exitStatus() != QProcess::NormalExit || zipProcess.exitCode() != 0) {
        return setError(QString::fromLocal8Bit(zipProcess.readAllStandardError()));
    }
    if (!QFile::exists(zipPath)) {
        return setError("未生成 ZIP 临时文件");
    }
    if (!QFile::rename(zipPath, filePath)) {
        QFile::remove(filePath);
        if (!QFile::rename(zipPath, filePath)) {
            return setError("ZIP 重命名为 XLSX 失败");
        }
    }
    if (!QFile::exists(filePath)) {
        return setError("未生成目标 XLSX 文件");
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

void MainWindow::refreshDeviceTable() {
    // 更新设备状态一览表
    auto* deviceTbl = ui->pageWaterPower->findChild<QTableWidget*>("deviceStatusTable");
    if (deviceTbl) {
        deviceTbl->setRowCount(0);
        for (int i = 0; i < m_devices.size(); ++i) {
            const auto& d = m_devices[i];
            deviceTbl->insertRow(i);
            deviceTbl->setItem(i, 0, new QTableWidgetItem(d.id));
            deviceTbl->setItem(i, 1, new QTableWidgetItem(d.name));
            deviceTbl->setItem(i, 2, new QTableWidgetItem(d.type));
            auto* statusItem = new QTableWidgetItem(d.status);
            if (d.status == "告警") statusItem->setForeground(QColor(239, 68, 68));
            else if (d.status == "预警") statusItem->setForeground(QColor(245, 158, 11));
            else if (d.status == "离线") statusItem->setForeground(QColor(148, 163, 184));
            else statusItem->setForeground(QColor(34, 197, 94));
            deviceTbl->setItem(i, 3, statusItem);
            deviceTbl->setItem(i, 4, new QTableWidgetItem(
                QString("%1 %2").arg(QString::number(d.latestValue, 'f', 1), d.latestValueUnit)));
            deviceTbl->setItem(i, 5, new QTableWidgetItem(QString("%1%").arg(d.battery)));
        }
    }

    // 更新远程设备下拉
    const QString selectedDeviceId = (m_remoteDeviceCombo != nullptr)
                                         ? m_remoteDeviceCombo->currentData().toString()
                                         : QString();

    if (m_remoteDeviceCombo != nullptr) {
        m_remoteDeviceCombo->blockSignals(true);
        m_remoteDeviceCombo->clear();
        for (const auto& device : std::as_const(m_devices)) {
            m_remoteDeviceCombo->addItem(
                QString("%1 - %2").arg(device.name, device.id),
                device.id);
        }
        int targetIndex = -1;
        if (!selectedDeviceId.isEmpty()) {
            targetIndex = m_remoteDeviceCombo->findData(selectedDeviceId);
        }
        if (targetIndex < 0 && m_remoteDeviceCombo->count() > 0) {
            targetIndex = 0;
        }
        if (targetIndex >= 0) {
            m_remoteDeviceCombo->setCurrentIndex(targetIndex);
        }
        m_remoteDeviceCombo->blockSignals(false);
    }
}

void MainWindow::onUpdateWaterPowerData() {
    if (m_devices.isEmpty()) {
        return;
    }

    const int row = QRandomGenerator::global()->bounded(m_devices.size());
    m_devices[row].battery = qMax(18, m_devices[row].battery - QRandomGenerator::global()->bounded(2));
    if (QRandomGenerator::global()->bounded(10) > 7) {
        m_devices[row].status = (m_devices[row].status == "运行中") ? "维护中" : "运行中";
    }
    const double delta = QRandomGenerator::global()->bounded(-60, 61) / 10.0;
    m_devices[row].latestValue = qMax(0.0, m_devices[row].latestValue + delta);
    const bool thresholdExceeded =
        (m_devices[row].warningUpper > m_devices[row].warningLower && m_devices[row].latestValue > m_devices[row].warningUpper)
        || (m_devices[row].warningLower > 0.0 && m_devices[row].latestValue < m_devices[row].warningLower);
    if (thresholdExceeded) {
        m_devices[row].status = "波动";
    } else if (m_devices[row].status == "波动") {
        m_devices[row].status = "运行中";
    }
    refreshDeviceTable();
}

void MainWindow::onAddDeviceClicked() {
    const int nextId = m_devices.size() + 1;
    const QString defaultId = QString("NEW-%1").arg(nextId, 3, 10, QChar('0'));

    bool ok = false;
    const QString deviceId = QInputDialog::getText(
        this, "添加新设备", "设备 ID（必填）", QLineEdit::Normal, defaultId, &ok).trimmed();
    if (!ok) {
        return;
    }
    if (deviceId.isEmpty()) {
        customMessage(this, "添加失败", "设备 ID 不能为空。", true);
        return;
    }
    for (const auto& item : std::as_const(m_devices)) {
        if (item.id.compare(deviceId, Qt::CaseInsensitive) == 0) {
            customMessage(this, "添加失败", "设备 ID 已存在，请使用其他 ID。", true);
            return;
        }
    }

    const QString deviceName = QInputDialog::getText(
        this, "添加新设备", "设备名称（必填）", QLineEdit::Normal,
        QString("新设备%1").arg(nextId), &ok).trimmed();
    if (!ok) {
        return;
    }
    if (deviceName.isEmpty()) {
        customMessage(this, "添加失败", "设备名称不能为空。", true);
        return;
    }

    const QStringList typeOptions = {
        QStringLiteral("温度传感器"),
        QStringLiteral("湿度传感器"),
        QStringLiteral("水流传感器"),
        QStringLiteral("电流传感器"),
        QStringLiteral("PM2.5 传感器"),
        QStringLiteral("新传感器"),
    };
    const QString deviceType = QInputDialog::getItem(
        this, "添加新设备", "设备类型（必选）", typeOptions, 5, false, &ok).trimmed();
    if (!ok || deviceType.isEmpty()) {
        return;
    }

    const QString installLocation = QInputDialog::getText(
        this, "添加新设备", "安装位置（必填）", QLineEdit::Normal, QStringLiteral("未分配"), &ok).trimmed();
    if (!ok) {
        return;
    }
    if (installLocation.isEmpty()) {
        customMessage(this, "添加失败", "安装位置不能为空。", true);
        return;
    }

    const int sampleInterval = QInputDialog::getInt(
        this, "添加新设备", "采样间隔（秒）", 5, 1, 300, 1, &ok);
    if (!ok) {
        return;
    }

    m_devices.append({deviceId,
                      deviceName,
                      deviceType,
                      installLocation,
                      "运行中",
                      100,
                      "v1.0.0",
                      true,
                      sampleInterval,
                      0.0,
                      deviceType.contains(QStringLiteral("湿度")) ? "%" :
                          (deviceType.contains(QStringLiteral("电流")) ? QStringLiteral("A") :
                              (deviceType.contains(QStringLiteral("PM2.5")) ? QStringLiteral("ug/m3") :
                                  (deviceType.contains(QStringLiteral("水流")) ? QStringLiteral("L/min")
                                                                             : QStringLiteral("℃")))),
                      80.0,
                      0.0,
                      true,
                      false,
                      false});
    syncDeviceInfoToDatabase();
    buildDevicePage();
    customMessage(this, "添加成功", QString("设备 %1（%2）已添加。").arg(deviceName, deviceId));
}

void MainWindow::onRemoveDeviceClicked() {
    if (m_devices.isEmpty()) {
        customMessage(this, "提示", "当前没有可移除的设备。");
        return;
    }

    QStringList deviceDisplayList;
    deviceDisplayList.reserve(m_devices.size());
    for (const auto& device : std::as_const(m_devices)) {
        deviceDisplayList.append(QString("%1 | %2 | %3")
                                     .arg(device.id, device.name, device.status));
    }

    bool ok = false;
    const QString selectedDisplay = QInputDialog::getItem(
        this,
        "移除设备",
        "请选择要移除的设备：",
        deviceDisplayList,
        0,
        false,
        &ok);
    if (!ok || selectedDisplay.isEmpty()) {
        return;
    }

    const QString selectedId = selectedDisplay.section(" | ", 0, 0);
    int removeIndex = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].id == selectedId) {
            removeIndex = i;
            break;
        }
    }
    if (removeIndex < 0) {
        customMessage(this, "移除失败", "未找到所选设备。", true);
        return;
    }

    const DeviceInfo device = m_devices[removeIndex];
    if (!customConfirm(this, "确认移除",
            QString("确认移除以下设备：\n\n设备 ID：%1\n设备名称：%2\n当前状态：%3")
                .arg(device.id, device.name, device.status))) {
        return;
    }

    m_devices.removeAt(removeIndex);
    syncDeviceInfoToDatabase();
    buildDevicePage();
    customMessage(this, "移除成功", QString("设备 %1（%2）已移除。").arg(device.name, device.id));
}

void MainWindow::onDeviceDetailClicked() {
    if (m_deviceTable == nullptr) {
        return;
    }
    const int row = m_deviceTable->currentRow();
    if (row < 0) {
        return;
    }
    QTableWidgetItem* item = m_deviceTable->item(row, 0);
    if (item == nullptr) {
        return;
    }
    const QString deviceId = item->text().section('\n', 1, 1).section(" | ", 0, 0).trimmed();
    int deviceIndex = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].id == deviceId) {
            deviceIndex = i;
            break;
        }
    }
    if (deviceIndex < 0) {
        return;
    }
    const DeviceInfo& device = m_devices[deviceIndex];
    customMessage(this, "设备详情",
        QString("设备 ID：%1\n设备名称：%2\n类型：%3\n位置：%4\n运行状态：%5\n实时读数：%6 %7\n阈值范围：%8 ~ %9 %7\n电池：%10%%\n固件：%11")
            .arg(device.id,
                 device.name,
                 device.type,
                 device.location,
                 device.status,
                 QString::number(device.latestValue, 'f', 1),
                 device.latestValueUnit,
                 QString::number(device.warningLower, 'f', 0),
                 QString::number(device.warningUpper, 'f', 0),
                 QString::number(device.battery),
                 device.firmware));
}

void MainWindow::onUpdateFirmwareClicked() {
    if (m_deviceTable == nullptr) {
        return;
    }
    const int row = m_deviceTable->currentRow();
    if (row < 0) {
        return;
    }
    QTableWidgetItem* item = m_deviceTable->item(row, 0);
    if (item == nullptr) {
        return;
    }
    const QString deviceId = item->text().section('\n', 1, 1).section(" | ", 0, 0).trimmed();
    int deviceIndex = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].id == deviceId) {
            deviceIndex = i;
            break;
        }
    }
    if (deviceIndex < 0) {
        return;
    }
    m_devices[deviceIndex].firmware = "v1.4.0";
    m_devices[deviceIndex].status = "运行中";
    refreshDeviceTable();
}

void MainWindow::triggerDeviceQuickAction(int deviceIndex, const QString& actionText) {
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return;
    }
    DeviceInfo& device = m_devices[deviceIndex];
    if (actionText.contains("采样")) {
        device.latestValue += QRandomGenerator::global()->bounded(0, 25) / 10.0;
        device.status = "运行中";
    } else if (actionText.contains("重启")) {
        device.status = "维护中";
        device.firmware = "v1.4.0";
    }
    appendRemoteControlLog(device.id, actionText, "执行成功");
    refreshDeviceTable();
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

void MainWindow::syncDeviceInfoToDatabase() {
    if (m_db == nullptr) {
        return;
    }
    for (const auto& device : std::as_const(m_devices)) {
        QString dbErr;
        if (!m_db->upsertDeviceInfo(device.id, device.name, device.type, device.location, &dbErr)) {
            qDebug() << "[DB] upsert device info failed:" << device.id << dbErr;
        }
    }
}

int MainWindow::indexOfDeviceById(const QString& deviceId) const {
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].id == deviceId) {
            return i;
        }
    }
    return -1;
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

void MainWindow::onSendRemoteControlClicked() {
    if (m_remoteCommandCombo == nullptr) {
        return;
    }

    const QString commandText = m_remoteCommandCombo->currentText();
    int code = 1;
    if (commandText.contains("关闭") || commandText.contains("停止")) {
        code = 0;
    }

    QJsonObject cmd;
    cmd["code"] = code;
    cmd["name"] = commandText;

    const QString topic = QStringLiteral("test001up");
    m_mqtt.publishText(topic, QString::fromUtf8(QJsonDocument(cmd).toJson(QJsonDocument::Compact)));

    appendRemoteControlLog("BEMFA", commandText, QString("已发送到 %1").arg(topic));
    customMessage(this, "提示", QString("已发送控制指令到主题：%1\n命令：%2").arg(topic, commandText));
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
