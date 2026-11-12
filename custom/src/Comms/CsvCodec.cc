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
#include <QtCore/QRegularExpression>
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

QByteArray CsvCodec::formatTemplate(const QString &frameTemplate, const QVariantMap &values)
{
    static const QRegularExpression placeholder(QStringLiteral(R"(\{([A-Za-z_][A-Za-z0-9_]*)(?::\.(\d+)f)?\})"));

    QString result;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = placeholder.globalMatch(frameTemplate);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += frameTemplate.mid(last, match.capturedStart() - last);
        last = match.capturedEnd();

        const QString name = match.captured(1);
        if (!values.contains(name)) {
            result += match.captured(0);
            continue;
        }

        double number = values.value(name).toDouble();
        if (number == 0.0) {
            number = 0.0;   // normalise -0.0, which would otherwise print as "-0"
        }
        if (match.capturedLength(2) > 0) {
            result += QString::number(number, 'f', match.captured(2).toInt());
        } else {
            result += QString::number(number, 'g', 15);
        }
    }
    result += frameTemplate.mid(last);

    return terminate(result);
}

QByteArray CsvCodec::terminate(const QString &frame)
{
    QByteArray bytes = frame.toUtf8();
    if (!bytes.endsWith('\n')) {
        bytes.append('\n');
    }
    return bytes;
}
