/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "UdpProtocolChannelTest.h"
#include "UdpProtocolChannel.h"

#include <QtNetwork/QUdpSocket>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

/// Binds an ephemeral port by asking the OS, so parallel test runs never collide.
static quint16 freePort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress::LocalHost, 0);
    const quint16 port = probe.localPort();
    probe.close();
    return port;
}

static ChannelConfig makeConfig(quint16 port, int timeoutMs = 0)
{
    ChannelConfig config;
    config.name      = QStringLiteral("telemetry");
    config.direction = QStringLiteral("in");
    config.transport = QStringLiteral("udp");
    config.host      = QStringLiteral("127.0.0.1");
    config.port      = port;
    config.codec     = QStringLiteral("json-datagram");
    config.timeoutMs = timeoutMs;
    config.valid     = true;
    return config;
}

void UdpProtocolChannelTest::_opensAndBindsPort_test()
{
    UdpProtocolChannel channel(makeConfig(freePort()));

    QVERIFY(channel.open());
    QVERIFY(channel.isOpen());

    channel.close();
    QVERIFY(!channel.isOpen());
}

void UdpProtocolChannelTest::_receivesDatagram_test()
{
    const quint16 port = freePort();
    UdpProtocolChannel channel(makeConfig(port));
    QVERIFY(channel.open());

    QSignalSpy spy(&channel, &ProtocolChannel::framesReceived);

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"seq": 1})"), QHostAddress::LocalHost, port);

    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toByteArray(), QByteArrayLiteral(R"({"seq": 1})"));
    QCOMPARE(channel.packetCount(), static_cast<quint64>(1));
}

void UdpProtocolChannelTest::_rejectsInvalidPort_test()
{
    ChannelConfig config = makeConfig(0);
    config.valid = false;

    UdpProtocolChannel channel(config);
    QVERIFY(!channel.open());
    QVERIFY(!channel.isOpen());
}

void UdpProtocolChannelTest::_closeStopsReceiving_test()
{
    const quint16 port = freePort();
    UdpProtocolChannel channel(makeConfig(port));
    QVERIFY(channel.open());
    channel.close();

    QSignalSpy spy(&channel, &ProtocolChannel::framesReceived);

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"seq": 2})"), QHostAddress::LocalHost, port);

    QVERIFY(!spy.wait(500));
    QCOMPARE(spy.count(), 0);
}

void UdpProtocolChannelTest::_emitsTimeoutWhenSilent_test()
{
    UdpProtocolChannel channel(makeConfig(freePort(), 300));
    QVERIFY(channel.open());

    QSignalSpy spy(&channel, &ProtocolChannel::timedOut);

    QVERIFY(spy.wait(3000));
    QVERIFY(spy.count() >= 1);
}

// The five tests below were added on top of the original five to close branches those
// left unexercised: calling open() a second time, the "out" direction / send() path,
// the port-only half of the invalid-config check, a real bind failure, and the timeout
// latch's single-fire-then-recoverable behavior.

/// open() must be a no-op (not a rebind, not a duplicate readyRead connection) the
/// second time it is called on an already-open channel.
void UdpProtocolChannelTest::_secondOpenIsIdempotent_test()
{
    const quint16 port = freePort();
    UdpProtocolChannel channel(makeConfig(port));
    QVERIFY(channel.open());

    QSignalSpy errorSpy(&channel, &ProtocolChannel::errorOccurred);
    QVERIFY(channel.open());
    QVERIFY(channel.isOpen());
    QCOMPARE(errorSpy.count(), 0);

    // If the second open() had rebound or reconnected readyRead a second time, one
    // datagram would be delivered twice.
    QSignalSpy frameSpy(&channel, &ProtocolChannel::framesReceived);

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"seq": 4})"), QHostAddress::LocalHost, port);

    QVERIFY(frameSpy.wait(2000));
    QCOMPARE(frameSpy.count(), 1);
}

