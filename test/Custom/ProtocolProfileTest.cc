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

#include <QtCore/QFile>
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

void ProtocolProfileTest::_rejectsNonObjectRoot_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(QByteArrayLiteral("[1, 2, 3]"), &error));
    QVERIFY(!error.isEmpty());
}

void ProtocolProfileTest::_rejectsNonPositiveHeartbeat_test()
{
    const QByteArray json = R"({ "name": "Test", "vehicle": { "heartbeatHz": 0 } })";

    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(json, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("heartbeatHz")));
}

void ProtocolProfileTest::_rejectsProfileWithNoChannels_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(QByteArrayLiteral(R"({"name": "Test"})"), &error));
    QVERIFY(!error.isEmpty());
}

void ProtocolProfileTest::_rejectsChannelWithInvalidDirection_test()
{
    const QByteArray json = R"({
        "name": "Test",
        "channels": { "telemetry": { "dir": "sideways", "bind": "0.0.0.0:5001" } }
    })";

    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(json, &error));
    QVERIFY(!error.isEmpty());
}

void ProtocolProfileTest::_rejectsChannelWithUnparsableEndpoint_test()
{
    const QByteArray missingPort = R"({
        "name": "Test",
        "channels": { "telemetry": { "dir": "in" } }
    })";
    const QByteArray outOfRangePort = R"({
        "name": "Test",
        "channels": { "telemetry": { "dir": "in", "bind": "0.0.0.0:99999" } }
    })";

    ProtocolProfile missingPortProfile;
    QString missingPortError;
    QVERIFY(!missingPortProfile.loadFromJson(missingPort, &missingPortError));
    QVERIFY(!missingPortError.isEmpty());

    ProtocolProfile outOfRangeProfile;
    QString outOfRangeError;
    QVERIFY(!outOfRangeProfile.loadFromJson(outOfRangePort, &outOfRangeError));
    QVERIFY(!outOfRangeError.isEmpty());
}

void ProtocolProfileTest::_rejectsUnknownTelemetrySource_test()
{
    const QByteArray json = R"({
        "name": "Test",
        "channels": { "telemetry": { "dir": "in", "bind": "0.0.0.0:5001" } },
        "telemetry": { "source": "nonexistent" }
    })";

    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(json, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("telemetry.source")));
}

void ProtocolProfileTest::_rejectsMappingRuleMissingFrom_test()
{
    const QByteArray json = R"({
        "name": "Test",
        "channels": { "telemetry": { "dir": "in", "bind": "0.0.0.0:5001" } },
        "telemetry": { "source": "telemetry", "mavlink": { "ATTITUDE.yaw": {} } }
    })";

    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(json, &error));
    QVERIFY(!error.isEmpty());
}

