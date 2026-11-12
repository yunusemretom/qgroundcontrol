/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ExternalProtocolConfiguration.h"

#include <QtCore/QSettings>

ExternalProtocolConfiguration::ExternalProtocolConfiguration(const QString &name, QObject *parent)
    : LinkConfiguration(name, parent)
{
}

ExternalProtocolConfiguration::ExternalProtocolConfiguration(const ExternalProtocolConfiguration *copy, QObject *parent)
    : LinkConfiguration(copy, parent)
    , _profilePath(copy->_profilePath)
{
}

void ExternalProtocolConfiguration::copyFrom(const LinkConfiguration *source)
{
    LinkConfiguration::copyFrom(source);

    const ExternalProtocolConfiguration *const typedSource =
            qobject_cast<const ExternalProtocolConfiguration*>(source);
    if (typedSource) {
        setProfilePath(typedSource->profilePath());
    }
}

void ExternalProtocolConfiguration::setProfilePath(const QString &path)
{
    if (_profilePath != path) {
        _profilePath = path;
        emit profilePathChanged();
    }
}

void ExternalProtocolConfiguration::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    setProfilePath(settings.value(QStringLiteral("profilePath"), _profilePath).toString());
    settings.endGroup();
}

void ExternalProtocolConfiguration::saveSettings(QSettings &settings, const QString &root) const
{
    settings.beginGroup(root);
    settings.setValue(QStringLiteral("profilePath"), _profilePath);
    settings.endGroup();
}
