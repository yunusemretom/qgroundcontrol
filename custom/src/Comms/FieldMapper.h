/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Applies a ProtocolProfile's mapping rules to an incoming telemetry packet.
 */

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>

/// Turns a raw telemetry object into the values the synthesizer needs.
/// Pure transformation: no sockets, no MAVLink, no Qt object lifetime.
class FieldMapper
{
public:
    explicit FieldMapper(const ProtocolProfile &profile) : _profile(profile) {}

    /// Keys are "MESSAGE.field" exactly as written in the profile. A rule whose source
    /// field is absent and which declares no default produces no entry, so the caller
    /// keeps whatever value it already had.
    QHash<QString, QVariant> mapTelemetry(const QJsonObject &packet) const;

    /// Copies the profile's declared passthrough fields verbatim. Undeclared fields
    /// never leak through, so the QML side sees a stable, documented surface.
    QVariantMap passthrough(const QJsonObject &packet) const;

    /// Resolves the profile's flight-mode field to its numeric custom_mode value.
    /// Sets *found to false when the field is absent or its value is not in the map.
    int flightModeValue(const QJsonObject &packet, bool *found) const;

    /// Walks a dot-separated path. Returns an invalid QVariant when any segment is
    /// missing, null, or not an object where the path expects to descend.
    static QVariant valueAtPath(const QJsonObject &object, const QString &dottedPath);

private:
    static QVariant _applyRule(const MappingRule &rule, const QVariant &raw);

    ProtocolProfile _profile;
};
