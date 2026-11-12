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

#include <QtNetwork/QTcpSocket>
#include <QtCore/QTimer>

/// TCP transport channel supporting connect, send, auto-reconnect, and timeout.
class TcpProtocolChannel : public ProtocolChannel
{
    Q_OBJECT

public:
    explicit TcpProtocolChannel(const ChannelConfig &config, QObject *parent = nullptr);
    ~TcpProtocolChannel() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool send(const QByteArray &bytes) override;

private slots:
    void _onConnected();
    void _onDisconnected();
    void _onReadyRead();
    void _onErrorOccurred(QAbstractSocket::SocketError socketError);
    void _attemptReconnect();
    void _checkTimeout();

private:
    QTcpSocket _socket;
    QTimer     _reconnectTimer;
    QTimer     _timeoutTimer;
    quint64    _lastReadTime = 0;
    bool       _closed       = true;    ///< close() was called; a disconnect is then final, not a drop
    bool       _timedOut     = false;   ///< timedOut already emitted for the current silence
};
