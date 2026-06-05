#include "databasemanager.h"
#include "dbconfig.h"

#include <QDir>
#include <QDate>
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
    if (!ensureSchema(err)) {
        return false;
    }
    return backfillDailyResourceUsageFromSamples(err);
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
            "CREATE TABLE IF NOT EXISTS daily_resource_usage ("
            "  day TEXT PRIMARY KEY,"
            "  power_mah INTEGER NOT NULL DEFAULT 0,"
            "  water_cl INTEGER NOT NULL DEFAULT 0,"
            "  last_power_counter_mah INTEGER NOT NULL DEFAULT 0,"
            "  last_water_counter_cl INTEGER NOT NULL DEFAULT 0"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS remote_exec_logs ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  execute_time TEXT NOT NULL,"
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

bool DatabaseManager::backfillDailyResourceUsageFromSamples(QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = QStringLiteral("数据库未打开");
        return false;
    }

    // 始终从原始 data 表重建 daily_resource_usage（派生数据，可重建）
    {
        QSqlQuery delQ(m_db);
        delQ.exec(QStringLiteral("DELETE FROM daily_resource_usage;"));
    }

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT ts, current_a, flow_l_min "
        "FROM data "
        "ORDER BY ts ASC;");
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return false;
    }

    struct DayTotals {
        double powerMAh = 0.0;
        double waterCL = 0.0;
    };

    QMap<QString, DayTotals> totalsByDay;
    QDateTime prevTs;
    double prevCurrentA = 0.0;   // current_a 列存的是安培(A)
    double prevFlowLMin = 0.0;
    bool hasPrev = false;

    while (q.next()) {
        const QDateTime ts = QDateTime::fromString(q.value(0).toString(), Qt::ISODateWithMs);
        const double currentA = q.value(1).toDouble();   // 安培
        const double flowLMin = q.value(2).toDouble();
        if (!ts.isValid()) {
            continue;
        }

        if (hasPrev) {
            const qint64 dtMsRaw = prevTs.msecsTo(ts);
            const double dtSec = qBound(0.0, static_cast<double>(dtMsRaw) / 1000.0, 3600.0);
            if (dtSec > 0.0) {
                DayTotals& totals = totalsByDay[prevTs.date().toString(Qt::ISODate)];
                // A × (s/3600) = Ah，×1000 → mAh
                totals.powerMAh += prevCurrentA * (dtSec / 3600.0) * 1000.0;
                // L/min × (s/60) × 100 = cL
                totals.waterCL += prevFlowLMin * (dtSec / 60.0) * 100.0;
            }
        }

        prevTs = ts;
        prevCurrentA = currentA;
        prevFlowLMin = flowLMin;
        hasPrev = true;
    }

    if (totalsByDay.isEmpty()) {
        return true;
    }

    if (!m_db.transaction()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }

    QSqlQuery insertQ(m_db);
    insertQ.prepare(
        "INSERT OR REPLACE INTO daily_resource_usage("
        "  day, power_mah, water_cl, last_power_counter_mah, last_water_counter_cl"
        ") VALUES(?,?,?,?,?);");

    for (auto it = totalsByDay.constBegin(); it != totalsByDay.constEnd(); ++it) {
        const int dayPowerMAh = qMax(0, qRound(it.value().powerMAh));
        const int dayWaterCL = qMax(0, qRound(it.value().waterCL));

        insertQ.addBindValue(it.key());
        insertQ.addBindValue(dayPowerMAh);
        insertQ.addBindValue(dayWaterCL);
        // 回填不写计数器（写0），避免与STM32计数器混用导致双源不一致
        insertQ.addBindValue(0);
        insertQ.addBindValue(0);
        if (!insertQ.exec()) {
            const QString dbErr = insertQ.lastError().text();
            m_db.rollback();
            if (err) *err = dbErr;
            return false;
        }
    }

    if (!m_db.commit()) {
        if (err) *err = m_db.lastError().text();
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

bool DatabaseManager::updateDailyResourceUsage(const QDateTime& sampleTime,
                                               int powerCounterMAh,
                                               int waterCounterCL,
                                               QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = QStringLiteral("数据库未打开");
        return false;
    }
    if (!sampleTime.isValid()) {
        if (err) *err = QStringLiteral("采样时间无效");
        return false;
    }

    const int safePowerCounter = qMax(0, powerCounterMAh);
    const int safeWaterCounter = qMax(0, waterCounterCL);
    const QString day = sampleTime.date().toString(Qt::ISODate);

    auto upsertRow = [&](const QString& targetDay,
                         int addPowerMAh,
                         int addWaterCL,
                         int latestPowerCounterMAh,
                         int latestWaterCounterCL,
                         QString* upsertErr) -> bool {
        QSqlQuery q(m_db);
        q.prepare(
            "INSERT INTO daily_resource_usage("
            "  day, power_mah, water_cl, last_power_counter_mah, last_water_counter_cl"
            ") VALUES(?,?,?,?,?) "
            "ON CONFLICT(day) DO UPDATE SET "
            "  power_mah = daily_resource_usage.power_mah + excluded.power_mah,"
            "  water_cl = daily_resource_usage.water_cl + excluded.water_cl,"
            "  last_power_counter_mah = excluded.last_power_counter_mah,"
            "  last_water_counter_cl = excluded.last_water_counter_cl;");
        q.addBindValue(targetDay);
        q.addBindValue(qMax(0, addPowerMAh));
        q.addBindValue(qMax(0, addWaterCL));
        q.addBindValue(qMax(0, latestPowerCounterMAh));
        q.addBindValue(qMax(0, latestWaterCounterCL));
        if (!q.exec()) {
            if (upsertErr) *upsertErr = q.lastError().text();
            return false;
        }
        return true;
    };

    int prevPowerCounter = -1;
    int prevWaterCounter = -1;
    QString prevDay;
    {
        QSqlQuery q(m_db);
        q.prepare(
            "SELECT day, last_power_counter_mah, last_water_counter_cl "
            "FROM daily_resource_usage "
            "ORDER BY day DESC "
            "LIMIT 1;");
        if (!q.exec()) {
            if (err) *err = q.lastError().text();
            return false;
        }
        if (q.next()) {
            prevDay = q.value(0).toString();
            prevPowerCounter = q.value(1).toInt();
            prevWaterCounter = q.value(2).toInt();
        }
    }

    int powerDelta = 0;
    int waterDelta = 0;
    if (prevPowerCounter > 0) {
        // 正常增量：已有 STM32 计数器基线
        powerDelta = safePowerCounter - prevPowerCounter;
        waterDelta = safeWaterCounter - prevWaterCounter;

        // STM32 counter is cumulative since boot. If it becomes smaller, treat it as reboot/reset.
        if (powerDelta < 0) powerDelta = safePowerCounter;
        if (waterDelta < 0) waterDelta = safeWaterCounter;
    } else if (prevPowerCounter == 0) {
        // 回填后首次收到遥测：没有 STM32 基线，不累加，只记录计数器供后续 delta 计算
        powerDelta = 0;
        waterDelta = 0;
    } else {
        // 表完全为空（prevPowerCounter < 0）
        powerDelta = safePowerCounter;
        waterDelta = safeWaterCounter;
    }

    if (prevDay == day || prevDay.isEmpty()) {
        return upsertRow(day, powerDelta, waterDelta, safePowerCounter, safeWaterCounter, err);
    }

    // Cross-day rollover: write today's delta to the new day, while keeping the previous day's
    // last observed counter aligned with the counter snapshot that caused the day change.
    QString upsertErr;
    if (!upsertRow(prevDay, 0, 0, safePowerCounter, safeWaterCounter, &upsertErr)) {
        if (err) *err = upsertErr;
        return false;
    }
    if (!upsertRow(day, powerDelta, waterDelta, safePowerCounter, safeWaterCounter, &upsertErr)) {
        if (err) *err = upsertErr;
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

QList<SensorData> DatabaseManager::queryDataRangeSampled(const QDateTime& start,
                                                         const QDateTime& end,
                                                         int maxPoints,
                                                         int* totalCount,
                                                         QString* err) const {
    QList<SensorData> rows;
    if (totalCount != nullptr) {
        *totalCount = 0;
    }
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

    const int safeMaxPoints = qMax(32, maxPoints);

    QSqlQuery countQuery(m_db);
    countQuery.prepare(
        "SELECT COUNT(*) "
        "FROM data "
        "WHERE ts >= ? AND ts <= ?;");
    countQuery.addBindValue(start.toString(Qt::ISODateWithMs));
    countQuery.addBindValue(end.toString(Qt::ISODateWithMs));
    if (!countQuery.exec()) {
        if (err) *err = countQuery.lastError().text();
        return rows;
    }

    int rowCount = 0;
    if (countQuery.next()) {
        rowCount = countQuery.value(0).toInt();
    }
    if (totalCount != nullptr) {
        *totalCount = rowCount;
    }
    if (rowCount <= 0) {
        return rows;
    }
    if (rowCount <= safeMaxPoints) {
        return queryDataRange(start, end, err);
    }

    const int step = qMax(1, (rowCount + safeMaxPoints - 1) / safeMaxPoints);

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT ts,temperature,humidity,pm25,air_index,current_a,flow_l_min "
        "FROM ("
        "  SELECT ts,temperature,humidity,pm25,air_index,current_a,flow_l_min, "
        "         ROW_NUMBER() OVER (ORDER BY ts ASC) AS rn "
        "  FROM data "
        "  WHERE ts >= ? AND ts <= ?"
        ") sampled "
        "WHERE ((rn - 1) % ?) = 0 OR rn = ? "
        "ORDER BY rn ASC;");
    q.addBindValue(start.toString(Qt::ISODateWithMs));
    q.addBindValue(end.toString(Qt::ISODateWithMs));
    q.addBindValue(step);
    q.addBindValue(rowCount);

    if (q.exec()) {
        while (q.next()) {
            SensorData item;
            item.ts = QDateTime::fromString(q.value(0).toString(), Qt::ISODateWithMs);
            item.tempC = q.value(1).toDouble();
            item.humiPercent = q.value(2).toDouble();
            item.pm25UgM3 = q.value(3).toDouble();
            item.airIndex = q.value(4).toDouble();
            item.currentA = q.value(5).toDouble();
            item.flowLMin = q.value(6).toDouble();
            rows.append(item);
        }
        return rows;
    }

    QString fallbackErr;
    const QList<SensorData> fullRows = queryDataRange(start, end, &fallbackErr);
    if (!fallbackErr.isEmpty() && err != nullptr) {
        *err = fallbackErr;
    }
    if (fullRows.isEmpty()) {
        return rows;
    }

    for (int i = 0; i < fullRows.size(); i += step) {
        rows.append(fullRows.at(i));
    }
    if (!rows.isEmpty() && rows.back().ts != fullRows.back().ts) {
        rows.append(fullRows.back());
    }
    return rows;
}

bool DatabaseManager::queryHistoryMetricStats(const QDateTime& start,
                                              const QDateTime& end,
                                              QVector<HistoryMetricStats>* out,
                                              QString* err) const {
    if (out == nullptr) {
        if (err) *err = QStringLiteral("Output container is null");
        return false;
    }
    out->clear();
    out->resize(4);

    if (!m_db.isOpen()) {
        if (err) *err = QStringLiteral("Database is not open");
        return false;
    }
    if (!start.isValid() || !end.isValid()) {
        if (err) *err = QStringLiteral("Invalid time range");
        return false;
    }
    if (start >= end) {
        if (err) *err = QStringLiteral("Start time must be earlier than end time");
        return false;
    }

    QSqlQuery aggQuery(m_db);
    aggQuery.prepare(
        "SELECT COUNT(*), "
        "       MIN(temperature), MAX(temperature), AVG(temperature), "
        "       MIN(humidity), MAX(humidity), AVG(humidity), "
        "       MIN(pm25), MAX(pm25), AVG(pm25), "
        "       MIN(air_index), MAX(air_index), AVG(air_index) "
        "FROM data "
        "WHERE ts >= ? AND ts <= ?;");
    aggQuery.addBindValue(start.toString(Qt::ISODateWithMs));
    aggQuery.addBindValue(end.toString(Qt::ISODateWithMs));
    if (!aggQuery.exec()) {
        if (err) *err = aggQuery.lastError().text();
        return false;
    }
    if (!aggQuery.next()) {
        return true;
    }

    const int sampleCount = aggQuery.value(0).toInt();
    if (sampleCount <= 0) {
        return true;
    }

    const int offsets[4] = {1, 4, 7, 10};
    for (int i = 0; i < 4; ++i) {
        HistoryMetricStats stats;
        stats.count = sampleCount;
        stats.minVal = aggQuery.value(offsets[i]).toDouble();
        stats.maxVal = aggQuery.value(offsets[i] + 1).toDouble();
        stats.avgVal = aggQuery.value(offsets[i] + 2).toDouble();
        (*out)[i] = stats;
    }

    QSqlQuery latestQuery(m_db);
    latestQuery.prepare(
        "SELECT temperature, humidity, pm25, air_index "
        "FROM data "
        "WHERE ts >= ? AND ts <= ? "
        "ORDER BY ts DESC "
        "LIMIT 1;");
    latestQuery.addBindValue(start.toString(Qt::ISODateWithMs));
    latestQuery.addBindValue(end.toString(Qt::ISODateWithMs));
    if (!latestQuery.exec()) {
        if (err) *err = latestQuery.lastError().text();
        return false;
    }
    if (latestQuery.next()) {
        for (int i = 0; i < 4; ++i) {
            (*out)[i].latestVal = latestQuery.value(i).toDouble();
        }
    }

    return true;
}

QList<QDateTime> DatabaseManager::queryAllDataTimestamps(QString* err) const {
    QList<QDateTime> rows;
    if (!m_db.isOpen()) {
        if (err) *err = QStringLiteral("数据库未打开");
        return rows;
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT ts FROM data ORDER BY ts ASC;");
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return rows;
    }

    while (q.next()) {
        const QDateTime ts = QDateTime::fromString(q.value(0).toString(), Qt::ISODateWithMs);
        if (ts.isValid()) {
            rows.append(ts);
        }
    }
    return rows;
}

QList<DatabaseManager::DailyResourceUsageEntry> DatabaseManager::queryDailyResourceUsage(
    const QDate& startDay,
    const QDate& endDay,
    QString* err) const {
    QList<DailyResourceUsageEntry> rows;
    if (!m_db.isOpen()) {
        if (err) *err = QStringLiteral("数据库未打开");
        return rows;
    }
    if (startDay.isValid() && endDay.isValid() && startDay > endDay) {
        if (err) *err = QStringLiteral("起始日期必须早于结束日期");
        return rows;
    }

    QSqlQuery q(m_db);
    QString sql =
        "SELECT day, power_mah, water_cl, last_power_counter_mah, last_water_counter_cl "
        "FROM daily_resource_usage ";
    if (startDay.isValid() && endDay.isValid()) {
        sql += "WHERE day >= ? AND day <= ? ";
    } else if (startDay.isValid()) {
        sql += "WHERE day >= ? ";
    } else if (endDay.isValid()) {
        sql += "WHERE day <= ? ";
    }
    sql += "ORDER BY day ASC;";
    q.prepare(sql);
    if (startDay.isValid() && endDay.isValid()) {
        q.addBindValue(startDay.toString(Qt::ISODate));
        q.addBindValue(endDay.toString(Qt::ISODate));
    } else if (startDay.isValid()) {
        q.addBindValue(startDay.toString(Qt::ISODate));
    } else if (endDay.isValid()) {
        q.addBindValue(endDay.toString(Qt::ISODate));
    }

    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        return rows;
    }

    while (q.next()) {
        DailyResourceUsageEntry item;
        item.day = q.value(0).toString();
        item.powerMAh = q.value(1).toInt();
        item.waterCL = q.value(2).toInt();
        item.lastPowerCounterMAh = q.value(3).toInt();
        item.lastWaterCounterCL = q.value(4).toInt();
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

bool DatabaseManager::insertRemoteExecLog(const QString& commandText,
                                          const QString& resultText,
                                          const QDateTime& executeTime,
                                          QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }
    if (commandText.trimmed().isEmpty() || !executeTime.isValid()) {
        if (err) *err = "远程执行日志参数无效";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO remote_exec_logs(execute_time, command_text, result_text) "
        "VALUES(?,?,?);");
    q.addBindValue(executeTime.toString("yyyy-MM-dd HH:mm:ss"));
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
        "SELECT execute_time, command_text, result_text "
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
        item.commandText = q.value(1).toString();
        item.resultText = q.value(2).toString();
        rows.append(item);
    }
    return rows;
}

bool DatabaseManager::insertAlarmInfo(const QString& alarmTime,
                                     const QString& alarmContent,
                                     const QString& level,
                                     int alarmCode,
                                     QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "数据库未打开";
        return false;
    }
    if (alarmTime.trimmed().isEmpty() || alarmContent.trimmed().isEmpty()) {
        if (err) *err = "报警记录参数无效";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO alarm_info(alarm_time, alarm_content, level, alarm_code, status) "
              "VALUES(?,?,?,?,'active');");
    q.addBindValue(alarmTime.trimmed());
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
        "SELECT id, alarm_time, COALESCE(end_time,''), alarm_content, "
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
        item.alarmContent = q.value(3).toString();
        item.level = q.value(4).toString();
        item.status = q.value(5).toString();
        item.alarmCode = q.value(6).toInt();
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
