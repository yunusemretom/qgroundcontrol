/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ExternalProtocolLink.h"
#include "CsvCodec.h"
#include "LinkManager.h"
#include "QGCLoggingCategory.h"
#include "TcpProtocolChannel.h"
#include "UdpProtocolChannel.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>

QGC_LOGGING_CATEGORY(ExternalProtocolLinkLog, "custom.comms.externalprotocollink")

ExternalProtocolLink::ExternalProtocolLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
{
}

ExternalProtocolLink::~ExternalProtocolLink()
{
    ExternalProtocolLink::disconnect();
}

bool ExternalProtocolLink::_loadProfile()
{
    const ExternalProtocolConfiguration *const config =
            qobject_cast<const ExternalProtocolConfiguration*>(linkConfiguration().get());
    if (!config) {
        qCWarning(ExternalProtocolLinkLog) << "Link created without an ExternalProtocolConfiguration";
        return false;
    }

    QFile file(config->profilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(ExternalProtocolLinkLog) << "Cannot open profile" << config->profilePath() << file.errorString();
        return false;
    }

    QString error;
    if (!_profile.loadFromJson(file.readAll(), &error)) {
        qCWarning(ExternalProtocolLinkLog) << "Invalid profile" << config->profilePath() << error;
        return false;
    }

    qCDebug(ExternalProtocolLinkLog) << "Loaded profile" << _profile.name();
    return true;
}

QVariantList ExternalProtocolLink::channelStates() const
{
    QVariantList states;

    const QStringList names = _profile.channelNames();
    for (const QString &name : names) {
        const ChannelConfig config = _profile.channel(name);

        // Only the three channels the link actually instantiates can report open/closed;
        // anything else the profile declares is listed as not open rather than hidden,
        // so a mis-declared channel is visible instead of silently absent.
        const ProtocolChannel *channel = nullptr;
        if (name == _profile.telemetrySourceChannel()) {
            channel = _telemetryChannel;
        } else if (name == QLatin1String("command")) {
            channel = _commandChannel;
        } else if (name == QLatin1String("joystick")) {
            channel = _joystickChannel;
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("name"), name);
        entry.insert(QStringLiteral("direction"), config.direction);
        entry.insert(QStringLiteral("transport"), config.transport.toUpper());
        entry.insert(QStringLiteral("endpoint"), QStringLiteral("%1:%2").arg(config.host).arg(config.port));
        entry.insert(QStringLiteral("open"), channel && channel->isOpen());
        states.append(entry);
    }

    return states;
}

bool ExternalProtocolLink::_allocateMavlinkChannel()
{
    if (!LinkInterface::_allocateMavlinkChannel()) {
        return false;
    }

    _auxChannel = LinkManager::instance()->allocateMavlinkChannel();
    if (_auxChannel == LinkManager::invalidMavlinkChannel()) {
        qCWarning(ExternalProtocolLinkLog) << "No free MAVLink channel for the synthesized side";
        LinkInterface::_freeMavlinkChannel();
        return false;
    }

    return true;
}

void ExternalProtocolLink::_freeMavlinkChannel()
{
    if (_auxChannel != LinkManager::invalidMavlinkChannel()) {
        LinkManager::instance()->freeMavlinkChannel(_auxChannel);
        _auxChannel = LinkManager::invalidMavlinkChannel();
    }
    LinkInterface::_freeMavlinkChannel();
}

