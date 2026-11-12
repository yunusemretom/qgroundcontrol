/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CommandTranslator.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>

QGC_LOGGING_CATEGORY(CommandTranslatorLog, "custom.comms.commandtranslator")

namespace {

CommandTemplate builtIn(const QString &cmd, const QJsonObject &params)
{
    CommandTemplate command;
    command.channel = QStringLiteral("command");
    command.payload.insert(QStringLiteral("cmd"), cmd);
    command.payload.insert(QStringLiteral("params"), params);
    return command;
}

QJsonValue substitute(const QJsonValue &value, const QString &replacement, const QVariant &rawValue);

QJsonObject substituteObject(const QJsonObject &object, const QString &replacement, const QVariant &rawValue)
{
    QJsonObject result;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        result.insert(it.key(), substitute(it.value(), replacement, rawValue));
    }
    return result;
}

/// A string that is exactly "{value}" takes the value with its own JSON type (so a number
/// stays a number); a string that merely contains it gets the text spliced in.
QJsonValue substitute(const QJsonValue &value, const QString &replacement, const QVariant &rawValue)
{
    static const QString placeholder = QStringLiteral("{value}");

    if (value.isString()) {
        const QString text = value.toString();
        if (text == placeholder) {
            return QJsonValue::fromVariant(rawValue);
        }
        if (text.contains(placeholder)) {
            return QString(text).replace(placeholder, replacement);
        }
        return value;
    }
    if (value.isObject()) {
        return substituteObject(value.toObject(), replacement, rawValue);
    }
    if (value.isArray()) {
        QJsonArray array;
        for (const QJsonValue &element : value.toArray()) {
            array.append(substitute(element, replacement, rawValue));
        }
        return array;
    }
    return value;
}

} // namespace

CommandTranslator::CommandTranslator(const ProtocolProfile &profile)
{
    if (profile.hasCommandSection()) {
        _templates = profile.commands();
    } else {
        _templates.insert(QStringLiteral("arm"),     builtIn(QStringLiteral("arm"), QJsonObject()));
        _templates.insert(QStringLiteral("disarm"),  builtIn(QStringLiteral("disarm"), QJsonObject()));
        _templates.insert(QStringLiteral("setMode"), builtIn(QStringLiteral("set_mode"),
                                                             QJsonObject{ { QStringLiteral("mode"), QStringLiteral("{value}") } }));
    }
}

bool CommandTranslator::isSupported(const QString &commandName) const
{
    return _templates.contains(commandName);
}

QString CommandTranslator::channelFor(const QString &commandName) const
{
    const QString channel = _templates.value(commandName).channel;
    return channel.isEmpty() ? QStringLiteral("command") : channel;
}

QJsonObject CommandTranslator::translate(const QString &commandName, const QVariant &value)
{
    const auto it = _templates.constFind(commandName);
    if (it == _templates.constEnd()) {
        qCDebug(CommandTranslatorLog) << "Profile defines no command" << commandName;
        return QJsonObject();
    }

    QJsonObject payload = substituteObject(it.value().payload, value.toString(), value);

    if (!payload.contains(QStringLiteral("id"))) {
        payload.insert(QStringLiteral("id"), _nextId++);
    }
    if (!payload.contains(QStringLiteral("timestamp"))) {
        payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    }

    const QString cmd = payload.value(QStringLiteral("cmd")).toString(commandName);
    _pendingById.insert(payload.value(QStringLiteral("id")).toVariant().toString(), cmd);

    // A device that never ACKs would otherwise grow this table without bound.
    if (_pendingById.size() > 256) {
        _pendingById.clear();
    }

    return payload;
}

bool CommandTranslator::parseAck(const QJsonObject &json, QString &commandName, bool &success)
{
    if (!json.contains(QStringLiteral("ack"))) {
        return false;
    }

    const QString ack = json.value(QStringLiteral("ack")).toVariant().toString();
    commandName = _pendingById.contains(ack) ? _pendingById.take(ack) : ack;

    const QJsonValue statusValue = json.value(QStringLiteral("status"));
    if (statusValue.isBool()) {
        success = statusValue.toBool();
    } else {
        const QString status = statusValue.toString().toLower();
        success = status.isEmpty() || (status == QLatin1String("ok")) ||
                  (status == QLatin1String("success")) || (status == QLatin1String("true"));
    }
    return true;
}
