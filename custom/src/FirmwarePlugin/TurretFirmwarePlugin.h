/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Flight mode names for the profile-driven turret bridge.
 */

#pragma once

#include "FirmwarePlugin.h"

/// Names the turret's tracker states so QGC's mode indicator and mode selector show
/// ARAMA / TAKIP / KILITLI instead of a bare custom_mode number.
///
/// The numbers must match telemetry.flightMode.values in the shipped profile
/// (custom/res/protocols/celik-kubbe.json).
class TurretFirmwarePlugin : public FirmwarePlugin
{
    Q_OBJECT

public:
    explicit TurretFirmwarePlugin(QObject *parent = nullptr);

    void updateAvailableFlightModes(FlightModeList &flightModeList) final;

    /// A fixed air-defence mount is not an aircraft in transit. The stock arrow implies
    /// heading-of-travel; a crosshair reads as an emplacement holding a bearing, which is
    /// what the turret's yaw actually means.
    QString vehicleImageOpaque(const Vehicle *vehicle) const final;
    QString vehicleImageOutline(const Vehicle *vehicle) const final;
};
