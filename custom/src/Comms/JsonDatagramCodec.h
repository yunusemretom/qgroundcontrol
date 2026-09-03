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

#include <QtCore/QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(JsonDatagramCodecLog)

/// One datagram carries exactly one JSON object. There is no partial-frame state:
/// a datagram either parses or it does not.
class JsonDatagramCodec : public FrameCodec
{
public:
    JsonDatagramCodec() = default;

    QList<QJsonObject> decode(const QByteArray &bytes) final;
    QByteArray encode(const QJsonObject &object) final;
    int malformedCount() const final { return _malformedCount; }

private:
    int _malformedCount = 0;
};
