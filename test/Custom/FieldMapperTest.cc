/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "FieldMapperTest.h"
#include "FieldMapper.h"
#include "ProtocolProfile.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtMath>
#include <QtTest/QTest>

QByteArray FieldMapperTest::_profileJson()
{
    return R"({
        "name": "Mapper Test", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat": { "from": "gps.lat", "scale": 1e7 },
                "ATTITUDE.yaw":            { "from": "yaw", "unit": "deg2rad" },
                "GPS_RAW_INT.fix_type":    { "from": "gps.fix", "map": { "true": 3, "false": 0 } },
                "VFR_HUD.groundspeed":     { "from": "speed", "default": 0 },
                "SYS_STATUS.battery_remaining": { "from": "battery" }
            },
            "flightMode": { "from": "mode", "values": { "ARAMA": 0, "TAKIP": 1, "KILITLI": 2 } },
            "passthrough": [ "tracking", "shots", "target" ]
        }
    })";
}

static QJsonObject packetFrom(const char *json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

void FieldMapperTest::_valueAtPathFindsNestedValue_test()
{
    const QJsonObject packet = packetFrom(R"({"gps": {"lat": 39.925533}})");

    QCOMPARE(FieldMapper::valueAtPath(packet, QStringLiteral("gps.lat")).toDouble(), 39.925533);
}

void FieldMapperTest::_valueAtPathMissingReturnsInvalid_test()
{
    const QJsonObject packet = packetFrom(R"({"gps": {"lat": 1.0}})");

    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("gps.lon")).isValid());
    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("nope")).isValid());
    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("gps.lat.deeper")).isValid());
}

void FieldMapperTest::_valueAtPathThroughNullReturnsInvalid_test()
{
    const QJsonObject packet = packetFrom(R"({"target": null})");

    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("target.range_m")).isValid());
}

void FieldMapperTest::_appliesScale_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"gps": {"lat": 39.925533}})"));

    QVERIFY(mapped.contains(QStringLiteral("GLOBAL_POSITION_INT.lat")));
    QCOMPARE(mapped.value(QStringLiteral("GLOBAL_POSITION_INT.lat")).toDouble(), 39.925533 * 1e7);
}

void FieldMapperTest::_appliesDegToRad_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"yaw": 180.0})"));

    QVERIFY(qFuzzyCompare(mapped.value(QStringLiteral("ATTITUDE.yaw")).toDouble() + 1.0, M_PI + 1.0));
}

void FieldMapperTest::_appliesDiscreteMap_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    QCOMPARE(mapper.mapTelemetry(packetFrom(R"({"gps": {"fix": true}})"))
                .value(QStringLiteral("GPS_RAW_INT.fix_type")).toInt(), 3);
    QCOMPARE(mapper.mapTelemetry(packetFrom(R"({"gps": {"fix": false}})"))
                .value(QStringLiteral("GPS_RAW_INT.fix_type")).toInt(), 0);
}

void FieldMapperTest::_usesDefaultWhenSourceMissing_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"battery": 50})"));

    QVERIFY(mapped.contains(QStringLiteral("VFR_HUD.groundspeed")));
    QCOMPARE(mapped.value(QStringLiteral("VFR_HUD.groundspeed")).toInt(), 0);
}

void FieldMapperTest::_omitsFieldWhenSourceMissingAndNoDefault_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    // "battery" has no default; a packet without it must not produce the key at all,
    // so the synthesizer leaves the previous value in place.
    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"yaw": 0.0})"));

    QVERIFY(!mapped.contains(QStringLiteral("SYS_STATUS.battery_remaining")));
}

void FieldMapperTest::_nullTargetDoesNotCrash_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QJsonObject packet = packetFrom(R"({"target": null, "tracking": false, "shots": 0})");

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packet);
    Q_UNUSED(mapped);

    const QVariantMap through = mapper.passthrough(packet);
    QVERIFY(through.contains(QStringLiteral("target")));
    QVERIFY(through.value(QStringLiteral("target")).isNull());
}

