/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CsvCodec.h"

#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

QList<QJsonObject> CsvCodec::decode(const QByteArray &bytes)
{
    Q_UNUSED(bytes);
    // Joystick channel is out-only for QGC.
    return {};
}

QByteArray CsvCodec::encode(const QJsonObject &object)
{
    const double pitch = object.value(QStringLiteral("pitch")).toDouble(0.0);
    const double yaw   = object.value(QStringLiteral("yaw")).toDouble(0.0);
    const bool   fire  = object.value(QStringLiteral("fire")).toBool(false);
    const bool   arm   = object.value(QStringLiteral("arm")).toBool(false);

    return formatJoystick(pitch, yaw, fire, arm);
}

QByteArray CsvCodec::formatJoystick(double pitch, double yaw, bool fire, bool arm)
{
    return QStringLiteral("%1,%2,%3,%4\n")
            .arg(pitch, 0, 'f', 3)
            .arg(yaw, 0, 'f', 3)
            .arg(fire ? 1 : 0)
            .arg(arm ? 1 : 0)
            .toUtf8();
}
