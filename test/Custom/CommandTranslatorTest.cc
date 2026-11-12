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
