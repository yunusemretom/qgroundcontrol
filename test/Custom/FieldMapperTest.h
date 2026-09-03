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

class FieldMapperTest : public UnitTest
{
    Q_OBJECT

public:
    FieldMapperTest() = default;

private slots:
    void _valueAtPathFindsNestedValue_test();
    void _valueAtPathMissingReturnsInvalid_test();
    void _valueAtPathThroughNullReturnsInvalid_test();
    void _appliesScale_test();
    void _appliesDegToRad_test();
    void _appliesDiscreteMap_test();
    void _usesDefaultWhenSourceMissing_test();
    void _omitsFieldWhenSourceMissingAndNoDefault_test();
    void _nullTargetDoesNotCrash_test();
    void _passthroughCopiesDeclaredFields_test();
    void _flightModeMapsKnownValue_test();
    void _flightModeUnknownValueReportsNotFound_test();

private:
    static QByteArray _profileJson();
};
