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

class TcpProtocolChannelTest : public UnitTest
{
    Q_OBJECT

public:
    TcpProtocolChannelTest() = default;

private slots:
    void _connectsAndReceives_test();
    void _closeDoesNotReconnect_test();
    void _reconnectsAfterRemoteClose_test();
    void _timeoutFiresOncePerSilence_test();
};
