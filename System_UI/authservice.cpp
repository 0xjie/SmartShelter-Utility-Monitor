#include "authservice.h"
#include "dbconfig.h"

#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

AuthService::AuthService(QObject* parent)
    : QObject(parent),
      m_connectionName(QString("system_ui_auth_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))) {
    QString err;
    if (!ensureDbReady(&err)) {
        qWarning() << "[Auth] DB init failed:" << err;
    }
}

AuthService::~AuthService() {
    const QString connName = m_connectionName;
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
    }
    if (!connName.isEmpty()) {
        QSqlDatabase::removeDatabase(connName);
    }
}

bool AuthService::ensureDbReady(QString* err) {
    if (m_db.isValid() && m_db.isOpen()) {
        return true;
    }

    const QString dirPath = DbConfig::kDbDir;
    if (!QDir().mkpath(dirPath)) {
        if (err) *err = QString("无法创建认证数据库目录：%1").arg(dirPath);
        return false;
    }

    m_dbPath = QDir(dirPath).filePath(DbConfig::kDbFileName);
    if (!m_db.isValid()) {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        m_db.setDatabaseName(m_dbPath);
    }
    if (!m_db.open()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }

    if (!ensureUserSchema(err)) {
        return false;
    }
    return true;
}

bool AuthService::ensureUserSchema(QString* err) {
    if (!m_db.isOpen()) {
        if (err) *err = "认证数据库未打开";
        return false;
    }

    QSqlQuery q(m_db);
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  username TEXT NOT NULL UNIQUE,"
            "  password TEXT NOT NULL,"
            "  role TEXT NOT NULL DEFAULT 'admin'"
            ");")) {
        if (err) *err = q.lastError().text();
        return false;
    }

    if (!q.exec(
            "INSERT OR IGNORE INTO users(username,password,role) "
            "VALUES('admin','Admin@123','admin');")) {
        if (err) *err = q.lastError().text();
        return false;
    }
    if (!q.exec(
            "INSERT OR IGNORE INTO users(username,password,role) "
            "VALUES('test','Test@123','admin');")) {
        if (err) *err = q.lastError().text();
        return false;
    }
    return true;
}

bool AuthService::isUsernameAvailable(const QString& username) {
    QString err;
    if (!ensureDbReady(&err)) {
        qWarning() << "[Auth] ensureDbReady failed in isUsernameAvailable:" << err;
        return false;
    }

    const QString trimmed = username.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM users WHERE lower(username)=lower(?) LIMIT 1;");
    q.addBindValue(trimmed);
    if (!q.exec()) {
        qWarning() << "[Auth] username query failed:" << q.lastError().text();
        return false;
    }
    return !q.next();
}

bool AuthService::login(const QString& account, const QString& password, QString& errMsg) {
    if (account.trimmed().isEmpty()) {
        errMsg = "账号不能为空";
        return false;
    }
    if (password.size() < 6) {
        errMsg = "密码长度至少 6 位";
        return false;
    }

    if (!ensureDbReady(&errMsg)) {
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT password FROM users WHERE lower(username)=lower(?) LIMIT 1;");
    q.addBindValue(account.trimmed());
    if (!q.exec()) {
        errMsg = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        errMsg = "账号不存在";
        return false;
    }
    const QString dbPwd = q.value(0).toString();
    if (dbPwd != password) {
        errMsg = "账号或密码错误";
        return false;
    }

    errMsg.clear();
    return true;
}

QString AuthService::roleForUser(const QString& username) {
    QString err;
    if (!ensureDbReady(&err)) {
        qWarning() << "[Auth] ensureDbReady failed in roleForUser:" << err;
        return "user";
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT role FROM users WHERE lower(username)=lower(?) LIMIT 1;");
    q.addBindValue(username.trimmed());
    if (!q.exec() || !q.next()) {
        return "user";
    }
    const QString role = q.value(0).toString().trimmed().toLower();
    return role.isEmpty() ? "user" : role;
}

bool AuthService::registerUser(const QString& username, const QString& password, QString& errMsg) {
    if (username.trimmed().isEmpty()) {
        errMsg = "用户名不能为空";
        return false;
    }
    if (!isUsernameAvailable(username)) {
        errMsg = "用户名已存在";
        return false;
    }
    if (password.size() < 6) {
        errMsg = "密码至少 6 位";
        return false;
    }

    QString dbErr;
    if (!ensureDbReady(&dbErr)) {
        errMsg = dbErr;
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO users(username,password,role) "
        "VALUES(?,?,?);");
    q.addBindValue(username.trimmed());
    q.addBindValue(password);
    q.addBindValue(QStringLiteral("admin"));
    if (!q.exec()) {
        errMsg = q.lastError().text();
        return false;
    }

    m_registeredUsernames.insert(username.trimmed().toLower());
    errMsg.clear();
    return true;
}
