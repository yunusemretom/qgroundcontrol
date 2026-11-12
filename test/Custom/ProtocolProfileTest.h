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
    void _rejectsNonObjectRoot_test();
    void _rejectsNonPositiveHeartbeat_test();
    void _rejectsProfileWithNoChannels_test();
    void _rejectsChannelWithInvalidDirection_test();
    void _rejectsChannelWithUnparsableEndpoint_test();
    void _rejectsUnknownTelemetrySource_test();
    void _rejectsMappingRuleMissingFrom_test();
    void _rejectsMappingRuleWithUnsupportedUnit_test();
    void _parsesChannelEndpoint_test();
    void _parsesMappingRuleKinds_test();
    void _parsesFlightModeValues_test();
    void _missingOptionalSectionsAreEmpty_test();
    void _shippedCelikKubbeProfileIsValid_test();
    void _parsesCommandTemplates_test();
    void _rejectsCommandOnUnknownChannel_test();
    void _rejectsCommandOnInboundChannel_test();
    void _rejectsCommandWithoutTemplateObject_test();
    void _parsesJoystickChannelOptions_test();
    void _resolvesMavType_test();
    void _rejectsUnknownMavType_test();

private:
    static QByteArray _minimalProfile();
};
