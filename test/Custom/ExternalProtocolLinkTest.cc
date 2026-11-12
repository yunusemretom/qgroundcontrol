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
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
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

/// Writes a profile with a telemetry channel plus `extra` (more channels and top-level
/// sections, each starting with a comma) and returns its path.
QString writeProfile(const QTemporaryDir &dir, quint16 port, int timeoutMs,
                     const QString &extraChannels = QString(), const QString &extraSections = QString())
{
    const QString json = QStringLiteral(R"({
        "name": "Test Turret",
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 20 },
        "channels": {
            "telemetry": { "dir": "in", "transport": "udp", "bind": "127.0.0.1:%1",
                           "codec": "json-datagram", "timeoutMs": %2 } %3
        },
        "telemetry": {
            "source": "telemetry",
            "mavlink": { "ATTITUDE.yaw": { "from": "yaw", "unit": "deg2rad" } }
        } %4
    })").arg(port).arg(timeoutMs).arg(extraChannels, extraSections);

    const QString path = dir.filePath(QStringLiteral("profile.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(json.toUtf8());
    return path;
}

QString joystickChannel(quint16 port, int rateHz, const QString &extra = QString())
{
    return QStringLiteral(R"(, "joystick": { "dir": "out", "transport": "udp", "connect": "127.0.0.1:%1",
                                             "codec": "csv", "rateHz": %2 %3 })").arg(port).arg(rateHz).arg(extra);
}

/// Sends a MAVLink message into the link the way QGC does: packed on the link's own
/// channel and handed over through writeBytesThreadSafe().
void sendToLink(LinkInterface *link, const mavlink_message_t &message)
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN]{};
    const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
    link->writeBytesThreadSafe(reinterpret_cast<const char *>(buffer), length);
}

void sendManualControl(LinkInterface *link, int16_t x, int16_t r, uint16_t buttons)
{
    mavlink_manual_control_t control{};
    control.target  = 1;
    control.x       = x;
    control.r       = r;
    control.buttons = buttons;

    mavlink_message_t message{};
    (void) mavlink_msg_manual_control_encode_chan(255, MAV_COMP_ID_MISSIONPLANNER, link->mavlinkChannel(), &message, &control);
    sendToLink(link, message);
}

void sendCommandLong(LinkInterface *link, uint16_t command, float param1 = 0)
{
    mavlink_command_long_t cmd{};
    cmd.target_system    = 1;
    cmd.target_component = 1;
    cmd.command          = command;
    cmd.param1           = param1;

    mavlink_message_t message{};
    (void) mavlink_msg_command_long_encode_chan(255, MAV_COMP_ID_MISSIONPLANNER, link->mavlinkChannel(), &message, &cmd);
    sendToLink(link, message);
}

/// Collects datagrams arriving on a local UDP port: the "ESP32" end of the joystick channel.
class UdpSink : public QObject
{
public:
    UdpSink()
    {
        _socket.bind(QHostAddress::LocalHost, 0);
        (void) connect(&_socket, &QUdpSocket::readyRead, this, [this]() {
            while (_socket.hasPendingDatagrams()) {
                QByteArray datagram(static_cast<int>(_socket.pendingDatagramSize()), Qt::Uninitialized);
                const qint64 read = _socket.readDatagram(datagram.data(), datagram.size());
                if (read >= 0) {
                    datagram.truncate(static_cast<int>(read));
                    frames.append(datagram);
                }
            }
        });
    }

    quint16 port() const { return _socket.localPort(); }

