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

class CommandTranslatorTest : public UnitTest
{
    Q_OBJECT

public:
    CommandTranslatorTest() = default;

private slots:
    void _translatesArmAndDisarm_test();
    void _translatesSetMode_test();
    void _parsesAckResponse_test();
    void _usesProfileTemplate_test();
    void _substitutesValuePlaceholder_test();
    void _addsIncrementingIdAndTimestamp_test();
    void _keepsIdAndTimestampFromTemplate_test();
    void _matchesAckById_test();
    void _reportsFailedAckStatus_test();
    void _omittedCommandIsUnsupported_test();
    void _routesCommandToItsChannel_test();
};
