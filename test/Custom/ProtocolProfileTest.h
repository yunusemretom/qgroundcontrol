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

class ProtocolProfileTest : public UnitTest
{
    Q_OBJECT

public:
    ProtocolProfileTest() = default;

private slots:
    void _loadsValidProfile_test();
    void _rejectsMalformedJson_test();
    void _rejectsProfileWithoutName_test();
    void _rejectsRuleWithBothScaleAndUnit_test();
    void _parsesChannelEndpoint_test();
    void _parsesMappingRuleKinds_test();
    void _parsesFlightModeValues_test();
    void _missingOptionalSectionsAreEmpty_test();

private:
    static QByteArray _minimalProfile();
};
