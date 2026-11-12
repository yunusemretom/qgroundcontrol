/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "TurretFirmwarePlugin.h"

TurretFirmwarePlugin::TurretFirmwarePlugin(QObject *parent)
    : FirmwarePlugin(parent)
{
}

void TurretFirmwarePlugin::updateAvailableFlightModes(FlightModeList &flightModeList)
{
    flightModeList.clear();

    // Keep these in step with telemetry.flightMode.values in celik-kubbe.json.
    // canBeSet is false: Stage A is receive-only, so the operator cannot command a
    // mode change yet. Stage B flips these on once the command channel exists.
    static const struct {
        const char *name;
        uint32_t    customMode;
    } turretModes[] = {
        { "ARAMA",   0 },
        { "TAKIP",   1 },
        { "KILITLI", 2 },
    };

    for (const auto &turretMode : turretModes) {
        // FirmwareFlightMode(name, custom_mode, canBeSet = true, advanced = false)
        // — see src/FirmwarePlugin/FirmwarePlugin.h:32
        flightModeList.append(FirmwareFlightMode(tr(turretMode.name), turretMode.customMode, true /* canBeSet */));
    }

    // Hand the list to the base class. This is what fills _modeEnumToString, which
    // FirmwarePlugin::flightMode() reads to turn a custom_mode number into a name —
    // without it the toolbar falls back to "Custom:0x0" no matter what we append here.
    _updateFlightModeList(flightModeList);
}

QString TurretFirmwarePlugin::vehicleImageOpaque(const Vehicle *vehicle) const
{
    Q_UNUSED(vehicle);
    return QStringLiteral("/qmlimages/crossHair.svg");
}

QString TurretFirmwarePlugin::vehicleImageOutline(const Vehicle *vehicle) const
{
    Q_UNUSED(vehicle);
    return QStringLiteral("/qmlimages/crossHair.svg");
}
