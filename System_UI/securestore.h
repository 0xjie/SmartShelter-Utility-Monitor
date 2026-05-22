#pragma once

#include <QByteArray>
#include <QString>

class SecureStore {
public:
    static void saveRemembered(const QString& account, const QString& plainPassword, bool remember);
    static bool loadRemembered(QString& account, QString& plainPassword, bool& remember);
    static void clearRemembered();

private:
    static QByteArray encrypt(const QByteArray& data);
    static QByteArray decrypt(const QByteArray& data);
};
