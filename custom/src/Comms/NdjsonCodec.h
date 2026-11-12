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

/// Codec for newline-delimited JSON objects over a streaming connection (like TCP).
class NdjsonCodec : public FrameCodec
{
public:
    NdjsonCodec() = default;

    QList<QJsonObject> decode(const QByteArray &bytes) override;
    QByteArray encode(const QJsonObject &object) override;

    int malformedCount() const override { return _malformedCount; }
    int bufferedBytes() const { return static_cast<int>(_buffer.size()); }

    /// Longest line accepted. A peer that sends more without a newline is broken or
    /// hostile; its partial line is discarded rather than buffered forever.
    static constexpr int kMaxLineBytes = 1024 * 1024;

private:
    QByteArray _buffer;
    int        _malformedCount = 0;
};
