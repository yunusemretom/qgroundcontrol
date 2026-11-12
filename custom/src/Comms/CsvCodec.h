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

#include <QtCore/QVariantMap>

/// Codec for formatting joystick axes and buttons into CSV lines.
class CsvCodec : public FrameCodec
{
public:
    CsvCodec() = default;

    QList<QJsonObject> decode(const QByteArray &bytes) override;
    QByteArray encode(const QJsonObject &object) override;

    /// Formats explicit joystick fields into a CSV line.
    static QByteArray formatJoystick(double pitch, double yaw, bool fire, bool arm);

    /// Fills a profile frame template such as "{pitch:.3f},{yaw:.3f},{fire:.0f}".
    /// Supports "{name}" and "{name:.Nf}". Unknown names are left as written so a
    /// profile mistake is visible on the wire. The result ends in exactly one newline.
    static QByteArray formatTemplate(const QString &frameTemplate, const QVariantMap &values);

    /// Appends the line terminator unless the frame already ends with one.
    static QByteArray terminate(const QString &frame);
};
