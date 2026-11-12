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

class ExternalProtocolLinkTest : public UnitTest
{
    Q_OBJECT

public:
    ExternalProtocolLinkTest() = default;

private slots:
    void cleanup() override;

    void _heartbeatResumesAfterTelemetryTimeout_test();
    void _synthesizedMessagesKeepTheirOwnSequence_test();
    void _joystickUsesProfileTemplate_test();
    void _joystickSendsLatestValueAfterRateLimit_test();
    void _joystickSendsProfileFailsafeOnDisconnect_test();
    void _unsupportedCommandIsRejected_test();
    void _armWithoutCommandChannelFails_test();
    void _armRoundTripsThroughDevice_test();
};
