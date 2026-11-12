/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools
import QGroundControl.AppSettings 1.0

/// Live state of the external-protocol bridge — the fixed-emplacement counterpart to
/// the flight instruments, which say nothing useful about a turret.
SettingsPage {
    id: root

    property string _noValue:   qsTr("—")
    property string _noLink:    qsTr("Not connected")

    QGCPalette { id: qgcPal }

    /// The live ExternalProtocolLink, found by duck-typing rather than by link type:
    /// the LinkType enum value is not exposed to QML, but only this link publishes
    /// a profileName.
    property var _link: null

    function _findLink() {
        var configs = QGroundControl.linkManager.linkConfigurations
        for (var i = 0; i < configs.count; i++) {
            var config = configs.get(i)
            if (config.link && config.link.profileName !== undefined) {
                return config.link
            }
        }
        return null
    }

    Component.onCompleted: _link = _findLink()

    Connections {
        target: QGroundControl.linkManager.linkConfigurations
        function onCountChanged() { root._link = root._findLink() }
    }

    // The link object is destroyed on disconnect, so re-resolve whenever the active
    // vehicle changes rather than holding a dangling reference.
    Connections {
        target: QGroundControl.multiVehicleManager
        function onActiveVehicleChanged() { root._link = root._findLink() }
    }

    property var _data: _link ? _link.passthroughData : ({})

    function _field(key, suffix) {
        if (!_link || _data[key] === undefined || _data[key] === null) {
            return _noValue
        }
        return suffix ? _data[key] + suffix : String(_data[key])
    }

    function _boolField(key) {
        if (!_link || _data[key] === undefined) {
            return _noValue
        }
        return _data[key] ? qsTr("Yes") : qsTr("No")
    }

    function _number(key, decimals, suffix) {
        if (!_link || _data[key] === undefined || _data[key] === null) {
            return _noValue
        }
        return Number(_data[key]).toFixed(decimals) + (suffix ? suffix : "")
    }

    SettingsGroupLayout {
        heading:            qsTr("Bridge")
        headingDescription: qsTr("Ports, field mappings and commands all come from the profile file selected in Comm Links.")

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Profile")
            labelText:          _link ? _link.profileName : _noLink
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Telemetry")
            labelText:          _link ? (_link.telemetryAlive ? qsTr("Receiving") : qsTr("Silent")) : _noLink
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Packets received")
            labelText:          _link ? _link.packetCount : _noValue
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Malformed packets")
            labelText:          _link ? _link.malformedCount : _noValue
        }
    }

    SettingsGroupLayout {
        heading:  qsTr("Channels")
        visible:  !!_link

        Repeater {
            model: _link ? _link.channelStates : []

            RowLayout {
                Layout.fillWidth: true
                spacing:          ScreenTools.defaultFontPixelWidth

                Rectangle {
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight * 0.6
                    Layout.preferredHeight: Layout.preferredWidth
                    radius:             width / 2
                    Layout.alignment:   Qt.AlignVCenter
                    color:              modelData.open ? qgcPal.colorGreen : qgcPal.colorGrey
                }

                QGCLabel {
                    text:                   modelData.name
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 12
                }

                QGCLabel {
                    text:                   modelData.transport + " " + modelData.direction
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 10
                    color:                  qgcPal.colorGrey
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               modelData.endpoint
                }
            }
        }
    }

    SettingsGroupLayout {
        heading:            qsTr("Turret")
        headingDescription: qsTr("A fixed emplacement has no altitude or ground speed of its own. \"Target altitude\" is the turret's estimate for the tracked object, and \"Slew rate\" is how fast the mount is turning.")
        visible:            !!_link

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Tracking")
            labelText:          _boolField("tracking")
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Locked")
            labelText:          _boolField("locked")
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Slew rate")
            labelText:          _number("speed", 1, qsTr("°/s"))
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Target altitude")
            labelText:          _number("altitude", 1, qsTr(" m"))
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Shots fired")
            labelText:          _field("shots")
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Detections")
            labelText:          _field("detections")
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Camera frame rate")
            labelText:          _number("fps", 1, qsTr(" fps"))
        }

        LabelledLabel {
            Layout.fillWidth:   true
            label:              qsTr("Link quality")
            labelText:          _number("rssi", 0, qsTr(" dBm"))
        }
    }

    SettingsGroupLayout {
        id:       targetGroup
        heading:  qsTr("Current target")
        visible:  !!_link

        // Referenced by id, not `parent`: SettingsGroupLayout reparents its children into
        // an inner layout, so `parent._target` was always undefined and the target was
        // never shown.
        property var _target: _data["target"] ? _data["target"] : null

        QGCLabel {
            Layout.fillWidth:   true
            wrapMode:           Text.WordWrap
            text:               qsTr("No target being tracked.")
            visible:            !targetGroup._target
            color:              qgcPal.colorGrey
        }

        LabelledLabel {
            Layout.fillWidth:   true
            visible:            !!targetGroup._target
            label:              qsTr("Label")
            labelText:          targetGroup._target && targetGroup._target.label ? targetGroup._target.label : _noValue
        }

        LabelledLabel {
            Layout.fillWidth:   true
            visible:            !!targetGroup._target
            label:              qsTr("Faction")
            labelText:          targetGroup._target && targetGroup._target.faction ? targetGroup._target.faction : _noValue
        }

        LabelledLabel {
            Layout.fillWidth:   true
            visible:            !!targetGroup._target
            label:              qsTr("Track ID")
            labelText:          targetGroup._target && targetGroup._target.track_id !== undefined ? targetGroup._target.track_id : _noValue
        }

        LabelledLabel {
            Layout.fillWidth:   true
            visible:            !!targetGroup._target
            label:              qsTr("Range")
            labelText:          targetGroup._target && targetGroup._target.range_m !== undefined
                                    ? Number(targetGroup._target.range_m).toFixed(1) + qsTr(" m") : _noValue
        }

        LabelledLabel {
            Layout.fillWidth:   true
            visible:            !!targetGroup._target
            label:              qsTr("In range window")
            labelText:          targetGroup._target
                                    ? (targetGroup._target.in_range_window ? qsTr("Yes") : qsTr("No")) : _noValue
        }
    }
}
