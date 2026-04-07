/****************************************************************************
 *
 * Custom integration settings for checklist management.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.AppSettings 1.0

SettingsPage {
    id: root

    property string _newChecklistItemText: ""

    function _splitLines(rawText) {
        if (!rawText || rawText.length === 0) {
            return []
        }

        const lines = rawText.split(/\r?\n/)
        const filtered = []
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i].trim()
            if (line.length > 0) {
                filtered.push(line)
            }
        }
        return filtered
    }

    function _saveChecklistTemplateFromModel() {
        const lines = []
        for (let i = 0; i < checklistTemplateModel.count; i++) {
            lines.push(checklistTemplateModel.get(i).title)
        }
        integrationSettings.checklistTemplate = lines.join("\n")
    }

    function _rebuildChecklistTemplateModel() {
        checklistTemplateModel.clear()
        const lines = _splitLines(integrationSettings.checklistTemplate)
        for (let i = 0; i < lines.length; i++) {
            checklistTemplateModel.append({ title: lines[i] })
        }
    }

    function _addChecklistItem() {
        const newItem = _newChecklistItemText.trim()
        if (newItem.length === 0) {
            return
        }

        for (let i = 0; i < checklistTemplateModel.count; i++) {
            if (checklistTemplateModel.get(i).title === newItem) {
                _newChecklistItemText = ""
                return
            }
        }

        checklistTemplateModel.append({ title: newItem })
        _saveChecklistTemplateFromModel()
        _newChecklistItemText = ""
    }

    function _removeChecklistItem(rowIndex) {
        if (rowIndex < 0 || rowIndex >= checklistTemplateModel.count) {
            return
        }
        checklistTemplateModel.remove(rowIndex)
        _saveChecklistTemplateFromModel()
    }

    Settings {
        id: integrationSettings
        category: "CustomIntegration"

        property string checklistTemplate: "Airframe visual check\nBattery level verified\nRC link verified\nGPS lock confirmed\nFailsafe reviewed"
    }

    ListModel {
        id: checklistTemplateModel
    }

    Connections {
        target: integrationSettings
        function onChecklistTemplateChanged() {
            _rebuildChecklistTemplateModel()
        }
    }

    Component.onCompleted: _rebuildChecklistTemplateModel()

    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Preflight Checklist")
        headingDescription: qsTr("Add or remove checklist items. Changes are used directly in Fly view.")

        Repeater {
            model: checklistTemplateModel

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "- " + model.title
                }

                QGCButton {
                    text: qsTr("Delete")
                    onClicked: _removeChecklistItem(index)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                Layout.fillWidth: true
                radius: ScreenTools.defaultFontPixelWidth * 0.2
                border.width: 1
                border.color: qgcPal.text
                color: qgcPal.windowShade
                implicitHeight: checklistInputField.implicitHeight + (ScreenTools.defaultFontPixelHeight * 0.4)

                QGCTextField {
                    id: checklistInputField
                    anchors.fill: parent
                    anchors.margins: ScreenTools.defaultFontPixelWidth * 0.25
                    placeholderText: qsTr("New checklist item")
                    text: _newChecklistItemText
                    onTextChanged: _newChecklistItemText = text
                    onAccepted: _addChecklistItem()
                }
            }

            QGCButton {
                text: qsTr("Add")
                onClicked: _addChecklistItem()
            }
        }
    }
}
