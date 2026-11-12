/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "TcpProtocolChannel.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtNetwork/QHostAddress>

QGC_LOGGING_CATEGORY(TcpProtocolChannelLog, "custom.comms.tcpprotocolchannel")

TcpProtocolChannel::TcpProtocolChannel(const ChannelConfig &config, QObject *parent)
    : ProtocolChannel(config, parent)
{
    (void) connect(&_socket, &QTcpSocket::connected, this, &TcpProtocolChannel::_onConnected);
    (void) connect(&_socket, &QTcpSocket::disconnected, this, &TcpProtocolChannel::_onDisconnected);
    (void) connect(&_socket, &QTcpSocket::readyRead, this, &TcpProtocolChannel::_onReadyRead);
    (void) connect(&_socket, &QTcpSocket::errorOccurred, this, &TcpProtocolChannel::_onErrorOccurred);

    (void) connect(&_reconnectTimer, &QTimer::timeout, this, &TcpProtocolChannel::_attemptReconnect);
    (void) connect(&_timeoutTimer, &QTimer::timeout, this, &TcpProtocolChannel::_checkTimeout);
}

TcpProtocolChannel::~TcpProtocolChannel()
{
    TcpProtocolChannel::close();
}

bool TcpProtocolChannel::open()
{
    if (!_config.valid) {
        qCWarning(TcpProtocolChannelLog) << "Refusing to open channel with invalid config:" << _config.name;
        return false;
    }

    _closed = false;

    if (_socket.state() != QAbstractSocket::UnconnectedState) {
        return true;
    }

    _socket.connectToHost(_config.host, _config.port);

    if (_config.timeoutMs > 0) {
        _lastReadTime = QDateTime::currentMSecsSinceEpoch();
        _timedOut = false;
        _timeoutTimer.start(qMax(50, _config.timeoutMs / 2));
    }

    return true;
}

void TcpProtocolChannel::close()
{
    // Set first: disconnectFromHost() can emit disconnected() synchronously, and
    // _onDisconnected() must see this as a deliberate close rather than a drop.
    _closed = true;
    _reconnectTimer.stop();
    _timeoutTimer.stop();

    if (_socket.state() != QAbstractSocket::UnconnectedState) {
        // Push out anything still queued without blocking the GUI thread, then drop the
        // connection. Waiting for a graceful close here would freeze the UI.
        (void) _socket.flush();
        _socket.disconnectFromHost();
        if (_socket.state() != QAbstractSocket::UnconnectedState) {
            _socket.abort();
        }
    }
}

bool TcpProtocolChannel::isOpen() const
{
    return _socket.state() == QAbstractSocket::ConnectedState;
}

bool TcpProtocolChannel::send(const QByteArray &bytes)
{
    if (!isOpen()) {
        return false;
    }
    return _socket.write(bytes) == bytes.size();
}

void TcpProtocolChannel::_onConnected()
{
    qCDebug(TcpProtocolChannelLog) << "TCP Connected:" << _config.name << _config.host << _config.port;
    _reconnectTimer.stop();
    _lastReadTime = QDateTime::currentMSecsSinceEpoch();
    _timedOut = false;
}

void TcpProtocolChannel::_onDisconnected()
{
    qCDebug(TcpProtocolChannelLog) << "TCP Disconnected:" << _config.name;
    if (!_closed && (_config.reconnectMs > 0)) {
        _reconnectTimer.start(_config.reconnectMs);
    }
}

void TcpProtocolChannel::_onReadyRead()
{
    _lastReadTime = QDateTime::currentMSecsSinceEpoch();
    _timedOut = false;
    const QByteArray data = _socket.readAll();
    if (!data.isEmpty()) {
        emit framesReceived(data);
    }
}

void TcpProtocolChannel::_onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    qCWarning(TcpProtocolChannelLog) << "TCP Error on" << _config.name << ":" << socketError << _socket.errorString();
    emit errorOccurred(_socket.errorString());
    if (!_closed && (_config.reconnectMs > 0) && !_reconnectTimer.isActive()) {
        _reconnectTimer.start(_config.reconnectMs);
    }
}

void TcpProtocolChannel::_attemptReconnect()
{
    if (!_closed && (_socket.state() == QAbstractSocket::UnconnectedState)) {
        qCDebug(TcpProtocolChannelLog) << "Attempting reconnect for:" << _config.name;
        _socket.connectToHost(_config.host, _config.port);
    }
}

void TcpProtocolChannel::_checkTimeout()
{
    if (_timedOut || !isOpen() || (_config.timeoutMs <= 0)) {
        return;
    }

    const quint64 now = QDateTime::currentMSecsSinceEpoch();
    if ((now - _lastReadTime) > static_cast<quint64>(_config.timeoutMs)) {
        // Latched until data arrives, so one silence produces one signal.
        _timedOut = true;
        qCDebug(TcpProtocolChannelLog) << "Channel timed out:" << _config.name;
        emit timedOut();
    }
}
