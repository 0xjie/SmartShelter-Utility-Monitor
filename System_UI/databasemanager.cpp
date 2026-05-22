#include "databasemanager.h"
#include "dbconfig.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent),
      m_connectionName(QString("system_ui_sqlite_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))) {
}

DatabaseManager::~DatabaseManager() {
    const QString connName = m_connectionName;
    if (m_db.isValid()) {
        m_db.close();
        // Important: clear the database handle before removeDatabase,
        // otherwise Qt warns the connection is still in use.
        m_db = QSqlDatabase();
    }
    if (!connName.isEmpty()) {
        QSqlDatabase::removeDatabase(connName);
    }
}

bool DatabaseManager::openOrCreate(QString* err) {
    const QString dirPath = DbConfig::kDbDir;
    if (!QDir().mkpath(dirPath)) {
        if (err) *err = QString("无法创建数据库目录：%1").arg(QDir::toNativeSeparators(dirPath));
        return false;
    }
    m_dbPath = QDir(dirPath).filePath(DbConfig::kDbFileName);

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }
    return ensureSchema(err);
}

bool DatabaseManager::ensureSchema(QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }

    QSqlQuery q(m_db);
    // 页面查询、统计与导出统一使用 data 表。
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS data ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  ts TEXT NOT NULL,"                // ISO8601
            "  temperature REAL,"
            "  humidity REAL,"
            "  pm25 REAL,"
            "  air_index REAL,"
            "  current_a REAL,"
            "  flow_l_min REAL"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_data_ts ON data(ts);")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS device_abnormal_records ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  sensor_name TEXT NOT NULL,"
            "  offline_time TEXT NOT NULL,"
            "  offline_duration_sec INTEGER NOT NULL DEFAULT 0,"
            "  UNIQUE(sensor_name, offline_time)"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_device_abnormal_sensor_time "
                "ON device_abnormal_records(sensor_name, offline_time);")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS device_info ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  device_id TEXT NOT NULL UNIQUE,"
            "  device_name TEXT NOT NULL,"
            "  device_type TEXT NOT NULL,"
            "  location TEXT"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS remote_exec_logs ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  execute_time TEXT NOT NULL,"
            "  device_id TEXT NOT NULL,"
            "  command_text TEXT NOT NULL,"
            "  result_text TEXT"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_remote_exec_time ON remote_exec_logs(execute_time);")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS alarm_info ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  alarm_time TEXT NOT NULL,"
            "  sensor_name TEXT NOT NULL,"
            "  alarm_content TEXT NOT NULL"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    // 升级旧表：新增列（忽略已存在的错误）
    q.exec("ALTER TABLE alarm_info ADD COLUMN end_time TEXT");
    q.exec("ALTER TABLE alarm_info ADD COLUMN level TEXT DEFAULT '预警'");
    q.exec("ALTER TABLE alarm_info ADD COLUMN status TEXT DEFAULT 'active'");
    q.exec("ALTER TABLE alarm_info ADD COLUMN alarm_code INTEGER DEFAULT 0");

    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_alarm_info_time ON alarm_info(alarm_time);")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::insertSensorSample(const SensorData& data, QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }

    QSqlQuery qData(m_db);
    qData.prepare(
        "INSERT INTO data(ts,temperature,humidity,pm25,air_index,current_a,flow_l_min) "
        "VALUES(?,?,?,?,?,?,?);");
    qData.addBindValue(data.ts.toString(Qt::ISODateWithMs));
    qData.addBindValue(data.tempC);
    qData.addBindValue(data.humiPercent);
    qData.addBindValue(data.pm25UgM3);
    qData.addBindValue(data.airIndex);
    qData.addBindValue(data.currentA);
    qData.addBindValue(data.flowLMin);

    if (!qData.exec()) {
        if (err) *err = qData.lastError().text();
        return false;
    }
    return true;
}

QList<SensorData> DatabaseManager::queryRecentData(int limit,
                                                   const QDateTime& since,
                                                   QString* err) const {
    QList<SensorData> rows;
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return rows;
    }

    QSqlQuery q(m_db);
    QString sql =
        "SELECT ts,temperature,humidity,pm25,air_index,current_a,flow_l_min "
        "FROM data ";
    if (since.isValid()) {
        sql += "WHERE ts >= ? ";
    }
    sql += "ORDER BY ts DESC LIMIT ?;";

    q.prepare(sql);
    if (since.isValid()) {
        q.addBindValue(since.toString(Qt::ISODateWithMs));
    }
    q.addBindValue(qMax(1, limit));

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return rows;
    }

    while (q.next()) {
        SensorData item;
        item.ts = QDateTime::fromString(q.value(0).toString(), Qt::ISODateWithMs);
        item.tempC = q.value(1).toDouble();
        item.humiPercent = q.value(2).toDouble();
        item.pm25UgM3 = q.value(3).toDouble();
        item.airIndex = q.value(4).toDouble();
        item.currentA = q.value(5).toDouble();
        item.flowLMin = q.value(6).toDouble();
        if (item.airIndex > 80.0) {
            item.airQuality = "差";
        } else if (item.airIndex > 50.0) {
            item.airQuality = "良";
        } else {
            item.airQuality = "优";
        }
        rows.append(item);
    }
    return rows;
}

