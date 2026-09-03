/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Builds MAVLink messages out of mapped telemetry values.
 */

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QVariant>

#include "MAVLinkLib.h"

/// Produces MAVLink messages from the values FieldMapper extracted.
///
/// Holds the last known value of every field it has ever seen, so a packet that
/// omits a field yields a message carrying the previous value rather than a zero.
/// Only messages whose fields the profile actually maps are produced.
class MAVLinkSynthesizer
{
public:
    MAVLinkSynthesizer(const ProtocolProfile &profile, uint8_t mavlinkChannel);

    /// Merges `mapped` into the running state and returns one message per
    /// MAVLink message the profile mentions. Empty when the profile maps nothing.
    QList<mavlink_message_t> buildTelemetryMessages(const QHash<QString, QVariant> &mapped);

    mavlink_message_t buildHeartbeat(int customMode) const;

private:
    mavlink_message_t _buildGlobalPositionInt() const;
    mavlink_message_t _buildAttitude() const;
    mavlink_message_t _buildSysStatus() const;
    mavlink_message_t _buildGpsRawInt() const;
    mavlink_message_t _buildVfrHud() const;

    double _value(const QString &key, double fallback = 0.0) const;

    ProtocolProfile          _profile;
    uint8_t                  _channel;
    QHash<QString, QVariant> _state;      ///< last known value per "MESSAGE.field" key
    QSet<QString>            _messageIds; ///< MESSAGE names the profile maps
};
