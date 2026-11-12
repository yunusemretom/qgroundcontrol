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

static QVariantMap joystickValues()
{
    return {
        { QStringLiteral("pitch"), 0.5 },
        { QStringLiteral("yaw"),   -0.25 },
        { QStringLiteral("fire"),  1 },
        { QStringLiteral("arm"),   0 },
    };
}

void CsvCodecTest::_formatsTemplateWithPrecision_test()
{
    // The shipped celik-kubbe.json template.
    QCOMPARE(CsvCodec::formatTemplate(QStringLiteral("{pitch:.3f},{yaw:.3f},{fire:.0f},{arm:.0f}"), joystickValues()),
             QByteArrayLiteral("0.500,-0.250,1,0\n"));
}

void CsvCodecTest::_formatsTemplateWithoutPrecision_test()
{
    QCOMPARE(CsvCodec::formatTemplate(QStringLiteral("P={pitch};F={fire}"), joystickValues()),
             QByteArrayLiteral("P=0.5;F=1\n"));
}

/// A typo in the profile must show up on the wire, not silently become a zero that
/// moves hardware.
void CsvCodecTest::_templateKeepsUnknownPlaceholderVisible_test()
{
    QCOMPARE(CsvCodec::formatTemplate(QStringLiteral("{pitch:.1f},{roll:.1f}"), joystickValues()),
             QByteArrayLiteral("0.5,{roll:.1f}\n"));
}

void CsvCodecTest::_terminatesFrameWithNewlineOnce_test()
{
    QCOMPARE(CsvCodec::terminate(QStringLiteral("0,0,0,0")), QByteArrayLiteral("0,0,0,0\n"));
    QCOMPARE(CsvCodec::terminate(QStringLiteral("0,0,0,0\n")), QByteArrayLiteral("0,0,0,0\n"));
}
