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

class MAVLinkSynthesizerTest : public UnitTest
{
    Q_OBJECT

public:
    MAVLinkSynthesizerTest() = default;

private slots:
    void _heartbeatCarriesProfileIdentity_test();
    void _heartbeatCarriesCustomMode_test();
    void _heartbeatCarriesProfileMavType_test();
    void _buildsGlobalPositionFromMappedValues_test();
    void _buildsAttitudeFromMappedValues_test();
    void _omittedFieldKeepsPreviousValue_test();
    void _emptyMappingProducesNoMessages_test();
    void _buildsSysStatusFromMappedValues_test();
    void _buildsGpsRawIntReusesGlobalPositionState_test();
    void _buildsVfrHudFromMappedValues_test();

private:
    static QByteArray _profileJson();

    /// A second profile that additionally maps SYS_STATUS, GPS_RAW_INT and VFR_HUD,
    /// so those messages can be exercised without disturbing the assertions the
    /// GLOBAL_POSITION_INT / ATTITUDE tests make against the shared _profileJson().
    static QByteArray _fullProfileJson();
};
