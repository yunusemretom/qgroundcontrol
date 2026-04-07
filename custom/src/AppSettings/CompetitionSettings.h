/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "SettingsGroup.h"

class CompetitionSettings : public SettingsGroup
{
    Q_OBJECT

public:
    CompetitionSettings(QObject* parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(serverIpAddress)
    DEFINE_SETTINGFACT(serverPort)
    DEFINE_SETTINGFACT(useAuthentication)
    DEFINE_SETTINGFACT(serverUsername)
    DEFINE_SETTINGFACT(serverPassword)
    DEFINE_SETTINGFACT(autoReconnect)
    DEFINE_SETTINGFACT(reconnectIntervalMs)
    DEFINE_SETTINGFACT(systemId)
    DEFINE_SETTINGFACT(competitionNumber)
    DEFINE_SETTINGFACT(teamName)
    DEFINE_SETTINGFACT(videoSaveDirectory)
};