QList<SensorData> DatabaseManager::queryDataRange(const QDateTime& start,
                                                  const QDateTime& end,
                                                  QString* err) const {
    QList<SensorData> rows;
    if (!m_db.isOpen()) {
        if (err) *err = QStringLiteral("Database is not open");
        return rows;
    }
    if (!start.isValid() || !end.isValid()) {
        if (err) *err = QStringLiteral("Invalid time range");
        return rows;
    }
    if (start >= end) {
        if (err) *err = QStringLiteral("Start time must be earlier than end time");
        return rows;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT ts,temperature,humidity,pm25,air_index,current_a,flow_l_min "
        "FROM data "
        "WHERE ts >= ? AND ts <= ? "
        "ORDER BY ts ASC;");
    q.addBindValue(start.toString(Qt::ISODateWithMs));
    q.addBindValue(end.toString(Qt::ISODateWithMs));

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return rows;
    }

    while (q.next()) {
        SensorData item;
        item.ts = QDateTime::fromString(q.value(0).toString(), Qt::ISODateWithMs);
        item.tempC = q.value(1).toDouble();
        item.humiPercent = q.value(2).toDouble();
        item.pm25UgM3 = q.value(3).toDouble();
        item.airIndex = q.value(4).toDouble();
        item.currentA = q.value(5).toDouble();
        item.flowLMin = q.value(6).toDouble();
        if (item.airIndex > 80.0) {
            item.airQuality = QStringLiteral("差");
        } else if (item.airIndex > 50.0) {
            item.airQuality = QStringLiteral("良");
        } else {
            item.airQuality = QStringLiteral("优");
        }
        rows.append(item);
    }
    return rows;
}

bool DatabaseManager::queryAverageSince(const QDateTime& since,
                                        DataAverages* out,
                                        QString* err) const {
    if (out == nullptr) {
        if (err) *err = "输出参数为空";
        return false;
    }
    *out = DataAverages{};

    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT COUNT(*),"
        "       AVG(temperature),"
        "       AVG(humidity),"
        "       AVG(pm25),"
        "       AVG(air_index),"
        "       AVG(current_a),"
        "       AVG(flow_l_min) "
        "FROM data WHERE ts >= ?;");
    q.addBindValue(since.toString(Qt::ISODateWithMs));

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        return true;
    }

    out->sampleCount = q.value(0).toInt();
    if (out->sampleCount <= 0) {
        return true;
    }

    out->temperature = q.value(1).toDouble();
    out->humidity = q.value(2).toDouble();
    out->pm25 = q.value(3).toDouble();
    out->airIndex = q.value(4).toDouble();
    out->currentA = q.value(5).toDouble();
    out->flowLMin = q.value(6).toDouble();
    return true;
}

bool DatabaseManager::upsertDeviceOfflineRecord(const QString& sensorName,
                                                const QDateTime& offlineTime,
                                                qint64 offlineDurationSec,
                                                QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }
    if (sensorName.trimmed().isEmpty() || !offlineTime.isValid()) {
        if (err) *err = "离线记录参数无效";
        return false;
    }

    const qint64 safeDuration = qMax<qint64>(0, offlineDurationSec);
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO device_abnormal_records(sensor_name, offline_time, offline_duration_sec) "
        "VALUES(?,?,?) "
        "ON CONFLICT(sensor_name, offline_time) DO UPDATE SET offline_duration_sec=excluded.offline_duration_sec;");
    q.addBindValue(sensorName.trimmed());
    q.addBindValue(offlineTime.toString("yyyy-MM-dd HH:mm:ss"));
    q.addBindValue(safeDuration);

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::upsertDeviceInfo(const QString& deviceId,
                                       const QString& deviceName,
                                       const QString& deviceType,
                                       const QString& location,
                                       QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }
    if (deviceId.trimmed().isEmpty() || deviceName.trimmed().isEmpty() || deviceType.trimmed().isEmpty()) {
        if (err) *err = "设备信息参数无效";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO device_info(device_id, device_name, device_type, location) "
        "VALUES(?,?,?,?) "
        "ON CONFLICT(device_id) DO UPDATE SET "
        "device_name=excluded.device_name, device_type=excluded.device_type, location=excluded.location;");
    q.addBindValue(deviceId.trimmed());
    q.addBindValue(deviceName.trimmed());
    q.addBindValue(deviceType.trimmed());
    q.addBindValue(location.trimmed());

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::insertRemoteExecLog(const QString& deviceId,
                                          const QString& commandText,
                                          const QString& resultText,
                                          const QDateTime& executeTime,
                                          QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }
    if (deviceId.trimmed().isEmpty() || commandText.trimmed().isEmpty() || !executeTime.isValid()) {
        if (err) *err = "远程执行日志参数无效";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO remote_exec_logs(execute_time, device_id, command_text, result_text) "
        "VALUES(?,?,?,?);");
    q.addBindValue(executeTime.toString("yyyy-MM-dd HH:mm:ss"));
    q.addBindValue(deviceId.trimmed());
    q.addBindValue(commandText.trimmed());
    q.addBindValue(resultText.trimmed());

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}

