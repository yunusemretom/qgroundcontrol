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
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.AppSettings 1.0

SettingsPage {
    id: root

    readonly property real _labelWidth: ScreenTools.defaultFontPixelWidth * 20

    property var _brandImageSettings: QGroundControl.settingsManager ? QGroundControl.settingsManager.brandImageSettings : null
    property var _userBrandImageIndoor: _brandImageSettings ? _brandImageSettings.userBrandImageIndoor : null
    property var _userBrandImageOutdoor: _brandImageSettings ? _brandImageSettings.userBrandImageOutdoor : null

    QGCPalette { id: qgcPal }

    function _imageDisplayPath(fact) {
        if (!fact || fact.valueString.length === 0) {
            return qsTr("Seçilmedi")
        }

        return fact.valueString.replace("file:///", "")
    }

    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Uygulama Resmi")
        headingDescription: qsTr("Ana araç çubuğunda görünen marka görselini proje bazında değiştirin.")

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: qgcPal.text
            text: qsTr("Bu ayar uygulama içi marka görselini değiştirir. Çalışma zamanı için uygundur; sistem uygulama simgesini değiştirmez.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 2
            visible: _userBrandImageIndoor && _userBrandImageIndoor.visible

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Indoor Image")
                }

                QGCLabel {
                    Layout.fillWidth: true
                    font.pointSize: ScreenTools.smallFontPointSize
                    text: _imageDisplayPath(_userBrandImageIndoor)
                    elide: Text.ElideMiddle
                }
            }

            QGCButton {
                text: qsTr("Browse")
                onClicked: indoorBrowseDialog.openForLoad()

                QGCFileDialog {
                    id: indoorBrowseDialog
                    title: qsTr("Choose custom indoor brand image file")
                    folder: _userBrandImageIndoor ? _userBrandImageIndoor.rawValue.replace("file:///", "") : ""
                    selectFolder: false
                    onAcceptedForLoad: (file) => {
                        if (_userBrandImageIndoor) {
                            _userBrandImageIndoor.rawValue = "file:///" + file
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 2
            visible: _userBrandImageOutdoor && _userBrandImageOutdoor.visible

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Outdoor Image")
                }

                QGCLabel {
                    Layout.fillWidth: true
                    font.pointSize: ScreenTools.smallFontPointSize
                    text: _imageDisplayPath(_userBrandImageOutdoor)
                    elide: Text.ElideMiddle
                }
            }

            QGCButton {
                text: qsTr("Browse")
                onClicked: outdoorBrowseDialog.openForLoad()

                QGCFileDialog {
                    id: outdoorBrowseDialog
                    title: qsTr("Choose custom outdoor brand image file")
                    folder: _userBrandImageOutdoor ? _userBrandImageOutdoor.rawValue.replace("file:///", "") : ""
                    selectFolder: false
                    onAcceptedForLoad: (file) => {
                        if (_userBrandImageOutdoor) {
                            _userBrandImageOutdoor.rawValue = "file:///" + file
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            QGCButton {
                text: qsTr("Reset Images")
                onClicked: {
                    if (_userBrandImageIndoor) {
                        _userBrandImageIndoor.rawValue = ""
                    }
                    if (_userBrandImageOutdoor) {
                        _userBrandImageOutdoor.rawValue = ""
                    }
                }
            }
        }
    }
}