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

class CsvCodecTest : public UnitTest
{
    Q_OBJECT

public:
    CsvCodecTest() = default;

private slots:
    void _formatsJoystickCsv_test();
};
