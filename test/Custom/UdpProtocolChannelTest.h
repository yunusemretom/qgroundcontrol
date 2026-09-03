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

class UdpProtocolChannelTest : public UnitTest
{
    Q_OBJECT

public:
    UdpProtocolChannelTest() = default;

private slots:
    void _opensAndBindsPort_test();
    void _receivesDatagram_test();
    void _rejectsInvalidPort_test();
    void _closeStopsReceiving_test();
    void _emitsTimeoutWhenSilent_test();

    // Additional coverage beyond the initial five, closing branches those five leave
    // unexercised: see the comment above each definition in the .cc for what it targets.
    void _secondOpenIsIdempotent_test();
    void _outDirectionSendsWithoutBinding_test();
    void _rejectsZeroPortEvenWhenMarkedValid_test();
    void _bindFailureEmitsError_test();
    void _timeoutFiresOnceThenAgainAfterRecovery_test();
};
