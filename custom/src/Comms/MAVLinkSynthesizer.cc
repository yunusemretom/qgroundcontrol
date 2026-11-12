/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MAVLinkSynthesizer.h"

#include <QtCore/QDateTime>
#include <QtCore/QSet>

MAVLinkSynthesizer::MAVLinkSynthesizer(const ProtocolProfile &profile, uint8_t mavlinkChannel)
    : _profile(profile)
    , _channel(mavlinkChannel)
{
    // Remember which MAVLink messages this profile actually feeds, so we never emit
    // a message the operator did not ask for.
    const QHash<QString, MappingRule> rules = _profile.mavlinkRules();
    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
        _messageIds.insert(it.key().section(QLatin1Char('.'), 0, 0));
    }
}

double MAVLinkSynthesizer::_value(const QString &key, double fallback) const
{
    return _state.contains(key) ? _state.value(key).toDouble() : fallback;
}

QList<mavlink_message_t> MAVLinkSynthesizer::buildTelemetryMessages(const QHash<QString, QVariant> &mapped)
{
    for (auto it = mapped.constBegin(); it != mapped.constEnd(); ++it) {
        _state.insert(it.key(), it.value());
    }

    QList<mavlink_message_t> messages;

    if (_messageIds.contains(QStringLiteral("GLOBAL_POSITION_INT"))) {
        messages.append(_buildGlobalPositionInt());
    }
    if (_messageIds.contains(QStringLiteral("ATTITUDE"))) {
        messages.append(_buildAttitude());
    }
    if (_messageIds.contains(QStringLiteral("SYS_STATUS"))) {
        messages.append(_buildSysStatus());
    }
    if (_messageIds.contains(QStringLiteral("GPS_RAW_INT"))) {
        messages.append(_buildGpsRawInt());
    }
    if (_messageIds.contains(QStringLiteral("VFR_HUD"))) {
        messages.append(_buildVfrHud());
    }

    return messages;
}

mavlink_message_t MAVLinkSynthesizer::buildHeartbeat(int customMode) const
{
    mavlink_message_t message{};

    (void) mavlink_msg_heartbeat_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint8_t>(_profile.mavType()),
        MAV_AUTOPILOT_GENERIC,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        static_cast<uint32_t>(customMode),
        MAV_STATE_ACTIVE);

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildGlobalPositionInt() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_global_position_int_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch()),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lat"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lon"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.alt"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.relative_alt"))),
        static_cast<int16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.vx"))),
        static_cast<int16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.vy"))),
        static_cast<int16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.vz"))),
        static_cast<uint16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.hdg"), UINT16_MAX)));

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildAttitude() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_attitude_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch()),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.roll"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.pitch"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.yaw"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.rollspeed"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.pitchspeed"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.yawspeed"))));

    return message;
}

uint32_t MAVLinkSynthesizer::_readyBits() const
{
    // Advertising PREARM_CHECK as present, enabled and healthy is what makes QGC treat
    // the bridged device as operational. A profile can opt out with
    // "vehicle": { "reportsReady": false }, in which case we advertise nothing and QGC
    // falls back to its own judgement.
    return _profile.reportsReady() ? static_cast<uint32_t>(MAV_SYS_STATUS_PREARM_CHECK) : 0u;
}

mavlink_message_t MAVLinkSynthesizer::_buildSysStatus() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_sys_status_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        // QGC decides "Ready To Fly" from the PREARM_CHECK bit: it needs the bit in
        // `enabled` for readyToFlyAvailable, and in `health` for readyToFly itself
        // (Vehicle.cc:992-1003). With all three words zero it falls back to requiring
        // autopilotPlugin.setupComplete, which a device with no parameters can never
        // satisfy — that is what showed "Not Ready" in the toolbar.
        _readyBits(), _readyBits(), _readyBits(),  // sensors present / enabled / health
        0,                                  // load
        static_cast<uint16_t>(_value(QStringLiteral("SYS_STATUS.voltage_battery"), UINT16_MAX)),
        -1,                                 // current_battery, -1 = unknown
        static_cast<int8_t>(_value(QStringLiteral("SYS_STATUS.battery_remaining"), -1)),
        0, 0, 0, 0, 0, 0,                   // comm drop/error counters
        0, 0, 0);                           // extended sensor fields

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildGpsRawInt() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_gps_raw_int_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000,
        static_cast<uint8_t>(_value(QStringLiteral("GPS_RAW_INT.fix_type"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lat"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lon"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.alt"))),
        UINT16_MAX, UINT16_MAX,             // eph, epv unknown
        UINT16_MAX,                         // vel unknown
        UINT16_MAX,                         // cog unknown
        static_cast<uint8_t>(_value(QStringLiteral("GPS_RAW_INT.satellites_visible"))),
        0, 0, 0, 0, 0, 0);                  // alt_ellipsoid, accuracies, yaw

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildVfrHud() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_vfr_hud_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<float>(_value(QStringLiteral("VFR_HUD.airspeed"))),
        static_cast<float>(_value(QStringLiteral("VFR_HUD.groundspeed"))),
        static_cast<int16_t>(_value(QStringLiteral("VFR_HUD.heading"))),
        static_cast<uint16_t>(_value(QStringLiteral("VFR_HUD.throttle"))),
        static_cast<float>(_value(QStringLiteral("VFR_HUD.alt"))),
        static_cast<float>(_value(QStringLiteral("VFR_HUD.climb"))));

    return message;
}