bool ExternalProtocolLink::_connect()
{
    if (!_loadProfile()) {
        return false;
    }

    // The synthesized side always speaks MAVLink 2, whatever QGC negotiates for its own.
    mavlink_get_channel_status(_auxChannel)->flags &= ~MAVLINK_STATUS_FLAG_OUT_MAVLINK1;

    _mapper.reset(new FieldMapper(_profile));
    _synthesizer.reset(new MAVLinkSynthesizer(_profile, _auxChannel));
    _commandTranslator.reset(new CommandTranslator(_profile));

    const ChannelConfig telemetryConfig = _profile.channel(_profile.telemetrySourceChannel());
    _telemetryChannel = new UdpProtocolChannel(telemetryConfig, this);

    (void) connect(_telemetryChannel, &ProtocolChannel::framesReceived,
                   this, &ExternalProtocolLink::_onFramesReceived);
    (void) connect(_telemetryChannel, &ProtocolChannel::timedOut,
                   this, &ExternalProtocolLink::_onChannelTimedOut);

    if (!_telemetryChannel->open()) {
        delete _telemetryChannel;
        _telemetryChannel = nullptr;
        return false;
    }

    // Optional Command Channel (TCP)
    const ChannelConfig commandConfig = _profile.channel(QStringLiteral("command"));
    if (commandConfig.valid) {
        _commandChannel = new TcpProtocolChannel(commandConfig, this);
        (void) connect(_commandChannel, &ProtocolChannel::framesReceived,
                       this, &ExternalProtocolLink::_onCommandResponseReceived);
        _commandChannel->open();
    }

    // Optional Joystick Channel (UDP)
    const ChannelConfig joystickConfig = _profile.channel(QStringLiteral("joystick"));
    if (joystickConfig.valid) {
        _joystickChannel = new UdpProtocolChannel(joystickConfig, this);
        _joystickChannel->open();
    }

    _heartbeatTimer = new QTimer(this);
    _heartbeatTimer->setInterval(1000 / _profile.heartbeatHz());
    (void) connect(_heartbeatTimer, &QTimer::timeout, this, &ExternalProtocolLink::_sendHeartbeat);
    _heartbeatTimer->start();

    _connected = true;
    emit statusChanged();
    emit connected();
    return true;
}

void ExternalProtocolLink::disconnect()
{
    if (_heartbeatTimer) {
        _heartbeatTimer->stop();
        delete _heartbeatTimer;
        _heartbeatTimer = nullptr;
    }

    if (_joystickChannel) {
        // Send failsafe stop packet before closing
        _joystickChannel->send("0.000,0.000,0,0\n");
        _joystickChannel->close();
        delete _joystickChannel;
        _joystickChannel = nullptr;
    }

    if (_commandChannel) {
        _commandChannel->close();
        delete _commandChannel;
        _commandChannel = nullptr;
    }

    if (_telemetryChannel) {
        _telemetryChannel->close();
        delete _telemetryChannel;
        _telemetryChannel = nullptr;
    }

    if (_connected) {
        _connected = false;
        emit disconnected();
    }
}

bool ExternalProtocolLink::isConnected() const
{
    return _connected;
}

void ExternalProtocolLink::_writeBytes(const QByteArray &bytes)
{
    mavlink_message_t msg;
    mavlink_status_t status;

    for (int i = 0; i < bytes.size(); ++i) {
        const uint8_t byte = static_cast<uint8_t>(bytes.at(i));
        if (mavlink_parse_char(_auxChannel, byte, &msg, &status)) {
            _processMavlinkCommand(msg);
        }
    }
}

void ExternalProtocolLink::_processMavlinkCommand(const mavlink_message_t &msg)
{
    if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
        mavlink_command_long_t cmd{};
        mavlink_msg_command_long_decode(&msg, &cmd);

        if (cmd.command == MAV_CMD_COMPONENT_ARM_DISARM && _commandChannel && _commandTranslator) {
            const bool arm = (cmd.param1 == 1.0f);
            const QJsonObject jsonCmd = _commandTranslator->translateArm(arm);
            const QByteArray encoded = _commandCodec.encode(jsonCmd);
            _commandChannel->send(encoded);
            qCDebug(ExternalProtocolLinkLog) << "Sent Arm/Disarm command to turret:" << encoded.trimmed();
        }
    } else if (msg.msgid == MAVLINK_MSG_ID_SET_MODE) {
        mavlink_set_mode_t sm{};
        mavlink_msg_set_mode_decode(&msg, &sm);

        QString modeName;
        const auto modeMap = _profile.flightModeValues();
        for (auto it = modeMap.constBegin(); it != modeMap.constEnd(); ++it) {
            if (it.value() == static_cast<int>(sm.custom_mode)) {
                modeName = it.key();
                break;
            }
        }

        if (!modeName.isEmpty() && _commandChannel && _commandTranslator) {
            const QJsonObject jsonCmd = _commandTranslator->translateSetMode(modeName);
            const QByteArray encoded = _commandCodec.encode(jsonCmd);
            _commandChannel->send(encoded);
            qCDebug(ExternalProtocolLinkLog) << "Sent SetMode command to turret:" << modeName << encoded.trimmed();
        }
    } else if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL && _joystickChannel) {
        mavlink_manual_control_t mc{};
        mavlink_msg_manual_control_decode(&msg, &mc);

        const quint64 now = QDateTime::currentMSecsSinceEpoch();
        if ((now - _lastJoystickSendTime) >= 20) { // 50 Hz rate limit
            _lastJoystickSendTime = now;
            const double pitch = mc.x / 1000.0;
            const double yaw   = mc.r / 1000.0;
            const bool fire    = (mc.buttons & 1) != 0;
            const bool arm     = (mc.buttons & 2) != 0;

            const QByteArray csv = CsvCodec::formatJoystick(pitch, yaw, fire, arm);
            _joystickChannel->send(csv);
        }
    }
}

