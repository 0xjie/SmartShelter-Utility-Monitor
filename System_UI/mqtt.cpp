#include "mqtt.h"
#include <QMqttClient>
#include <QMqttSubscription>
#include <QTimer>
#include <QDebug>

Mqtt::Mqtt(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
{
    m_client->setKeepAlive(30);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &Mqtt::tryReconnect);

    connect(m_client, &QMqttClient::messageReceived, this,
            [this](const QByteArray &message, const QMqttTopicName &topic) {
                emit textMessageReceived(topic.name(), QString::fromUtf8(message));
            });

    connect(m_client, &QMqttClient::stateChanged, this, &Mqtt::onStateChanged);
}

Mqtt::~Mqtt() = default;

bool Mqtt::isConnected() const {
    return m_client && m_client->state() == QMqttClient::Connected;
}

void Mqtt::connectWithKey(const QString &host, quint16 port, const QString &key)
{
    m_host = host;
    m_port = port;
    m_key  = key;

    tryReconnect();
}

void Mqtt::tryReconnect()
{
    if (m_client->state() == QMqttClient::Connected ||
        m_client->state() == QMqttClient::Connecting) {
        return;
    }

    m_client->setHostname(m_host);
    m_client->setPort(m_port);
    m_client->setClientId(m_key);
    m_client->setUsername(QString());
    m_client->setPassword(QString());
    m_client->setKeepAlive(30);
    m_client->connectToHost();
}

void Mqtt::onStateChanged(int stateInt)
{
    const auto state = static_cast<QMqttClient::ClientState>(stateInt);
    emit stateChanged(stateInt);

    if (state == QMqttClient::Connected) {
        m_reconnectDelaySec = 3;
        // 重新订阅所有已记录的主题
        for (const QString& topic : std::as_const(m_topics)) {
            m_client->subscribe(topic, 0);
        }
        qDebug() << "[MQTT] connected, subscribed" << m_topics.size() << "topics";
    } else if (state == QMqttClient::Disconnected) {
        qDebug() << "[MQTT] disconnected, will reconnect in" << m_reconnectDelaySec << "s";
        m_reconnectTimer->start(m_reconnectDelaySec * 1000);
        m_reconnectDelaySec = qMin(m_reconnectDelaySec * 2, 60);
    }
}

void Mqtt::subscribeTopic(const QString &topic)
{
    if (!m_topics.contains(topic)) {
        m_topics.append(topic);
    }
    if (m_client->state() == QMqttClient::Connected) {
        m_client->subscribe(topic, 0);
    }
}

void Mqtt::publishText(const QString &topic, const QString &text)
{
    if (m_client->state() == QMqttClient::Connected) {
        m_client->publish(topic, text.toUtf8());
    }
}
