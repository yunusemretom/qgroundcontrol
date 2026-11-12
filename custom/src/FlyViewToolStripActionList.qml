/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQml
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightDisplay 1.0

ToolStripActionList {
    id: _root

    signal displayPreFlightChecklist

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    /// The external-protocol bridge reports MAV_AUTOPILOT_GENERIC, and this build only
    /// ever talks to the PX4 competition drone and the turret — so "neither PX4 nor APM"
    /// identifies the turret without QML needing the LinkType enum.
    property bool _isFixedMount: _activeVehicle && !_activeVehicle.px4Firmware && !_activeVehicle.apmFirmware

    // Two separate lists rather than per-action visibility: takeoff/land/RTL/pause each
    // compute their own visibility from the guided controller, and ANDing a condition
    // into that would mean reproducing logic that belongs to them. A fixed mount simply
    // gets a different list.
    property list<QtObject> _flightActions: [
        PreFlightCheckListShowAction { onTriggered: _root.displayPreFlightChecklist() },
        GuidedActionTakeoff { },
        GuidedActionLand { },
        GuidedActionRTL { },
        GuidedActionPause { },
        FlyViewAdditionalActionsButton { },
        GuidedToolStripAction {
            text:       _guidedController._customController.customButtonTitle
            iconSource: "/res/gear-white.svg"
            visible:    true
            enabled:    true
            actionID:   _guidedController._customController.actionCustomButton
        }
    ]

    /// A turret cannot take off, land or return home. Only the custom action stays.
    property list<QtObject> _fixedMountActions: [
        GuidedToolStripAction {
            text:       _guidedController._customController.customButtonTitle
            iconSource: "/res/gear-white.svg"
            visible:    true
            enabled:    true
            actionID:   _guidedController._customController.actionCustomButton
        }
    ]

    model: _isFixedMount ? _fixedMountActions : _flightActions
}
