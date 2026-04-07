/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CompetitionSettings.h"

#include <QtQml/QQmlEngine>

DECLARE_SETTINGGROUP(Competition, "Competition")
{
    qmlRegisterUncreatableType<CompetitionSettings>("QGroundControl.SettingsManager", 1, 0, "CompetitionSettings", "Reference only");
}

DECLARE_SETTINGSFACT(CompetitionSettings, serverIpAddress)
DECLARE_SETTINGSFACT(CompetitionSettings, serverPort)
DECLARE_SETTINGSFACT(CompetitionSettings, useAuthentication)
DECLARE_SETTINGSFACT(CompetitionSettings, serverUsername)
DECLARE_SETTINGSFACT(CompetitionSettings, serverPassword)
DECLARE_SETTINGSFACT(CompetitionSettings, autoReconnect)
DECLARE_SETTINGSFACT(CompetitionSettings, reconnectIntervalMs)
DECLARE_SETTINGSFACT(CompetitionSettings, systemId)
DECLARE_SETTINGSFACT(CompetitionSettings, competitionNumber)
DECLARE_SETTINGSFACT(CompetitionSettings, teamName)
DECLARE_SETTINGSFACT(CompetitionSettings, videoSaveDirectory)
