/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "TcpProtocolChannelTest.h"
#include "TcpProtocolChannel.h"

#include <QtCore/QElapsedTimer>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

static ChannelConfig makeConfig(quint16 port, int reconnectMs = 0, int timeoutMs = 0)
{
    ChannelConfig config;
    config.name        = QStringLiteral("command");
    config.direction   = QStringLiteral("out");
    config.transport   = QStringLiteral("tcp");
    config.host        = QStringLiteral("127.0.0.1");
    config.port        = port;
    config.codec       = QStringLiteral("ndjson");
    config.reconnectMs = reconnectMs;
    config.timeoutMs   = timeoutMs;
    config.valid       = true;
    return config;
}

/// Waits for the channel to reach the server and returns the server side of the connection.
/// Spins the event loop rather than blocking in waitForNewConnection(), so the channel's
/// own timers (reconnect, timeout) keep running while we wait.
static QTcpSocket *acceptOne(QTcpServer &server)
{
    QElapsedTimer timer;
    timer.start();
    while (!server.hasPendingConnections() && (timer.elapsed() < 2000)) {
        QTest::qWait(10);
    }
    return server.nextPendingConnection();
}

void TcpProtocolChannelTest::_connectsAndReceives_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    TcpProtocolChannel channel(makeConfig(server.serverPort()));
    QVERIFY(channel.open());

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(channel.isOpen(), 2000);

    QSignalSpy spy(&channel, &ProtocolChannel::framesReceived);
    peer->write(QByteArrayLiteral("{\"ack\":\"arm\"}\n"));
    peer->flush();

    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.first().first().toByteArray(), QByteArrayLiteral("{\"ack\":\"arm\"}\n"));
}

/// Closing the channel must be final: the disconnect it causes is not a dropped
/// connection, so it must not arm the reconnect timer.
void TcpProtocolChannelTest::_closeDoesNotReconnect_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    TcpProtocolChannel channel(makeConfig(server.serverPort(), 100));
    QVERIFY(channel.open());
    QVERIFY(acceptOne(server));
    QTRY_VERIFY_WITH_TIMEOUT(channel.isOpen(), 2000);

    channel.close();
    QVERIFY(!channel.isOpen());

    // Several reconnect periods pass; no new connection may arrive.
    QTest::qWait(500);
    QVERIFY(!server.hasPendingConnections());
}

/// The far end going away is a real drop and must be recovered from.
void TcpProtocolChannelTest::_reconnectsAfterRemoteClose_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    TcpProtocolChannel channel(makeConfig(server.serverPort(), 100));
    QVERIFY(channel.open());

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(channel.isOpen(), 2000);

    peer->disconnectFromHost();
    QTRY_VERIFY_WITH_TIMEOUT(!channel.isOpen(), 2000);

    QVERIFY(acceptOne(server));
    QTRY_VERIFY_WITH_TIMEOUT(channel.isOpen(), 2000);
}

/// A silent far end produces one timedOut per silence, not one per timer tick.
void TcpProtocolChannelTest::_timeoutFiresOncePerSilence_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    TcpProtocolChannel channel(makeConfig(server.serverPort(), 0, 200));
    QSignalSpy spy(&channel, &ProtocolChannel::timedOut);
    QVERIFY(channel.open());

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(channel.isOpen(), 2000);

    QTest::qWait(1000);
    QCOMPARE(spy.count(), 1);

    // Data ends the silence; the next silence is reported again.
    peer->write(QByteArrayLiteral("{}\n"));
    peer->flush();
    QTest::qWait(1000);
    QCOMPARE(spy.count(), 2);
}
