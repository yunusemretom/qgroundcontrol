/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

class TurretFirmwarePluginTest : public UnitTest
{
    Q_OBJECT

public:
    TurretFirmwarePluginTest() = default;

private slots:
    void _namesTrackerStatesInsteadOfCustomHex_test();
    void _unknownCustomModeStillFallsBackToHex_test();
    void _flightModeListMatchesShippedProfile_test();
};
