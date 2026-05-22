#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>
#include <QMainWindow>
#include <QList>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <QDateTime>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QMenu>
#include <QAction>
#include <QGridLayout>

#include "mqtt.h"
#include "sensordata.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTimer;
class QLabel;
class QFrame;
class QColor;
class QComboBox;
class QPushButton;
class QProgressBar;
class QTableWidget;
class QPlainTextEdit;
class QSpinBox;
class QFile;
class QLineEdit;
class QCheckBox;

class DatabaseManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // 构造函数：
    // userName - 当前登录用户名
    // userRole - 当前角色（admin 或 user）
    explicit MainWindow(const QString& userName,
                        const QString& userRole,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    // 首页数据接口：供外部或模拟数据更新 UI
    void updateData(double temp,
                    double hum,
                    double current,
                    double flow,
                    double airIndex,
                    double pm25);

    // 新接口：接收 STM32 判定的传感器级别，不做本地计算
    void updateDataWithLevels(double temp, double hum, double current,
                              double flow, double airIndex, double pm25,
                              int linkLv, const QVector<int>& sensorLvs,
                              const QJsonArray& almArr, const QJsonArray& actnArr);

    // 更新水电剩余百分比（来自 MQTT bp/wp 字段）
    void updateResourcePct(double batteryPct, double waterPct,
                           double currentMA, double flowLMin,
                           int usedPowerMAh = 0, int usedWaterCL = 0,
                           int batRemainMAh = 0, int wtrRemainCL = 0, int powerStatus = 0,
                           int batCapMAh = 10000, int tankCapCL = 1000,
                           int batRemainMin = 0, int wtrRemainMin = 0);

signals:
    void switchAccountRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    // 左侧导航切换时，切换右侧页面
    void onNavCurrentRowChanged(int row);

    // 周期刷新首页假数据
    void onUpdateDashboardData();

    // 周期刷新环境监测页数值和折线图
    void onUpdateEnvironmentData();

    // 周期刷新水电管理页面数据（当前值、累计值、表格）
    void onUpdateWaterPowerData();

    // 导出历史数据

    // 设备管理按钮点击
    void onAddDeviceClicked();
    void onRemoveDeviceClicked();
    void onDeviceDetailClicked();
    void onUpdateFirmwareClicked();
    void onSendRemoteControlClicked();

    // 历史数据导出按钮
    void onExportHistoryClicked();

    void onSwitchAccountClicked();
    void onEditAccountInfoClicked();

private:
    // 初始化界面（标题、角色、默认页面等）
    void initUi();

    // 初始化信号槽连接
    void initConnections();
    void initMqtt();

    // 初始化环境监测页面的折线图
    void initEnvironmentChart();

    // 重建 6 个主页面
    void buildMainPages();
    void buildRealtimePage();
    void buildWaterPowerPage();
    void buildHistoryPage();
    void buildAlarmPage();
    void buildDevicePage();
    void buildSettingsPage();

    // 顶部时间更新
    void updateTopBarTime();

    // 往报警信息表新增一条记录（通知方式为弹窗，由实时告警对话框体现）
    void addAlarmRecord(const QString& timeText,
                        const QString& sensorText,
                        const QString& contentText,
                        const QString& level = "预警",
                        int alarmCode = 0);

    // 从 SQLite（D:/System_UI_Data）加载报警信息到界面
    void refreshAlarmInfoFromDatabase();

    // 报警表格"已处理"按钮点击
    void onAlarmHandledClicked(int alarmId);

    // 生成历史数据展示
    void refreshHistoryPage();

    // 刷新设备列表展示
    void refreshDeviceTable();
    void appendRemoteControlLog(const QString& deviceId,
                                const QString& command,
                                const QString& result);
    void syncDeviceInfoToDatabase();
    void loadRemoteExecLogTable();
    int indexOfDeviceById(const QString& deviceId) const;
    void triggerDeviceQuickAction(int deviceIndex, const QString& actionText);

    void toggleMaximizedState();
    void updateTitleBarButtons();

    // 为卡片状态点设置颜色
    void setCardStateDot(QLabel* dot, const QColor& color);
    void updateRealtimeCardOfflineState(bool offline);
    void syncDeviceOfflineRecords(const QDateTime& now);

    // 导出真实 XLSX 文件
    bool exportHistoryAsXlsx(const QString& filePath, QString* errorMessage);
    bool loadCurrentUserBasicInfo(QString* password, QString* errMsg);
    bool updateCurrentUserBasicInfo(const QString& newUsername,
                                    const QString& newPassword,
                                    QString* errMsg);

private:
    Ui::MainWindow* ui;

    QString m_userName;
    QString m_userRole;

    // 用于首页和环境监测页模拟实时数据
    QTimer* m_dataTimer;

    // 下面这几个对象用于环境监测页面折线图
    QLineSeries* m_tempSeries;
    QLineSeries* m_humiSeries;
    QLineSeries* m_pmSeries;
    QDateTimeAxis* m_axisX = nullptr;
    QValueAxis* m_axisY;
    QLineSeries* m_dashboardTempSeries;
    QLineSeries* m_dashboardHumiSeries;
    QLineSeries* m_dashboardPmSeries;
    QList<QLineSeries*> m_historyLineSeries;
    QList<QLineSeries*> m_historyLowerSeries;
    QList<QChartView*> m_historyChartViews;
    // 水电管理页：自定义绘制组件
    QWidget* m_batteryGauge = nullptr;
    QWidget* m_tankGauge = nullptr;
    QLabel* m_batteryInfoLabel = nullptr;
    QLabel* m_tankInfoLabel = nullptr;
    // 活跃报警追踪（检测报警消失以记录结束时间）
    QSet<int> m_prevAlmCodes;
    // 实时负载大字
    QLabel* m_wpLoadValueLabel = nullptr;
    QLabel* m_wpLoadStatusLabel = nullptr;
    QLabel* m_wpTodayPowerLabel = nullptr;
    QLabel* m_wpTodayWaterLabel = nullptr;
    // 水电管理页图表：电流/水流折线图
    QLineSeries* m_wpCurrentSeries = nullptr;
    QLineSeries* m_wpFlowSeries = nullptr;
    QChart* m_wpChart = nullptr;
    QChartView* m_wpChartView = nullptr;
    QDateTimeAxis* m_wpAxisX = nullptr;
    QValueAxis* m_wpAxisY_Cur = nullptr;
    QValueAxis* m_wpAxisY_Flow = nullptr;
    QWidget* m_historyChartContainer = nullptr;
    QGridLayout* m_historyGridLayout = nullptr;
    QList<QCheckBox*> m_historySensorCheckBoxes;
    QPushButton* m_historyConfirmBtn = nullptr;
    QValueAxis* m_dashboardAxisX1;
    QValueAxis* m_dashboardAxisY1;
    QValueAxis* m_dashboardAxisX2;
    QValueAxis* m_dashboardAxisY2;
    QLabel* m_timeLabel;
    QLabel* m_cardTempValue;
    QLabel* m_cardHumiValue;
    QLabel* m_cardCurrentValue;
    QLabel* m_cardFlowValue;
    QLabel* m_cardAirValue;
    QLabel* m_cardPmValue;
    QLabel* m_dotTemp;
    QLabel* m_dotHumi;
    QLabel* m_dotCurrent;
    QLabel* m_dotFlow;
    QLabel* m_dotAir;
    QLabel* m_dotPm;
    QLabel* m_stateTempLabel = nullptr;
    QLabel* m_stateHumiLabel = nullptr;
    QLabel* m_stateCurrentLabel = nullptr;
    QLabel* m_stateFlowLabel = nullptr;
    QLabel* m_stateAirLabel = nullptr;
    QLabel* m_statePmLabel = nullptr;
    QLabel* m_totalCurrentLabel;
    QLabel* m_totalPowerLabel;
    QLabel* m_totalWaterLabel;
    QLabel* m_realtimeStatusLabel;
    QLabel* m_realtimeTimestampLabel;
    QProgressBar* m_batteryPctBar = nullptr;
    QLabel* m_batteryPctLabel = nullptr;
    QLabel* m_batteryRemainLabel = nullptr;
    QProgressBar* m_waterPctBar = nullptr;
    QLabel* m_waterPctLabel = nullptr;
    QLabel* m_waterRemainLabel = nullptr;
    QLabel* m_windowIconLabel;
    QPushButton* m_minimizeButton;
    QPushButton* m_maximizeButton;
    QPushButton* m_closeButton;
    bool m_dragging = false;
    QPoint m_dragOffset;
    int m_dashboardStep;

    // X 轴时间步，显示最近 20 个点
    int m_chartStep;

    // 实时页面累计数据（用于教学演示）
    double m_totalCurrent;
    double m_totalWater;
    double m_totalPower;

    QLabel* m_historyStatsLabel;
    QLabel* m_historyStatsLeftLabel;
    QLabel* m_historyStatsRightLabel;
    QLabel* m_historySummaryLabel;
    QLabel* m_historyDataSourceLabel;
    QTableWidget* m_realtimeSensorTable;
    QTableWidget* m_alarmInfoTable;
    QTableWidget* m_deviceTable;
    QLineEdit* m_deviceSearchEdit;
    QComboBox* m_deviceStatusFilterCombo;
    QComboBox* m_deviceTypeFilterCombo;
    QComboBox* m_remoteDeviceCombo;
    QComboBox* m_remoteCommandCombo;
    QSpinBox* m_remoteIntervalSpin;
    QPlainTextEdit* m_logViewer;
    QTableWidget* m_remoteControlLogTable;
    QPlainTextEdit* m_remoteLogMarqueeView = nullptr;

    struct DeviceInfo {
        QString id;
        QString name;
        QString type;
        QString location;
        QString status;
        int battery;
        QString firmware;
        bool remoteControlEnabled;
        int sampleIntervalSec;
        double latestValue;
        QString latestValueUnit;
        double warningUpper;
        double warningLower;
        bool notifyBuzzer;
        bool notifyEmail;
        bool notifySms;
    };

    struct HistoryPoint {
        QString timeLabel;
        double temp;
        double humi;
        double pm25;
        double airIndex;
        double current;
        double flow;
        QDateTime timestamp;
    };

    QList<DeviceInfo> m_devices;
    QList<HistoryPoint> m_historyPoints;

    DatabaseManager* m_db = nullptr;
    Mqtt m_mqtt;
    bool m_useMqttRealtime = false;
    bool m_isDeviceOffline = false;
    qint64 m_lastOfflineDurationSec = -1;
    QDateTime m_lastLocalAlarmAt;
    QDateTime m_lastRealtimeDataAt;
    /** 用于累计能耗/电量/流量的上一次采样时刻（与 MQTT / 定时刷新共用 updateData） */
    QDateTime m_lastCumulativeSampleAt;
    QDateTime m_offlineStartAt;
    QDateTime m_loginTime;
    double m_batteryPct = 100.0;
    double m_waterPct = 100.0;
};

#endif  // MAINWINDOW_H
