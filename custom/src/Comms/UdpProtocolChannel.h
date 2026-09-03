/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "ProtocolChannel.h"

#include <QtCore/QLoggingCategory>

class QTimer;
class QUdpSocket;

Q_DECLARE_LOGGING_CATEGORY(UdpProtocolChannelLog)

/// Binds a UDP port and emits each datagram as it arrives. For an "out" channel it
/// sends to the configured host instead.
class UdpProtocolChannel : public ProtocolChannel
{
    Q_OBJECT

public:
    explicit UdpProtocolChannel(const ChannelConfig &config, QObject *parent = nullptr);
    ~UdpProtocolChannel() override;

    bool open() final;
    void close() final;
    bool isOpen() const final;
    bool send(const QByteArray &bytes) final;

    quint64 packetCount() const { return _packetCount; }

private slots:
    void _readPendingDatagrams();
    void _checkTimeout();

private:
    QUdpSocket *_socket       = nullptr;
    QTimer     *_timeoutTimer = nullptr;
    qint64      _lastRxMs     = 0;
    quint64     _packetCount  = 0;
    bool        _timedOut     = false;
};
