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
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
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
class QDateTimeEdit;
class QPushButton;
class QProgressBar;
class QTableWidget;
class QPlainTextEdit;
class QSpinBox;
class QFile;
class QLineEdit;
class QCheckBox;
class QDateEdit;

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


    // 导出历史数据




    // 历史数据导出按钮
    void onExportHistoryClicked();

    void onSwitchAccountClicked();
    void onEditAccountInfoClicked();
    void onAddAccountClicked();

private:
    // 初始化界面（标题、角色、默认页面等）
    void initUi();

    // 初始化信号槽连接
    void initConnections();
    void initMqtt();

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
                        const QString& contentText,
                        const QString& level = "预警",
                        int alarmCode = 0);

    // 从 SQLite（D:/System_UI_Data）加载报警信息到界面
    void refreshAlarmInfoFromDatabase();

    // 报警表格"已处理"按钮点击
    void onAlarmHandledClicked(int alarmId);

    // 生成历史数据展示
    void refreshHistoryPage();
    void ensureChartBuilt(int metricIdx);

    void appendRemoteControlLog(const QString& command,
                                const QString& result);
    void loadRemoteExecLogTable();
    void toggleMaximizedState();
    void updateTitleBarButtons();

    // 为卡片状态点设置颜色
    void setCardStateDot(QLabel* dot, const QColor& color);
    void updateRealtimeCardOfflineState(bool offline);
    void exportDashboardHistoryData();
    QString resolveDashboardHistoryDataPath() const;
    void refreshWaterPowerUsageSummary();
    void refreshWaterPowerAnalysisPage();
    void exportWaterPowerAnalysis(bool includeSingleDay, bool includeTrend);

    // 导出真实 XLSX 文件
    bool exportHistoryAsXlsx(const QString& filePath, QString* errorMessage);
    bool exportWaterPowerAnalysisAsXlsx(const QString& filePath,
                                        bool includeSingleDay,
                                        bool includeTrend,
                                        QString* errorMessage);
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

    // 实时监控页折线图：电流 + 水流（双Y轴）
    QLineSeries* m_currentSeries;
    QLineSeries* m_flowSeries;
    QDateTimeAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_Current;   // 左侧：电流(A)
    QValueAxis* m_axisY_Flow;      // 右侧：水流(L/min)
    QList<QLineSeries*> m_historyLineSeries;
    QList<QLineSeries*> m_historyLowerSeries;
    QList<QChartView*> m_historyChartViews;
    QList<QChart*> m_historyCharts;
    QList<QDateTimeAxis*> m_historyAxisXs;
    QList<QValueAxis*> m_historyAxisYs;
    QList<QWidget*> m_historyChartCards;
    QLabel* m_historyPowerStatsLabel = nullptr;
    QLabel* m_historyWaterStatsLabel = nullptr;
    QDateTimeEdit* m_historyStartCombo = nullptr;
    QDateTimeEdit* m_historyEndCombo = nullptr;
    QList<QCheckBox*> m_historyMetricChecks;
    QList<QLabel*> m_historyStatLabels;
    QHBoxLayout* m_historyStatsLayout = nullptr;
    QTimer* m_historyRefreshTimer = nullptr;
    QDateEdit* m_wpAnalysisDateEdit = nullptr;
    QComboBox* m_wpTrendDaysCombo = nullptr;
    QBarSet* m_wpUsagePowerSet = nullptr;
    QBarSet* m_wpUsageWaterSet = nullptr;
    QBarSeries* m_wpUsagePowerSeries = nullptr;
    QBarSeries* m_wpUsageWaterSeries = nullptr;
    QChart* m_wpUsagePowerChart = nullptr;
    QChart* m_wpUsageWaterChart = nullptr;
    QChartView* m_wpUsagePowerChartView = nullptr;
    QChartView* m_wpUsageWaterChartView = nullptr;
    QBarCategoryAxis* m_wpUsageAxisX_Power = nullptr;
    QBarCategoryAxis* m_wpUsageAxisX_Water = nullptr;
    QValueAxis* m_wpUsageAxisY_Power = nullptr;
    QValueAxis* m_wpUsageAxisY_Water = nullptr;
    QLabel* m_wpUsageStatsLabel = nullptr;
    QBarSet* m_wpTrendPowerSet = nullptr;
    QBarSet* m_wpTrendWaterSet = nullptr;
    QBarSeries* m_wpTrendPowerSeries = nullptr;
    QBarSeries* m_wpTrendWaterSeries = nullptr;
    QChart* m_wpTrendPowerChart = nullptr;
    QChart* m_wpTrendWaterChart = nullptr;
    QChartView* m_wpTrendPowerChartView = nullptr;
    QChartView* m_wpTrendWaterChartView = nullptr;
    QBarCategoryAxis* m_wpTrendAxisX_Power = nullptr;
    QBarCategoryAxis* m_wpTrendAxisX_Water = nullptr;
    QValueAxis* m_wpTrendAxisY_Power = nullptr;
    QValueAxis* m_wpTrendAxisY_Water = nullptr;
    QLabel* m_wpTrendStatsLabel = nullptr;
    // 水电管理页：自定义绘制组件
    QWidget* m_batteryGauge = nullptr;
    QWidget* m_tankGauge = nullptr;
    QLabel* m_batteryInfoLabel = nullptr;
    QLabel* m_tankInfoLabel = nullptr;
    // 活跃报警追踪（检测报警消失以记录结束时间）
    QSet<int> m_prevAlmCodes;
    QSet<int> m_builtPages;
    // 实时负载大字
    QLabel* m_wpLoadValueLabel = nullptr;
    QLabel* m_wpLoadStatusLabel = nullptr;
    QLabel* m_wpTodayPowerLabel = nullptr;
    QLabel* m_wpTodayWaterLabel = nullptr;
    QLabel* m_wpRecentRangeLabel = nullptr;
    QLabel* m_wpRecentPowerLabel = nullptr;
    QLabel* m_wpRecentWaterLabel = nullptr;
    QPushButton* m_historyConfirmBtn = nullptr;
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
    QLabel* m_realtimeStatusLabel;
    QLabel* m_windowIconLabel;
    QPushButton* m_minimizeButton;
    QPushButton* m_maximizeButton;
    QPushButton* m_closeButton;
    bool m_dragging = false;
    QPoint m_dragOffset;
    int m_lastUsedPowerMAh = 0;
    int m_lastUsedWaterCL = 0;

    QTableWidget* m_alarmInfoTable;
    QComboBox* m_remoteDeviceCombo;
    QComboBox* m_remoteCommandCombo;
    QPlainTextEdit* m_logViewer;
    QTableWidget* m_remoteLogTable = nullptr;

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

    QList<HistoryPoint> m_historyPoints;

    DatabaseManager* m_db = nullptr;
    Mqtt m_mqtt;
    bool m_useMqttRealtime = false;
    bool m_isDeviceOffline = false;
    qint64 m_lastOfflineDurationSec = -1;
    QDateTime m_lastLocalAlarmAt;
    QDateTime m_lastRealtimeDataAt;
    QDateTime m_lastDashboardHistoryExportAt;
    QDateTime m_offlineStartAt;
    QDateTime m_loginTime;
    double m_batteryPct = 100.0;
    double m_waterPct = 100.0;

    // 阈值持久化：key(ta/tb/ha/...) → value
    QMap<QString, int> m_thresholdValues;
    void saveThresholdsToSettings();
    void loadThresholdsFromSettings();
};

#endif  // MAINWINDOW_H
