/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "FieldMapper.h"

#include <QtCore/QJsonValue>
#include <QtCore/QStringList>
#include <QtCore/QtMath>

QVariant FieldMapper::valueAtPath(const QJsonObject &object, const QString &dottedPath)
{
    if (dottedPath.isEmpty()) {
        return QVariant();
    }

    const QStringList segments = dottedPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return QVariant();
    }

    QJsonValue current = object;
    for (const QString &segment : segments) {
        if (!current.isObject()) {
            return QVariant();
        }

        const QJsonObject currentObject = current.toObject();
        if (!currentObject.contains(segment)) {
            return QVariant();
        }
        current = currentObject.value(segment);
    }

    // A JSON null carries no value; treat it as absent so callers fall back cleanly.
    if (current.isNull() || current.isUndefined()) {
        return QVariant();
    }

    return current.toVariant();
}

QVariant FieldMapper::_applyRule(const MappingRule &rule, const QVariant &raw)
{
    switch (rule.kind) {
    case MappingRule::Scale:
        return raw.toDouble() * rule.scale;

    case MappingRule::Unit:
        if (rule.unit == QLatin1String("deg2rad")) {
            return qDegreesToRadians(raw.toDouble());
        }
        if (rule.unit == QLatin1String("rad2deg")) {
            return qRadiansToDegrees(raw.toDouble());
        }
        if (rule.unit == QLatin1String("m2cm")) {
            return raw.toDouble() * 100.0;
        }
        if (rule.unit == QLatin1String("cm2m")) {
            return raw.toDouble() / 100.0;
        }
        // ProtocolProfile rejects unknown units at load time, so this is unreachable
        // in practice; passing the value through is the safe fallback.
        return raw;

    case MappingRule::Map: {
        // JSON object keys are strings, so booleans and numbers are looked up by their
        // string form: true -> "true", 3 -> "3".
        QString key = raw.toString();
        if (raw.typeId() == QMetaType::Bool) {
            key = raw.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        return rule.valueMap.value(key);
    }

    case MappingRule::None:
    default:
        return raw;
    }
}

QHash<QString, QVariant> FieldMapper::mapTelemetry(const QJsonObject &packet) const
{
    QHash<QString, QVariant> mapped;

    const QHash<QString, MappingRule> rules = _profile.mavlinkRules();
    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
        const MappingRule &rule = it.value();

        const QVariant raw = valueAtPath(packet, rule.from);
        if (!raw.isValid()) {
            if (rule.hasDefault) {
                mapped.insert(it.key(), rule.defaultValue);
            }
            // No default: leave the key out entirely so the synthesizer keeps its
            // previous value rather than writing a zero.
            continue;
        }

        const QVariant converted = _applyRule(rule, raw);
        if (converted.isValid()) {
            mapped.insert(it.key(), converted);
        }
    }

    return mapped;
}

QVariantMap FieldMapper::passthrough(const QJsonObject &packet) const
{
    QVariantMap through;

    const QStringList fields = _profile.passthroughFields();
    for (const QString &field : fields) {
        if (packet.contains(field)) {
            through.insert(field, packet.value(field).toVariant());
        }
    }

    return through;
}

int FieldMapper::flightModeValue(const QJsonObject &packet, bool *found) const
{
    if (found) {
        *found = false;
    }

    const QString field = _profile.flightModeField();
    if (field.isEmpty()) {
        return 0;
    }

    const QVariant raw = valueAtPath(packet, field);
    if (!raw.isValid()) {
        return 0;
    }

    const QMap<QString, int> values = _profile.flightModeValues();
    const QString key = raw.toString();
    if (!values.contains(key)) {
        return 0;
    }

    if (found) {
        *found = true;
    }
    return values.value(key);
}
