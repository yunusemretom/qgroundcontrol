/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MAVLinkSynthesizerTest.h"
#include "MAVLinkSynthesizer.h"
#include "ProtocolProfile.h"

#include <QtTest/QTest>

QByteArray MAVLinkSynthesizerTest::_profileJson()
{
    return R"({
        "name": "Synth Test", "schemaVersion": 1,
        "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 42, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat":          { "from": "gps.lat", "scale": 1e7 },
                "GLOBAL_POSITION_INT.lon":          { "from": "gps.lon", "scale": 1e7 },
                "GLOBAL_POSITION_INT.relative_alt": { "from": "altitude", "scale": 1000 },
                "ATTITUDE.yaw":                     { "from": "yaw", "unit": "deg2rad" },
                "ATTITUDE.pitch":                   { "from": "pitch", "unit": "deg2rad" }
            }
        }
    })";
}

/// Finds the first message with the given id, or returns false.
static bool findMessage(const QList<mavlink_message_t> &messages, uint32_t msgId, mavlink_message_t &out)
{
    for (const mavlink_message_t &message : messages) {
        if (message.msgid == msgId) {
            out = message;
            return true;
        }
    }
    return false;
}

void MAVLinkSynthesizerTest::_heartbeatCarriesProfileIdentity_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    const mavlink_message_t heartbeat = synthesizer.buildHeartbeat(0);

    QCOMPARE(heartbeat.msgid, static_cast<uint32_t>(MAVLINK_MSG_ID_HEARTBEAT));
    QCOMPARE(static_cast<int>(heartbeat.sysid), 42);
    QCOMPARE(static_cast<int>(heartbeat.compid), 1);
}

void MAVLinkSynthesizerTest::_heartbeatCarriesCustomMode_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    const mavlink_message_t heartbeat = synthesizer.buildHeartbeat(2);

    mavlink_heartbeat_t decoded{};
    mavlink_msg_heartbeat_decode(&heartbeat, &decoded);
    QCOMPARE(decoded.custom_mode, static_cast<uint32_t>(2));
}

void MAVLinkSynthesizerTest::_buildsGlobalPositionFromMappedValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.lat"), 39.925533 * 1e7);
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.lon"), 32.866287 * 1e7);
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.relative_alt"), 150.3 * 1000);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(mapped), MAVLINK_MSG_ID_GLOBAL_POSITION_INT, message));

    mavlink_global_position_int_t decoded{};
    mavlink_msg_global_position_int_decode(&message, &decoded);
    QCOMPARE(decoded.lat, static_cast<int32_t>(39.925533 * 1e7));
    QCOMPARE(decoded.lon, static_cast<int32_t>(32.866287 * 1e7));
    QCOMPARE(decoded.relative_alt, static_cast<int32_t>(150.3 * 1000));
}

void MAVLinkSynthesizerTest::_buildsAttitudeFromMappedValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("ATTITUDE.yaw"), 1.5);
    mapped.insert(QStringLiteral("ATTITUDE.pitch"), 0.25);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(mapped), MAVLINK_MSG_ID_ATTITUDE, message));

    mavlink_attitude_t decoded{};
    mavlink_msg_attitude_decode(&message, &decoded);
    QVERIFY(qFuzzyCompare(decoded.yaw, 1.5f));
    QVERIFY(qFuzzyCompare(decoded.pitch, 0.25f));
}

void MAVLinkSynthesizerTest::_omittedFieldKeepsPreviousValue_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> first;
    first.insert(QStringLiteral("ATTITUDE.yaw"), 1.5);
    first.insert(QStringLiteral("ATTITUDE.pitch"), 0.25);
    (void) synthesizer.buildTelemetryMessages(first);

    // Second packet carries only yaw; pitch must survive from the first.
    QHash<QString, QVariant> second;
    second.insert(QStringLiteral("ATTITUDE.yaw"), 2.0);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(second), MAVLINK_MSG_ID_ATTITUDE, message));

    mavlink_attitude_t decoded{};
    mavlink_msg_attitude_decode(&message, &decoded);
    QVERIFY(qFuzzyCompare(decoded.yaw, 2.0f));
    QVERIFY(qFuzzyCompare(decoded.pitch, 0.25f));
}

