/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief One transport endpoint declared by a ProtocolProfile.
 */

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>

/// Moves bytes to and from one endpoint. Knows nothing about their meaning:
/// framing is the codec's job, interpretation is the mapper's.
class ProtocolChannel : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolChannel(const ChannelConfig &config, QObject *parent = nullptr)
        : QObject(parent), _config(config) {}

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool send(const QByteArray &bytes) = 0;

    ChannelConfig config() const { return _config; }

signals:
    /// Raw bytes as they arrived. One emission per datagram for packet transports,
    /// one per readable chunk for stream transports.
    void framesReceived(const QByteArray &bytes);

    /// No data for `config().timeoutMs`. Only emitted when timeoutMs is greater than zero.
    void timedOut();

    void errorOccurred(const QString &message);

protected:
    ChannelConfig _config;
};
