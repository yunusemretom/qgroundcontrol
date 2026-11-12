/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.MultiVehicleManager
import QGroundControl.ScreenTools
import QGroundControl.Palette
import QGroundControl.FactSystem
import QGroundControl.FactControls

// This indicator page is used both when showing RTK status only with no vehicle connect and when showing GPS/RTK status with a vehicle connected

ToolIndicatorPage {
    showExpand: true

    property var    activeVehicle:      QGroundControl.multiVehicleManager.activeVehicle
    property string na:                 qsTr("N/A", "No data to display")
    property string valueNA:            qsTr("--.--", "No data to display")
    property var    rtkSettings:        QGroundControl.settingsManager.rtkSettings
    property bool   useFixedPosition:   rtkSettings.useFixedBasePosition.rawValue

    // QGroundControl.gpsRtk is null in builds without serial links (iOS); fall back to an
    // inert stand-in so the RTK bindings below read "not connected" instead of throwing.
    property var    _rtk:               QGroundControl.gpsRtk ? QGroundControl.gpsRtk : _noRtk

    QtObject {
        id: _noRtk
        readonly property var connected:        ({ value: false, rawValue: false, valueString: "" })
        readonly property var active:           ({ value: false, rawValue: false, valueString: "" })
        readonly property var valid:            ({ value: false, rawValue: false, valueString: "" })
        readonly property var numSatellites:    ({ value: 0, rawValue: 0, valueString: "0" })
        readonly property var currentDuration:  ({ value: 0, rawValue: 0, valueString: "0" })
        readonly property var currentAccuracy:  ({ value: 0, rawValue: 0, valueString: "" })
        readonly property var currentLatitude:  ({ value: 0, rawValue: 0, valueString: "" })
        readonly property var currentLongitude: ({ value: 0, rawValue: 0, valueString: "" })
        readonly property var currentAltitude:  ({ value: 0, rawValue: 0, valueString: "" })
    }

    contentComponent: Component {
        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight / 2

            SettingsGroupLayout {
                heading: qsTr("Vehicle GPS Status")
                visible: activeVehicle

                LabelledLabel {
                    label:      qsTr("Satellites")
                    labelText:  activeVehicle ? activeVehicle.gps.count.valueString : na
                }

                LabelledLabel {
                    label:      qsTr("GPS Lock")
                    labelText:  activeVehicle ? activeVehicle.gps.lock.enumStringValue : na
                }

                LabelledLabel {
                    label:      qsTr("HDOP")
                    labelText:  activeVehicle ? activeVehicle.gps.hdop.valueString : valueNA
                }

                LabelledLabel {
                    label:      qsTr("VDOP")
                    labelText:  activeVehicle ? activeVehicle.gps.vdop.valueString : valueNA
                }

                LabelledLabel {
                    label:      qsTr("Course Over Ground")
                    labelText:  activeVehicle ? activeVehicle.gps.courseOverGround.valueString : valueNA
                }
            }

            SettingsGroupLayout {
                heading:    qsTr("RTK GPS Status")
                visible:    _rtk.connected.value

                QGCLabel {
                    text: (_rtk.active.value) ? qsTr("Survey-in Active") : qsTr("RTK Streaming")
                }

                LabelledLabel {
                    label:      qsTr("Satellites")
                    labelText:  _rtk.numSatellites.value
                }

                LabelledLabel {
                    label:      qsTr("Duration")
                    labelText:  _rtk.currentDuration.value + ' s'
                }

                LabelledLabel {
                    label:      _rtk.valid.value ? qsTr("Accuracy") : qsTr("Current Accuracy")
                    labelText:  _rtk.currentAccuracy.valueString + " " + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString
                    visible:    _rtk.currentAccuracy.value > 0
                }
            }
        }
    }

    expandedComponent: Component {
        SettingsGroupLayout {
            heading:        qsTr("RTK GPS Settings")

            property real sliderWidth: ScreenTools.defaultFontPixelWidth * 40

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("AutoConnect")
                fact:               QGroundControl.settingsManager.autoConnectSettings.autoConnectRTKGPS
                visible:            fact.visible
            }

            RowLayout {
                visible: rtkSettings.useFixedBasePosition.visible

                QGCRadioButton {
                    text:       qsTr("Survey-In")
                    checked:    !useFixedPosition
                    onClicked:  rtkSettings.useFixedBasePosition.rawValue = false
                }

                QGCRadioButton {
                    text: qsTr("Specify position")
                    checked:    useFixedPosition
                    onClicked:  rtkSettings.useFixedBasePosition.rawValue = true
                }
            }

            FactSlider {
                Layout.fillWidth:       true
                Layout.preferredWidth:  sliderWidth
                label:                  qsTr("Accuracy (u-blox only)")
                fact:                   QGroundControl.settingsManager.rtkSettings.surveyInAccuracyLimit
                majorTickStepSize:      0.1
                visible:                !useFixedPosition && rtkSettings.surveyInAccuracyLimit.visible
            }

            FactSlider {
                Layout.fillWidth:       true
                Layout.preferredWidth:  sliderWidth
                label:                  qsTr("Min Duration")
                fact:                   rtkSettings.surveyInMinObservationDuration
                majorTickStepSize:      10
                visible:                !useFixedPosition && rtkSettings.surveyInMinObservationDuration.visible
            }

            LabelledFactTextField {
                label:                  rtkSettings.fixedBasePositionLatitude.shortDescription
                fact:                   rtkSettings.fixedBasePositionLatitude
                visible:                useFixedPosition && rtkSettings.fixedBasePositionLatitude.visible
            }

            LabelledFactTextField {
                label:              rtkSettings.fixedBasePositionLongitude.shortDescription
                fact:               rtkSettings.fixedBasePositionLongitude
                visible:            useFixedPosition && rtkSettings.fixedBasePositionLongitude.visible
            }

            LabelledFactTextField {
                label:              rtkSettings.fixedBasePositionAltitude.shortDescription
                fact:               rtkSettings.fixedBasePositionAltitude
                visible:            useFixedPosition && rtkSettings.fixedBasePositionAltitude.visible
            }

            LabelledFactTextField {
                label:              rtkSettings.fixedBasePositionAccuracy.shortDescription
                fact:               rtkSettings.fixedBasePositionAccuracy
                visible:            useFixedPosition && rtkSettings.fixedBasePositionAccuracy.visible
            }

            LabelledButton {
                label:              qsTr("Current Base Position")
                buttonText:         enabled ? qsTr("Save") : qsTr("Not Yet Valid")
                visible:            useFixedPosition
                enabled:            _rtk.valid.value

                onClicked: {
                    rtkSettings.fixedBasePositionLatitude.rawValue  = _rtk.currentLatitude.rawValue
                    rtkSettings.fixedBasePositionLongitude.rawValue = _rtk.currentLongitude.rawValue
                    rtkSettings.fixedBasePositionAltitude.rawValue  = _rtk.currentAltitude.rawValue
                    rtkSettings.fixedBasePositionAccuracy.rawValue  = _rtk.currentAccuracy.rawValue
                }
            }
        }
    }
}
