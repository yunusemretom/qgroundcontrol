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

        // Inbound channels other than the telemetry source are not opened; they are listed
        // as not open rather than hidden, so a mis-declared channel is visible.
        const ProtocolChannel *channel = (name == _profile.telemetrySourceChannel())
                ? static_cast<const ProtocolChannel*>(_telemetryChannel)
                : _outChannels.value(name, nullptr);

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
    if (telemetryConfig.transport != QLatin1String("udp")) {
        qCWarning(ExternalProtocolLinkLog) << "Telemetry source must be a UDP channel, got" << telemetryConfig.transport;
        return false;
    }
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

    const QStringList channelNames = _profile.channelNames();
    for (const QString &name : channelNames) {
        const ChannelConfig channelConfig = _profile.channel(name);
        if ((name == _profile.telemetrySourceChannel()) || (channelConfig.direction != QLatin1String("out"))) {
            continue;
        }

        ProtocolChannel *channel = nullptr;
        if (channelConfig.transport == QLatin1String("tcp")) {
            channel = new TcpProtocolChannel(channelConfig, this);
        } else {
            channel = new UdpProtocolChannel(channelConfig, this);
        }

        // Command channels answer with NDJSON ACKs; listen on each one separately so a
        // partial line on one never mixes with another's.
        if (channelConfig.codec == QLatin1String("ndjson")) {
            _responseCodecs.insert(name, NdjsonCodec());
            (void) connect(channel, &ProtocolChannel::framesReceived, this, [this, name](const QByteArray &bytes) {
                _onCommandResponseReceived(name, bytes);
            });
        }

        // A TCP channel that cannot connect yet keeps retrying on its own; it is not a
        // reason to refuse the link.
        (void) channel->open();
        _outChannels.insert(name, channel);
    }

    _joystickChannel = _outChannels.value(QStringLiteral("joystick"), nullptr);
    if (!_joystickChannel) {
        for (auto it = _outChannels.constBegin(); it != _outChannels.constEnd(); ++it) {
            if (_profile.channel(it.key()).codec == QLatin1String("csv")) {
                _joystickChannel = it.value();
                break;
            }
        }
    }
    if (_joystickChannel) {
        _joystickConfig = _joystickChannel->config();
        _joystickClock.start();
        _lastJoystickSendMs = -1;
        _joystickTimer = new QTimer(this);
        _joystickTimer->setSingleShot(true);
        (void) connect(_joystickTimer, &QTimer::timeout, this, &ExternalProtocolLink::_flushPendingJoystickFrame);
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

    if (_joystickTimer) {
        _joystickTimer->stop();
        delete _joystickTimer;
        _joystickTimer = nullptr;
    }
    _pendingJoystickFrame.clear();

    if (_joystickChannel) {
        // Last word to the hardware before the channel goes: stop moving, stop firing.
        const QString failsafe = _joystickConfig.failsafe.isEmpty()
                ? QStringLiteral("0.000,0.000,0,0")
                : _joystickConfig.failsafe;
        (void) _joystickChannel->send(CsvCodec::terminate(failsafe));
        _joystickChannel = nullptr;
    }

    for (ProtocolChannel *channel : std::as_const(_outChannels)) {
        channel->close();
        delete channel;
    }
    _outChannels.clear();
    _responseCodecs.clear();
    _mavCommandForDeviceCmd.clear();

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
    switch (msg.msgid) {
    case MAVLINK_MSG_ID_COMMAND_LONG: {
        mavlink_command_long_t cmd{};
        mavlink_msg_command_long_decode(&msg, &cmd);
        _gcsSysId  = msg.sysid;
        _gcsCompId = msg.compid;

        if (cmd.command == MAV_CMD_COMPONENT_ARM_DISARM) {
            const bool arm = (cmd.param1 == 1.0f);
            _sendDeviceCommand(arm ? QStringLiteral("arm") : QStringLiteral("disarm"), QVariant(), cmd.command);
        } else if (cmd.command == MAV_CMD_DO_SET_MODE) {
            const QString modeName = _profile.flightModeValues().key(static_cast<int>(cmd.param2));
            if (modeName.isEmpty()) {
                _sendCommandAck(cmd.command, MAV_RESULT_DENIED);
            } else {
                _sendDeviceCommand(QStringLiteral("setMode"), modeName, cmd.command);
            }
        } else {
            _sendCommandAck(cmd.command, MAV_RESULT_UNSUPPORTED);
        }
        break;
    }
    case MAVLINK_MSG_ID_COMMAND_INT: {
        mavlink_command_int_t cmd{};
        mavlink_msg_command_int_decode(&msg, &cmd);
        _gcsSysId  = msg.sysid;
        _gcsCompId = msg.compid;
        _sendCommandAck(cmd.command, MAV_RESULT_UNSUPPORTED);
        break;
    }
    case MAVLINK_MSG_ID_SET_MODE: {
        // QGC changes modes with SET_MODE, which has no ACK of its own; the new mode shows
        // up in telemetry once the device has switched.
        mavlink_set_mode_t sm{};
        mavlink_msg_set_mode_decode(&msg, &sm);
        const QString modeName = _profile.flightModeValues().key(static_cast<int>(sm.custom_mode));
        if (!modeName.isEmpty()) {
            _sendDeviceCommand(QStringLiteral("setMode"), modeName, 0);
        }
        break;
    }
    case MAVLINK_MSG_ID_MANUAL_CONTROL: {
        mavlink_manual_control_t control{};
        mavlink_msg_manual_control_decode(&msg, &control);
        _handleManualControl(control);
        break;
    }
    default:
        break;
    }
}

