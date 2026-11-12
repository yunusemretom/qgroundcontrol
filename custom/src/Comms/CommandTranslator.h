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

#include <QtCore/QJsonObject>
#include <QtCore/QString>

/// Translates outgoing MAVLink commands and flight mode changes into JSON command payloads,
/// and processes ACK responses.
class CommandTranslator
{
public:
    explicit CommandTranslator(const ProtocolProfile &profile);

    /// Generates a JSON command payload for arm (arm = true) or disarm (arm = false).
    QJsonObject translateArm(bool arm) const;

    /// Generates a JSON command payload for set flight mode.
    QJsonObject translateSetMode(const QString &modeName) const;

    /// Checks if a received JSON response is an ACK for a command.
    bool parseAck(const QJsonObject &json, QString &commandName, bool &success) const;

private:
    ProtocolProfile _profile;
};
