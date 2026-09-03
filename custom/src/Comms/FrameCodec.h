/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Framing strategy: turns raw bytes into JSON objects and back.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QList>

/// Converts between the wire's byte framing and JSON objects.
///
/// decode() takes whatever bytes arrived and returns however many complete frames
/// they contained: zero for a malformed datagram, one for a JSON datagram, many for
/// a stream carrying several newline-delimited objects. Implementations keep any
/// partial trailing frame internally.
///
/// Neither method throws. Malformed input is dropped and counted.
class FrameCodec
{
public:
    virtual ~FrameCodec() = default;

    virtual QList<QJsonObject> decode(const QByteArray &bytes) = 0;
    virtual QByteArray encode(const QJsonObject &object) = 0;

    /// Number of frames dropped as malformed since construction.
    virtual int malformedCount() const = 0;
};
