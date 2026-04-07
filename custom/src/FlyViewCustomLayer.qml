/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 * @file
 *   @author Gus Grubba <gus@auterion.com>
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtLocation
import QtPositioning
import QtCore
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

import Custom.Widgets

Item {
    property var parentToolInsets                       // These insets tell you what screen real estate is available for positioning the controls in your overlay
    property var totalToolInsets:   _totalToolInsets    // The insets updated for the custom overlay additions
    property var mapControl

    readonly property string noGPS:         qsTr("NO GPS")
    readonly property real   indicatorValueWidth:   ScreenTools.defaultFontPixelWidth * 7

    property var    _activeVehicle:         QGroundControl.multiVehicleManager.activeVehicle
    property real   _indicatorDiameter:     ScreenTools.defaultFontPixelWidth * 18
    property real   _indicatorsHeight:      ScreenTools.defaultFontPixelHeight
    property var    _sepColor:              qgcPal.globalTheme === QGCPalette.Light ? Qt.rgba(0,0,0,0.5) : Qt.rgba(1,1,1,0.5)
    property color  _indicatorsColor:       qgcPal.text
    property bool   _isVehicleGps:          _activeVehicle ? _activeVehicle.gps.count.rawValue > 1 && _activeVehicle.gps.hdop.rawValue < 1.4 : false
    property string _altitude:              _activeVehicle ? (isNaN(_activeVehicle.altitudeRelative.value) ? "0.0" : _activeVehicle.altitudeRelative.value.toFixed(1)) + ' ' + _activeVehicle.altitudeRelative.units : "0.0"
    property string _distanceStr:           isNaN(_distance) ? "0" : _distance.toFixed(0) + ' ' + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString
    property real   _heading:               _activeVehicle   ? _activeVehicle.heading.rawValue : 0
    property real   _distance:              _activeVehicle ? _activeVehicle.distanceToHome.rawValue : 0
    property string _messageTitle:          ""
    property string _messageText:           ""
    property real   _toolsMargin:           ScreenTools.defaultFontPixelWidth * 0.75
    property bool   _opsPanelVisible:       true
    property bool   _checklistReadyForFlight: false
    property int    _checklistCompletedCount: 0
    property string _newChecklistItemText:  ""
    property var    _competitionClient:     QGroundControl.corePlugin ? QGroundControl.corePlugin.competitionServerClient : null
    property var    _rivalColorPalette:     ["#FF3B30", "#007AFF", "#34C759", "#FF9500", "#AF52DE", "#00A7A7", "#FF2D55", "#5AC8FA"]
    property string _selectedRivalKey:      ""
    property bool   _boundaryWarning:       false
    property var    _boundaryVertices:      []
    property var    _zoneActivationState:   ({})
    property int    _lockRemainingMsUi:     0
    property bool   _lockActiveUi:          false
    property int    _serverClockMs:         0
    property int    _blinkPhase:            0

    function _toNumber(value, fallback) {
        const n = Number(value)
        return isFinite(n) ? n : fallback
    }

    function _valueForKeys(obj, keys, fallback) {
        if (!obj) {
            return fallback
        }
        for (let i = 0; i < keys.length; i++) {
            const key = keys[i]
            if (obj[key] !== undefined && obj[key] !== null) {
                return obj[key]
            }
        }
        return fallback
    }

    function _modeText(obj) {
        const modeRaw = _valueForKeys(obj, ["flightMode", "mode", "uav_mode"], "-")
        return modeRaw === undefined || modeRaw === null ? "-" : String(modeRaw)
    }

    function _formatMsClock(ms) {
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

    function _formatRemainingSeconds(ms) {
        const remaining = Math.max(0, Math.ceil(ms / 1000.0))
        return String(remaining).padStart(2, "0") + " sn"
    }

    function _normalizeRivals() {
        const output = []
        const raw = _competitionClient ? _competitionClient.rivalList : []

        for (let i = 0; i < raw.length; i++) {
            const rival = raw[i]
            const lat = _toNumber(_valueForKeys(rival, ["lat", "latitude"], NaN), NaN)
            const lon = _toNumber(_valueForKeys(rival, ["lon", "lng", "longitude"], NaN), NaN)
            const alt = _toNumber(_valueForKeys(rival, ["alt", "altitude"], 0), 0)
            const heading = _toNumber(_valueForKeys(rival, ["heading", "yaw"], 0), 0)
            const sysId = _valueForKeys(rival, ["sysId", "systemId", "id"], i + 1)

            if (!isFinite(lat) || !isFinite(lon)) {
                continue
            }

            output.push({
                key: String(sysId) + "_" + String(i),
                sysId: sysId,
                lat: lat,
                lon: lon,
                alt: alt,
                heading: heading,
                mode: _modeText(rival)
            })
        }

        return output
    }

    function _normalizeNoFlyZones() {
        const nowMs = Date.now()
        const raw = _competitionClient ? _competitionClient.noFlyZones : []
        const nextState = {}
        const output = []

        for (let i = 0; i < raw.length; i++) {
            const zone = raw[i]
            const lat = _toNumber(_valueForKeys(zone, ["lat", "latitude", "centerLat"], NaN), NaN)
            const lon = _toNumber(_valueForKeys(zone, ["lon", "lng", "longitude", "centerLon"], NaN), NaN)
            const radiusM = _toNumber(_valueForKeys(zone, ["radius", "radiusM", "radiusMeters"], NaN), NaN)
            const active = !!_valueForKeys(zone, ["active", "isActive", "enabled"], false)

            if (!isFinite(lat) || !isFinite(lon) || !isFinite(radiusM) || radiusM <= 0) {
                continue
            }

            const key = lat.toFixed(6) + "_" + lon.toFixed(6) + "_" + radiusM.toFixed(1)
            const wasActive = _zoneActivationState[key] === true
            nextState[key] = active

            output.push({
                key: key,
                lat: lat,
                lon: lon,
                radiusM: radiusM,
                active: active,
                blinkUntilMs: (active && !wasActive) ? (nowMs + 1000) : 0
            })
        }

        _zoneActivationState = nextState
        return output
    }

    function _normalizeBoundaryVertices() {
        const vertices = []
        const raw = _competitionClient ? _competitionClient.boundaryPolygon : []

        for (let i = 0; i < raw.length; i++) {
            const node = raw[i]
            let lat = NaN
            let lon = NaN

            if (Array.isArray(node) && node.length >= 2) {
                lat = _toNumber(node[0], NaN)
                lon = _toNumber(node[1], NaN)
            } else {
                lat = _toNumber(_valueForKeys(node, ["lat", "latitude"], NaN), NaN)
                lon = _toNumber(_valueForKeys(node, ["lon", "lng", "longitude"], NaN), NaN)
            }

            if (!isFinite(lat) || !isFinite(lon)) {
                continue
            }

            vertices.push(QtPositioning.coordinate(lat, lon))
        }

        return vertices
    }

    function _distancePointToSegmentMeters(pointXY, aXY, bXY) {
        const abX = bXY.x - aXY.x
        const abY = bXY.y - aXY.y
        const abLenSq = abX * abX + abY * abY
        if (abLenSq <= 1e-6) {
            const dx = pointXY.x - aXY.x
            const dy = pointXY.y - aXY.y
            return Math.sqrt(dx * dx + dy * dy)
        }

        const apX = pointXY.x - aXY.x
        const apY = pointXY.y - aXY.y
        const t = Math.max(0, Math.min(1, (apX * abX + apY * abY) / abLenSq))
        const projX = aXY.x + t * abX
        const projY = aXY.y + t * abY
        const dx = pointXY.x - projX
        const dy = pointXY.y - projY
        return Math.sqrt(dx * dx + dy * dy)
    }

    function _minDistanceToBoundaryMeters(coord, boundaryVertices) {
        if (!coord || !coord.isValid || boundaryVertices.length < 2) {
            return Number.POSITIVE_INFINITY
        }

        const earthRadiusM = 6371000.0
        const lat0Rad = coord.latitude * Math.PI / 180.0
        const toXY = function(c) {
            return {
                x: earthRadiusM * (c.longitude * Math.PI / 180.0) * Math.cos(lat0Rad),
                y: earthRadiusM * (c.latitude * Math.PI / 180.0)
            }
        }

        const pointXY = toXY(coord)
        let minDistance = Number.POSITIVE_INFINITY

        for (let i = 0; i < boundaryVertices.length; i++) {
            const a = boundaryVertices[i]
            const b = boundaryVertices[(i + 1) % boundaryVertices.length]
            if (!a || !b || !a.isValid || !b.isValid) {
                continue
            }

            const segDistance = _distancePointToSegmentMeters(pointXY, toXY(a), toXY(b))
            if (segDistance < minDistance) {
                minDistance = segDistance
            }
        }

        return minDistance
    }

    function _refreshBoundaryWarning() {
        const vehicleCoord = _activeVehicle ? _activeVehicle.coordinate : null
        const distance = _minDistanceToBoundaryMeters(vehicleCoord, _boundaryVertices)
        _boundaryWarning = isFinite(distance) && distance <= 100.0
    }

    function _syncLockStatusFromClient() {
        if (!_competitionClient) {
            _lockActiveUi = false
            _lockRemainingMsUi = 0
            return
        }

        const active = _competitionClient.lockStatusActive === true
        const remaining = _toNumber(_competitionClient.lockRemainingMs, 0)
        if (active) {
            _lockActiveUi = true
            _lockRemainingMsUi = remaining > 0 ? remaining : 4000
        } else {
            _lockActiveUi = false
            _lockRemainingMsUi = 0
        }
    }

    function _rebuildRivalMapItems() {
        if (!mapControl || !_competitionClient) {
            return
        }

        _rivalMapObjectManager.destroyObjects()
        const rivals = _normalizeRivals()

        for (let i = 0; i < rivals.length; i++) {
            const marker = _rivalMapObjectManager.createObject(_rivalMarkerComponent, mapControl, true)
            if (!marker) {
                continue
            }

            marker.rivalData = rivals[i]
            marker.iconColor = _rivalColorPalette[i % _rivalColorPalette.length]
        }
    }

    function _rebuildNoFlyZoneMapItems() {
        if (!mapControl || !_competitionClient) {
            return
        }

        _noFlyZoneMapObjectManager.destroyObjects()
        const zones = _normalizeNoFlyZones()

        for (let i = 0; i < zones.length; i++) {
            const zoneCircle = _noFlyZoneMapObjectManager.createObject(_noFlyZoneCircleComponent, mapControl, true)
            if (!zoneCircle) {
                continue
            }
            zoneCircle.zoneData = zones[i]

            if (!zones[i].active) {
                const centerCoord = QtPositioning.coordinate(zones[i].lat, zones[i].lon)
                const dotCount = 24
                for (let dotIndex = 0; dotIndex < dotCount; dotIndex++) {
                    const dotMarker = _noFlyZoneMapObjectManager.createObject(_noFlyZoneDotComponent, mapControl, true)
                    if (!dotMarker) {
                        continue
                    }
                    dotMarker.centerCoordinate = centerCoord
                    dotMarker.radiusMeters = zones[i].radiusM
                    dotMarker.azimuthDeg = dotIndex * (360.0 / dotCount)
                }
            }
        }
    }

    function _rebuildBoundaryMapItem() {
        if (!mapControl || !_competitionClient) {
            return
        }

        _boundaryMapObjectManager.destroyObjects()

        _boundaryVertices = _normalizeBoundaryVertices()
        if (_boundaryVertices.length < 2) {
            _boundaryWarning = false
            return
        }

        const path = _boundaryVertices.slice(0)
        if (_boundaryVertices.length > 2) {
            path.push(_boundaryVertices[0])
        }

        const polyline = _boundaryMapObjectManager.createObject(_boundaryPolylineComponent, mapControl, true)
        if (polyline) {
            polyline.pathPoints = path
        }

        _refreshBoundaryWarning()
    }

    function _rebuildCompetitionOverlay() {
        if (!mapControl || !_competitionClient) {
            return
        }

        _rebuildRivalMapItems()
        _rebuildNoFlyZoneMapItems()
        _rebuildBoundaryMapItem()
        _syncLockStatusFromClient()

        if (_competitionClient.serverTimeValid) {
            _serverClockMs = _competitionClient.currentServerTimeMs()
        }
    }

    function secondsToHHMMSS(timeS) {
        var sec_num = parseInt(timeS, 10);
        var hours   = Math.floor(sec_num / 3600);
        var minutes = Math.floor((sec_num - (hours * 3600)) / 60);
        var seconds = sec_num - (hours * 3600) - (minutes * 60);
        if (hours   < 10) {hours   = "0"+hours;}
        if (minutes < 10) {minutes = "0"+minutes;}
        if (seconds < 10) {seconds = "0"+seconds;}
        return hours+':'+minutes+':'+seconds;
    }

    function _splitLines(rawText) {
        if (!rawText || rawText.length === 0) {
            return []
        }

        const lines = rawText.split(/\r?\n/)
        const trimmed = []
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i].trim()
            if (line.length > 0) {
                trimmed.push(line)
            }
        }
        return trimmed
    }

    function _rebuildChecklist(resetChecked) {
        const currentState = {}
        if (!resetChecked) {
            for (let i = 0; i < checklistModel.count; i++) {
                const item = checklistModel.get(i)
                currentState[item.title] = item.checked
            }
        }

        checklistModel.clear()
        const templateItems = _splitLines(integrationSettings.checklistTemplate)

        for (let j = 0; j < templateItems.length; j++) {
            const title = templateItems[j]
            checklistModel.append({
                title: title,
                checked: resetChecked ? false : !!currentState[title]
            })
        }

        _updateChecklistProgress()
    }

    function _saveChecklistTemplateFromModel() {
        const lines = []
        for (let i = 0; i < checklistModel.count; i++) {
            lines.push(checklistModel.get(i).title)
        }
        integrationSettings.checklistTemplate = lines.join("\n")
    }

    function _updateChecklistProgress() {
        let checkedCount = 0
        for (let i = 0; i < checklistModel.count; i++) {
            if (checklistModel.get(i).checked) {
                checkedCount++
            }
        }

        _checklistCompletedCount = checkedCount
        _checklistReadyForFlight = checklistModel.count > 0 && checkedCount === checklistModel.count
    }

    function _resetChecklist() {
        for (let i = 0; i < checklistModel.count; i++) {
            checklistModel.setProperty(i, "checked", false)
        }
        _updateChecklistProgress()
    }

    function _addChecklistItem() {
        const title = _newChecklistItemText.trim()
        if (title.length === 0) {
            return
        }

        for (let i = 0; i < checklistModel.count; i++) {
            if (checklistModel.get(i).title === title) {
                _newChecklistItemText = ""
                return
            }
        }

        checklistModel.append({
            title: title,
            checked: false
        })
        _newChecklistItemText = ""
        _updateChecklistProgress()
        _saveChecklistTemplateFromModel()
    }

    function _removeChecklistItem(rowIndex) {
        if (rowIndex < 0 || rowIndex >= checklistModel.count) {
            return
        }

        checklistModel.remove(rowIndex)
        _updateChecklistProgress()
        _saveChecklistTemplateFromModel()
    }

    Settings {
        id: integrationSettings
        category: "CustomIntegration"

        property string checklistTemplate: "Airframe visual check\nBattery level verified\nRC link verified\nGPS lock confirmed\nFailsafe reviewed"
    }

    ListModel { id: checklistModel }

    Connections {
        target: integrationSettings
        function onChecklistTemplateChanged() { _rebuildChecklist(false) }
    }

    Connections {
        target: QGroundControl.multiVehicleManager
        function onActiveVehicleChanged() {
            _rebuildChecklist(true)
            _refreshBoundaryWarning()
        }
    }

    Connections {
        target: _activeVehicle
        ignoreUnknownSignals: true
        function onArmedChanged() {
            if (!_activeVehicle || !_activeVehicle.armed) {
                _resetChecklist()
            }
        }
        function onCoordinateChanged() {
            _refreshBoundaryWarning()
        }
    }

    Connections {
        target: _competitionClient
        ignoreUnknownSignals: true

        function onRivalListChanged() {
            _rebuildRivalMapItems()
        }

        function onNoFlyZonesChanged() {
            _rebuildNoFlyZoneMapItems()
        }

        function onBoundaryPolygonChanged() {
            _rebuildBoundaryMapItem()
        }

        function onServerTimeBaseMsChanged() {
            _serverClockMs = _competitionClient.currentServerTimeMs()
        }

        function onLockStatusActiveChanged() {
            _syncLockStatusFromClient()
        }

        function onLockRemainingMsChanged() {
            _syncLockStatusFromClient()
        }
    }

    Component.onCompleted: {
        _rebuildChecklist(true)
        _rebuildCompetitionOverlay()
    }

    onMapControlChanged: {
        _rebuildCompetitionOverlay()
    }

    Timer {
        id: _overlayUpdateTimer
        interval: 100
        running: true
        repeat: true

        onTriggered: {
            if (_competitionClient && _competitionClient.serverTimeValid) {
                _serverClockMs = _competitionClient.currentServerTimeMs()
            }

            if (_lockActiveUi) {
                _lockRemainingMsUi = Math.max(0, _lockRemainingMsUi - interval)
                if (_lockRemainingMsUi === 0) {
                    _lockActiveUi = false
                }
            }
        }
    }

    Timer {
        id: _blinkTimer
        interval: 150
        running: true
        repeat: true

        onTriggered: {
            _blinkPhase = _blinkPhase === 0 ? 1 : 0
        }
    }

    QGCToolInsets {
        id:                     _totalToolInsets
        leftEdgeTopInset:       parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset:    Math.max(
                                    parentToolInsets.leftEdgeCenterInset,
                                    exampleRectangle.leftEdgeCenterInset,
                                    _opsPanelVisible ? (opsPanel.x + opsPanel.width + _toolsMargin) : (showOpsButton.x + showOpsButton.width + _toolsMargin)
                                )
        leftEdgeBottomInset:    parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset:      Math.max(parentToolInsets.rightEdgeTopInset, parent.width - _serverClockBadge.x + _toolsMargin)
        rightEdgeCenterInset:   parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset:   parent.width - compassBackground.x
        topEdgeLeftInset:       Math.max(parentToolInsets.topEdgeLeftInset, _lockStatusBadge.y + _lockStatusBadge.height + _toolsMargin)
        topEdgeCenterInset:     compassArrowIndicator.y + compassArrowIndicator.height
        topEdgeRightInset:      Math.max(parentToolInsets.topEdgeRightInset, _serverClockBadge.y + _serverClockBadge.height + _toolsMargin)
        bottomEdgeLeftInset:    parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset:  parentToolInsets.bottomEdgeCenterInset
        bottomEdgeRightInset:   parent.height - attitudeIndicator.y
    }

    // This is an example of how you can use parent tool insets to position an element on the custom fly view layer
    // - we use parent topEdgeLeftInset to position the widget below the toolstrip
    // - we use parent bottomEdgeLeftInset to dodge the virtual joystick if enabled
    // - we use the parent leftEdgeTopInset to size our element to the same width as the ToolStripAction
    // - we export the width of this element as the leftEdgeCenterInset so that the map will recenter if the vehicle flys behind this element
    Rectangle {
        id: exampleRectangle
        visible: false // to see this example, set this to true. To view insets, enable the insets viewer FlyView.qml
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: parentToolInsets.topEdgeLeftInset + _toolsMargin
        anchors.bottomMargin: parentToolInsets.bottomEdgeLeftInset + _toolsMargin
        anchors.leftMargin: _toolsMargin
        width: parentToolInsets.leftEdgeTopInset - _toolsMargin
        color: 'red'

        property real leftEdgeCenterInset: visible ? x + width : 0
    }

    //-------------------------------------------------------------------------
    //-- Heading Indicator
    Rectangle {
        id:                         compassBar
        height:                     ScreenTools.defaultFontPixelHeight * 1.5
        width:                      ScreenTools.defaultFontPixelWidth  * 50
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       _toolsMargin
        color:                      "#DEDEDE"
        radius:                     2
        clip:                       true
        anchors.horizontalCenter:   parent.horizontalCenter
        Repeater {
            model: 720
            QGCLabel {
                function _normalize(degrees) {
                    var a = degrees % 360
                    if (a < 0) a += 360
                    return a
                }
                property int _startAngle: modelData + 180 + _heading
                property int _angle: _normalize(_startAngle)
                anchors.verticalCenter: parent.verticalCenter
                x:              visible ? ((modelData * (compassBar.width / 360)) - (width * 0.5)) : 0
                visible:        _angle % 45 == 0
                color:          "#75505565"
                font.pointSize: ScreenTools.smallFontPointSize
                text: {
                    switch(_angle) {
                    case 0:     return "N"
                    case 45:    return "NE"
                    case 90:    return "E"
                    case 135:   return "SE"
                    case 180:   return "S"
                    case 225:   return "SW"
                    case 270:   return "W"
                    case 315:   return "NW"
                    }
                    return ""
                }
            }
        }
    }
    Rectangle {
        id:                         headingIndicator
        height:                     ScreenTools.defaultFontPixelHeight
        width:                      ScreenTools.defaultFontPixelWidth * 4
        color:                      qgcPal.windowShadeDark
        anchors.top:                compassBar.top
        anchors.topMargin:          -headingIndicator.height / 2
        anchors.horizontalCenter:   parent.horizontalCenter
        QGCLabel {
            text:                   _heading
            color:                  qgcPal.text
            font.pointSize:         ScreenTools.smallFontPointSize
            anchors.centerIn:       parent
        }
    }
    Image {
        id:                         compassArrowIndicator
        height:                     _indicatorsHeight
        width:                      height
        source:                     "/custom/img/compass_pointer.svg"
        fillMode:                   Image.PreserveAspectFit
        sourceSize.height:          height
        anchors.top:                compassBar.bottom
        anchors.topMargin:          -height / 2
        anchors.horizontalCenter:   parent.horizontalCenter
    }

    Rectangle {
        id:                     compassBackground
        anchors.bottom:         attitudeIndicator.bottom
        anchors.right:          attitudeIndicator.left
        anchors.rightMargin:    -attitudeIndicator.width / 2
        width:                  -anchors.rightMargin + compassBezel.width + (_toolsMargin * 2)
        height:                 attitudeIndicator.height * 0.75
        radius:                 2
        color:                  qgcPal.window

        Rectangle {
            id:                     compassBezel
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin:     _toolsMargin
            anchors.left:           parent.left
            width:                  height
            height:                 parent.height - (northLabelBackground.height / 2) - (headingLabelBackground.height / 2)
            radius:                 height / 2
            border.color:           qgcPal.text
            border.width:           1
            color:                  Qt.rgba(0,0,0,0)
        }

        Rectangle {
            id:                         northLabelBackground
            anchors.top:                compassBezel.top
            anchors.topMargin:          -height / 2
            anchors.horizontalCenter:   compassBezel.horizontalCenter
            width:                      northLabel.contentWidth * 1.5
            height:                     northLabel.contentHeight * 1.5
            radius:                     ScreenTools.defaultFontPixelWidth  * 0.25
            color:                      qgcPal.windowShade

            QGCLabel {
                id:                 northLabel
                anchors.centerIn:   parent
                text:               "N"
                color:              qgcPal.text
                font.pointSize:     ScreenTools.smallFontPointSize
            }
        }

        Image {
            id:                 headingNeedle
            anchors.centerIn:   compassBezel
            height:             compassBezel.height * 0.75
            width:              height
            source:             "/custom/img/compass_needle.svg"
            fillMode:           Image.PreserveAspectFit
            sourceSize.height:  height
            transform: [
                Rotation {
                    origin.x:   headingNeedle.width  / 2
                    origin.y:   headingNeedle.height / 2
                    angle:      _heading
                }]
        }

        Rectangle {
            id:                         headingLabelBackground
            anchors.top:                compassBezel.bottom
            anchors.topMargin:          -height / 2
            anchors.horizontalCenter:   compassBezel.horizontalCenter
            width:                      headingLabel.contentWidth * 1.5
            height:                     headingLabel.contentHeight * 1.5
            radius:                     ScreenTools.defaultFontPixelWidth  * 0.25
            color:                      qgcPal.windowShade

            QGCLabel {
                id:                 headingLabel
                anchors.centerIn:   parent
                text:               _heading
                color:              qgcPal.text
                font.pointSize:     ScreenTools.smallFontPointSize
            }
        }
    }

    Rectangle {
        id:                     attitudeIndicator
        anchors.bottomMargin:   _toolsMargin + parentToolInsets.bottomEdgeRightInset
        anchors.rightMargin:    _toolsMargin
        anchors.bottom:         parent.bottom
        anchors.right:          parent.right
        height:                 ScreenTools.defaultFontPixelHeight * 6
        width:                  height
        radius:                 height * 0.5
        color:                  qgcPal.windowShade

        CustomAttitudeWidget {
            size:               parent.height * 0.95
            vehicle:            _activeVehicle
            showHeading:        false
            anchors.centerIn:   parent
        }
    }

    Rectangle {
        id:                     opsPanel
        visible:                _opsPanelVisible
        anchors.left:           parent.left
        anchors.top:            parent.top
        anchors.leftMargin:     _toolsMargin
        anchors.topMargin:      parentToolInsets.topEdgeLeftInset + _lockStatusBadge.height + (_toolsMargin * 2)
        width:                  Math.min(ScreenTools.defaultFontPixelWidth * 40, parent.width * 0.5)
        height:                 Math.min(ScreenTools.defaultFontPixelHeight * 31, parent.height - anchors.topMargin - parentToolInsets.bottomEdgeLeftInset - _toolsMargin)
        radius:                 ScreenTools.defaultFontPixelWidth * 0.45
        color:                  Qt.rgba(0.07, 0.09, 0.12, 0.92)
        border.color:           Qt.rgba(0.22, 0.78, 0.60, 0.65)
        border.width:           1
        opacity:                0.98

        Rectangle {
            anchors.fill: parent
            radius: opsPanel.radius
            color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.05)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: ScreenTools.defaultFontPixelHeight * 0.35
            radius: opsPanel.radius
            color: Qt.rgba(0.22, 0.78, 0.60, 0.35)
        }

        ColumnLayout {
            anchors.fill:       parent
            anchors.margins:    _toolsMargin
            spacing:            _toolsMargin * 0.8

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    text:           qsTr("Ops Control")
                    font.bold:      true
                    font.pointSize: ScreenTools.defaultFontPointSize + 1
                }

                Rectangle {
                    radius: ScreenTools.defaultFontPixelWidth * 0.22
                    color: _checklistReadyForFlight ? Qt.rgba(0.15, 0.75, 0.35, 0.22) : Qt.rgba(0.97, 0.57, 0.12, 0.22)
                    border.width: 1
                    border.color: _checklistReadyForFlight ? "#27D17C" : "#F7A645"
                    width: statusText.contentWidth + ScreenTools.defaultFontPixelWidth
                    height: statusText.contentHeight + ScreenTools.defaultFontPixelHeight * 0.4

                    QGCLabel {
                        id: statusText
                        anchors.centerIn: parent
                        text: _checklistReadyForFlight ? qsTr("READY") : qsTr("CHECKLIST")
                        color: _checklistReadyForFlight ? "#A9FFD0" : "#FFD39B"
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                QGCButton {
                    text:           qsTr("Hide")
                    onClicked:      _opsPanelVisible = false
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Rectangle {
                    Layout.fillWidth: true
                    radius: ScreenTools.defaultFontPixelWidth * 0.22
                    color: Qt.rgba(0.10, 0.12, 0.16, 0.88)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.10)
                    height: vehicleInfoLabel.contentHeight + ScreenTools.defaultFontPixelHeight * 0.7

                    QGCLabel {
                        id: vehicleInfoLabel
                        anchors.centerIn: parent
                        text: _activeVehicle ? qsTr("Vehicle %1").arg(_activeVehicle.id) : qsTr("No active vehicle")
                        color: qgcPal.text
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: ScreenTools.defaultFontPixelWidth * 0.22
                    color: Qt.rgba(0.10, 0.12, 0.16, 0.88)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.10)
                    height: modeInfoLabel.contentHeight + ScreenTools.defaultFontPixelHeight * 0.7

                    QGCLabel {
                        id: modeInfoLabel
                        anchors.centerIn: parent
                        text: _activeVehicle ? qsTr("Mode %1").arg(_activeVehicle.flightMode) : qsTr("Mode -")
                        color: qgcPal.text
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: ScreenTools.defaultFontPixelWidth * 0.25
                color: Qt.rgba(0.10, 0.12, 0.16, 0.92)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)
                implicitHeight: ScreenTools.defaultFontPixelHeight * 3.0

                Column {
                    anchors.fill: parent
                    anchors.margins: ScreenTools.defaultFontPixelWidth * 0.45
                    spacing: ScreenTools.defaultFontPixelHeight * 0.2

                    Row {
                        width: parent.width
                        spacing: ScreenTools.defaultFontPixelWidth * 0.4

                        QGCLabel {
                            text: qsTr("Checklist Progress")
                            color: qgcPal.text
                            font.bold: true
                        }

                        QGCLabel {
                            text: qsTr("%1/%2").arg(_checklistCompletedCount).arg(checklistModel.count)
                            color: _checklistReadyForFlight ? qgcPal.colorGreen : qgcPal.warningText
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: ScreenTools.defaultFontPixelHeight * 0.55
                        radius: height * 0.5
                        color: Qt.rgba(1, 1, 1, 0.12)

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * (checklistModel.count > 0 ? (_checklistCompletedCount / checklistModel.count) : 0)
                            radius: parent.radius
                            color: _checklistReadyForFlight ? "#27D17C" : "#4BA3FF"
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: ScreenTools.defaultFontPixelWidth * 0.3
                color: Qt.rgba(0.09, 0.10, 0.13, 0.94)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)

                ScrollView {
                    anchors.fill: parent
                    clip: true

                    ColumnLayout {
                        width: opsPanel.width - (_toolsMargin * 3)
                        spacing: _toolsMargin

                        QGCLabel {
                            Layout.fillWidth: true
                            font.bold: true
                            text: qsTr("Preflight Checklist")
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            text: qsTr("%1/%2 complete").arg(_checklistCompletedCount).arg(checklistModel.count)
                            color: _checklistReadyForFlight ? qgcPal.colorGreen : qgcPal.warningText
                        }

                        Repeater {
                            model: checklistModel

                            Rectangle {
                                Layout.fillWidth: true
                                radius: ScreenTools.defaultFontPixelWidth * 0.2
                                color: model.checked ? Qt.rgba(0.16, 0.60, 0.36, 0.22) : Qt.rgba(1, 1, 1, 0.03)
                                border.width: 1
                                border.color: model.checked ? Qt.rgba(0.20, 0.85, 0.45, 0.45) : Qt.rgba(1, 1, 1, 0.10)
                                implicitHeight: rowContent.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.45

                                RowLayout {
                                    id: rowContent
                                    anchors.fill: parent
                                    anchors.margins: ScreenTools.defaultFontPixelWidth * 0.35
                                    spacing: ScreenTools.defaultFontPixelWidth * 0.35

                                    QGCCheckBox {
                                        checked: model.checked
                                        onClicked: {
                                            checklistModel.setProperty(index, "checked", checked)
                                            _updateChecklistProgress()
                                        }
                                    }

                                    QGCLabel {
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        text: model.title
                                        font.bold: model.checked
                                    }

                                    Rectangle {
                                        radius: ScreenTools.defaultFontPixelWidth * 0.2
                                        color: Qt.rgba(0.95, 0.23, 0.23, 0.22)
                                        border.width: 1
                                        border.color: Qt.rgba(1, 0.28, 0.28, 0.45)
                                        width: deleteText.contentWidth + ScreenTools.defaultFontPixelWidth * 0.8
                                        height: deleteText.contentHeight + ScreenTools.defaultFontPixelHeight * 0.45

                                        QGCLabel {
                                            id: deleteText
                                            anchors.centerIn: parent
                                            text: qsTr("Del")
                                            color: "#FFC3C3"
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: _removeChecklistItem(index)
                                        }
                                    }
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

                        RowLayout {
                            Layout.fillWidth: true

                            QGCButton {
                                text: qsTr("Reset")
                                onClicked: _resetChecklist()
                            }

                            QGCButton {
                                text: qsTr("Mark all")
                                enabled: checklistModel.count > 0
                                onClicked: {
                                    for (let i = 0; i < checklistModel.count; i++) {
                                        checklistModel.setProperty(i, "checked", true)
                                    }
                                    _updateChecklistProgress()
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }
                    }
                }
            }
        }
    }

    QGCDynamicObjectManager {
        id: _rivalMapObjectManager
    }

    QGCDynamicObjectManager {
        id: _noFlyZoneMapObjectManager
    }

    QGCDynamicObjectManager {
        id: _boundaryMapObjectManager
    }

    Component {
        id: _rivalMarkerComponent

        MapQuickItem {
            property var rivalData: ({})
            property color iconColor: "#007AFF"

            z:              QGroundControl.zOrderMapItems + 10
            coordinate:     QtPositioning.coordinate(rivalData.lat, rivalData.lon)
            anchorPoint.x:  sourceItem.width * 0.5
            anchorPoint.y:  sourceItem.height * 0.5

            sourceItem: Item {
                width:  ScreenTools.defaultFontPixelWidth * 10
                height: ScreenTools.defaultFontPixelHeight * 9

                Canvas {
                    id: rivalIcon
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width:  ScreenTools.defaultFontPixelHeight * 2.2
                    height: width
                    antialiasing: true
                    rotation: _toNumber(rivalData.heading, 0)

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.beginPath()
                        ctx.moveTo(width * 0.5, 0)
                        ctx.lineTo(width * 0.87, height * 0.8)
                        ctx.lineTo(width * 0.5, height * 0.62)
                        ctx.lineTo(width * 0.13, height * 0.8)
                        ctx.closePath()
                        ctx.fillStyle = iconColor
                        ctx.fill()
                        ctx.lineWidth = 1.2
                        ctx.strokeStyle = "#FFFFFF"
                        ctx.stroke()
                    }
                }

                Rectangle {
                    id: rivalTag
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: rivalIcon.bottom
                    anchors.topMargin: ScreenTools.defaultFontPixelHeight * 0.1
                    radius: ScreenTools.defaultFontPixelWidth * 0.2
                    color: Qt.rgba(0, 0, 0, 0.68)
                    border.width: 1
                    border.color: iconColor
                    width: tagLabel.contentWidth + ScreenTools.defaultFontPixelWidth
                    height: tagLabel.contentHeight + ScreenTools.defaultFontPixelHeight * 0.35

                    QGCLabel {
                        id: tagLabel
                        anchors.centerIn: parent
                        color: "#FFFFFF"
                        font.bold: true
                        text: "ID " + String(rivalData.sysId)
                    }
                }

                Rectangle {
                    anchors.left: parent.horizontalCenter
                    anchors.leftMargin: ScreenTools.defaultFontPixelWidth * 0.6
                    anchors.top: rivalTag.bottom
                    anchors.topMargin: ScreenTools.defaultFontPixelHeight * 0.25
                    visible: _selectedRivalKey === rivalData.key
                    width: popupText.contentWidth + ScreenTools.defaultFontPixelWidth * 1.6
                    height: popupText.contentHeight + ScreenTools.defaultFontPixelHeight
                    radius: ScreenTools.defaultFontPixelWidth * 0.25
                    color: Qt.rgba(0, 0, 0, 0.8)
                    border.color: iconColor
                    border.width: 1

                    QGCLabel {
                        id: popupText
                        anchors.centerIn: parent
                        color: "#FFFFFF"
                        text: qsTr("Lat: %1\nLon: %2\nAlt: %3 m\nMod: %4")
                              .arg(_toNumber(rivalData.lat, 0).toFixed(6))
                              .arg(_toNumber(rivalData.lon, 0).toFixed(6))
                              .arg(_toNumber(rivalData.alt, 0).toFixed(1))
                              .arg(rivalData.mode)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (_selectedRivalKey === rivalData.key) {
                            _selectedRivalKey = ""
                        } else {
                            _selectedRivalKey = rivalData.key
                        }
                    }
                }
            }
        }
    }

    Component {
        id: _noFlyZoneCircleComponent

        MapCircle {
            property var zoneData: ({})

            z:          QGroundControl.zOrderMapItems + 3
            center:     QtPositioning.coordinate(zoneData.lat, zoneData.lon)
            radius:     _toNumber(zoneData.radiusM, 0)
            border.width:  zoneData.active ? 2 : 1
            border.color:  zoneData.active ? "#FF3B30" : "#BF4A4A"
            color:         zoneData.active ? Qt.rgba(1, 0, 0, 0.28) : Qt.rgba(1, 0, 0, 0.06)
            opacity: {
                if (zoneData.active && zoneData.blinkUntilMs > Date.now()) {
                    return _blinkPhase === 0 ? 0.25 : 1.0
                }
                return 0.95
            }
        }
    }

    Component {
        id: _noFlyZoneDotComponent

        MapQuickItem {
            property var centerCoordinate: QtPositioning.coordinate()
            property real radiusMeters: 0
            property real azimuthDeg: 0

            z:              QGroundControl.zOrderMapItems + 4
            coordinate:     centerCoordinate.isValid ? centerCoordinate.atDistanceAndAzimuth(radiusMeters, azimuthDeg) : QtPositioning.coordinate()
            anchorPoint.x:  sourceItem.width * 0.5
            anchorPoint.y:  sourceItem.height * 0.5

            sourceItem: Rectangle {
                width:  ScreenTools.defaultFontPixelWidth * 0.35
                height: width
                radius: width * 0.5
                color: "#D84A4A"
            }
        }
    }

    Component {
        id: _boundaryPolylineComponent

        MapPolyline {
            property var pathPoints: []

            z:              QGroundControl.zOrderMapItems + 5
            path:           pathPoints
            line.width:     3
            line.color:     _boundaryWarning ? "#FF3B30" : "#1589FF"
            opacity:        0.95
        }
    }

    Rectangle {
        id: _lockStatusBadge
        anchors.left:       parent.left
        anchors.top:        parent.top
        anchors.leftMargin: _toolsMargin
        anchors.topMargin:  parentToolInsets.topEdgeLeftInset + _toolsMargin
        radius:             ScreenTools.defaultFontPixelWidth * 0.3
        color:              _lockActiveUi ? Qt.rgba(0.05, 0.55, 0.17, 0.88) : Qt.rgba(0.25, 0.25, 0.25, 0.82)
        border.color:       _lockActiveUi ? "#2AD16D" : "#A6A6A6"
        border.width:       1
        width:              lockStatusColumn.implicitWidth + ScreenTools.defaultFontPixelWidth * 1.1
        height:             lockStatusColumn.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.8

        Column {
            id: lockStatusColumn
            anchors.centerIn: parent
            spacing: ScreenTools.defaultFontPixelHeight * 0.1

            QGCLabel {
                text: _lockActiveUi ? qsTr("KİLİTLENME AKTİF") : qsTr("KİLİTSİZ")
                color: "#FFFFFF"
                font.bold: true
            }

            QGCLabel {
                visible: _lockActiveUi
                text: qsTr("Kalan: %1").arg(_formatRemainingSeconds(_lockRemainingMsUi))
                color: "#E8FFE8"
            }
        }
    }

    Rectangle {
        id: _serverClockBadge
        anchors.right:      parent.right
        anchors.top:        parent.top
        anchors.rightMargin: _toolsMargin
        anchors.topMargin:   parentToolInsets.topEdgeRightInset + _toolsMargin
        radius:             ScreenTools.defaultFontPixelWidth * 0.3
        color:              Qt.rgba(0, 0, 0, 0.72)
        border.width:       1
        border.color:       _competitionClient && _competitionClient.serverTimeValid ? "#4CD964" : "#808080"
        width:              serverClockText.contentWidth + ScreenTools.defaultFontPixelWidth * 1.5
        height:             serverClockText.contentHeight + ScreenTools.defaultFontPixelHeight * 0.8

        QGCLabel {
            id: serverClockText
            anchors.centerIn: parent
            color: "#FFFFFF"
            font.bold: true
            text: _formatMsClock(_serverClockMs)
        }
    }

    Rectangle {
        id:                 showOpsButton
        visible:            !_opsPanelVisible
        anchors.left:       parent.left
        anchors.top:        parent.top
        anchors.leftMargin: _toolsMargin
        anchors.topMargin:  parentToolInsets.topEdgeLeftInset + _lockStatusBadge.height + (_toolsMargin * 2)
        radius:             ScreenTools.defaultFontPixelWidth * 0.32
        color:              Qt.rgba(0.08, 0.11, 0.14, 0.92)
        border.width:       1
        border.color:       Qt.rgba(0.22, 0.78, 0.60, 0.70)
        width:              showOpsLabel.contentWidth + ScreenTools.defaultFontPixelWidth * 1.4
        height:             showOpsLabel.contentHeight + ScreenTools.defaultFontPixelHeight * 0.8

        QGCLabel {
            id:                 showOpsLabel
            anchors.centerIn:   parent
            text:               qsTr("Ops Panel")
            color:              "#D4FFF0"
            font.bold:          true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: _opsPanelVisible = true
        }
    }
}
