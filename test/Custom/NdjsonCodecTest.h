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

class NdjsonCodecTest : public UnitTest
{
    Q_OBJECT

public:
    NdjsonCodecTest() = default;

private slots:
    void _decodesSingleLine_test();
    void _decodesMultipleLines_test();
    void _handlesChunkedStream_test();
    void _encodeAppendsNewline_test();
};
