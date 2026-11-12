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

class CompetitionServerClientTest : public UnitTest
{
    Q_OBJECT

public:
    CompetitionServerClientTest() = default;

private slots:
    void _sendsHelloAndAuthOnConnect_test();
    void _parsesCompetitionPacket_test();
    void _answersPing_test();
    void _packetWithoutLockFieldsKeepsLockState_test();
    void _discardsOversizedUnterminatedLine_test();
    void _manualDisconnectDoesNotReconnect_test();
};
