/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Palette
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.AppSettings

Rectangle {
    id:     settingsView
    color:  qgcPal.window
    z:      QGroundControl.zOrderTopMost

    readonly property real _defaultTextHeight:  ScreenTools.defaultFontPixelHeight
    readonly property real _defaultTextWidth:   ScreenTools.defaultFontPixelWidth
    readonly property real _horizontalMargin:   _defaultTextWidth / 2
    readonly property real _verticalMargin:     _defaultTextHeight / 2
    readonly property real _buttonHeight:       ScreenTools.isTinyScreen ? ScreenTools.defaultFontPixelHeight * 3 : ScreenTools.defaultFontPixelHeight * 2

    property bool _first: true

    property bool _commingFromRIDSettings:  false
    property bool _pageSwitchAnimating: false
    property string _pendingSettingsUrl: ""
    property string _rightPanelErrorText: ""

    function _setRightPanelSource(url, animate) {
        if (!url || url.length === 0) {
            return
        }

        if (!animate || !rightPanel.item) {
            _pendingSettingsUrl = ""
            _pageSwitchAnimating = false
            rightPanel.opacity = 1
            rightPanel.source = url
            return
        }

        if (!_pageSwitchAnimating && rightPanel.source === url) {
            return
        }

        _pendingSettingsUrl = url

        if (!_pageSwitchAnimating) {
            _pageSwitchAnimating = true
            _pageSwitchAnimation.start()
        }
    }

    function showSettingsPage(settingsPage) {
        for (var i=0; i<buttonRepeater.count; i++) {
            var button = buttonRepeater.itemAt(i)
            if (button.text === settingsPage) {
                button.clicked()
                break
            }
        }
    }

    // This need to block click event leakage to underlying map.
    DeadMouseArea {
        anchors.fill: parent
    }

    QGCPalette { id: qgcPal }

    Component.onCompleted: {
        //-- Default Settings
        if (globals.commingFromRIDIndicator) {
            _setRightPanelSource("qrc:/qml/QGroundControl/AppSettings/RemoteIDSettings.qml", false)
            globals.commingFromRIDIndicator = false
        } else {
            _setRightPanelSource("qrc:/qml/QGroundControl/AppSettings/GeneralSettings.qml", false)
        }
    }

    SequentialAnimation {
        id: _pageSwitchAnimation

        NumberAnimation {
            target: rightPanel
            property: "opacity"
            to: 0.0
            duration: 90
            easing.type: Easing.OutCubic
        }

        ScriptAction {
            script: {
                if (settingsView._pendingSettingsUrl.length > 0 && rightPanel.source !== settingsView._pendingSettingsUrl) {
                    rightPanel.source = settingsView._pendingSettingsUrl
                }
                settingsView._pendingSettingsUrl = ""
            }
        }

        NumberAnimation {
            target: rightPanel
            property: "opacity"
            to: 1.0
            duration: 150
            easing.type: Easing.OutQuad
        }

        ScriptAction {
            script: {
                settingsView._pageSwitchAnimating = false
                if (settingsView._pendingSettingsUrl.length > 0 && rightPanel.source !== settingsView._pendingSettingsUrl) {
                    settingsView._setRightPanelSource(settingsView._pendingSettingsUrl, true)
                }
            }
        }
    }

    SettingsPagesModel { id: settingsPagesModel }

    QGCFlickable {
        id:                 buttonList
        width:              buttonColumn.width
        anchors.topMargin:  _verticalMargin
        anchors.top:        parent.top
        anchors.bottom:     parent.bottom
        anchors.leftMargin: _horizontalMargin
        anchors.left:       parent.left
        contentHeight:      buttonColumn.height + _verticalMargin
        flickableDirection: Flickable.VerticalFlick
        clip:               true

        ColumnLayout {
            id:         buttonColumn
            spacing:    ScreenTools.defaultFontPixelHeight / 4

            property real _maxButtonWidth: 0

            Repeater {
                id:     buttonRepeater
                model:  settingsPagesModel

                SettingsButton {
                    Layout.fillWidth:   true
                    text:               name
                    icon.source:        iconUrl
                    visible:            pageVisible()

                    onClicked: {
                        if (mainWindow.allowViewSwitch()) {
                            if (rightPanel.source !== url || settingsView._pageSwitchAnimating) {
                                settingsView._setRightPanelSource(url, true)
                            }
                            checked = true
                        }
                    }

                    Component.onCompleted: {
                        if (globals.commingFromRIDIndicator) {
                            _commingFromRIDSettings = true
                        }
                        if(_first) {
                            _first = false
                            checked = true
                        }
                        if (_commingFromRIDSettings) {
                            checked = false
                            _commingFromRIDSettings = false
                            if (modelData.url == "qrc:/qml/QGroundControl/AppSettings/RemoteIDSettings.qml") {
                                checked = true
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id:                     divider
        anchors.topMargin:      _verticalMargin
        anchors.bottomMargin:   _verticalMargin
        anchors.leftMargin:     _horizontalMargin
        anchors.left:           buttonList.right
        anchors.top:            parent.top
        anchors.bottom:         parent.bottom
        width:                  1
        color:                  qgcPal.windowShade
    }

    //-- Panel Contents
    Loader {
        id:                     rightPanel
        anchors.leftMargin:     _horizontalMargin
        anchors.rightMargin:    _horizontalMargin
        anchors.topMargin:      _verticalMargin
        anchors.bottomMargin:   _verticalMargin
        anchors.left:           divider.right
        anchors.right:          parent.right
        anchors.top:            parent.top
        anchors.bottom:         parent.bottom
        opacity:                1.0

        onStatusChanged: {
            if (status === Loader.Error) {
                _rightPanelErrorText = qsTr("Sayfa yuklenemedi: %1").arg(source)
            } else if (status === Loader.Ready) {
                _rightPanelErrorText = ""
            }
        }
    }

    Rectangle {
        anchors.fill: rightPanel
        visible: _rightPanelErrorText.length > 0
        color: qgcPal.window
        border.width: 1
        border.color: qgcPal.warningText

        QGCLabel {
            anchors.centerIn: parent
            width: parent.width * 0.8
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: qgcPal.warningText
            text: _rightPanelErrorText
        }
    }
}
