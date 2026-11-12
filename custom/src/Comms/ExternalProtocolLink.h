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

#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariantMap>

class QTimer;
class UdpProtocolChannel;
class TcpProtocolChannel;

Q_DECLARE_LOGGING_CATEGORY(ExternalProtocolLinkLog)

/// Reads telemetry and sends outgoing commands/joystick control.
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
    void _onCommandResponseReceived(const QByteArray &bytes);
    void _onChannelTimedOut();
    void _sendHeartbeat();

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

    ProtocolProfile                     _profile;
    QScopedPointer<FieldMapper>         _mapper;
    QScopedPointer<MAVLinkSynthesizer>  _synthesizer;
    QScopedPointer<CommandTranslator>   _commandTranslator;
    JsonDatagramCodec                   _codec;
    NdjsonCodec                         _commandCodec;

    UdpProtocolChannel                 *_telemetryChannel = nullptr;
    TcpProtocolChannel                 *_commandChannel   = nullptr;
    UdpProtocolChannel                 *_joystickChannel  = nullptr;
    QTimer                             *_heartbeatTimer   = nullptr;

    uint8_t     _auxChannel = std::numeric_limits<uint8_t>::max();

    QVariantMap _passthrough;
    int         _customMode  = 0;
    bool        _connected   = false;
    quint64     _packetCount = 0;
    bool        _telemetryAlive = false;
    quint64     _lastJoystickSendTime = 0;
};