/// An "out" channel must not bind the local port at all: it only sends to the
/// configured host/port. Proven here by having another socket hold the port
/// exclusively (DontShareAddress) - if UdpProtocolChannel::open() tried to bind for
/// an "out" channel, that bind would fail against this exclusive holder and open()
/// would return false.
void UdpProtocolChannelTest::_outDirectionSendsWithoutBinding_test()
{
    const quint16 port = freePort();

    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, port, QUdpSocket::DontShareAddress));

    ChannelConfig config = makeConfig(port);
    config.direction = QStringLiteral("out");

    UdpProtocolChannel channel(config);
    QVERIFY(channel.open());
    QVERIFY(channel.isOpen());

    QSignalSpy receiverSpy(&receiver, &QUdpSocket::readyRead);
    QVERIFY(channel.send(QByteArrayLiteral(R"({"cmd": "fire"})")));

    QVERIFY(receiverSpy.wait(2000));
    QVERIFY(receiver.hasPendingDatagrams());

    QByteArray datagram;
    datagram.resize(static_cast<int>(receiver.pendingDatagramSize()));
    receiver.readDatagram(datagram.data(), datagram.size());
    QCOMPARE(datagram, QByteArrayLiteral(R"({"cmd": "fire"})"));

    // Nothing is wired to read on the channel's own socket for an "out" channel.
    QCOMPARE(channel.packetCount(), static_cast<quint64>(0));
}

/// _rejectsInvalidPort_test (above) sets both `valid = false` and `port = 0`, so it
/// never isolates the port-only half of `!_config.valid || (_config.port == 0)`. This
/// test marks the config valid and only zeroes the port.
void UdpProtocolChannelTest::_rejectsZeroPortEvenWhenMarkedValid_test()
{
    const ChannelConfig config = makeConfig(0);
    QVERIFY(config.valid);

    UdpProtocolChannel channel(config);
    QSignalSpy errorSpy(&channel, &ProtocolChannel::errorOccurred);

    QVERIFY(!channel.open());
    QVERIFY(!channel.isOpen());
    QCOMPARE(errorSpy.count(), 1);
}

/// 192.0.2.1 is a TEST-NET-1 address (RFC 5737): it is never assigned to a local
/// interface, so binding to it fails deterministically - unlike a port-collision
/// approach, this does not race with other processes on the machine.
void UdpProtocolChannelTest::_bindFailureEmitsError_test()
{
    ChannelConfig config = makeConfig(freePort());
    config.host = QStringLiteral("192.0.2.1");

    UdpProtocolChannel channel(config);
    QSignalSpy errorSpy(&channel, &ProtocolChannel::errorOccurred);

    QVERIFY(!channel.open());
    QVERIFY(!channel.isOpen());
    QCOMPARE(errorSpy.count(), 1);
}

/// The timeout latch must fire exactly once per silent stretch, and receiving a
/// datagram must clear it so a later silent stretch can time out again.
void UdpProtocolChannelTest::_timeoutFiresOnceThenAgainAfterRecovery_test()
{
    const quint16 port = freePort();
    UdpProtocolChannel channel(makeConfig(port, 300));
    QVERIFY(channel.open());

    QSignalSpy timeoutSpy(&channel, &ProtocolChannel::timedOut);
    QSignalSpy frameSpy(&channel, &ProtocolChannel::framesReceived);

    QVERIFY(timeoutSpy.wait(3000));
    QCOMPARE(timeoutSpy.count(), 1);

    // The check timer fires roughly every 100ms for this config; waiting through a
    // few more ticks without new data must not add a second emission.
    QTest::qWait(250);
    QCOMPARE(timeoutSpy.count(), 1);

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"seq": 5})"), QHostAddress::LocalHost, port);
    QVERIFY(frameSpy.wait(2000));

    QVERIFY(timeoutSpy.wait(3000));
    QCOMPARE(timeoutSpy.count(), 2);
}
