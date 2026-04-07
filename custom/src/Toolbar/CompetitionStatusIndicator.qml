/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools

Item {
    id: control

    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width:          statusPill.width

    property var  _serverClient:  QGroundControl.corePlugin ? QGroundControl.corePlugin.competitionServerClient : null
    property bool showIndicator:  _serverClient
    property int  _serverClockMs: (_serverClient && _serverClient.serverTimeValid) ? _serverClient.currentServerTimeMs() : 0
    property string _lockTargetIdText: ""
    property string _lockWindowMsText: "4000"
    property string _kamikazeTargetIdText: ""
    property string _kamikazeReasonText: ""

    function _connectionState() {
        return _serverClient ? _serverClient.connectionState : 0
    }

    function _stateTextShort() {
        const state = _connectionState()
        if (state === 2) {
            return "ON"
        }
        if (state === 1) {
            return "CONN"
        }
        if (state === 3) {
            return "ERR"
        }
        return "OFF"
    }

    function _stateTextLong() {
        const state = _connectionState()
        if (state === 2) {
            return qsTr("Connected")
        }
        if (state === 1) {
            return qsTr("Connecting")
        }
        if (state === 3) {
            return qsTr("Error")
        }
        return qsTr("Disconnected")
    }

    function _stateColor() {
        const state = _connectionState()
        if (state === 2) {
            return qgcPal.colorGreen
        }
        if (state === 1) {
            return qgcPal.colorOrange
        }
        if (state === 3) {
            return qgcPal.colorRed
        }
        return qgcPal.colorGrey
    }

    function _activeNoFlyCount() {
        if (!_serverClient) {
            return 0
        }

        let count = 0
        for (let i = 0; i < _serverClient.noFlyZones.length; i++) {
            const zone = _serverClient.noFlyZones[i]
            if (zone && (zone.active === true || zone.isActive === true || zone.enabled === true)) {
                count++
            }
        }
        return count
    }

    function _threatColor() {
        return _activeNoFlyCount() > 0 ? qgcPal.colorRed : qgcPal.text
    }

    function _formatClock(ms) {
        if (ms <= 0) {
            return "--:--:--.---"
        }

        const dayMs = 24 * 60 * 60 * 1000
        const wrapped = ((Math.floor(ms) % dayMs) + dayMs) % dayMs
        const h = Math.floor(wrapped / 3600000)
        const m = Math.floor((wrapped % 3600000) / 60000)
        const s = Math.floor((wrapped % 60000) / 1000)
        const milli = wrapped % 1000

        return String(h).padStart(2, "0") + ":" +
               String(m).padStart(2, "0") + ":" +
               String(s).padStart(2, "0") + "." +
               String(milli).padStart(3, "0")
    }

    function _lockText() {
        if (!_serverClient || !_serverClient.lockStatusActive) {
            return "L:OFF"
        }
        const remainingSec = Math.max(0, Math.ceil(Number(_serverClient.lockRemainingMs) / 1000.0))
        return "L:" + String(remainingSec) + "s"
    }

    function _toInt(value, fallbackValue) {
        const parsed = parseInt(value, 10)
        return isNaN(parsed) ? fallbackValue : parsed
    }

    function _isPositiveIntText(value) {
        const parsed = parseInt(value, 10)
        return !isNaN(parsed) && parsed > 0
    }

    function _applyLockPayload() {
        if (!_serverClient) {
            return
        }

        const payload = {
            targetId: _toInt(_lockTargetIdText, 0),
            lockWindowMs: Math.max(0, _toInt(_lockWindowMsText, 4000)),
            requestTimeMs: Date.now(),
            source: "qgc-toolbar"
        }
        _serverClient.setLockPayload(payload)
    }

    function _applyKamikazePayload() {
        if (!_serverClient) {
            return
        }

        const payload = {
            targetId: _toInt(_kamikazeTargetIdText, 0),
            reason: _kamikazeReasonText.trim(),
            requestTimeMs: Date.now(),
            source: "qgc-toolbar"
        }
        _serverClient.setKamikazePayload(payload)
    }

    Timer {
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            if (_serverClient && _serverClient.serverTimeValid) {
                _serverClockMs = _serverClient.currentServerTimeMs()
            }
        }
    }

    Rectangle {
        id:                 statusPill
        anchors.verticalCenter: parent.verticalCenter
        radius:             ScreenTools.defaultFontPixelWidth * 0.25
        height:             parent.height * 0.72
        width:              statusRow.implicitWidth + ScreenTools.defaultFontPixelWidth
        color:              Qt.rgba(_stateColor().r, _stateColor().g, _stateColor().b, 0.18)
        border.width:       1
        border.color:       _stateColor()

        Row {
            id:             statusRow
            anchors.centerIn: parent
            spacing:        ScreenTools.defaultFontPixelWidth * 0.5

            QGCLabel {
                text:       "YRS"
                color:      _stateColor()
                font.bold:  true
            }

            QGCLabel {
                text:       _stateTextShort()
                color:      qgcPal.text
                font.bold:  true
            }

            QGCLabel {
                text:       _lockText()
                color:      qgcPal.text
            }

            QGCLabel {
                text:       "NFZ:" + String(_activeNoFlyCount())
                color:      _threatColor()
                font.bold:  _activeNoFlyCount() > 0
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: mainWindow.showIndicatorDrawer(indicatorPage, control)
    }

    Component {
        id: indicatorPage

        ToolIndicatorPage {
            showExpand: false

            contentComponent: SettingsGroupLayout {
                heading: qsTr("Competition Status")

                LabelledLabel {
                    label:      qsTr("Connection:")
                    labelText:  _stateTextLong()
                }

                LabelledLabel {
                    label:      qsTr("Host:")
                    labelText:  _serverClient ? _serverClient.host : "-"
                }

                LabelledLabel {
                    label:      qsTr("Port:")
                    labelText:  _serverClient ? String(_serverClient.port) : "-"
                }

                LabelledLabel {
                    label:      qsTr("Server Clock:")
                    labelText:  _formatClock(_serverClockMs)
                }

                LabelledLabel {
                    label:      qsTr("Rivals:")
                    labelText:  _serverClient ? String(_serverClient.rivalList.length) : "0"
                }

                LabelledLabel {
                    label:      qsTr("No-Fly Zones:")
                    labelText:  _serverClient ? String(_serverClient.noFlyZones.length) : "0"
                }

                LabelledLabel {
                    label:      qsTr("Active NFZ:")
                    labelText:  String(_activeNoFlyCount())
                }

                LabelledLabel {
                    label:      qsTr("Boundary Vertices:")
                    labelText:  _serverClient ? String(_serverClient.boundaryPolygon.length) : "0"
                }

                LabelledLabel {
                    label:      qsTr("Lock:")
                    labelText:  _serverClient && _serverClient.lockStatusActive
                                ? qsTr("Active (%1 ms)").arg(_serverClient.lockRemainingMs)
                                : qsTr("Inactive")
                }

                LabelledLabel {
                    label:      qsTr("Lock Payload:")
                    labelText:  _serverClient && _serverClient.lockPayloadActive ? qsTr("Queued") : qsTr("None")
                }

                LabelledLabel {
                    label:      qsTr("Kamikaze Payload:")
                    labelText:  _serverClient && _serverClient.kamikazePayloadActive ? qsTr("Queued") : qsTr("None")
                }

                LabelledLabel {
                    label:      qsTr("Last Error:")
                    labelText:  _serverClient && _serverClient.lastErrorString.length > 0
                                ? _serverClient.lastErrorString
                                : "-"
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCButton {
                        text: qsTr("Connect")
                        enabled: _serverClient
                        onClicked: {
                            if (_serverClient) {
                                _serverClient.connectToServer()
                            }
                        }
                    }

                    QGCButton {
                        text: qsTr("Disconnect")
                        enabled: _serverClient
                        onClicked: {
                            if (_serverClient) {
                                _serverClient.disconnectFromServer()
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: qgcPal.groupBorder
                }

                QGCLabel {
                    text: qsTr("Command Payload")
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCLabel {
                        text: qsTr("Lock Target ID")
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                    }

                    QGCTextField {
                        Layout.fillWidth: true
                        text: _lockTargetIdText
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextChanged: _lockTargetIdText = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCLabel {
                        text: qsTr("Lock Window (ms)")
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                    }

                    QGCTextField {
                        Layout.fillWidth: true
                        text: _lockWindowMsText
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextChanged: _lockWindowMsText = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCButton {
                        text: qsTr("Send Lock")
                        enabled: _serverClient && _serverClient.connected && _isPositiveIntText(_lockTargetIdText)
                        onClicked: _applyLockPayload()
                    }

                    QGCButton {
                        text: qsTr("Clear Lock")
                        enabled: _serverClient && _serverClient.lockPayloadActive
                        onClicked: {
                            if (_serverClient) {
                                _serverClient.clearLockPayload()
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCLabel {
                        text: qsTr("Kamikaze Target ID")
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                    }

                    QGCTextField {
                        Layout.fillWidth: true
                        text: _kamikazeTargetIdText
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextChanged: _kamikazeTargetIdText = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCLabel {
                        text: qsTr("Kamikaze Reason")
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                    }

                    QGCTextField {
                        Layout.fillWidth: true
                        text: _kamikazeReasonText
                        onTextChanged: _kamikazeReasonText = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCButton {
                        text: qsTr("Send Kamikaze")
                        enabled: _serverClient && _serverClient.connected && _isPositiveIntText(_kamikazeTargetIdText)
                        onClicked: _applyKamikazePayload()
                    }

                    QGCButton {
                        text: qsTr("Clear Kamikaze")
                        enabled: _serverClient && _serverClient.kamikazePayloadActive
                        onClicked: {
                            if (_serverClient) {
                                _serverClient.clearKamikazePayload()
                            }
                        }
                    }
                }
            }
        }
    }
}
