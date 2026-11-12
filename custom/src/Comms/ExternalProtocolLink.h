/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Presents a non-MAVLink device to QGC as an ordinary MAVLink vehicle.
 */

#pragma once

#include "CommandTranslator.h"
#include "ExternalProtocolConfiguration.h"
#include "FieldMapper.h"
#include "JsonDatagramCodec.h"
#include "LinkInterface.h"
#include "MAVLinkSynthesizer.h"
#include "NdjsonCodec.h"
#include "ProtocolProfile.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariantMap>

class QTimer;
class ProtocolChannel;
class UdpProtocolChannel;

Q_DECLARE_LOGGING_CATEGORY(ExternalProtocolLinkLog)

/// Presents a profile-described device to QGC as a MAVLink vehicle.
///
/// Every channel the profile declares is opened: the telemetry source feeds the MAVLink
/// synthesizer, "out" channels carry commands (NDJSON over TCP or UDP) and joystick frames
/// (the "joystick" channel, or the first "csv" one). Which channel a command uses, and the
/// JSON it is built from, come from the profile's "commands" section.
class ExternalProtocolLink : public LinkInterface
{
    Q_OBJECT

    // Exposed so the turret settings page can bind straight to the live link, reached
    // from QML as linkConfiguration.link. QML resolves properties on the instance's
    // metaobject, so the LinkInterface* declared type does not hide these.
    Q_PROPERTY(QVariantMap  passthroughData READ passthroughData   NOTIFY passthroughDataChanged)
    Q_PROPERTY(QString      profileName     READ profileName       NOTIFY statusChanged)
    Q_PROPERTY(qulonglong   packetCount     READ packetCount       NOTIFY statusChanged)
    Q_PROPERTY(int          malformedCount  READ malformedCount    NOTIFY statusChanged)
    Q_PROPERTY(bool         telemetryAlive  READ telemetryAlive    NOTIFY statusChanged)
    Q_PROPERTY(QVariantList channelStates   READ channelStates     NOTIFY statusChanged)

public:
    explicit ExternalProtocolLink(SharedLinkConfigurationPtr &config, QObject *parent = nullptr);
    ~ExternalProtocolLink() override;

    void disconnect() final;
    bool isConnected() const final;

    /// Profile fields not mapped to MAVLink, exposed for the turret QML panel.
    QVariantMap passthroughData() const { return _passthrough; }

    QString     profileName() const { return _profile.name(); }
    qulonglong  packetCount() const { return _packetCount; }
    int         malformedCount() const { return _codec.malformedCount(); }

    /// False once the telemetry channel has gone quiet for its timeoutMs. Distinct from
    /// isConnected(): the link stays up while the far end is silent.
    bool telemetryAlive() const { return _telemetryAlive; }

    /// One entry per declared channel: { name, direction, transport, endpoint, open }.
    QVariantList channelStates() const;

signals:
    void passthroughDataChanged(const QVariantMap &data);
    void statusChanged();

private slots:
    void _onFramesReceived(const QByteArray &bytes);
    void _onChannelTimedOut();
    void _sendHeartbeat();
    void _flushPendingJoystickFrame();

private:
    bool _connect() final;
    void _writeBytes(const QByteArray &bytes) final;

    /// Allocates a second MAVLink channel next to the one QGC uses for this link, the
    /// way MockLink does. QGC packs its outgoing messages and parses our synthesized
    /// ones on mavlinkChannel(); we do the reverse on the aux channel. Sharing one
    /// channel would interleave both directions' sequence numbers and parser state.
    bool _allocateMavlinkChannel() final;
    void _freeMavlinkChannel() final;

    bool _loadProfile();
    void _respondWithMavlinkMessage(const mavlink_message_t &message);
    void _processMavlinkCommand(const mavlink_message_t &msg);
    void _onCommandResponseReceived(const QString &channelName, const QByteArray &bytes);

    /// Sends profile command `name` and, for a MAVLink command, arranges for its ACK.
    /// Answers QGC at once when the command cannot be delivered, so it never waits out
    /// a retry cycle for an ACK that is not coming. `mavCommand` 0 means "no ACK wanted".
    void _sendDeviceCommand(const QString &name, const QVariant &value, uint16_t mavCommand);
    void _sendCommandAck(uint16_t mavCommand, uint8_t result);

    void _handleManualControl(const mavlink_manual_control_t &control);
    void _sendJoystickFrame(const QByteArray &frame);

    ProtocolProfile                     _profile;
    QScopedPointer<FieldMapper>         _mapper;
    QScopedPointer<MAVLinkSynthesizer>  _synthesizer;
    QScopedPointer<CommandTranslator>   _commandTranslator;
    JsonDatagramCodec                   _codec;

    UdpProtocolChannel                 *_telemetryChannel = nullptr;
    QHash<QString, ProtocolChannel*>    _outChannels;       ///< every "out" channel, by profile name
    QHash<QString, NdjsonCodec>         _responseCodecs;    ///< per NDJSON channel, for its replies
    QHash<QString, uint16_t>            _mavCommandForDeviceCmd;  ///< device "cmd" -> MAV_CMD to ACK
    QTimer                             *_heartbeatTimer   = nullptr;

    ProtocolChannel                    *_joystickChannel  = nullptr;
    ChannelConfig                       _joystickConfig;
    QTimer                             *_joystickTimer    = nullptr;
    QElapsedTimer                       _joystickClock;
    qint64                              _lastJoystickSendMs = -1;
    QByteArray                          _pendingJoystickFrame;

    QList<QByteArray>                   _outgoingQueue;     ///< see _respondWithMavlinkMessage
    bool                                _emittingOutgoing = false;

    uint8_t                             _gcsSysId  = 255;
    uint8_t                             _gcsCompId = MAV_COMP_ID_MISSIONPLANNER;

    uint8_t     _auxChannel = std::numeric_limits<uint8_t>::max();

    QVariantMap _passthrough;
    int         _customMode  = 0;
    bool        _connected   = false;
    quint64     _packetCount = 0;
    bool        _telemetryAlive = false;
};
