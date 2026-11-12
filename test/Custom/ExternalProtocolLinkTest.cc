/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ExternalProtocolLinkTest.h"
#include "ExternalProtocolConfiguration.h"
#include "LinkManager.h"
#include "MAVLinkLib.h"
#include "MultiVehicleManager.h"

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QUdpSocket>
#include <QtTest/QTest>

namespace {

quint16 freeUdpPort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress::LocalHost, 0);
    const quint16 port = probe.localPort();
    probe.close();
    return port;
}

/// Writes a minimal telemetry-only profile and returns its path.
QString writeProfile(const QTemporaryDir &dir, quint16 port, int timeoutMs)
{
    const QString json = QStringLiteral(R"({
        "name": "Test Turret",
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 20 },
        "channels": {
            "telemetry": { "dir": "in", "transport": "udp", "bind": "127.0.0.1:%1",
                           "codec": "json-datagram", "timeoutMs": %2 }
        },
        "telemetry": {
            "source": "telemetry",
            "mavlink": { "ATTITUDE.yaw": { "from": "yaw", "unit": "deg2rad" } }
        }
    })").arg(port).arg(timeoutMs);

    const QString path = dir.filePath(QStringLiteral("profile.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(json.toUtf8());
    return path;
}

/// Decodes the MAVLink stream a link hands to QGC, with its own parser state so the
/// test never touches the global channel table it is checking.
class MavlinkTap : public QObject
{
public:
    explicit MavlinkTap(LinkInterface *link)
    {
        (void) connect(link, &LinkInterface::bytesReceived, this, [this](LinkInterface *, const QByteArray &bytes) {
            for (const char c : bytes) {
                mavlink_message_t message{};
                mavlink_status_t  messageStatus{};
                if (mavlink_frame_char_buffer(&_rxBuffer, &_rxStatus, static_cast<uint8_t>(c), &message, &messageStatus) == MAVLINK_FRAMING_OK) {
                    if (message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                        heartbeatSeqs.append(message.seq);
                    }
                }
            }
        });
    }

    QList<uint8_t> heartbeatSeqs;

private:
    mavlink_message_t _rxBuffer{};
    mavlink_status_t  _rxStatus{};
};

SharedLinkConfigurationPtr connectLink(const QString &profilePath)
{
    ExternalProtocolConfiguration *const config = new ExternalProtocolConfiguration(QStringLiteral("External Test"));
    config->setProfilePath(profilePath);
    config->setDynamic(true);

    SharedLinkConfigurationPtr shared(config);
    if (!LinkManager::instance()->createConnectedLink(shared)) {
        return SharedLinkConfigurationPtr();
    }
    return shared;
}

} // namespace

void ExternalProtocolLinkTest::cleanup()
{
    LinkManager::instance()->disconnectAll();
    QTRY_VERIFY_WITH_TIMEOUT(!MultiVehicleManager::instance()->activeVehicle(), 5000);
    UnitTest::cleanup();
}

/// A turret that reboots goes silent and then talks again. Heartbeats stop during the
/// silence so QGC reports the loss, and must come back with the telemetry.
void ExternalProtocolLinkTest::_heartbeatResumesAfterTelemetryTimeout_test()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const quint16 port = freeUdpPort();
    const QString profilePath = writeProfile(dir, port, 300);
    QVERIFY(!profilePath.isEmpty());

    SharedLinkConfigurationPtr config = connectLink(profilePath);
    QVERIFY(config);
    QVERIFY(config->link());
    MavlinkTap tap(config->link());

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"yaw": 10})"), QHostAddress::LocalHost, port);

    // Let the telemetry time out, then confirm heartbeats really have stopped.
    QTest::qWait(800);
    tap.heartbeatSeqs.clear();
    QTest::qWait(300);
    QCOMPARE(tap.heartbeatSeqs.count(), 0);

    // Telemetry returns: heartbeats must too.
    sender.writeDatagram(QByteArrayLiteral(R"({"yaw": 20})"), QHostAddress::LocalHost, port);
    QTRY_VERIFY_WITH_TIMEOUT(tap.heartbeatSeqs.count() >= 2, 1000);
}

/// Messages the link synthesizes must be packed on a channel of their own. Sharing
/// QGC's outgoing channel interleaves the two sequence counters, so QGC sees gaps in
/// the vehicle's sequence and reports packet loss that never happened.
void ExternalProtocolLinkTest::_synthesizedMessagesKeepTheirOwnSequence_test()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profilePath = writeProfile(dir, freeUdpPort(), 0);
    QVERIFY(!profilePath.isEmpty());

    SharedLinkConfigurationPtr config = connectLink(profilePath);
    QVERIFY(config);
    LinkInterface *const link = config->link();
    QVERIFY(link);
    MavlinkTap tap(link);

    QTRY_VERIFY_WITH_TIMEOUT(tap.heartbeatSeqs.count() >= 1, 2000);
    const int before = tap.heartbeatSeqs.count();
    const uint8_t lastSeq = tap.heartbeatSeqs.last();

    // QGC packing outgoing messages on the link's channel, as Vehicle does.
    for (int i = 0; i < 5; ++i) {
        mavlink_message_t outgoing{};
        (void) mavlink_msg_heartbeat_pack_chan(255, MAV_COMP_ID_MISSIONPLANNER, link->mavlinkChannel(), &outgoing,
                                               MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
    }

    QTRY_VERIFY_WITH_TIMEOUT(tap.heartbeatSeqs.count() > before, 2000);
    QCOMPARE(tap.heartbeatSeqs.at(before), static_cast<uint8_t>(lastSeq + 1));
}
