#ifndef MAINWINDOW_UTILS_H
#define MAINWINDOW_UTILS_H

#include <QDialog>
#include <QFrame>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include <QtMath>

// MQTT topic constants
constexpr int kDashboardHistoryExportIntervalSec = 10;
extern const char kMqttTelemetryTopic[];
extern const char kMqttCommandTopic[];
extern const char kMqttCommandPublishTopic[];
extern const char kMqttHelpTopic[];

// JSON builders
QString buildWrappedMqttJson(const QString& type, const QString& source, const QJsonObject& payload);
QJsonObject unwrapWrappedPayload(const QJsonObject& obj, const QString& expectedType);
QString buildThresholdMqttJson(const QJsonObject& values);
QString buildSingleThresholdMqttJson(const QString& key, int value);
QString buildResetThresholdMqttJson();

// Sensor level display
void applySensorLevelStyle(QLabel* valueLabel, QLabel* dotLabel, QLabel* stateLabel, int lv);
QString lvStatusZh(int lv);

// Custom dialogs
bool customConfirm(QWidget* parent, const QString& title, const QString& text);
void customMessage(QWidget* parent, const QString& title, const QString& text, bool isWarning = false);
void showExportSuccessDialog(QWidget* parent, const QString& nativePath);
void showRealtimeAlarmDialog(QWidget* parent, const QString& detailText);
void showRealtimeWarnDialog(QWidget* parent, const QString& detailText);
bool isAlertDialogActive();

// String helpers
QString stripRemoteLogCodeSuffix(QString text);
QString formatRemoteLogTableLine(const QString& executeTime,
                                 const QString& commandText, const QString& resultText);

// UI helpers
void clearLayout(QLayout* layout);
void applyShadow(QWidget* widget);
QFrame* createPanelCard(QWidget* parent);
QIcon createEmergencyWindowIcon();
QPixmap createAlarmIconPixmap(const QSize& size);

// Custom gauge widgets
class BatteryGauge : public QWidget {
public:
    explicit BatteryGauge(QWidget* parent = nullptr);
    void setPct(double p);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    double m_pct = 100;
};

class TankGauge : public QWidget {
public:
    explicit TankGauge(QWidget* parent = nullptr);
    void setPct(double p);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    double m_pct = 100;
};

#endif // MAINWINDOW_UTILS_H
