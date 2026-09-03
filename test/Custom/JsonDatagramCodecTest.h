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

class JsonDatagramCodecTest : public UnitTest
{
    Q_OBJECT

public:
    JsonDatagramCodecTest() = default;

private slots:
    void _decodesSingleObject_test();
    void _decodesNestedObject_test();
    void _malformedInputYieldsNothing_test();
    void _malformedInputIsCounted_test();
    void _nonObjectRootYieldsNothing_test();
    void _emptyInputYieldsNothing_test();
    void _encodeRoundTrips_test();
};