void ProtocolProfileTest::_rejectsMappingRuleWithUnsupportedUnit_test()
{
    const QByteArray json = R"({
        "name": "Test",
        "channels": { "telemetry": { "dir": "in", "bind": "0.0.0.0:5001" } },
        "telemetry": { "source": "telemetry",
                       "mavlink": { "ATTITUDE.yaw": { "from": "yaw", "unit": "furlong2parsec" } } }
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

void ProtocolProfileTest::_shippedCelikKubbeProfileIsValid_test()
{
    QFile file(QStringLiteral(":/Custom/protocols/celik-kubbe.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Shipped profile is missing from the Qt resources");

    ProtocolProfile profile;
    QString error;
    QVERIFY2(profile.loadFromJson(file.readAll(), &error), qPrintable(error));

    QCOMPARE(profile.channelNames().size(), 3);
    QCOMPARE(profile.channel(QStringLiteral("telemetry")).port, static_cast<quint16>(5001));
    QCOMPARE(profile.mavlinkRules().size(), 9);
    QCOMPARE(profile.flightModeValues().size(), 3);
    QCOMPARE(profile.passthroughFields().size(), 14);

    // A fixed emplacement has no altitude of its own and no ground speed. The turret's
    // "altitude" field is its estimate of the TARGET's altitude and "speed" is the
    // mount's slew rate in deg/s, so neither may be mapped onto GLOBAL_POSITION_INT or
    // VFR_HUD — doing so made the HUD assert two things that are not true of the
    // emplacement. They belong in passthrough, labelled correctly by the turret panel.
    const QHash<QString, MappingRule> rules = profile.mavlinkRules();
    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
        QVERIFY2(!it.key().startsWith(QStringLiteral("VFR_HUD.")),
                 qPrintable(QStringLiteral("VFR_HUD must not be mapped for a fixed mount: %1").arg(it.key())));
        QVERIFY2(it.key() != QStringLiteral("GLOBAL_POSITION_INT.relative_alt"),
                 "relative_alt must not carry the target's altitude");
    }

    const QStringList passthrough = profile.passthroughFields();
    QVERIFY(passthrough.contains(QStringLiteral("altitude")));
    QVERIFY(passthrough.contains(QStringLiteral("speed")));
}

/// Builds a profile with a telemetry channel plus whatever extra channels/commands a test
/// needs, so each test only spells out the part it is about.
static QByteArray profileWith(const QByteArray &extraChannels, const QByteArray &commands,
                              const QByteArray &mavType = QByteArrayLiteral("MAV_TYPE_GENERIC"))
{
    return QByteArrayLiteral(R"({
        "name": "Cmd", "vehicle": { "mavType": ")") + mavType + QByteArrayLiteral(R"(" },
        "channels": {
            "telemetry": { "dir": "in", "transport": "udp", "bind": "0.0.0.0:5001", "codec": "json-datagram" })")
        + extraChannels + QByteArrayLiteral(R"(
        },
        "telemetry": { "source": "telemetry", "mavlink": {} })")
        + (commands.isEmpty() ? QByteArray() : (QByteArrayLiteral(R"(, "commands": )") + commands))
        + QByteArrayLiteral("}");
}

static const QByteArray kCommandChannel = QByteArrayLiteral(R"(,
    "command": { "dir": "out", "transport": "tcp", "connect": "127.0.0.1:5000", "codec": "ndjson" })");

void ProtocolProfileTest::_parsesCommandTemplates_test()
{
    ProtocolProfile profile;
    QString error;
    QVERIFY2(profile.loadFromJson(profileWith(kCommandChannel, R"({
        "arm":     { "channel": "command", "template": { "cmd": "arm", "params": {} } },
        "setMode": { "channel": "command", "template": { "cmd": "set_mode", "params": { "mode": "{value}" } } }
    })"), &error), qPrintable(error));

    QVERIFY(profile.hasCommandSection());
    QCOMPARE(profile.commands().size(), 2);
    const CommandTemplate setMode = profile.commands().value(QStringLiteral("setMode"));
    QCOMPARE(setMode.channel, QStringLiteral("command"));
    QCOMPARE(setMode.payload.value(QStringLiteral("cmd")).toString(), QStringLiteral("set_mode"));
    QCOMPARE(setMode.payload.value(QStringLiteral("params")).toObject().value(QStringLiteral("mode")).toString(),
             QStringLiteral("{value}"));
}

void ProtocolProfileTest::_rejectsCommandOnUnknownChannel_test()
{
    ProtocolProfile profile;
    QString error;
    QVERIFY(!profile.loadFromJson(profileWith(kCommandChannel, R"({
        "arm": { "channel": "nowhere", "template": { "cmd": "arm" } }
    })"), &error));
    QVERIFY(error.contains(QStringLiteral("nowhere")));
}

void ProtocolProfileTest::_rejectsCommandOnInboundChannel_test()
{
    ProtocolProfile profile;
    QString error;
    QVERIFY(!profile.loadFromJson(profileWith(kCommandChannel, R"({
        "arm": { "channel": "telemetry", "template": { "cmd": "arm" } }
    })"), &error));
    QVERIFY(error.contains(QStringLiteral("arm")));
}

void ProtocolProfileTest::_rejectsCommandWithoutTemplateObject_test()
{
    ProtocolProfile profile;
    QString error;
    QVERIFY(!profile.loadFromJson(profileWith(kCommandChannel, R"({
        "arm": { "channel": "command", "template": "arm" }
    })"), &error));
    QVERIFY(error.contains(QStringLiteral("template")));
}

void ProtocolProfileTest::_parsesJoystickChannelOptions_test()
{
    ProtocolProfile profile;
    QString error;
    QVERIFY2(profile.loadFromJson(profileWith(QByteArrayLiteral(R"(,
        "joystick": { "dir": "out", "transport": "udp", "connect": "127.0.0.1:5005", "codec": "csv",
                      "rateHz": 25, "template": "{pitch:.2f};{yaw:.2f}", "failsafe": "0;0" })"), QByteArray()), &error),
             qPrintable(error));

    QVERIFY(!profile.hasCommandSection());
    const ChannelConfig joystick = profile.channel(QStringLiteral("joystick"));
    QCOMPARE(joystick.rateHz, 25);
    QCOMPARE(joystick.frameTemplate, QStringLiteral("{pitch:.2f};{yaw:.2f}"));
    QCOMPARE(joystick.failsafe, QStringLiteral("0;0"));
}

void ProtocolProfileTest::_resolvesMavType_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(profileWith(QByteArray(), QByteArray(), QByteArrayLiteral("MAV_TYPE_GROUND_ROVER")), nullptr));
    QCOMPARE(profile.mavType(), 10);    // MAV_TYPE_GROUND_ROVER

    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));
    QCOMPARE(profile.mavType(), 0);     // MAV_TYPE_GENERIC
}

void ProtocolProfileTest::_rejectsUnknownMavType_test()
{
    ProtocolProfile profile;
    QString error;
    QVERIFY(!profile.loadFromJson(profileWith(QByteArray(), QByteArray(), QByteArrayLiteral("MAV_TYPE_TOASTER")), &error));
    QVERIFY(error.contains(QStringLiteral("MAV_TYPE_TOASTER")));
}
