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
};
