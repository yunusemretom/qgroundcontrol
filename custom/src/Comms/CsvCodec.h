/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "FrameCodec.h"

/// Codec for formatting joystick axes and buttons into CSV lines.
class CsvCodec : public FrameCodec
{
public:
    CsvCodec() = default;

    QList<QJsonObject> decode(const QByteArray &bytes) override;
    QByteArray encode(const QJsonObject &object) override;

    /// Formats explicit joystick fields into a CSV line.
    static QByteArray formatJoystick(double pitch, double yaw, bool fire, bool arm);
};
