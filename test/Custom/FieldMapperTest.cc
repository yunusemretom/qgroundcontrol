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
