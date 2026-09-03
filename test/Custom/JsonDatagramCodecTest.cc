/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "JsonDatagramCodecTest.h"
#include "JsonDatagramCodec.h"

#include <QtCore/QJsonObject>
#include <QtTest/QTest>

void JsonDatagramCodecTest::_decodesSingleObject_test()
{
    JsonDatagramCodec codec;

    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral(R"({"seq": 7, "battery": 85.5})"));

    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first().value(QStringLiteral("seq")).toInt(), 7);
    QCOMPARE(frames.first().value(QStringLiteral("battery")).toDouble(), 85.5);
}

void JsonDatagramCodecTest::_decodesNestedObject_test()
{
    JsonDatagramCodec codec;

    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral(R"({"gps": {"lat": 39.9, "fix": true}})"));

    QCOMPARE(frames.size(), 1);
    const QJsonObject gps = frames.first().value(QStringLiteral("gps")).toObject();
    QCOMPARE(gps.value(QStringLiteral("lat")).toDouble(), 39.9);
    QCOMPARE(gps.value(QStringLiteral("fix")).toBool(), true);
}

void JsonDatagramCodecTest::_malformedInputYieldsNothing_test()
{
    JsonDatagramCodec codec;

    QVERIFY(codec.decode(QByteArrayLiteral("{ this is not json")).isEmpty());
    QVERIFY(codec.decode(QByteArrayLiteral("\x01\x02\x03")).isEmpty());
}

void JsonDatagramCodecTest::_malformedInputIsCounted_test()
{
    JsonDatagramCodec codec;
    QCOMPARE(codec.malformedCount(), 0);

    (void) codec.decode(QByteArrayLiteral("{ bad"));
    (void) codec.decode(QByteArrayLiteral("{ also bad"));
    QCOMPARE(codec.malformedCount(), 2);

    (void) codec.decode(QByteArrayLiteral(R"({"ok": 1})"));
    QCOMPARE(codec.malformedCount(), 2);
}

void JsonDatagramCodecTest::_nonObjectRootYieldsNothing_test()
{
    JsonDatagramCodec codec;

    QVERIFY(codec.decode(QByteArrayLiteral("[1, 2, 3]")).isEmpty());
    QVERIFY(codec.decode(QByteArrayLiteral("42")).isEmpty());
}

void JsonDatagramCodecTest::_emptyInputYieldsNothing_test()
{
    JsonDatagramCodec codec;

    QVERIFY(codec.decode(QByteArray()).isEmpty());
    QCOMPARE(codec.malformedCount(), 0); // empty is not malformed, just nothing
}

void JsonDatagramCodecTest::_encodeRoundTrips_test()
{
    JsonDatagramCodec codec;

    QJsonObject original;
    original.insert(QStringLiteral("cmd"), QStringLiteral("arm"));
    original.insert(QStringLiteral("id"), 3);

    const QList<QJsonObject> frames = codec.decode(codec.encode(original));

    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first(), original);
}
