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
import QGroundControl.ScreenTools
import QGroundControl.Palette

ColumnLayout {
    spacing: _rowSpacing

    property var subEditConfig: null

    function saveSettings() {
        if (subEditConfig && profileField.text !== "") {
            subEditConfig.profilePath = profileField.text
        }
    }

    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("Ports, field mappings and commands all come from the profile file. Point this at a different profile to talk to different hardware.")
    }

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Profile File") }
        QGCTextField {
            id:                     profileField
            text:                   subEditConfig ? subEditConfig.profilePath : ""
            focus:                  true
            Layout.preferredWidth:  _secondColumnWidth
            onEditingFinished: {
                if (subEditConfig) {
                    subEditConfig.profilePath = profileField.text
                }
            }
        }
    }
}