    QList<QByteArray> frames;

private:
    QUdpSocket _socket;
};

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
                    allSeqs.append(message.seq);
                    if (message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                        heartbeatSeqs.append(message.seq);
                    } else if (message.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
                        mavlink_command_ack_t ack{};
                        mavlink_msg_command_ack_decode(&message, &ack);
                        acks.append(ack);
                    }
                }
            }
        });
    }

    QList<uint8_t> heartbeatSeqs;
    QList<uint8_t> allSeqs;     ///< every message, in arrival order
    QList<mavlink_command_ack_t> acks;

    /// ACKs for one MAV_CMD. QGC sends its own requests to the vehicle (camera info and
    /// the like) and the link answers those too, so tests pick out the one they sent.
    QList<mavlink_command_ack_t> acksFor(uint16_t command) const
    {
        QList<mavlink_command_ack_t> result;
        for (const mavlink_command_ack_t &ack : acks) {
            if (ack.command == command) {
                result.append(ack);
            }
        }
        return result;
    }

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

    QTRY_VERIFY_WITH_TIMEOUT(tap.allSeqs.count() >= 1, 2000);
    const int before = tap.allSeqs.count();
    const uint8_t lastSeq = tap.allSeqs.last();

    // QGC packing outgoing messages on the link's channel, as Vehicle does.
    for (int i = 0; i < 5; ++i) {
        mavlink_message_t outgoing{};
        (void) mavlink_msg_heartbeat_pack_chan(255, MAV_COMP_ID_MISSIONPLANNER, link->mavlinkChannel(), &outgoing,
                                               MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
    }

    // Whatever the link emits next (heartbeat, or an ACK to one of QGC's own requests)
    // continues the link's sequence, untouched by the five packed above.
    QTRY_VERIFY_WITH_TIMEOUT(tap.allSeqs.count() > before, 2000);
    QTest::qWait(300);  // let QGC's own requests and their ACKs flow too
    QCOMPARE(tap.allSeqs.at(before), static_cast<uint8_t>(lastSeq + 1));

    // And the whole stream is gap-free.
    for (int i = 1; i < tap.allSeqs.count(); ++i) {
        QCOMPARE(tap.allSeqs.at(i), static_cast<uint8_t>(tap.allSeqs.at(i - 1) + 1));
    }
}

void ExternalProtocolLinkTest::_joystickUsesProfileTemplate_test()
{
    QTemporaryDir dir;
    UdpSink esp32;
    const QString profilePath = writeProfile(dir, freeUdpPort(), 0,
        joystickChannel(esp32.port(), 50, QStringLiteral(R"(, "template": "{pitch:.1f}|{yaw:.1f}|{fire:.0f}|{arm:.0f}")")));

    SharedLinkConfigurationPtr config = connectLink(profilePath);
    QVERIFY(config && config->link());

    sendManualControl(config->link(), 500, -200, 0b01);

    QTRY_VERIFY_WITH_TIMEOUT(!esp32.frames.isEmpty(), 2000);
    QCOMPARE(esp32.frames.first(), QByteArrayLiteral("0.5|-0.2|1|0\n"));
}

/// The rate limit must never swallow the newest value. Dropping the last sample is how
/// a released stick leaves the turret slewing: the "back to zero" frame never arrives.
void ExternalProtocolLinkTest::_joystickSendsLatestValueAfterRateLimit_test()
{
    QTemporaryDir dir;
    UdpSink esp32;
    const QString profilePath = writeProfile(dir, freeUdpPort(), 0, joystickChannel(esp32.port(), 5));

    SharedLinkConfigurationPtr config = connectLink(profilePath);
    QVERIFY(config && config->link());

    sendManualControl(config->link(), 500, 0, 0);
    sendManualControl(config->link(), 250, 0, 0);
    sendManualControl(config->link(), 0, 0, 0);

    QTRY_VERIFY_WITH_TIMEOUT(esp32.frames.count() >= 2, 2000);
    QTest::qWait(400);  // nothing further may be queued behind the trailing frame
    QCOMPARE(esp32.frames.count(), 2);
    QCOMPARE(esp32.frames.first(), QByteArrayLiteral("0.500,0.000,0,0\n"));
    QCOMPARE(esp32.frames.last(),  QByteArrayLiteral("0.000,0.000,0,0\n"));
}

void ExternalProtocolLinkTest::_joystickSendsProfileFailsafeOnDisconnect_test()
{
    QTemporaryDir dir;
    UdpSink esp32;
    const QString profilePath = writeProfile(dir, freeUdpPort(), 0,
        joystickChannel(esp32.port(), 50, QStringLiteral(R"(, "failsafe": "STOP")")));

    SharedLinkConfigurationPtr config = connectLink(profilePath);
    QVERIFY(config && config->link());

    LinkManager::instance()->disconnectAll();

    QTRY_VERIFY_WITH_TIMEOUT(!esp32.frames.isEmpty(), 2000);
    QCOMPARE(esp32.frames.last(), QByteArrayLiteral("STOP\n"));
}

