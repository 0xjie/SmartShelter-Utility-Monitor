#ifndef MQTT_H
#define MQTT_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QMqttClient;
QT_END_NAMESPACE
class QTimer;

class Mqtt : public QObject
{
    Q_OBJECT
public:
    explicit Mqtt(QObject *parent = nullptr);
    ~Mqtt() override;

    bool isConnected() const;
    void connectWithKey(const QString &host, quint16 port, const QString &key);
    void subscribeTopic(const QString &topic);
    void publishText(const QString &topic, const QString &text);

signals:
    void textMessageReceived(const QString &topic, const QString &payload);
    void stateChanged(int state);

private slots:
    void onStateChanged(int state);
    void tryReconnect();

private:
    QMqttClient *m_client;
    QString m_host;
    quint16 m_port = 0;
    QString m_key;
    QStringList m_topics;
    QTimer* m_reconnectTimer = nullptr;
    int m_reconnectDelaySec = 3;
};

#endif // MQTT_H