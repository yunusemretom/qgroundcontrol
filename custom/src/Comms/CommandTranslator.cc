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

QGC_LOGGING_CATEGORY(CommandTranslatorLog, "custom.comms.commandtranslator")

CommandTranslator::CommandTranslator(const ProtocolProfile &profile)
    : _profile(profile)
{
}

QJsonObject CommandTranslator::translateArm(bool arm) const
{
    QJsonObject cmd;
    cmd[QStringLiteral("cmd")] = arm ? QStringLiteral("arm") : QStringLiteral("disarm");
    cmd[QStringLiteral("params")] = QJsonObject();
    return cmd;
}

QJsonObject CommandTranslator::translateSetMode(const QString &modeName) const
{
    QJsonObject cmd;
    cmd[QStringLiteral("cmd")] = QStringLiteral("set_mode");
    QJsonObject params;
    params[QStringLiteral("mode")] = modeName;
    cmd[QStringLiteral("params")] = params;
    return cmd;
}

bool CommandTranslator::parseAck(const QJsonObject &json, QString &commandName, bool &success) const
{
    if (json.contains(QStringLiteral("ack"))) {
        commandName = json.value(QStringLiteral("ack")).toString();
        const QString status = json.value(QStringLiteral("status")).toString();
        success = (status.isEmpty() || status == QLatin1String("ok") || status == QLatin1String("success") || status == QLatin1String("true"));
        return true;
    }
    return false;
}
