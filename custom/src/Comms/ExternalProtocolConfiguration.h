/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "LinkConfiguration.h"

/// Link configuration whose only setting is which protocol profile to load.
/// Everything else — ports, mappings, message set — comes from that file.
class ExternalProtocolConfiguration : public LinkConfiguration
{
    Q_OBJECT
    Q_PROPERTY(QString profilePath READ profilePath WRITE setProfilePath NOTIFY profilePathChanged)

public:
    explicit ExternalProtocolConfiguration(const QString &name, QObject *parent = nullptr);
    explicit ExternalProtocolConfiguration(const ExternalProtocolConfiguration *copy, QObject *parent = nullptr);

    LinkType type() const final { return LinkConfiguration::TypeExternalProtocol; }
    void copyFrom(const LinkConfiguration *source) final;
    void loadSettings(QSettings &settings, const QString &root) final;
    void saveSettings(QSettings &settings, const QString &root) const final;
    QString settingsURL() const final { return QStringLiteral("ExternalProtocolSettings.qml"); }
    QString settingsTitle() const final { return tr("External Protocol Settings"); }

    QString profilePath() const { return _profilePath; }
    void setProfilePath(const QString &path);

signals:
    void profilePathChanged();

private:
    QString _profilePath = QStringLiteral(":/Custom/protocols/celik-kubbe.json");
};
