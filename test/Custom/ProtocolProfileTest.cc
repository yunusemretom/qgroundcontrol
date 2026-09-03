/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ProtocolProfileTest.h"
#include "ProtocolProfile.h"

#include <QtTest/QTest>

QByteArray ProtocolProfileTest::_minimalProfile()
{
    return R"({
        "name": "Test Profile",
        "schemaVersion": 1,
        "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 42, "compId": 1, "heartbeatHz": 2 },
        "channels": {
            "telemetry": { "dir": "in", "transport": "udp", "bind": "0.0.0.0:5001",
                           "codec": "json-datagram", "timeoutMs": 3000 }
        },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat": { "from": "gps.lat", "scale": 1e7 },
                "ATTITUDE.yaw":            { "from": "yaw", "unit": "deg2rad" },
                "GPS_RAW_INT.fix_type":    { "from": "gps.fix", "map": { "true": 3, "false": 0 } },
                "VFR_HUD.groundspeed":     { "from": "speed", "default": 0 }
            },
            "flightMode": { "from": "mode", "values": { "ARAMA": 0, "TAKIP": 1, "KILITLI": 2 } },
            "passthrough": [ "tracking", "shots" ]
        }
    })";
}

void ProtocolProfileTest::_loadsValidProfile_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(profile.loadFromJson(_minimalProfile(), &error));
    QVERIFY(error.isEmpty());
    QVERIFY(profile.isValid());
    QCOMPARE(profile.name(), QStringLiteral("Test Profile"));
    QCOMPARE(profile.sysId(), 42);
    QCOMPARE(profile.compId(), 1);
    QCOMPARE(profile.heartbeatHz(), 2);
}

void ProtocolProfileTest::_rejectsMalformedJson_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(QByteArrayLiteral("{ not json"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!profile.isValid());
}

void ProtocolProfileTest::_rejectsProfileWithoutName_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(QByteArrayLiteral(R"({"schemaVersion": 1})"), &error));
    QVERIFY(error.contains(QStringLiteral("name")));
}

void ProtocolProfileTest::_rejectsRuleWithBothScaleAndUnit_test()
{
    const QByteArray json = R"({
        "name": "Bad", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": { "source": "telemetry",
                       "mavlink": { "ATTITUDE.yaw": { "from": "yaw", "scale": 2, "unit": "deg2rad" } } }
    })";

    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(json, &error));
    QVERIFY(!error.isEmpty());
}

void ProtocolProfileTest::_parsesChannelEndpoint_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));

    const ChannelConfig channel = profile.channel(QStringLiteral("telemetry"));
    QCOMPARE(channel.name,      QStringLiteral("telemetry"));
    QCOMPARE(channel.direction, QStringLiteral("in"));
    QCOMPARE(channel.transport, QStringLiteral("udp"));
    QCOMPARE(channel.host,      QStringLiteral("0.0.0.0"));
    QCOMPARE(channel.port,      static_cast<quint16>(5001));
    QCOMPARE(channel.codec,     QStringLiteral("json-datagram"));
    QCOMPARE(channel.timeoutMs, 3000);

    QCOMPARE(profile.channelNames().size(), 1);
}

void ProtocolProfileTest::_parsesMappingRuleKinds_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));

    const QHash<QString, MappingRule> rules = profile.mavlinkRules();
    QCOMPARE(rules.size(), 4);

    const MappingRule scaleRule = rules.value(QStringLiteral("GLOBAL_POSITION_INT.lat"));
    QCOMPARE(scaleRule.from,  QStringLiteral("gps.lat"));
    QCOMPARE(scaleRule.kind,  MappingRule::Scale);
    QCOMPARE(scaleRule.scale, 1e7);

    const MappingRule unitRule = rules.value(QStringLiteral("ATTITUDE.yaw"));
    QCOMPARE(unitRule.kind, MappingRule::Unit);
    QCOMPARE(unitRule.unit, QStringLiteral("deg2rad"));

    const MappingRule mapRule = rules.value(QStringLiteral("GPS_RAW_INT.fix_type"));
    QCOMPARE(mapRule.kind, MappingRule::Map);
    QCOMPARE(mapRule.valueMap.value(QStringLiteral("true")).toInt(), 3);

    const MappingRule defaultRule = rules.value(QStringLiteral("VFR_HUD.groundspeed"));
    QCOMPARE(defaultRule.kind, MappingRule::None);
    QVERIFY(defaultRule.hasDefault);
    QCOMPARE(defaultRule.defaultValue.toInt(), 0);
}

void ProtocolProfileTest::_parsesFlightModeValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));

    QCOMPARE(profile.flightModeField(), QStringLiteral("mode"));

    const QMap<QString, int> modes = profile.flightModeValues();
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.value(QStringLiteral("TAKIP")), 1);
}

void ProtocolProfileTest::_missingOptionalSectionsAreEmpty_test()
{
    const QByteArray json = R"({
        "name": "Sparse", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": { "source": "telemetry", "mavlink": {} }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(json, nullptr));
    QVERIFY(profile.passthroughFields().isEmpty());
    QVERIFY(profile.flightModeField().isEmpty());
    QVERIFY(profile.flightModeValues().isEmpty());
}
