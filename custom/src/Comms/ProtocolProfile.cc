/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ProtocolProfile.h"

#include "QGCLoggingCategory.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

QGC_LOGGING_CATEGORY(ProtocolProfileLog, "custom.comms.protocolprofile")

bool ProtocolProfile::_fail(QString *errorString, const QString &reason)
{
    _valid = false;
    if (errorString) {
        *errorString = reason;
    }
    qCWarning(ProtocolProfileLog) << "Profile rejected:" << reason;
    return false;
}

/// Splits "host:port" into its parts. Returns false when the port is missing or out of range.
static bool parseEndpoint(const QString &endpoint, QString &host, quint16 &port)
{
    const int separator = endpoint.lastIndexOf(QLatin1Char(':'));
    if (separator <= 0) {
        return false;
    }

    bool ok = false;
    const uint parsed = QStringView(endpoint).mid(separator + 1).toUInt(&ok);
    if (!ok || (parsed == 0) || (parsed > 65535)) {
        return false;
    }

    host = endpoint.left(separator);
    port = static_cast<quint16>(parsed);
    return true;
}

bool ProtocolProfile::loadFromJson(const QByteArray &json, QString *errorString)
{
    *this = ProtocolProfile();

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return _fail(errorString, QStringLiteral("JSON parse error: %1").arg(parseError.errorString()));
    }
    if (!document.isObject()) {
        return _fail(errorString, QStringLiteral("Profile root must be a JSON object"));
    }

    const QJsonObject root = document.object();

    _name = root.value(QStringLiteral("name")).toString();
    if (_name.isEmpty()) {
        return _fail(errorString, QStringLiteral("Profile is missing a non-empty \"name\""));
    }
    _schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);

    const QJsonObject vehicle = root.value(QStringLiteral("vehicle")).toObject();
    _mavTypeName = vehicle.value(QStringLiteral("mavType")).toString(QStringLiteral("MAV_TYPE_GENERIC"));
    _sysId       = vehicle.value(QStringLiteral("sysId")).toInt(1);
    _compId      = vehicle.value(QStringLiteral("compId")).toInt(1);
    _heartbeatHz = vehicle.value(QStringLiteral("heartbeatHz")).toInt(1);
    if (_heartbeatHz <= 0) {
        return _fail(errorString, QStringLiteral("vehicle.heartbeatHz must be greater than zero"));
    }

    const QJsonObject channels = root.value(QStringLiteral("channels")).toObject();
    if (channels.isEmpty()) {
        return _fail(errorString, QStringLiteral("Profile declares no channels"));
    }
    for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();

        ChannelConfig channel;
        channel.name        = it.key();
        channel.direction   = source.value(QStringLiteral("dir")).toString();
        channel.transport   = source.value(QStringLiteral("transport")).toString();
        channel.codec       = source.value(QStringLiteral("codec")).toString();
        channel.timeoutMs   = source.value(QStringLiteral("timeoutMs")).toInt(0);
        channel.reconnectMs = source.value(QStringLiteral("reconnectMs")).toInt(0);

        if ((channel.direction != QLatin1String("in")) && (channel.direction != QLatin1String("out"))) {
            return _fail(errorString, QStringLiteral("Channel \"%1\": dir must be \"in\" or \"out\"").arg(it.key()));
        }

        const QString endpoint = (channel.direction == QLatin1String("in"))
                ? source.value(QStringLiteral("bind")).toString()
                : source.value(QStringLiteral("connect")).toString();
        if (!parseEndpoint(endpoint, channel.host, channel.port)) {
            return _fail(errorString, QStringLiteral("Channel \"%1\": invalid endpoint \"%2\"").arg(it.key(), endpoint));
        }

        channel.valid = true;
        _channels.insert(channel.name, channel);
    }

    const QJsonObject telemetry = root.value(QStringLiteral("telemetry")).toObject();
    _telemetrySource = telemetry.value(QStringLiteral("source")).toString();
    if (!_channels.contains(_telemetrySource)) {
        return _fail(errorString, QStringLiteral("telemetry.source \"%1\" names no declared channel").arg(_telemetrySource));
    }

    const QJsonObject mavlink = telemetry.value(QStringLiteral("mavlink")).toObject();
    for (auto it = mavlink.constBegin(); it != mavlink.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();

        MappingRule rule;
        rule.from = source.value(QStringLiteral("from")).toString();
        if (rule.from.isEmpty()) {
            return _fail(errorString, QStringLiteral("Mapping \"%1\" is missing \"from\"").arg(it.key()));
        }

        const bool hasScale = source.contains(QStringLiteral("scale"));
        const bool hasUnit  = source.contains(QStringLiteral("unit"));
        const bool hasMap   = source.contains(QStringLiteral("map"));
        if ((hasScale + hasUnit + hasMap) > 1) {
            return _fail(errorString, QStringLiteral("Mapping \"%1\": scale, unit and map are mutually exclusive").arg(it.key()));
        }

        if (hasScale) {
            rule.kind  = MappingRule::Scale;
            rule.scale = source.value(QStringLiteral("scale")).toDouble(1.0);
        } else if (hasUnit) {
            rule.kind = MappingRule::Unit;
            rule.unit = source.value(QStringLiteral("unit")).toString();

            static const QStringList supportedUnits = {
                QStringLiteral("deg2rad"), QStringLiteral("rad2deg"),
                QStringLiteral("m2cm"),    QStringLiteral("cm2m")
            };
            if (!supportedUnits.contains(rule.unit)) {
                return _fail(errorString, QStringLiteral("Mapping \"%1\": unsupported unit \"%2\"").arg(it.key(), rule.unit));
            }
        } else if (hasMap) {
            rule.kind     = MappingRule::Map;
            rule.valueMap = source.value(QStringLiteral("map")).toObject().toVariantMap();
        }

        if (source.contains(QStringLiteral("default"))) {
            rule.hasDefault   = true;
            rule.defaultValue = source.value(QStringLiteral("default")).toVariant();
        }

        _mavlinkRules.insert(it.key(), rule);
    }

    const QJsonArray passthrough = telemetry.value(QStringLiteral("passthrough")).toArray();
    for (const QJsonValue &field : passthrough) {
        _passthrough.append(field.toString());
    }

    const QJsonObject flightMode = telemetry.value(QStringLiteral("flightMode")).toObject();
    _flightModeField = flightMode.value(QStringLiteral("from")).toString();
    const QJsonObject modeValues = flightMode.value(QStringLiteral("values")).toObject();
    for (auto it = modeValues.constBegin(); it != modeValues.constEnd(); ++it) {
        _flightModeValues.insert(it.key(), it.value().toInt());
    }

    _valid = true;
    if (errorString) {
        errorString->clear();
    }
    return true;
}
