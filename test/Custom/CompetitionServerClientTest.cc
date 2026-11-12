/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CompetitionServerClientTest.h"
#include "CompetitionServerClient.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

namespace {

/// Waits for the client without blocking the event loop, so its own timers keep running.
QTcpSocket *acceptOne(QTcpServer &server, int timeoutMs = 2000)
{
    QElapsedTimer timer;
    timer.start();
    while (!server.hasPendingConnections() && (timer.elapsed() < timeoutMs)) {
        QTest::qWait(10);
    }
    return server.nextPendingConnection();
}

/// Reads NDJSON lines the client sent until one has `type`, or gives up.
QJsonObject readUntilType(QTcpSocket *peer, const QString &type, int timeoutMs = 2000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        while (peer->canReadLine()) {
            const QJsonObject obj = QJsonDocument::fromJson(peer->readLine()).object();
            if (obj.value(QStringLiteral("type")).toString() == type) {
                return obj;
            }
        }
        QTest::qWait(10);
    }
    return QJsonObject();
}

void configure(CompetitionServerClient &client, const QTcpServer &server)
{
    client.setAutoReconnect(false);
    client.setHost(QStringLiteral("127.0.0.1"));
    client.setPort(server.serverPort());
}

} // namespace

void CompetitionServerClientTest::_sendsHelloAndAuthOnConnect_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    CompetitionServerClient client;
    configure(client, server);
    client.setSysId(7);
    client.setTeamName(QStringLiteral("Pars"));
    client.setAuthEnabled(true);
    client.setUsername(QStringLiteral("pars"));
    client.setPassword(QStringLiteral("secret"));
    client.connectToServer();

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);

    const QJsonObject hello = readUntilType(peer, QStringLiteral("hello"));
    QCOMPARE(hello.value(QStringLiteral("sysId")).toInt(), 7);
    QCOMPARE(hello.value(QStringLiteral("teamName")).toString(), QStringLiteral("Pars"));

    const QJsonObject auth = readUntilType(peer, QStringLiteral("auth"));
    QCOMPARE(auth.value(QStringLiteral("username")).toString(), QStringLiteral("pars"));
}

void CompetitionServerClientTest::_parsesCompetitionPacket_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    CompetitionServerClient client;
    configure(client, server);
    client.connectToServer();

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    QSignalSpy packetSpy(&client, &CompetitionServerClient::competitionPacketReceived);
    peer->write(QByteArrayLiteral(R"({"type":"state","data":{"rivalList":[{"id":3}],"serverTimeMs":1700000000000,)"
                                  R"("lockStatus":{"active":true,"remainingSec":4}}})" "\n"));
    peer->flush();

    QVERIFY(packetSpy.wait(2000));
    QCOMPARE(client.rivalList().size(), 1);
    QVERIFY(client.serverTimeValid());
    QVERIFY(client.currentServerTimeMs() >= 1700000000000LL);
    QVERIFY(client.lockStatusActive());
    QCOMPARE(client.lockRemainingMs(), 4000);
}

void CompetitionServerClientTest::_answersPing_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    CompetitionServerClient client;
    configure(client, server);
    client.connectToServer();

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    peer->write(QByteArrayLiteral("{\"type\":\"ping\"}\n"));
    peer->flush();

    QVERIFY(!readUntilType(peer, QStringLiteral("pong")).isEmpty());
}

void CompetitionServerClientTest::_discardsOversizedUnterminatedLine_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    CompetitionServerClient client;
    configure(client, server);
    client.connectToServer();

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    peer->write(QByteArray(CompetitionServerClient::kMaxLineBytes + 1024, 'x'));
    peer->flush();
    QTRY_VERIFY_WITH_TIMEOUT(client.lastErrorString().contains(QStringLiteral("over-long")), 5000);

    // Still usable afterwards.
    QSignalSpy rivalSpy(&client, &CompetitionServerClient::rivalListChanged);
    peer->write(QByteArrayLiteral("\n{\"rivalList\":[{\"id\":1},{\"id\":2}]}\n"));
    peer->flush();
    QVERIFY(rivalSpy.wait(2000));
    QCOMPARE(client.rivalList().size(), 2);
}

void CompetitionServerClientTest::_manualDisconnectDoesNotReconnect_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    CompetitionServerClient client;
    client.setReconnectIntervalMs(250);
    client.setHost(QStringLiteral("127.0.0.1"));
    client.setPort(server.serverPort());
    client.setAutoReconnect(true);
    client.connectToServer();

    QVERIFY(acceptOne(server));
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    client.disconnectFromServer();
    QCOMPARE(client.connectionState(), CompetitionServerClient::ConnectionState::Disconnected);

    QTest::qWait(800);
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!client.connected());
}

/// A rival-list update says nothing about the lock, so it must not clear it.
void CompetitionServerClientTest::_packetWithoutLockFieldsKeepsLockState_test()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    CompetitionServerClient client;
    configure(client, server);
    client.connectToServer();

    QTcpSocket *const peer = acceptOne(server);
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 2000);

    peer->write(QByteArrayLiteral("{\"isLocked\":true,\"lockRemainingMs\":2500}\n"));
    peer->flush();
    QTRY_VERIFY_WITH_TIMEOUT(client.lockStatusActive(), 2000);
    QCOMPARE(client.lockRemainingMs(), 2500);

    QSignalSpy rivalSpy(&client, &CompetitionServerClient::rivalListChanged);
    peer->write(QByteArrayLiteral("{\"rivalList\":[{\"id\":9}]}\n"));
    peer->flush();
    QVERIFY(rivalSpy.wait(2000));

    QVERIFY(client.lockStatusActive());
    QCOMPARE(client.lockRemainingMs(), 2500);
}
