/****************************************************************************
 *
 * Remote connection settings for JSON POST telemetry forwarding.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AppSettings 1.0

SettingsPage {
    id: root

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    Settings {
        id: remoteSettings
        category: "CustomRemoteConnection"

        property bool remoteForwardEnabled: false
        property string remoteEndpointUrl: ""
        property string remoteApiKey: ""
        property int remoteSendIntervalSec: 2
    }

    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Remote Telemetry Connection")
        headingDescription: qsTr("Configure JSON POST endpoint and forwarding behavior.")

        QGCCheckBox {
            text: qsTr("Enable remote forwarding")
            checked: remoteSettings.remoteForwardEnabled
            onClicked: remoteSettings.remoteForwardEnabled = checked
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 6
            columnSpacing: 8

            QGCLabel { text: qsTr("Endpoint URL") }
            QGCTextField {
                Layout.fillWidth: true
                placeholderText: "https://example.com/api/telemetry"
                text: remoteSettings.remoteEndpointUrl
                onEditingFinished: remoteSettings.remoteEndpointUrl = text.trim()
            }

            QGCLabel { text: qsTr("API key (optional)") }
            QGCTextField {
                Layout.fillWidth: true
                echoMode: TextInput.Password
                text: remoteSettings.remoteApiKey
                onEditingFinished: remoteSettings.remoteApiKey = text.trim()
            }

            QGCLabel { text: qsTr("Send interval (sec)") }
            SpinBox {
                from: 1
                to: 60
                value: remoteSettings.remoteSendIntervalSec
                editable: true
                onValueChanged: remoteSettings.remoteSendIntervalSec = value
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: _activeVehicle
                  ? qsTr("Active vehicle: %1").arg(_activeVehicle.id)
                  : qsTr("No active vehicle. Forwarding starts when a vehicle connects.")
        }
    }
}
