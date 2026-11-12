/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CsvCodecTest.h"
#include "CsvCodec.h"

#include <QtTest/QTest>

void CsvCodecTest::_formatsJoystickCsv_test()
{
    const QByteArray line = CsvCodec::formatJoystick(0.123, -0.456, true, false);
    QCOMPARE(line, QByteArrayLiteral("0.123,-0.456,1,0\n"));
}