void ExternalProtocolLink::_sendDeviceCommand(const QString &name, const QVariant &value, uint16_t mavCommand)
{
    if (!_commandTranslator || !_commandTranslator->isSupported(name)) {
        qCDebug(ExternalProtocolLinkLog) << "Profile has no command" << name;
        if (mavCommand) {
            _sendCommandAck(mavCommand, MAV_RESULT_UNSUPPORTED);
        }
        return;
    }

    const QString channelName = _commandTranslator->channelFor(name);
    ProtocolChannel *const channel = _outChannels.value(channelName, nullptr);
    if (!channel || !channel->isOpen()) {
        qCWarning(ExternalProtocolLinkLog) << "Command" << name << "dropped: channel" << channelName << "is not open";
        if (mavCommand) {
            _sendCommandAck(mavCommand, MAV_RESULT_FAILED);
        }
        return;
    }

    const QJsonObject payload = _commandTranslator->translate(name, value);
    const QByteArray encoded = NdjsonCodec().encode(payload);
    if (!channel->send(encoded)) {
        qCWarning(ExternalProtocolLinkLog) << "Command" << name << "could not be written to" << channelName;
        if (mavCommand) {
            _sendCommandAck(mavCommand, MAV_RESULT_FAILED);
        }
        return;
    }

    if (mavCommand) {
        _mavCommandForDeviceCmd.insert(payload.value(QStringLiteral("cmd")).toString(name), mavCommand);
    }
    qCDebug(ExternalProtocolLinkLog) << "Sent" << name << "on" << channelName << encoded.trimmed();
}

void ExternalProtocolLink::_sendCommandAck(uint16_t mavCommand, uint8_t result)
{
    mavlink_message_t ackMsg{};
    (void) mavlink_msg_command_ack_pack_chan(
        static_cast<uint8_t>(_profile.sysId()), static_cast<uint8_t>(_profile.compId()), _auxChannel, &ackMsg,
        mavCommand, result, 100, 0, _gcsSysId, _gcsCompId);
    _respondWithMavlinkMessage(ackMsg);
}