void ExternalProtocolLink::_onCommandResponseReceived(const QByteArray &bytes)
{
    const QList<QJsonObject> responses = _commandCodec.decode(bytes);
    for (const QJsonObject &resp : responses) {
        QString cmdName;
        bool success = false;
        if (_commandTranslator->parseAck(resp, cmdName, success)) {
            qCDebug(ExternalProtocolLinkLog) << "Received ACK for:" << cmdName << "status:" << success;

            uint16_t mavCmd = MAV_CMD_COMPONENT_ARM_DISARM;
            if (cmdName == QLatin1String("arm") || cmdName == QLatin1String("disarm")) {
                mavCmd = MAV_CMD_COMPONENT_ARM_DISARM;
            } else if (cmdName == QLatin1String("set_mode")) {
                mavCmd = MAV_CMD_DO_SET_MODE;
            }

            mavlink_message_t ackMsg;
            mavlink_msg_command_ack_pack_chan(
                _profile.sysId(), _profile.compId(), _auxChannel, &ackMsg,
                mavCmd, success ? MAV_RESULT_ACCEPTED : MAV_RESULT_FAILED,
                100, 0, 0, 0
            );
            _respondWithMavlinkMessage(ackMsg);
        }
    }
}

void ExternalProtocolLink::_respondWithMavlinkMessage(const mavlink_message_t &message)
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN]{};
    const int length = mavlink_msg_to_send_buffer(buffer, &message);
    emit bytesReceived(this, QByteArray(reinterpret_cast<char*>(buffer), length));
}

void ExternalProtocolLink::_sendHeartbeat()
{
    if (_synthesizer) {
        _respondWithMavlinkMessage(_synthesizer->buildHeartbeat(_customMode));
    }
}

void ExternalProtocolLink::_onFramesReceived(const QByteArray &bytes)
{
    const QList<QJsonObject> packets = _codec.decode(bytes);
    for (const QJsonObject &packet : packets) {
        _packetCount++;
        if (!_telemetryAlive) {
            _telemetryAlive = true;
            // A timeout stopped the heartbeat so QGC would report the loss. The device
            // is talking again, so bring the vehicle back without a manual reconnect.
            if (_heartbeatTimer && !_heartbeatTimer->isActive()) {
                _heartbeatTimer->start();
                _sendHeartbeat();
                qCDebug(ExternalProtocolLinkLog) << "Telemetry resumed, heartbeat restarted";
            }
        }
        emit statusChanged();

        bool modeFound = false;
        const int mode = _mapper->flightModeValue(packet, &modeFound);
        if (modeFound) {
            _customMode = mode;
        }

        const QList<mavlink_message_t> messages =
                _synthesizer->buildTelemetryMessages(_mapper->mapTelemetry(packet));
        for (const mavlink_message_t &message : messages) {
            _respondWithMavlinkMessage(message);
        }

        const QVariantMap through = _mapper->passthrough(packet);
        if (through != _passthrough) {
            _passthrough = through;
            emit passthroughDataChanged(_passthrough);
        }
    }
}

void ExternalProtocolLink::_onChannelTimedOut()
{
    _telemetryAlive = false;
    emit statusChanged();

    if (_heartbeatTimer) {
        _heartbeatTimer->stop();
    }
    qCDebug(ExternalProtocolLinkLog) << "Telemetry timed out, heartbeat stopped";
}
