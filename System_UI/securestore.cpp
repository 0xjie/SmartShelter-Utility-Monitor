#include "securestore.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QSysInfo>

namespace {
constexpr auto kOrg = "CleanAuthDemo";
constexpr auto kApp = "SystemUI";

QByteArray machineKey() {
    const auto src = QSysInfo::machineUniqueId();
    return QCryptographicHash::hash(src, QCryptographicHash::Sha256);
}

QByteArray xorWithKey(const QByteArray& data, const QByteArray& key) {
    QByteArray out = data;
    if (key.isEmpty()) {
        return out;
    }
    for (int i = 0; i < out.size(); ++i) {
        out[i] = out[i] ^ key[i % key.size()];
    }
    return out;
}

bool decodeUtf8Strict(const QByteArray& data, QString& outText) {
    const QString decoded = QString::fromUtf8(data);
    if (decoded.toUtf8() != data) {
        return false;
    }
    outText = decoded;
    return true;
}

bool tryDecrypt(const QByteArray& encrypted, const QByteArray& key, QString& outText) {
    const QByteArray raw = QByteArray::fromBase64(encrypted);
    if (raw.isEmpty() && !encrypted.isEmpty()) {
        return false;
    }
    return decodeUtf8Strict(xorWithKey(raw, key), outText);
}
}  // namespace

QByteArray SecureStore::encrypt(const QByteArray& data) {
    return xorWithKey(data, machineKey()).toBase64();
}

QByteArray SecureStore::decrypt(const QByteArray& data) {
    const auto raw = QByteArray::fromBase64(data);
    return xorWithKey(raw, machineKey());
}

void SecureStore::saveRemembered(const QString& account, const QString& plainPassword, bool remember) {
    QSettings s(kOrg, kApp);
    s.setValue("remember/enabled", remember);
    if (!remember) {
        s.remove("remember/account");
        s.remove("remember/password");
        return;
    }
    s.setValue("remember/account", account);
    s.setValue("remember/password", encrypt(plainPassword.toUtf8()));
}

bool SecureStore::loadRemembered(QString& account, QString& plainPassword, bool& remember) {
    QSettings s(kOrg, kApp);
    remember = s.value("remember/enabled", false).toBool();
    if (!remember) {
        return false;
    }

    account = s.value("remember/account").toString();
    const QByteArray encrypted = s.value("remember/password").toByteArray();

    QString decodedPwd;
    if (tryDecrypt(encrypted, machineKey(), decodedPwd)) {
        plainPassword = decodedPwd;
        return !account.isEmpty();
    }

    // Avoid showing garbled text in UI when decrypt fails.
    plainPassword.clear();
    s.remove("remember/password");
    return false;
}

void SecureStore::clearRemembered() {
    QSettings s(kOrg, kApp);
    s.remove("remember");
}
