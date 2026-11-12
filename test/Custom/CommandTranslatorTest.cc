/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CommandTranslatorTest.h"
#include "CommandTranslator.h"

#include <QtCore/QJsonObject>
#include <QtTest/QTest>

void CommandTranslatorTest::_translatesArmAndDisarm_test()
{
    ProtocolProfile profile;
    CommandTranslator translator(profile);

    const QJsonObject armObj = translator.translateArm(true);
    QCOMPARE(armObj.value(QStringLiteral("cmd")).toString(), QStringLiteral("arm"));

    const QJsonObject disarmObj = translator.translateArm(false);
    QCOMPARE(disarmObj.value(QStringLiteral("cmd")).toString(), QStringLiteral("disarm"));
}

void CommandTranslatorTest::_translatesSetMode_test()
{
    ProtocolProfile profile;
    CommandTranslator translator(profile);

    const QJsonObject modeObj = translator.translateSetMode(QStringLiteral("TAKIP"));
    QCOMPARE(modeObj.value(QStringLiteral("cmd")).toString(), QStringLiteral("set_mode"));
    const QJsonObject params = modeObj.value(QStringLiteral("params")).toObject();
    QCOMPARE(params.value(QStringLiteral("mode")).toString(), QStringLiteral("TAKIP"));
}

void CommandTranslatorTest::_parsesAckResponse_test()
{
    ProtocolProfile profile;
    CommandTranslator translator(profile);

    QJsonObject ackObj;
    ackObj[QStringLiteral("ack")] = QStringLiteral("arm");
    ackObj[QStringLiteral("status")] = QStringLiteral("ok");

    QString cmdName;
    bool success = false;
    QVERIFY(translator.parseAck(ackObj, cmdName, success));
    QCOMPARE(cmdName, QStringLiteral("arm"));
    QVERIFY(success);
}

static ProtocolProfile profileWithCommands(const QByteArray &commands)
{
    const QByteArray json = QByteArrayLiteral(R"({
        "name": "Cmd",
        "channels": {
            "telemetry": { "dir": "in",  "transport": "udp", "bind": "0.0.0.0:5001", "codec": "json-datagram" },
            "command":   { "dir": "out", "transport": "tcp", "connect": "127.0.0.1:5000", "codec": "ndjson" },
            "aux":       { "dir": "out", "transport": "tcp", "connect": "127.0.0.1:5010", "codec": "ndjson" }
        },
        "telemetry": { "source": "telemetry", "mavlink": {} },
        "commands": )") + commands + QByteArrayLiteral("}");

    ProtocolProfile profile;
    QString error;
    if (!profile.loadFromJson(json, &error)) {
        qWarning() << "test profile rejected:" << error;
    }
    return profile;
}

void CommandTranslatorTest::_usesProfileTemplate_test()
{
    CommandTranslator translator(profileWithCommands(R"({
        "arm": { "channel": "command", "template": { "cmd": "ARM_NOW", "params": { "force": true } } }
    })"));

    const QJsonObject armObj = translator.translateArm(true);
    QCOMPARE(armObj.value(QStringLiteral("cmd")).toString(), QStringLiteral("ARM_NOW"));
    QCOMPARE(armObj.value(QStringLiteral("params")).toObject().value(QStringLiteral("force")).toBool(), true);
}

void CommandTranslatorTest::_substitutesValuePlaceholder_test()
{
    CommandTranslator translator(profileWithCommands(R"({
        "setMode": { "channel": "command",
                     "template": { "cmd": "set_mode", "params": { "mode": "{value}", "label": "mode={value}" } } }
    })"));

    const QJsonObject params = translator.translateSetMode(QStringLiteral("KILITLI"))
            .value(QStringLiteral("params")).toObject();
    QCOMPARE(params.value(QStringLiteral("mode")).toString(), QStringLiteral("KILITLI"));
    QCOMPARE(params.value(QStringLiteral("label")).toString(), QStringLiteral("mode=KILITLI"));
}

void CommandTranslatorTest::_addsIncrementingIdAndTimestamp_test()
{
    ProtocolProfile profile;
    CommandTranslator translator(profile);

    const QJsonObject first  = translator.translateArm(true);
    const QJsonObject second = translator.translateArm(false);

    QVERIFY(first.value(QStringLiteral("id")).isDouble());
    QCOMPARE(second.value(QStringLiteral("id")).toInt(), first.value(QStringLiteral("id")).toInt() + 1);
    QVERIFY(first.value(QStringLiteral("timestamp")).toDouble() > 0);
}

void CommandTranslatorTest::_keepsIdAndTimestampFromTemplate_test()
{
    CommandTranslator translator(profileWithCommands(R"({
        "arm": { "channel": "command", "template": { "cmd": "arm", "id": "fixed", "timestamp": 7 } }
    })"));

    const QJsonObject armObj = translator.translateArm(true);
    QCOMPARE(armObj.value(QStringLiteral("id")).toString(), QStringLiteral("fixed"));
    QCOMPARE(armObj.value(QStringLiteral("timestamp")).toInt(), 7);
}

void CommandTranslatorTest::_matchesAckById_test()
{
    ProtocolProfile profile;
    CommandTranslator translator(profile);

    (void) translator.translateArm(true);
    const QJsonObject modeObj = translator.translateSetMode(QStringLiteral("TAKIP"));

    QJsonObject ackObj;
    ackObj[QStringLiteral("ack")] = modeObj.value(QStringLiteral("id"));
    ackObj[QStringLiteral("status")] = QStringLiteral("ok");

    QString cmdName;
    bool success = false;
    QVERIFY(translator.parseAck(ackObj, cmdName, success));
    QCOMPARE(cmdName, QStringLiteral("set_mode"));
    QVERIFY(success);
}

void CommandTranslatorTest::_reportsFailedAckStatus_test()
{
    ProtocolProfile profile;
    CommandTranslator translator(profile);

    QJsonObject ackObj;
    ackObj[QStringLiteral("ack")] = QStringLiteral("arm");
    ackObj[QStringLiteral("status")] = QStringLiteral("error");

    QString cmdName;
    bool success = true;
    QVERIFY(translator.parseAck(ackObj, cmdName, success));
    QVERIFY(!success);
}

void CommandTranslatorTest::_omittedCommandIsUnsupported_test()
{
    CommandTranslator translator(profileWithCommands(R"({
        "arm": { "channel": "command", "template": { "cmd": "arm" } }
    })"));

    QVERIFY(translator.isSupported(QStringLiteral("arm")));
    QVERIFY(!translator.isSupported(QStringLiteral("setMode")));
    QVERIFY(translator.translateSetMode(QStringLiteral("TAKIP")).isEmpty());
}

void CommandTranslatorTest::_routesCommandToItsChannel_test()
{
    CommandTranslator translator(profileWithCommands(R"({
        "arm":    { "channel": "aux",     "template": { "cmd": "arm" } },
        "disarm": { "channel": "command", "template": { "cmd": "disarm" } }
    })"));

    QCOMPARE(translator.channelFor(QStringLiteral("arm")), QStringLiteral("aux"));
    QCOMPARE(translator.channelFor(QStringLiteral("disarm")), QStringLiteral("command"));

    ProtocolProfile bare;
    CommandTranslator fallback(bare);
    QCOMPARE(fallback.channelFor(QStringLiteral("arm")), QStringLiteral("command"));
}
