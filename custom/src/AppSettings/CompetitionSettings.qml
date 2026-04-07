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
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.Palette
import QGroundControl.AppSettings 1.0

SettingsPage {
    id: root

    readonly property real _labelWidth: ScreenTools.defaultFontPixelWidth * 20

    property var _competitionSettings: QGroundControl.corePlugin ? QGroundControl.corePlugin.competitionSettings : null
    property var _serverClient: QGroundControl.corePlugin ? QGroundControl.corePlugin.competitionServerClient : null

    property var _serverIpFact: _competitionSettings ? _competitionSettings.serverIpAddress : null
    property var _serverPortFact: _competitionSettings ? _competitionSettings.serverPort : null
    property var _useAuthenticationFact: _competitionSettings ? _competitionSettings.useAuthentication : null
    property var _serverUsernameFact: _competitionSettings ? _competitionSettings.serverUsername : null
    property var _serverPasswordFact: _competitionSettings ? _competitionSettings.serverPassword : null
    property var _autoReconnectFact: _competitionSettings ? _competitionSettings.autoReconnect : null
    property var _reconnectIntervalFact: _competitionSettings ? _competitionSettings.reconnectIntervalMs : null
    property var _systemIdFact: _competitionSettings ? _competitionSettings.systemId : null
    property var _competitionNumberFact: _competitionSettings ? _competitionSettings.competitionNumber : null
    property var _teamNameFact: _competitionSettings ? _competitionSettings.teamName : null
    property var _videoSaveDirectoryFact: _competitionSettings ? _competitionSettings.videoSaveDirectory : null

    property bool _showPassword: false

    QGCPalette {
        id: qgcPal
    }

    function _connectionStateText() {
        if (!_serverClient) {
            return qsTr("Sunucu istemcisi olusturulmadi")
        }

        const disconnectedState = 0
        const connectingState = 1
        const connectedState = 2
        const errorState = 3

        switch (_serverClient.connectionState) {
        case connectedState:
            return qsTr("Bagli")
        case connectingState:
            return qsTr("Baglaniyor")
        case errorState:
            return _serverClient.lastErrorString && _serverClient.lastErrorString.length > 0
                ? qsTr("Hata: %1").arg(_serverClient.lastErrorString)
                : qsTr("Baglanti hatasi")
        default:
            return qsTr("Bagli degil")
        }
    }

    function _connectionColor() {
        if (!_serverClient) {
            return qgcPal.warningText
        }

        if (_serverClient.connected) {
            return qgcPal.colorGreen
        }

        if (_serverClient.connectionState === 1) {
            return qgcPal.colorOrange
        }

        if (_serverClient.connectionState === 3) {
            return qgcPal.warningText
        }

        return qgcPal.text
    }

    function _applyAndReconnect() {
        if (_serverClient) {
            _serverClient.disconnectFromServer()
            _serverClient.connectToServer()
        }
    }

    function _authEnabled() {
        return _useAuthenticationFact && _useAuthenticationFact.rawValue === true
    }

    QGCLabel {
        Layout.fillWidth: true
        visible: !_competitionSettings
        wrapMode: Text.WordWrap
        color: qgcPal.warningText
        text: qsTr("Yarisma ayarlari yuklenemedi. Lutfen uygulamayi yeniden baslatin veya custom plugin ayarlarinin derlemeye dahil oldugunu kontrol edin.")
    }

    SettingsGroupLayout {
        Layout.fillWidth: true
        visible: !!_competitionSettings
        heading: qsTr("Yarisma Sunucusu")
        headingDescription: qsTr("IP, port, kimlik dogrulama ve yeniden baglanma ayarlari")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _serverIpFact ? _serverIpFact.shortDescription : qsTr("Sunucu IP")
            }

            FactTextField {
                Layout.fillWidth: true
                fact: _serverIpFact
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _serverPortFact ? _serverPortFact.shortDescription : qsTr("Sunucu Port")
            }

            FactTextField {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                fact: _serverPortFact
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            FactCheckBox {
                Layout.fillWidth: true
                fact: _useAuthenticationFact
                text: _useAuthenticationFact ? _useAuthenticationFact.shortDescription : qsTr("Kimlik Dogrulama")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: _authEnabled()
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _serverUsernameFact ? _serverUsernameFact.shortDescription : qsTr("Kullanici Adi")
            }

            FactTextField {
                Layout.fillWidth: true
                fact: _serverUsernameFact
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: _authEnabled()
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _serverPasswordFact ? _serverPasswordFact.shortDescription : qsTr("Sifre")
            }

            FactTextField {
                id: passwordField
                Layout.fillWidth: true
                fact: _serverPasswordFact
                echoMode: _showPassword ? TextInput.Normal : TextInput.Password
            }

            QGCCheckBox {
                text: qsTr("Goster")
                checked: _showPassword
                onClicked: _showPassword = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            FactCheckBox {
                Layout.fillWidth: true
                fact: _autoReconnectFact
                text: _autoReconnectFact ? _autoReconnectFact.shortDescription : qsTr("Otomatik Yeniden Baglan")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _reconnectIntervalFact ? _reconnectIntervalFact.shortDescription : qsTr("Yeniden Baglanma Araligi")
            }

            FactTextField {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                fact: _reconnectIntervalFact
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _systemIdFact ? _systemIdFact.shortDescription : qsTr("Takim/Sistem ID")
            }

            FactTextField {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                fact: _systemIdFact
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: _connectionColor()
            text: qsTr("Durum: %1").arg(_connectionStateText())
        }

        QGCLabel {
            Layout.fillWidth: true
            visible: _authEnabled()
            wrapMode: Text.WordWrap
            color: qgcPal.text
            text: qsTr("Kimlik dogrulama acik. Baglanti kurulunca istemci auth paketi gonderir.")
        }

        RowLayout {
            Layout.fillWidth: true

            QGCButton {
                text: qsTr("Sunucuya Baglan")
                enabled: _serverClient
                onClicked: {
                    if (_serverClient) {
                        _serverClient.connectToServer()
                    }
                }
            }

            QGCButton {
                text: qsTr("Kopar")
                enabled: _serverClient
                onClicked: {
                    if (_serverClient) {
                        _serverClient.disconnectFromServer()
                    }
                }
            }

            QGCButton {
                text: qsTr("Uygula ve Yeniden Baglan")
                enabled: _serverClient
                onClicked: _applyAndReconnect()
            }
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth: true
        visible: !!_competitionSettings
        heading: qsTr("Takim Bilgileri")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _competitionNumberFact ? _competitionNumberFact.shortDescription : qsTr("Musabaka Numarasi")
            }

            FactTextField {
                Layout.fillWidth: true
                fact: _competitionNumberFact
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _teamNameFact ? _teamNameFact.shortDescription : qsTr("Takim Adi")
            }

            FactTextField {
                Layout.fillWidth: true
                fact: _teamNameFact
            }
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth: true
        visible: !!_competitionSettings
        heading: qsTr("Video Kayit")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.preferredWidth: _labelWidth
                text: _videoSaveDirectoryFact ? _videoSaveDirectoryFact.shortDescription : qsTr("Video Kayit Klasoru")
            }

            FactTextField {
                Layout.fillWidth: true
                fact: _videoSaveDirectoryFact
            }

            QGCButton {
                text: qsTr("Sec")
                onClicked: folderDialog.openForLoad()
            }

            QGCFileDialog {
                id: folderDialog
                title: qsTr("Video kayit klasoru sec")
                folder: _videoSaveDirectoryFact ? _videoSaveDirectoryFact.rawValue : ""
                selectFolder: true

                onAcceptedForLoad: (path) => {
                    if (_videoSaveDirectoryFact) {
                        _videoSaveDirectoryFact.rawValue = path
                    }
                }
            }
        }
    }
}
