/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "UdpProtocolChannel.h"

#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtNetwork/QUdpSocket>

QGC_LOGGING_CATEGORY(UdpProtocolChannelLog, "custom.comms.udpprotocolchannel")

UdpProtocolChannel::UdpProtocolChannel(const ChannelConfig &config, QObject *parent)
    : ProtocolChannel(config, parent)
{
}

UdpProtocolChannel::~UdpProtocolChannel()
{
    close();
}

bool UdpProtocolChannel::open()
{
    if (!_config.valid || (_config.port == 0)) {
        qCWarning(UdpProtocolChannelLog) << "Refusing to open channel with invalid config:" << _config.name;
        emit errorOccurred(tr("Channel \"%1\" has an invalid endpoint").arg(_config.name));
        return false;
    }

    if (_socket) {
        return true;
    }

    _socket = new QUdpSocket(this);

    if (_config.direction == QLatin1String("in")) {
        const QHostAddress bindAddress = (_config.host == QLatin1String("0.0.0.0"))
                ? QHostAddress(QHostAddress::AnyIPv4)
                : QHostAddress(_config.host);

        if (!_socket->bind(bindAddress, _config.port, QUdpSocket::ShareAddress)) {
            const QString reason = _socket->errorString();
            qCWarning(UdpProtocolChannelLog) << "Bind failed on" << _config.port << reason;
            emit errorOccurred(tr("Could not bind UDP port %1: %2").arg(_config.port).arg(reason));
            delete _socket;
            _socket = nullptr;
            return false;
        }

        (void) connect(_socket, &QUdpSocket::readyRead, this, &UdpProtocolChannel::_readPendingDatagrams);
    }

    _lastRxMs = QDateTime::currentMSecsSinceEpoch();
    _timedOut = false;

    if (_config.timeoutMs > 0) {
        _timeoutTimer = new QTimer(this);
        // Check several times per timeout window so the signal is reasonably prompt.
        _timeoutTimer->setInterval(qMax(100, _config.timeoutMs / 3));
        (void) connect(_timeoutTimer, &QTimer::timeout, this, &UdpProtocolChannel::_checkTimeout);
        _timeoutTimer->start();
    }

    qCDebug(UdpProtocolChannelLog) << "Opened channel" << _config.name << _config.host << _config.port;
    return true;
}

void UdpProtocolChannel::close()
{
    if (_timeoutTimer) {
        _timeoutTimer->stop();
        delete _timeoutTimer;
        _timeoutTimer = nullptr;
    }

    if (_socket) {
        _socket->close();
        delete _socket;
        _socket = nullptr;
    }
}

bool UdpProtocolChannel::isOpen() const
{
    return (_socket != nullptr);
}

bool UdpProtocolChannel::send(const QByteArray &bytes)
{
    if (!_socket) {
        return false;
    }

    const qint64 written = _socket->writeDatagram(bytes, QHostAddress(_config.host), _config.port);
    return (written == bytes.size());
}

void UdpProtocolChannel::_readPendingDatagrams()
{
    while (_socket && _socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(_socket->pendingDatagramSize()));
        const qint64 read = _socket->readDatagram(datagram.data(), datagram.size());
        if (read < 0) {
            continue;
        }
        datagram.truncate(static_cast<int>(read));

        _packetCount++;
        _lastRxMs = QDateTime::currentMSecsSinceEpoch();
        _timedOut = false;

        emit framesReceived(datagram);
    }
}

void UdpProtocolChannel::_checkTimeout()
{
    if (_timedOut || (_config.timeoutMs <= 0)) {
        return;
    }

    if ((QDateTime::currentMSecsSinceEpoch() - _lastRxMs) > _config.timeoutMs) {
        _timedOut = true;
        qCDebug(UdpProtocolChannelLog) << "Channel timed out:" << _config.name;
        emit timedOut();
    }
}