/// Without an ACK, QGC retries a command and finally reports a timeout. Saying
/// "unsupported" straight away is both faster and true.
void ExternalProtocolLinkTest::_unsupportedCommandIsRejected_test()
{
    QTemporaryDir dir;
    SharedLinkConfigurationPtr config = connectLink(writeProfile(dir, freeUdpPort(), 0));
    QVERIFY(config && config->link());
    MavlinkTap tap(config->link());

    sendCommandLong(config->link(), MAV_CMD_DO_SET_SERVO);

    QTRY_VERIFY_WITH_TIMEOUT(!tap.acksFor(MAV_CMD_DO_SET_SERVO).isEmpty(), 2000);
    QCOMPARE(tap.acksFor(MAV_CMD_DO_SET_SERVO).first().result, static_cast<uint8_t>(MAV_RESULT_UNSUPPORTED));
}

void ExternalProtocolLinkTest::_armWithoutCommandChannelFails_test()
{
    QTemporaryDir dir;
    SharedLinkConfigurationPtr config = connectLink(writeProfile(dir, freeUdpPort(), 0));
    QVERIFY(config && config->link());
    MavlinkTap tap(config->link());

    sendCommandLong(config->link(), MAV_CMD_COMPONENT_ARM_DISARM, 1);

    QTRY_VERIFY_WITH_TIMEOUT(!tap.acksFor(MAV_CMD_COMPONENT_ARM_DISARM).isEmpty(), 2000);
    QCOMPARE(tap.acksFor(MAV_CMD_COMPONENT_ARM_DISARM).first().result, static_cast<uint8_t>(MAV_RESULT_FAILED));
}

/// ARM goes out as the profile's JSON on the command channel, and the device's ACK
/// (echoing the command id) comes back to QGC as COMMAND_ACK.
void ExternalProtocolLinkTest::_armRoundTripsThroughDevice_test()
{
    QTcpServer device;
    QVERIFY(device.listen(QHostAddress::LocalHost, 0));

    QTemporaryDir dir;
    const QString commandChannel = QStringLiteral(R"(, "command": { "dir": "out", "transport": "tcp",
                                                    "connect": "127.0.0.1:%1", "codec": "ndjson" })").arg(device.serverPort());
    const QString commands = QStringLiteral(R"(, "commands": {
        "arm": { "channel": "command", "template": { "cmd": "ARM_TURRET", "params": {} } } })");
    SharedLinkConfigurationPtr config = connectLink(writeProfile(dir, freeUdpPort(), 0, commandChannel, commands));
    QVERIFY(config && config->link());
    MavlinkTap tap(config->link());

    QTRY_VERIFY_WITH_TIMEOUT(device.hasPendingConnections(), 2000);
    QTcpSocket *const peer = device.nextPendingConnection();
    QTest::qWait(50);   // let the link see its own connected() before we command it

    sendCommandLong(config->link(), MAV_CMD_COMPONENT_ARM_DISARM, 1);

    QTRY_VERIFY_WITH_TIMEOUT(peer->canReadLine(), 2000);
    const QJsonObject sent = QJsonDocument::fromJson(peer->readLine()).object();
    QCOMPARE(sent.value(QStringLiteral("cmd")).toString(), QStringLiteral("ARM_TURRET"));
    QVERIFY(sent.contains(QStringLiteral("id")));

    const QJsonObject ack{ { QStringLiteral("ack"), sent.value(QStringLiteral("id")) },
                           { QStringLiteral("status"), QStringLiteral("ok") } };
    peer->write(QJsonDocument(ack).toJson(QJsonDocument::Compact) + '\n');
    peer->flush();

    QTRY_VERIFY_WITH_TIMEOUT(!tap.acksFor(MAV_CMD_COMPONENT_ARM_DISARM).isEmpty(), 2000);
    QCOMPARE(tap.acksFor(MAV_CMD_COMPONENT_ARM_DISARM).first().result, static_cast<uint8_t>(MAV_RESULT_ACCEPTED));
}
