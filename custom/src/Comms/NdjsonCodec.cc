/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "NdjsonCodec.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

QGC_LOGGING_CATEGORY(NdjsonCodecLog, "custom.comms.ndjsoncodec")

QList<QJsonObject> NdjsonCodec::decode(const QByteArray &bytes)
{
    _buffer.append(bytes);
    QList<QJsonObject> frames;

    int newlineIndex = -1;
    while ((newlineIndex = _buffer.indexOf('\n')) != -1) {
        QByteArray line = _buffer.left(newlineIndex).trimmed();
        _buffer.remove(0, newlineIndex + 1);

        if (line.isEmpty()) {
            continue;
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            _malformedCount++;
            qCDebug(NdjsonCodecLog) << "Malformed NDJSON line:" << line << "reason:" << parseError.errorString();
            continue;
        }

        frames.append(doc.object());
    }

    if (_buffer.size() > kMaxLineBytes) {
        qCWarning(NdjsonCodecLog) << "Discarding" << _buffer.size() << "bytes with no line terminator";
        _buffer.clear();
        _malformedCount++;
    }

    return frames;
}

QByteArray NdjsonCodec::encode(const QJsonObject &object)
{
    QJsonDocument doc(object);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');
    return data;
}
