/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

/// Builds outgoing JSON commands from the profile's "commands" templates and matches the
/// device's ACKs back to them.
///
/// Command names are the profile keys: "arm", "disarm", "setMode". A profile with no
/// "commands" section gets built-in templates for those three, so older profiles keep
/// working; a profile with the section gets exactly what it lists.
///
/// Every command carries an "id" and a "timestamp" (unless its template sets them), as the
/// device protocol documents {id, cmd, params, timestamp}. An ACK's "ack" field may echo
/// either that id or the command's "cmd" string; both are recognised.
class CommandTranslator
{
public:
    explicit CommandTranslator(const ProtocolProfile &profile);

    bool    isSupported(const QString &commandName) const;

    /// Channel the command is sent on; "command" when the profile does not say.
    QString channelFor(const QString &commandName) const;

    /// Builds the payload for `commandName`, replacing "{value}" in string fields with
    /// `value`. Returns an empty object when the command is not supported.
    QJsonObject translate(const QString &commandName, const QVariant &value = QVariant());

    QJsonObject translateArm(bool arm) { return translate(arm ? QStringLiteral("arm") : QStringLiteral("disarm")); }
    QJsonObject translateSetMode(const QString &modeName) { return translate(QStringLiteral("setMode"), modeName); }

    /// Recognises an ACK. `commandName` is the acknowledged command's "cmd" value.
    bool parseAck(const QJsonObject &json, QString &commandName, bool &success);

private:
    QHash<QString, CommandTemplate> _templates;
    QHash<QString, QString>         _pendingById;   ///< sent id -> its "cmd" value
    qint64                          _nextId = 1;
};
