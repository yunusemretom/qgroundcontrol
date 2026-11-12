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
import QtLocation
import QtPositioning
import QtCore
import QtQuick.Shapes

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.Palette
import QGroundControl.ScreenTools
import QGroundControl.AppSettings 1.0

SettingsPage {
    id: root

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property string _newNameText: ""
    property string _newLatText: ""
    property string _newLonText: ""
    property string _newRadiusText: "150"
    property bool _newActive: true
    property bool _autoFit: true
    property var _flightMapSettings: QGroundControl.settingsManager ? QGroundControl.settingsManager.flightMapSettings : null
    property string _maxSquareLatText: ""
    property string _maxSquareLonText: ""
    property string _maxSquareRadiusText: "250"
    property bool _maxSquareActive: true

    readonly property real _mapHeight: ScreenTools.defaultFontPixelHeight * 22

    QGCPalette {
        id: qgcPal
    }

    Settings {
        id: localSettings
        category: "CustomNoFlyZones"

        property string zonesJson: "[]"
        property string maxSquareJson: ""
    }

    ListModel {
        id: zonesModel
    }

    function _toNumber(value, defaultValue) {
        var numberValue = Number(value)
        return isFinite(numberValue) ? numberValue : defaultValue
    }

    function _zonesFromSettings() {
        var zones = []
        try {
            var parsed = JSON.parse(localSettings.zonesJson)
            if (Array.isArray(parsed)) {
                zones = parsed
            }
        } catch (e) {
            zones = []
        }
        return zones
    }

    function _saveZonesToSettings() {
        var output = []
        for (var i = 0; i < zonesModel.count; i++) {
            var zone = zonesModel.get(i)
            output.push({
                name: zone.name,
                lat: _toNumber(zone.lat, 0),
                lon: _toNumber(zone.lon, 0),
                radiusM: _toNumber(zone.radiusM, 0),
                active: zone.active === true
            })
        }
        localSettings.zonesJson = JSON.stringify(output)
        globals.customNoFlyZonesJson = localSettings.zonesJson
        if (_autoFit && map.mapReady) {
            Qt.callLater(_fitZones)
        }
    }

    function _saveMaxSquareToSettings() {
        var lat = _toNumber(_maxSquareLatText, NaN)
        var lon = _toNumber(_maxSquareLonText, NaN)
        var radiusM = _toNumber(_maxSquareRadiusText, NaN)

        if (!isFinite(lat) || !isFinite(lon) || !isFinite(radiusM) || radiusM <= 0) {
            localSettings.maxSquareJson = ""
            globals.customMaxSquareJson = ""
            return
        }

        var output = {
            lat: lat,
            lon: lon,
            radiusM: radiusM,
            active: _maxSquareActive === true
        }
        localSettings.maxSquareJson = JSON.stringify(output)
        globals.customMaxSquareJson = localSettings.maxSquareJson
    }

    function _loadMaxSquareFromSettings() {
        var raw = []
        try {
            var source = (globals.customMaxSquareJson && globals.customMaxSquareJson.length > 0) ? globals.customMaxSquareJson : localSettings.maxSquareJson
            if (source && source.length > 0) {
                raw = JSON.parse(source)
            }
        } catch (e) {
            raw = []
        }

        if (!raw || typeof raw !== "object") {
            _maxSquareLatText = ""
            _maxSquareLonText = ""
            _maxSquareRadiusText = "250"
            _maxSquareActive = true
            return
        }

        _maxSquareLatText = isFinite(_toNumber(raw.lat, NaN)) ? String(_toNumber(raw.lat, NaN)) : ""
        _maxSquareLonText = isFinite(_toNumber(raw.lon, NaN)) ? String(_toNumber(raw.lon, NaN)) : ""
        _maxSquareRadiusText = isFinite(_toNumber(raw.radiusM, NaN)) ? String(_toNumber(raw.radiusM, NaN)) : "250"
        _maxSquareActive = raw.active === undefined ? true : raw.active === true
    }

    function _loadZonesFromSettings() {
        zonesModel.clear()
        var zones = _zonesFromSettings()
        for (var i = 0; i < zones.length; i++) {
            var zone = zones[i]
            var lat = _toNumber(zone.lat, NaN)
            var lon = _toNumber(zone.lon, NaN)
            var radiusM = _toNumber(zone.radiusM, NaN)
            if (!isFinite(lat) || !isFinite(lon) || !isFinite(radiusM) || radiusM <= 0) {
                continue
            }
            zonesModel.append({
                name: zone.name && zone.name.length > 0 ? String(zone.name) : qsTr("Bölge %1").arg(zonesModel.count + 1),
                lat: lat,
                lon: lon,
                radiusM: radiusM,
                active: zone.active === undefined ? true : zone.active === true
            })
        }

        if (_autoFit && map.mapReady) {
            Qt.callLater(_fitZones)
        }
    }

    function _addZone() {
        var lat = _toNumber(_newLatText, NaN)
        var lon = _toNumber(_newLonText, NaN)
        var radiusM = _toNumber(_newRadiusText, NaN)
        var name = _newNameText.trim()

        if (!isFinite(lat) || !isFinite(lon) || !isFinite(radiusM) || radiusM <= 0) {
            return
        }

        zonesModel.append({
            name: name.length > 0 ? name : qsTr("Bölge %1").arg(zonesModel.count + 1),
            lat: lat,
            lon: lon,
            radiusM: radiusM,
            active: _newActive
        })

        _newNameText = ""
        _saveZonesToSettings()
        Qt.callLater(_fitZones)
    }

    function _removeZone(index) {
        if (index < 0 || index >= zonesModel.count) {
            return
        }
        zonesModel.remove(index)
        _saveZonesToSettings()
    }

    function _useVehicleLocation() {
        if (!_activeVehicle || !_activeVehicle.coordinate || !_activeVehicle.coordinate.isValid) {
            return
        }

        _newLatText = _activeVehicle.coordinate.latitude.toFixed(6)
        _newLonText = _activeVehicle.coordinate.longitude.toFixed(6)
        if (_newNameText.length === 0) {
            _newNameText = qsTr("Arac Konumu")
        }
    }

    function _refreshZoneModel() {
        _loadZonesFromSettings()
    }

    function _useVehicleLocationForSquare() {
        if (!_activeVehicle || !_activeVehicle.coordinate || !_activeVehicle.coordinate.isValid) {
            return
        }

        _maxSquareLatText = _activeVehicle.coordinate.latitude.toFixed(6)
        _maxSquareLonText = _activeVehicle.coordinate.longitude.toFixed(6)
    }

    function _fitZones() {
        var coords = []

        for (var i = 0; i < zonesModel.count; i++) {
            var zone = zonesModel.get(i)
            var center = QtPositioning.coordinate(zone.lat, zone.lon)
            coords.push(center.atDistanceAndAzimuth(zone.radiusM, 0))
            coords.push(center.atDistanceAndAzimuth(zone.radiusM, 90))
            coords.push(center.atDistanceAndAzimuth(zone.radiusM, 180))
            coords.push(center.atDistanceAndAzimuth(zone.radiusM, 270))
        }

        if (_activeVehicle && _activeVehicle.coordinate && _activeVehicle.coordinate.isValid) {
            coords.push(_activeVehicle.coordinate)
        }

        if (coords.length === 0) {
            return
        }

        var north = coords[0].latitude
        var south = coords[0].latitude
        var east = coords[0].longitude
        var west = coords[0].longitude

        for (var k = 1; k < coords.length; k++) {
            var coord = coords[k]
            north = Math.max(north, coord.latitude)
            south = Math.min(south, coord.latitude)
            east = Math.max(east, coord.longitude)
            west = Math.min(west, coord.longitude)
        }

        var latPad = Math.max((north - south) * 0.12, 0.01)
        var lonPad = Math.max((east - west) * 0.12, 0.01)
        north = Math.min(90, north + latPad)
        south = Math.max(-90, south - latPad)
        east = Math.min(180, east + lonPad)
        west = Math.max(-180, west - lonPad)

        map.visibleRegion = QtPositioning.rectangle(
            QtPositioning.coordinate(north, west),
            QtPositioning.coordinate(south, east))
    }

    function _syncMapType() {
        if (!map.mapReady || !_flightMapSettings) {
            return
        }

        var fullMapName = _flightMapSettings.mapProvider.value + " " + _flightMapSettings.mapType.value
        for (var i = 0; i < map.supportedMapTypes.length; i++) {
            if (fullMapName === map.supportedMapTypes[i].name) {
                map.activeMapType = map.supportedMapTypes[i]
                return
            }
        }

        if (map.supportedMapTypes.length > 0) {
            map.activeMapType = map.supportedMapTypes[0]
        }
    }

    Component.onCompleted: {
        _refreshZoneModel()
        _loadMaxSquareFromSettings()
        if (!globals.customNoFlyZonesJson || globals.customNoFlyZonesJson.length === 0) {
            globals.customNoFlyZonesJson = localSettings.zonesJson
        }
        if (!globals.customMaxSquareJson || globals.customMaxSquareJson.length === 0) {
            globals.customMaxSquareJson = localSettings.maxSquareJson
        }
    }

    Connections {
        target: _flightMapSettings ? _flightMapSettings.mapType : null
        function onRawValueChanged() {
            _syncMapType()
        }
    }

    Connections {
        target: _flightMapSettings ? _flightMapSettings.mapProvider : null
        function onRawValueChanged() {
            _syncMapType()
        }
    }

    Connections {
        target: localSettings
        function onZonesJsonChanged() {
            _loadZonesFromSettings()
        }

        function onMaxSquareJsonChanged() {
            _loadMaxSquareFromSettings()
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Manuel Yasakli Alanlar")
        headingDescription: qsTr("Alanlari tek tek buradan ekle. Harita ve uçuş ekranı bu yerel listeyi kullanır.")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Arac Konumunu Al")
                onClicked: _useVehicleLocation()
            }

            QGCButton {
                text: qsTr("Ekle")
                onClicked: _addZone()
            }

            QGCButton {
                text: qsTr("Merkeze Al")
                onClicked: _fitZones()
            }

            QGCCheckBox {
                text: qsTr("Otomatik sığdır")
                checked: _autoFit
                onClicked: _autoFit = checked
            }

            Item {
                Layout.fillWidth: true
            }

            QGCLabel {
                text: qsTr("Kayıtlı bölge: %1").arg(zonesModel.count)
                color: qgcPal.text
                horizontalAlignment: Text.AlignRight
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: ScreenTools.defaultFontPixelWidth * 0.4
            columnSpacing: ScreenTools.defaultFontPixelWidth * 0.5

            QGCLabel { text: qsTr("Ad") }
            QGCLabel { text: qsTr("Enlem") }
            QGCLabel { text: qsTr("Boylam") }
            QGCLabel { text: qsTr("Yarıçap (m)") }

            QGCTextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Örn. Pist")
                text: _newNameText
                onTextChanged: _newNameText = text
            }

            QGCTextField {
                Layout.fillWidth: true
                placeholderText: "41.123456"
                text: _newLatText
                onTextChanged: _newLatText = text
            }

            QGCTextField {
                Layout.fillWidth: true
                placeholderText: "29.123456"
                text: _newLonText
                onTextChanged: _newLonText = text
            }

            QGCTextField {
                Layout.fillWidth: true
                placeholderText: "150"
                text: _newRadiusText
                onTextChanged: _newRadiusText = text
            }
        }

        QGCCheckBox {
            text: qsTr("Aktif")
            checked: _newActive
            onClicked: _newActive = checked
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Aşağıdaki listeden kayıtlı alanları düzenleyebilirsin. Alanlar sunucudan çekilmez; sadece bu cihazda saklanır.")
        }

        Rectangle {
            Layout.fillWidth: true
            radius: ScreenTools.defaultFontPixelWidth * 0.3
            border.width: 1
            border.color: qgcPal.groupBorder
            color: qgcPal.windowShade

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: ScreenTools.defaultFontPixelWidth * 0.5
                spacing: ScreenTools.defaultFontPixelHeight * 0.35

                QGCLabel {
                    text: qsTr("Maksimum Kare Sınır")
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: ScreenTools.defaultFontPixelWidth

                    QGCButton {
                        text: qsTr("Arac Konumunu Al")
                        onClicked: _useVehicleLocationForSquare()
                    }

                    QGCButton {
                        text: qsTr("Kaydet")
                        onClicked: _saveMaxSquareToSettings()
                    }

                    QGCCheckBox {
                        text: qsTr("Aktif")
                        checked: _maxSquareActive
                        onClicked: {
                            _maxSquareActive = checked
                            _saveMaxSquareToSettings()
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: ScreenTools.defaultFontPixelWidth * 0.4
                    columnSpacing: ScreenTools.defaultFontPixelWidth * 0.5

                    QGCLabel { text: qsTr("Merkez Enlem") }
                    QGCLabel { text: qsTr("Merkez Boylam") }
                    QGCLabel { text: qsTr("Yarıçap (m)") }

                    QGCTextField {
                        Layout.fillWidth: true
                        placeholderText: "41.123456"
                        text: _maxSquareLatText
                        onTextChanged: _maxSquareLatText = text
                        onEditingFinished: _saveMaxSquareToSettings()
                    }

                    QGCTextField {
                        Layout.fillWidth: true
                        placeholderText: "29.123456"
                        text: _maxSquareLonText
                        onTextChanged: _maxSquareLonText = text
                        onEditingFinished: _saveMaxSquareToSettings()
                    }

                    QGCTextField {
                        Layout.fillWidth: true
                        placeholderText: "250"
                        text: _maxSquareRadiusText
                        onTextChanged: _maxSquareRadiusText = text
                        onEditingFinished: _saveMaxSquareToSettings()
                    }
                }
            }
        }

        Repeater {
            model: zonesModel

            delegate: Rectangle {
                Layout.fillWidth: true
                radius: ScreenTools.defaultFontPixelWidth * 0.3
                border.width: 1
                border.color: qgcPal.groupBorder
                color: qgcPal.windowShade
                implicitHeight: contentRow.implicitHeight + (ScreenTools.defaultFontPixelHeight * 0.5)

                RowLayout {
                    id: contentRow
                    anchors.fill: parent
                    anchors.margins: ScreenTools.defaultFontPixelWidth * 0.35
                    spacing: ScreenTools.defaultFontPixelWidth * 0.4

                    QGCTextField {
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                        text: name
                        onEditingFinished: {
                            zonesModel.setProperty(index, "name", text.trim().length > 0 ? text.trim() : qsTr("Bölge %1").arg(index + 1))
                            _saveZonesToSettings()
                        }
                    }

                    QGCTextField {
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                        text: String(lat)
                        onEditingFinished: {
                            zonesModel.setProperty(index, "lat", _toNumber(text, lat))
                            _saveZonesToSettings()
                        }
                    }

                    QGCTextField {
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                        text: String(lon)
                        onEditingFinished: {
                            zonesModel.setProperty(index, "lon", _toNumber(text, lon))
                            _saveZonesToSettings()
                        }
                    }

                    QGCTextField {
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                        text: String(radiusM)
                        onEditingFinished: {
                            zonesModel.setProperty(index, "radiusM", _toNumber(text, radiusM))
                            _saveZonesToSettings()
                        }
                    }

                    QGCCheckBox {
                        text: qsTr("Aktif")
                        checked: active
                        onClicked: {
                            zonesModel.setProperty(index, "active", checked)
                            _saveZonesToSettings()
                        }
                    }

                    QGCButton {
                        text: qsTr("Sil")
                        onClicked: _removeZone(index)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: _mapHeight
            radius: ScreenTools.defaultFontPixelWidth * 0.5
            color: qgcPal.window
            border.color: qgcPal.groupBorder
            border.width: 1
            clip: true

            Map {
                id: map
                plugin: Plugin { name: "QGroundControl" }
                anchors.fill: parent
                center: _activeVehicle && _activeVehicle.coordinate && _activeVehicle.coordinate.isValid ? _activeVehicle.coordinate : QtPositioning.coordinate(39.0, 35.0)
                zoomLevel: 13

                onMapReadyChanged: _syncMapType()

                PinchHandler {
                    id: pinchHandler
                    target: null

                    property var pinchStartCentroid

                    onActiveChanged: {
                        if (active) {
                            pinchStartCentroid = map.toCoordinate(pinchHandler.centroid.position, false)
                        }
                    }

                    onScaleChanged: (delta) => {
                        var newZoomLevel = Math.max(map.zoomLevel + Math.log2(delta), 0)
                        map.zoomLevel = newZoomLevel
                        map.alignCoordinateToPoint(pinchStartCentroid, pinchHandler.centroid.position)
                    }
                }

                DragHandler {
                    id: dragHandler
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen

                    property real lastX
                    property real lastY

                    onActiveChanged: {
                        if (active) {
                            lastX = centroid.position.x
                            lastY = centroid.position.y
                        }
                    }

                    onTranslationChanged: {
                        if (active) {
                            map.pan(lastX - centroid.position.x, lastY - centroid.position.y)
                            lastX = centroid.position.x
                            lastY = centroid.position.y
                        }
                    }
                }

                WheelHandler {
                    acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland" ?
                                        PointerDevice.Mouse | PointerDevice.TouchPad : PointerDevice.Mouse
                    rotationScale: 1 / 120
                    property: "zoomLevel"
                }

                MapItemView {
                    model: zonesModel

                    delegate: MapCircle {
                        // One hue per state, used for both fill and border. The fill is the same
                        // color at low alpha, which is how the rest of QGC tints map overlays.
                        readonly property color _zoneColor: active ? qgcPal.colorRed : qgcPal.colorOrange

                        center: QtPositioning.coordinate(lat, lon)
                        radius: radiusM
                        color: Qt.rgba(_zoneColor.r, _zoneColor.g, _zoneColor.b, active ? 0.18 : 0.12)
                        border.color: _zoneColor
                        border.width: 2
                        visible: true
                    }
                }

                MapQuickItem {
                    visible: _activeVehicle && _activeVehicle.coordinate && _activeVehicle.coordinate.isValid
                    coordinate: _activeVehicle ? _activeVehicle.coordinate : QtPositioning.coordinate()
                    anchorPoint.x: sourceItem.width * 0.5
                    anchorPoint.y: sourceItem.height * 0.5

                    sourceItem: Rectangle {
                        width: ScreenTools.defaultFontPixelWidth * 0.75
                        height: width
                        radius: width * 0.5
                        color: qgcPal.colorBlue
                        border.width: 1
                        border.color: qgcPal.mapWidgetBorderLight
                    }
                }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Bu harita sadece senin girdiğin alanları gösterir. Ana ekrandaki uçuş görünümünde de aynı yerel liste kullanılır.")
        }
    }
}