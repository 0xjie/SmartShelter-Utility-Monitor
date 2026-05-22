#pragma once

#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSet>
#include <QString>

class AuthService : public QObject {
    Q_OBJECT
public:
    explicit AuthService(QObject* parent = nullptr);
    ~AuthService() override;

    bool isUsernameAvailable(const QString& username);

    bool login(const QString& account, const QString& password, QString& errMsg);
    QString roleForUser(const QString& username);
    bool registerUser(const QString& username, const QString& password, QString& errMsg);

private:
    bool ensureDbReady(QString* err = nullptr);
    bool ensureUserSchema(QString* err = nullptr);

private:
    QSet<QString> m_registeredUsernames;
    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};
