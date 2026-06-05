#pragma once

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "sensordata.h"

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    struct DataAverages {
        int sampleCount = 0;
        double temperature = 0.0;
        double humidity = 0.0;
        double pm25 = 0.0;
        double airIndex = 0.0;
        double currentA = 0.0;
        double flowLMin = 0.0;
    };
    struct RemoteExecLogEntry {
        QString executeTime;
        QString commandText;
        QString resultText;
    };

    struct DailyResourceUsageEntry {
        QString day;
        int powerMAh = 0;
        int waterCL = 0;
        int lastPowerCounterMAh = 0;
        int lastWaterCounterCL = 0;
    };

    struct AlarmInfoEntry {
        int id = 0;
        QString alarmTime;
        QString endTime;
        QString alarmContent;
        QString level;     // 严重/预警/求助
        QString status;    // active/resolved
        int alarmCode = 0;
    };
    struct HistoryMetricStats {
        int count = 0;
        double minVal = 0.0;
        double maxVal = 0.0;
        double avgVal = 0.0;
        double latestVal = 0.0;
    };

    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    bool openOrCreate(QString* err = nullptr);
    bool insertSensorSample(const SensorData& data, QString* err = nullptr);
    bool updateDailyResourceUsage(const QDateTime& sampleTime,
                                  int powerCounterMAh,
                                  int waterCounterCL,
                                  QString* err = nullptr);
    QList<SensorData> queryRecentData(int limit,
                                      const QDateTime& since = QDateTime(),
                                      QString* err = nullptr) const;
    QList<SensorData> queryDataRange(const QDateTime& start,
                                     const QDateTime& end,
                                     QString* err = nullptr) const;
    QList<SensorData> queryDataRangeSampled(const QDateTime& start,
                                            const QDateTime& end,
                                            int maxPoints,
                                            int* totalCount = nullptr,
                                            QString* err = nullptr) const;
    bool queryHistoryMetricStats(const QDateTime& start,
                                 const QDateTime& end,
                                 QVector<HistoryMetricStats>* out,
                                 QString* err = nullptr) const;
    QList<QDateTime> queryAllDataTimestamps(QString* err = nullptr) const;
    QList<DailyResourceUsageEntry> queryDailyResourceUsage(const QDate& startDay = QDate(),
                                                           const QDate& endDay = QDate(),
                                                           QString* err = nullptr) const;
    bool queryAverageSince(const QDateTime& since,
                           DataAverages* out,
                           QString* err = nullptr) const;
    bool insertRemoteExecLog(const QString& commandText,
                             const QString& resultText,
                             const QDateTime& executeTime,
                             QString* err = nullptr);
    QList<RemoteExecLogEntry> queryRemoteExecLogs(int limit = 0,
                                                  QString* err = nullptr) const;

    bool insertAlarmInfo(const QString& alarmTime,
                          const QString& alarmContent,
                          const QString& level = "预警",
                          int alarmCode = 0,
                          QString* err = nullptr);
    QList<AlarmInfoEntry> queryAlarmInfos(int limit,
                                          QString* err = nullptr) const;
    bool updateAlarmResolved(int alarmCode, const QString& endTime,
                              QString* err = nullptr);
    bool markAlarmHandled(int id, QString* err = nullptr);

    QString databasePath() const { return m_dbPath; }

private:
    bool ensureSchema(QString* err);
    bool backfillDailyResourceUsageFromSamples(QString* err);

    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};
