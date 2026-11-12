/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "NdjsonCodecTest.h"
#include "NdjsonCodec.h"

#include <QtCore/QJsonObject>
#include <QtTest/QTest>

void NdjsonCodecTest::_decodesSingleLine_test()
{
    NdjsonCodec codec;
    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral("{\"ack\":\"arm\",\"status\":\"ok\"}\n"));

    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first().value(QStringLiteral("ack")).toString(), QStringLiteral("arm"));
    QCOMPARE(frames.first().value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
}

void NdjsonCodecTest::_decodesMultipleLines_test()
{
    NdjsonCodec codec;
    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral("{\"cmd\":\"arm\"}\n{\"cmd\":\"disarm\"}\n"));

    QCOMPARE(frames.size(), 2);
    QCOMPARE(frames.at(0).value(QStringLiteral("cmd")).toString(), QStringLiteral("arm"));
    QCOMPARE(frames.at(1).value(QStringLiteral("cmd")).toString(), QStringLiteral("disarm"));
}

void NdjsonCodecTest::_handlesChunkedStream_test()
{
    NdjsonCodec codec;
    QList<QJsonObject> frames = codec.decode(QByteArrayLiteral("{\"ack\":\"set_"));
    QCOMPARE(frames.size(), 0);

    frames = codec.decode(QByteArrayLiteral("mode\",\"status\":\"ok\"}\n"));
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first().value(QStringLiteral("ack")).toString(), QStringLiteral("set_mode"));
}

void NdjsonCodecTest::_encodeAppendsNewline_test()
{
    NdjsonCodec codec;
    QJsonObject obj;
    obj[QStringLiteral("cmd")] = QStringLiteral("arm");

    const QByteArray encoded = codec.encode(obj);
    QVERIFY(encoded.endsWith('\n'));
}

/// A peer that never sends a newline must not grow the buffer without bound. The
/// runaway line is dropped and counted, and the stream recovers on the next line.
void NdjsonCodecTest::_discardsOversizedUnterminatedLine_test()
{
    NdjsonCodec codec;

    const QByteArray junk(NdjsonCodec::kMaxLineBytes + 1, 'x');
    QVERIFY(codec.decode(junk).isEmpty());
    QCOMPARE(codec.bufferedBytes(), 0);
    QCOMPARE(codec.malformedCount(), 1);

    // The tail of that runaway line is still junk up to its newline; after it, normal service.
    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral("tail\n{\"ack\":\"arm\"}\n"));
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first().value(QStringLiteral("ack")).toString(), QStringLiteral("arm"));
}