void MAVLinkSynthesizerTest::_emptyMappingProducesNoMessages_test()
{
    // Deliberately a profile whose telemetry.mavlink section maps nothing at all -- not
    // the shared _profileJson(), which does map GLOBAL_POSITION_INT/ATTITUDE and would
    // therefore still emit those two messages (carrying fallback/zero values) even for
    // an empty packet. Which MAVLink messages get built is decided once, from what the
    // profile declares, not from what any single packet happens to carry.
    static const QByteArray noMappingProfileJson = R"({
        "name": "No Mapping", "schemaVersion": 1,
        "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 42, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": { "source": "telemetry", "mavlink": {} }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(noMappingProfileJson, nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QVERIFY(synthesizer.buildTelemetryMessages(QHash<QString, QVariant>()).isEmpty());
}

QByteArray MAVLinkSynthesizerTest::_fullProfileJson()
{
    return R"({
        "name": "Synth Full Test", "schemaVersion": 1,
        "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 42, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5002", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat":        { "from": "gps.lat", "scale": 1e7 },
                "GLOBAL_POSITION_INT.lon":        { "from": "gps.lon", "scale": 1e7 },
                "SYS_STATUS.voltage_battery":     { "from": "battery.voltage" },
                "GPS_RAW_INT.fix_type":           { "from": "gps.fixType" },
                "GPS_RAW_INT.satellites_visible": { "from": "gps.satellites" },
                "VFR_HUD.airspeed":               { "from": "airspeed" },
                "VFR_HUD.groundspeed":             { "from": "groundspeed" },
                "VFR_HUD.heading":                 { "from": "heading" },
                "VFR_HUD.throttle":                { "from": "throttle" },
                "VFR_HUD.alt":                     { "from": "altitudeMsl" },
                "VFR_HUD.climb":                   { "from": "climb" }
            }
        }
    })";
}

void MAVLinkSynthesizerTest::_buildsSysStatusFromMappedValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_fullProfileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("SYS_STATUS.voltage_battery"), 16800);
    // battery_remaining is deliberately left unmapped: this call also proves the
    // fallback path in _value() -- an unset key must fall back to the "unknown"
    // sentinel (-1) rather than 0.

    const QList<mavlink_message_t> messages = synthesizer.buildTelemetryMessages(mapped);

    mavlink_message_t message{};
    QVERIFY(findMessage(messages, MAVLINK_MSG_ID_SYS_STATUS, message));

    mavlink_sys_status_t decoded{};
    mavlink_msg_sys_status_decode(&message, &decoded);
    QCOMPARE(static_cast<int>(decoded.voltage_battery), 16800);
    QCOMPARE(static_cast<int>(decoded.battery_remaining), -1);
    QCOMPARE(static_cast<int>(decoded.current_battery), -1);

    // _fullProfileJson() maps no ATTITUDE.* field, so this profile must never produce
    // one -- proving the per-message gating actually restricts output rather than
    // every guard incidentally being true whenever any message is requested.
    mavlink_message_t unexpected{};
    QVERIFY(!findMessage(messages, MAVLINK_MSG_ID_ATTITUDE, unexpected));
}

void MAVLinkSynthesizerTest::_buildsGpsRawIntReusesGlobalPositionState_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_fullProfileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.lat"), 39.925533 * 1e7);
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.lon"), 32.866287 * 1e7);
    mapped.insert(QStringLiteral("GPS_RAW_INT.fix_type"), 3);
    mapped.insert(QStringLiteral("GPS_RAW_INT.satellites_visible"), 11);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(mapped), MAVLINK_MSG_ID_GPS_RAW_INT, message));

    mavlink_gps_raw_int_t decoded{};
    mavlink_msg_gps_raw_int_decode(&message, &decoded);
    // GPS_RAW_INT deliberately carries the same lat/lon the profile mapped into
    // GLOBAL_POSITION_INT: the profile maps position once and both messages use it.
    QCOMPARE(decoded.lat, static_cast<int32_t>(39.925533 * 1e7));
    QCOMPARE(decoded.lon, static_cast<int32_t>(32.866287 * 1e7));
    QCOMPARE(static_cast<int>(decoded.fix_type), 3);
    QCOMPARE(static_cast<int>(decoded.satellites_visible), 11);
}

void MAVLinkSynthesizerTest::_buildsVfrHudFromMappedValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_fullProfileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("VFR_HUD.airspeed"), 12.5);
    mapped.insert(QStringLiteral("VFR_HUD.groundspeed"), 11.0);
    mapped.insert(QStringLiteral("VFR_HUD.heading"), 270);
    mapped.insert(QStringLiteral("VFR_HUD.throttle"), 65);
    mapped.insert(QStringLiteral("VFR_HUD.alt"), 543.2);
    mapped.insert(QStringLiteral("VFR_HUD.climb"), 1.2);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(mapped), MAVLINK_MSG_ID_VFR_HUD, message));

    mavlink_vfr_hud_t decoded{};
    mavlink_msg_vfr_hud_decode(&message, &decoded);
    QVERIFY(qFuzzyCompare(decoded.airspeed, 12.5f));
    QVERIFY(qFuzzyCompare(decoded.groundspeed, 11.0f));
    QCOMPARE(static_cast<int>(decoded.heading), 270);
    QCOMPARE(static_cast<int>(decoded.throttle), 65);
    QVERIFY(qFuzzyCompare(decoded.alt, 543.2f));
    QVERIFY(qFuzzyCompare(decoded.climb, 1.2f));
}
