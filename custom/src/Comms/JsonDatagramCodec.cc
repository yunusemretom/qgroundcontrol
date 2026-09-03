/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "JsonDatagramCodec.h"

#include "QGCLoggingCategory.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>

QGC_LOGGING_CATEGORY(JsonDatagramCodecLog, "custom.comms.jsondatagramcodec")

QList<QJsonObject> JsonDatagramCodec::decode(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return {};
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);

    if ((parseError.error != QJsonParseError::NoError) || !document.isObject()) {
        _malformedCount++;
        qCDebug(JsonDatagramCodecLog) << "Dropped malformed datagram, total:" << _malformedCount
                                      << "reason:" << parseError.errorString();
        return {};
    }

    return { document.object() };
}

QByteArray JsonDatagramCodec::encode(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