void ExternalProtocolLink::_handleManualControl(const mavlink_manual_control_t &control)
{
    if (!_joystickChannel) {
        return;
    }

    const double pitch = control.x / 1000.0;
    const double yaw   = control.r / 1000.0;
    const bool   fire  = (control.buttons & 1) != 0;
    const bool   arm   = (control.buttons & 2) != 0;

    const QByteArray frame = _joystickConfig.frameTemplate.isEmpty()
            ? CsvCodec::formatJoystick(pitch, yaw, fire, arm)
            : CsvCodec::formatTemplate(_joystickConfig.frameTemplate, {
                  { QStringLiteral("pitch"), pitch },
                  { QStringLiteral("yaw"),   yaw },
                  { QStringLiteral("fire"),  fire ? 1 : 0 },
                  { QStringLiteral("arm"),   arm ? 1 : 0 },
              });

    const int rateHz = (_joystickConfig.rateHz > 0) ? _joystickConfig.rateHz : 50;
    const qint64 intervalMs = qMax(1, 1000 / rateHz);
    const qint64 now = _joystickClock.elapsed();
    const qint64 sinceLast = (_lastJoystickSendMs < 0) ? intervalMs : (now - _lastJoystickSendMs);

    if (sinceLast >= intervalMs) {
        _pendingJoystickFrame.clear();
        _joystickTimer->stop();
        _sendJoystickFrame(frame);
        return;
    }

    // Too soon: hold the newest frame and send it when the slot opens, so the last
    // stick position — typically "centred" — is never the one that gets dropped.
    _pendingJoystickFrame = frame;
    if (!_joystickTimer->isActive()) {
        _joystickTimer->start(static_cast<int>(intervalMs - sinceLast));
    }
}

void ExternalProtocolLink::_flushPendingJoystickFrame()
{
    if (!_pendingJoystickFrame.isEmpty()) {
        const QByteArray frame = _pendingJoystickFrame;
        _pendingJoystickFrame.clear();
        _sendJoystickFrame(frame);
    }
}

void ExternalProtocolLink::_sendJoystickFrame(const QByteArray &frame)
{
    if (_joystickChannel) {
        (void) _joystickChannel->send(frame);
        _lastJoystickSendMs = _joystickClock.elapsed();
    }
}

void ExternalProtocolLink::_onCommandResponseReceived(const QString &channelName, const QByteArray &bytes)
{
    auto codec = _responseCodecs.find(channelName);
    if (codec == _responseCodecs.end() || !_commandTranslator) {
        return;
    }

    const QList<QJsonObject> responses = codec->decode(bytes);
    for (const QJsonObject &resp : responses) {
        QString cmdName;
        bool success = false;
        if (!_commandTranslator->parseAck(resp, cmdName, success)) {
            continue;
        }

        const auto mavCommand = _mavCommandForDeviceCmd.constFind(cmdName);
        if (mavCommand == _mavCommandForDeviceCmd.constEnd()) {
            qCDebug(ExternalProtocolLinkLog) << "ACK for a command QGC did not send:" << cmdName;
            continue;
        }

        qCDebug(ExternalProtocolLinkLog) << "Received ACK for:" << cmdName << "status:" << success;
        _sendCommandAck(mavCommand.value(), success ? MAV_RESULT_ACCEPTED : MAV_RESULT_FAILED);
    }
}

void ExternalProtocolLink::_respondWithMavlinkMessage(const mavlink_message_t &message)
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN]{};
    const int length = mavlink_msg_to_send_buffer(buffer, &message);
    _outgoingQueue.append(QByteArray(reinterpret_cast<char*>(buffer), length));

    // QGC handles bytesReceived synchronously and may answer inside that handler (a new
    // vehicle's first heartbeat triggers requests), which re-enters _writeBytes and makes
    // us emit again before the outer emit has reached every listener. Later listeners
    // would then see messages out of sequence order. Queue nested emissions and let the
    // outermost call drain them, so delivery order always matches sequence order.
    if (_emittingOutgoing) {
        return;
    }
    _emittingOutgoing = true;
    while (!_outgoingQueue.isEmpty()) {
        emit bytesReceived(this, _outgoingQueue.takeFirst());
    }
    _emittingOutgoing = false;
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