QList<DatabaseManager::RemoteExecLogEntry> DatabaseManager::queryRemoteExecLogs(int limit,
                                                                                 QString* err) const {
    QList<RemoteExecLogEntry> rows;
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return rows;
    }

    QSqlQuery q(m_db);
    QString sql =
        "SELECT execute_time, device_id, command_text, result_text "
        "FROM remote_exec_logs ";
    if (limit > 0) {
        sql += "ORDER BY execute_time DESC, id DESC LIMIT ?;";
    } else {
        sql += "ORDER BY execute_time ASC, id ASC;";
    }
    q.prepare(sql);
    if (limit > 0) {
        q.addBindValue(qMax(1, limit));
    }

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return rows;
    }

    while (q.next()) {
        RemoteExecLogEntry item;
        item.executeTime = q.value(0).toString();
        item.deviceId = q.value(1).toString();
        item.commandText = q.value(2).toString();
        item.resultText = q.value(3).toString();
        rows.append(item);
    }
    return rows;
}

bool DatabaseManager::insertAlarmInfo(const QString& alarmTime,
                                     const QString& sensorName,
                                     const QString& alarmContent,
                                     const QString& level,
                                     int alarmCode,
                                     QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }
    if (alarmTime.trimmed().isEmpty() || sensorName.trimmed().isEmpty() || alarmContent.trimmed().isEmpty()) {
        if (err) *err = "报警记录参数无效";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO alarm_info(alarm_time, sensor_name, alarm_content, level, alarm_code, status) "
              "VALUES(?,?,?,?,?,'active');");
    q.addBindValue(alarmTime.trimmed());
    q.addBindValue(sensorName.trimmed());
    q.addBindValue(alarmContent.trimmed());
    q.addBindValue(level.isEmpty() ? "预警" : level);
    q.addBindValue(alarmCode);

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}

QList<DatabaseManager::AlarmInfoEntry> DatabaseManager::queryAlarmInfos(int limit,
                                                                        QString* err) const {
    QList<AlarmInfoEntry> rows;
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return rows;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT id, alarm_time, COALESCE(end_time,''), sensor_name, alarm_content, "
        "COALESCE(level,'预警'), COALESCE(status,'active'), COALESCE(alarm_code,0) "
        "FROM alarm_info "
        "ORDER BY id DESC "
        "LIMIT ?;");
    q.addBindValue(qMax(1, limit));

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return rows;
    }

    while (q.next()) {
        AlarmInfoEntry item;
        item.id = q.value(0).toInt();
        item.alarmTime = q.value(1).toString();
        item.endTime = q.value(2).toString();
        item.sensorName = q.value(3).toString();
        item.alarmContent = q.value(4).toString();
        item.level = q.value(5).toString();
        item.status = q.value(6).toString();
        item.alarmCode = q.value(7).toInt();
        rows.append(item);
    }
    return rows;
}

bool DatabaseManager::updateAlarmResolved(int alarmCode, const QString& endTime,
                                          QString* err) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE alarm_info SET end_time=?, status='resolved' "
              "WHERE id=(SELECT id FROM alarm_info WHERE alarm_code=? AND end_time IS NULL "
              "ORDER BY id DESC LIMIT 1);");
    q.addBindValue(endTime);
    q.addBindValue(alarmCode);
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::markAlarmHandled(int id, QString* err) {
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE alarm_info SET status='resolved', end_time=datetime('now','localtime') WHERE id=?;");
    q.addBindValue(id);
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}
