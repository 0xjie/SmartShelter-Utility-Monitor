#pragma once

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>

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
        QString deviceId;
        QString commandText;
        QString resultText;
    };

    struct AlarmInfoEntry {
        int id = 0;
        QString alarmTime;
        QString endTime;
        QString sensorName;
        QString alarmContent;
        QString level;     // 严重/预警/求助
        QString status;    // active/resolved
        int alarmCode = 0;
    };

    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    bool openOrCreate(QString* err = nullptr);
    bool insertSensorSample(const SensorData& data, QString* err = nullptr);
    QList<SensorData> queryRecentData(int limit,
                                      const QDateTime& since = QDateTime(),
                                      QString* err = nullptr) const;
    QList<SensorData> queryDataRange(const QDateTime& start,
                                     const QDateTime& end,
                                     QString* err = nullptr) const;
    bool queryAverageSince(const QDateTime& since,
                           DataAverages* out,
                           QString* err = nullptr) const;
    bool upsertDeviceOfflineRecord(const QString& sensorName,
                                   const QDateTime& offlineTime,
                                   qint64 offlineDurationSec,
                                   QString* err = nullptr);
    bool upsertDeviceInfo(const QString& deviceId,
                          const QString& deviceName,
                          const QString& deviceType,
                          const QString& location,
                          QString* err = nullptr);
    bool insertRemoteExecLog(const QString& deviceId,
                             const QString& commandText,
                             const QString& resultText,
                             const QDateTime& executeTime,
                             QString* err = nullptr);
    QList<RemoteExecLogEntry> queryRemoteExecLogs(int limit = 0,
                                                  QString* err = nullptr) const;

    bool insertAlarmInfo(const QString& alarmTime,
                          const QString& sensorName,
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

    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};
