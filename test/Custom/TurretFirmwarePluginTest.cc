/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "TurretFirmwarePluginTest.h"
#include "ProtocolProfile.h"
#include "TurretFirmwarePlugin.h"

#include <QtCore/QFile>
#include <QtTest/QTest>

/// Asks the plugin for its modes the way Vehicle does, then reads a mode name back.
/// updateAvailableFlightModes() must feed the base class, or flightMode() has no
/// lookup table and answers "Custom:0x..." for everything.
static QString modeNameFor(TurretFirmwarePlugin &plugin, uint32_t customMode)
{
    FlightModeList modes;
    plugin.updateAvailableFlightModes(modes);

    return plugin.flightMode(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, customMode);
}

void TurretFirmwarePluginTest::_namesTrackerStatesInsteadOfCustomHex_test()
{
    TurretFirmwarePlugin plugin;

    // This is the exact defect that showed "Custom:0x0" in the toolbar: the override
    // appended to the caller's list but never handed it to _updateFlightModeList(),
    // so the custom_mode -> name mapping stayed empty.
    QCOMPARE(modeNameFor(plugin, 0), QStringLiteral("ARAMA"));
    QCOMPARE(modeNameFor(plugin, 1), QStringLiteral("TAKIP"));
    QCOMPARE(modeNameFor(plugin, 2), QStringLiteral("KILITLI"));
}

void TurretFirmwarePluginTest::_unknownCustomModeStillFallsBackToHex_test()
{
    TurretFirmwarePlugin plugin;

    // A mode the turret never declares must not silently render as some other mode.
    const QString name = modeNameFor(plugin, 99);
    QVERIFY2(name.startsWith(QStringLiteral("Custom:")), qPrintable(name));
}

void TurretFirmwarePluginTest::_flightModeListMatchesShippedProfile_test()
{
    QFile file(QStringLiteral(":/Custom/protocols/celik-kubbe.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Shipped profile is missing from the Qt resources");

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(file.readAll(), nullptr));

    // The triple is written twice by design (profile JSON and C++). This test is what
    // stops the two drifting apart silently.
    TurretFirmwarePlugin plugin;
    const QMap<QString, int> profileModes = profile.flightModeValues();

    for (auto it = profileModes.constBegin(); it != profileModes.constEnd(); ++it) {
        QCOMPARE(modeNameFor(plugin, static_cast<uint32_t>(it.value())), it.key());
    }
}
