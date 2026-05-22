#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

struct SensorData {
    QDateTime ts = QDateTime::currentDateTime();
    double tempC = 0.0;
    double humiPercent = 0.0;
    double airIndex = 0.0;
    double currentA = 0.0;  // 毫安 mA（与 STM32/MQTT 上报一致）
    double flowLMin = 0.0;
    double pm25UgM3 = 0.0;
    QString airQuality;     // "优/良/差"
    QString rawJsonUtf8;    // 原始 JSON 文本，方便排查
    QJsonObject rawObject;  // 可选：保留解析后的对象
};

