/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Declarative description of a non-MAVLink protocol, loaded from JSON.
 */

#pragma once

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

Q_DECLARE_LOGGING_CATEGORY(ProtocolProfileLog)

/// One telemetry field mapping: where the value comes from and how it is transformed.
/// Scale, Unit and Map are mutually exclusive; a rule carries at most one of them.
struct MappingRule
{
    enum Kind {
        None,   ///< value used as-is
        Scale,  ///< value multiplied by `scale`
        Unit,   ///< named conversion, see `unit`
        Map     ///< discrete lookup through `valueMap`
    };

    QString     from;                   ///< dot-separated source path, e.g. "gps.lat"
    Kind        kind        = None;
    double      scale       = 1.0;
    QString     unit;                   ///< deg2rad | rad2deg | m2cm | cm2m
    QVariantMap valueMap;
    QVariant    defaultValue;
    bool        hasDefault  = false;
};

/// One transport endpoint declared by the profile.
struct ChannelConfig
{
    QString name;
    QString direction;                  ///< "in" or "out"
    QString transport;                  ///< "udp" or "tcp"
    QString host;
    quint16 port        = 0;
    QString codec;
    int     timeoutMs   = 0;
    int     reconnectMs = 0;
    int     rateHz      = 0;            ///< max outgoing frames per second, 0 = unlimited
    QString frameTemplate;              ///< text frame layout, e.g. "{pitch:.3f},{yaw:.3f}"
    QString failsafe;                   ///< frame sent when the channel closes
    bool    valid       = false;
};

/// One outgoing command: which channel it goes to and the JSON it is built from.
/// String values equal to or containing "{value}" are substituted at send time.
struct CommandTemplate
{
    QString     channel;
    QJsonObject payload;
};

/// Parsed, validated protocol profile. Pure data: knows nothing about sockets or MAVLink.
class ProtocolProfile
{
public:
    ProtocolProfile() = default;

    /// Parses and validates `json`. On failure returns false, leaves the profile invalid,
    /// and writes a human-readable reason to `errorString` when it is non-null.
    bool loadFromJson(const QByteArray &json, QString *errorString);

    bool isValid() const { return _valid; }

    QString name() const { return _name; }
    int     schemaVersion() const { return _schemaVersion; }

    QString mavTypeName() const { return _mavTypeName; }
    int     mavType() const { return _mavType; }       ///< numeric MAV_TYPE resolved from mavTypeName
    int     sysId() const { return _sysId; }
    int     compId() const { return _compId; }
    int     heartbeatHz() const { return _heartbeatHz; }

    /// Whether the bridged device should advertise itself as ready to operate.
    /// A non-MAVLink device has no prearm concept, so a device that is talking to us
    /// is ready by definition; a profile can opt out with "reportsReady": false.
    bool    reportsReady() const { return _reportsReady; }

    QStringList   channelNames() const { return _channels.keys(); }
    ChannelConfig channel(const QString &name) const { return _channels.value(name); }

    QString telemetrySourceChannel() const { return _telemetrySource; }
    QHash<QString, MappingRule> mavlinkRules() const { return _mavlinkRules; }
    QStringList passthroughFields() const { return _passthrough; }

    QString          flightModeField() const { return _flightModeField; }
    QMap<QString,int> flightModeValues() const { return _flightModeValues; }

    /// True when the profile has a "commands" section. Without one, callers fall back to
    /// their built-in command shapes; with one, only the commands it lists exist.
    bool hasCommandSection() const { return _hasCommandSection; }
    QHash<QString, CommandTemplate> commands() const { return _commands; }

private:
    bool _fail(QString *errorString, const QString &reason);

    bool    _valid          = false;
    QString _name;
    int     _schemaVersion  = 0;
    QString _mavTypeName    = QStringLiteral("MAV_TYPE_GENERIC");
    int     _mavType        = 0;
    int     _sysId          = 1;
    int     _compId         = 1;
    int     _heartbeatHz    = 1;
    bool    _reportsReady   = true;

    QMap<QString, ChannelConfig> _channels;
    QString                      _telemetrySource;
    QHash<QString, MappingRule>  _mavlinkRules;
    QStringList                  _passthrough;
    QString                      _flightModeField;
    QMap<QString, int>           _flightModeValues;
    bool                         _hasCommandSection = false;
    QHash<QString, CommandTemplate> _commands;
};