void FieldMapperTest::_passthroughCopiesDeclaredFields_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QVariantMap through = mapper.passthrough(
        packetFrom(R"({"tracking": true, "shots": 3, "fps": 30, "target": {"track_id": 7}})"));

    QCOMPARE(through.value(QStringLiteral("tracking")).toBool(), true);
    QCOMPARE(through.value(QStringLiteral("shots")).toInt(), 3);
    QCOMPARE(through.value(QStringLiteral("target")).toMap().value(QStringLiteral("track_id")).toInt(), 7);
    // "fps" is not declared in passthrough, so it must not leak through
    QVERIFY(!through.contains(QStringLiteral("fps")));
}

void FieldMapperTest::_flightModeMapsKnownValue_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    bool found = false;
    QCOMPARE(mapper.flightModeValue(packetFrom(R"({"mode": "KILITLI"})"), &found), 2);
    QVERIFY(found);
}

void FieldMapperTest::_flightModeUnknownValueReportsNotFound_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    bool found = true;
    (void) mapper.flightModeValue(packetFrom(R"({"mode": "BILINMEYEN"})"), &found);
    QVERIFY(!found);

    found = true;
    (void) mapper.flightModeValue(packetFrom(R"({"battery": 1})"), &found);
    QVERIFY(!found);
}

void FieldMapperTest::_appliesRadToDeg_test()
{
    // Test rad2deg unit conversion using a custom profile with rad2deg unit
    QByteArray customProfile = R"({
        "name": "Rad2Deg Test", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "ATTITUDE.pitch": { "from": "pitch_rad", "unit": "rad2deg" }
            },
            "passthrough": []
        }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(customProfile, nullptr));
    const FieldMapper mapper(profile);

    // M_PI/2 radians = 90 degrees
    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"pitch_rad": 1.5707963})"));

    double result = mapped.value(QStringLiteral("ATTITUDE.pitch")).toDouble();
    QVERIFY(result > 89.0 && result < 91.0);  // Approximately 90 degrees
}

void FieldMapperTest::_appliesMetersToCentimeters_test()
{
    // Test m2cm unit conversion using a custom profile
    QByteArray customProfile = R"({
        "name": "M2CM Test", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "DISTANCE_SENSOR.current_distance": { "from": "range_m", "unit": "m2cm" }
            },
            "passthrough": []
        }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(customProfile, nullptr));
    const FieldMapper mapper(profile);

    // 1.5 meters = 150 centimeters
    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"range_m": 1.5})"));

    QCOMPARE(mapped.value(QStringLiteral("DISTANCE_SENSOR.current_distance")).toDouble(), 150.0);
}

void FieldMapperTest::_appliesCentimetersToMeters_test()
{
    // Test cm2m unit conversion using a custom profile
    QByteArray customProfile = R"({
        "name": "CM2M Test", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "LOCAL_POSITION_NED.x": { "from": "pos_cm", "unit": "cm2m" }
            },
            "passthrough": []
        }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(customProfile, nullptr));
    const FieldMapper mapper(profile);

    // 250 centimeters = 2.5 meters
    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"pos_cm": 250.0})"));

    QCOMPARE(mapped.value(QStringLiteral("LOCAL_POSITION_NED.x")).toDouble(), 2.5);
}

void FieldMapperTest::_appliesDiscreteMapWithNumericValue_test()
{
    // Test that numeric JSON values are converted to strings for map lookup.
    // Uses a rule with numeric string keys to verify the toString() branch runs.
    // If the implementation incorrectly returned the raw number instead of the
    // mapped value, this assertion would fail (e.g., returning 2 instead of 8).
    QByteArray customProfile = R"({
        "name": "Numeric Map Test", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "SYS_STATUS.system_status": { "from": "status_code", "map": { "1": 7, "2": 8, "3": 9 } }
            },
            "passthrough": []
        }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(customProfile, nullptr));
    const FieldMapper mapper(profile);

    // Send numeric 2; should map to 8, not return 2.
    // This proves the raw value was stringified and looked up in the map.
    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(
        packetFrom(R"({"status_code": 2})"));

    QCOMPARE(mapped.value(QStringLiteral("SYS_STATUS.system_status")).toInt(), 8);
}

void FieldMapperTest::_flightModeWithNullPointerDoesNotCrash_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    // Call flightModeValue with nullptr for found parameter
    // Should not crash and should return a value
    int result = mapper.flightModeValue(packetFrom(R"({"mode": "KILITLI"})"), nullptr);
    QCOMPARE(result, 2);
}
