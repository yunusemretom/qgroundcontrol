/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick

/// Attitude indicator and heading rose in one card.
///
/// Deliberately depends on nothing but QtQuick: the fly view feeds it angles and theme
/// colours, and the same file can be rendered standalone to check how it looks.
Item {
    id: root

    // ---- Inputs ------------------------------------------------------------
    property real   unit:           16          ///< base size, normally the default font pixel height
    property real   roll:           0           ///< degrees, right wing down positive
    property real   pitch:          0           ///< degrees, nose up positive
    property real   heading:        0           ///< degrees, 0..360
    property bool   valid:          true        ///< false: no vehicle, instruments shown dimmed

    property color  panelColor:     Qt.rgba(0.07, 0.09, 0.12, 0.88)
    property color  borderColor:    Qt.rgba(1, 1, 1, 0.10)
    property color  textColor:      "#ffffff"
    property color  mutedTextColor: Qt.rgba(1, 1, 1, 0.55)
    property color  accentColor:    "#38c79a"
    property color  symbolColor:    "#fcc419"   ///< fixed aircraft symbol / lubber line
    property string fontFamily:     "sans-serif"

    // ---- Layout ------------------------------------------------------------
    readonly property real _dial:     unit * 5.6
    readonly property real _pad:      unit * 0.55
    readonly property real _footer:   unit * 1.25

    implicitWidth:  _dial * 2 + _pad * 3
    implicitHeight: _dial + _footer + _pad * 2
    width:          implicitWidth
    height:         implicitHeight

    // Canvas, unlike Text, only accepts a family name exactly as the font database
    // registers it. QGC hands out "OpenSans" while the database knows "Open Sans", so
    // resolve once here and fall back to the generic family.
    readonly property string _canvasFamily: {
        var families = Qt.fontFamilies()
        if (families.indexOf(fontFamily) >= 0) return "'" + fontFamily + "'"
        var spaced = fontFamily.replace(/([a-z])([A-Z])/g, "$1 $2")
        if (families.indexOf(spaced) >= 0) return "'" + spaced + "'"
        return "sans-serif"
    }

    function _wrap360(a) { var r = a % 360; return r < 0 ? r + 360 : r }
    function _cardinal(a) {
        var names = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        return names[Math.round(_wrap360(a) / 45) % 8]
    }
    function _fmt(v) { return (v < 0 ? "−" : "") + Math.abs(Math.round(v)) + "°" }

    Rectangle {
        anchors.fill:   parent
        radius:         unit * 0.6
        color:          root.panelColor
        border.color:   root.borderColor
        border.width:   1
    }

    // ---- Attitude ------------------------------------------------------------
    Canvas {
        id:         attitude
        x:          _pad
        y:          _pad
        width:      _dial
        height:     _dial
        opacity:    root.valid ? 1 : 0.45
        antialiasing: true

        property real r: Math.min(width, height) / 2

        onPaint: {
            var ctx = getContext("2d")
            var r = attitude.r
            var cx = width / 2, cy = height / 2
            var pxPerDeg = r / 32
            var rollRad = root.roll * Math.PI / 180
            ctx.reset()

            // Horizon, rotated by roll and shifted by pitch, clipped to the dial.
            ctx.save()
            ctx.beginPath(); ctx.arc(cx, cy, r - 1, 0, 2 * Math.PI); ctx.closePath(); ctx.clip()
            ctx.translate(cx, cy)
            ctx.rotate(-rollRad)
            ctx.translate(0, root.pitch * pxPerDeg)

            var big = r * 4
            var sky = ctx.createLinearGradient(0, -big, 0, 0)
            sky.addColorStop(0, "#0b3a66"); sky.addColorStop(1, "#4a9be0")
            ctx.fillStyle = sky; ctx.fillRect(-big, -big, big * 2, big)
            var ground = ctx.createLinearGradient(0, 0, 0, big)
            ground.addColorStop(0, "#8a6440"); ground.addColorStop(1, "#3d2a18")
            ctx.fillStyle = ground; ctx.fillRect(-big, 0, big * 2, big)

            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 1.5
            ctx.beginPath(); ctx.moveTo(-big, 0); ctx.lineTo(big, 0); ctx.stroke()

            // Pitch ladder: long lines every 10 deg with numbers, short every 5 deg.
            ctx.lineWidth = 1
            ctx.fillStyle = "#ffffff"
            ctx.font = "600 " + Math.round(unit * 0.5) + "px " + root._canvasFamily
            ctx.textBaseline = "middle"
            for (var p = -40; p <= 40; p += 5) {
                if (p === 0) continue
                var y = -p * pxPerDeg
                var half = (p % 10 === 0) ? r * 0.28 : r * 0.13
                ctx.globalAlpha = 0.9
                ctx.beginPath(); ctx.moveTo(-half, y); ctx.lineTo(half, y); ctx.stroke()
                if (p % 10 === 0) {
                    ctx.textAlign = "left";  ctx.fillText(Math.abs(p), half + 3, y)
                    ctx.textAlign = "right"; ctx.fillText(Math.abs(p), -half - 3, y)
                }
            }
            ctx.globalAlpha = 1
            ctx.restore()

            // Roll scale: fixed ticks along the top of the rim.
            ctx.save()
            ctx.translate(cx, cy)
            ctx.strokeStyle = "rgba(255,255,255,0.85)"
            var ticks = [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]
            for (var i = 0; i < ticks.length; i++) {
                var t = ticks[i]
                var len = (t % 30 === 0) ? r * 0.12 : r * 0.07
                var a = (t - 90) * Math.PI / 180
                ctx.lineWidth = (t === 0) ? 2 : 1.2
                ctx.beginPath()
                ctx.moveTo(Math.cos(a) * (r - 2), Math.sin(a) * (r - 2))
                ctx.lineTo(Math.cos(a) * (r - 2 - len), Math.sin(a) * (r - 2 - len))
                ctx.stroke()
            }

            // Roll pointer rides the rim with the horizon.
            ctx.rotate(-rollRad)
            ctx.fillStyle = root.accentColor
            var tipY = -r + r * 0.15
            ctx.beginPath()
            ctx.moveTo(0, tipY)
            ctx.lineTo(-r * 0.07, tipY + r * 0.12)
            ctx.lineTo( r * 0.07, tipY + r * 0.12)
            ctx.closePath(); ctx.fill()
            ctx.restore()

            // Fixed aircraft symbol.
            ctx.save()
            ctx.translate(cx, cy)
            ctx.strokeStyle = "rgba(0,0,0,0.45)"; ctx.lineWidth = 5; ctx.lineCap = "round"; ctx.lineJoin = "round"
            var wing = r * 0.42, gap = r * 0.12, drop = r * 0.1
            function aircraft() {
                ctx.beginPath()
                ctx.moveTo(-wing, 0); ctx.lineTo(-gap, 0); ctx.lineTo(-gap * 0.4, drop)
                ctx.moveTo( wing, 0); ctx.lineTo( gap, 0); ctx.lineTo( gap * 0.4, drop)
                ctx.stroke()
            }
            aircraft()
            ctx.strokeStyle = root.symbolColor; ctx.lineWidth = 2.5
            aircraft()
            ctx.fillStyle = root.symbolColor
            ctx.beginPath(); ctx.arc(0, 0, 2.5, 0, 2 * Math.PI); ctx.fill()
            ctx.restore()

            // Bezel.
            ctx.strokeStyle = "rgba(255,255,255,0.18)"; ctx.lineWidth = 1.5
            ctx.beginPath(); ctx.arc(cx, cy, r - 0.75, 0, 2 * Math.PI); ctx.stroke()
        }
    }

    // ---- Heading rose --------------------------------------------------------
    Canvas {
        id:         rose
        x:          _pad * 2 + _dial
        y:          _pad
        width:      _dial
        height:     _dial
        opacity:    root.valid ? 1 : 0.45
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var r = Math.min(width, height) / 2
            var cx = width / 2, cy = height / 2
            ctx.reset()

            var face = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
            face.addColorStop(0, "rgba(255,255,255,0.07)")
            face.addColorStop(1, "rgba(255,255,255,0.02)")
            ctx.fillStyle = face
            ctx.beginPath(); ctx.arc(cx, cy, r - 1, 0, 2 * Math.PI); ctx.fill()
            ctx.strokeStyle = "rgba(255,255,255,0.18)"; ctx.lineWidth = 1.5
            ctx.stroke()

            // Rotating card: ticks every 5 deg, labels every 30 deg.
            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(-root.heading * Math.PI / 180)
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            for (var d = 0; d < 360; d += 5) {
                var major = (d % 30 === 0), mid = (d % 10 === 0)
                var len = major ? r * 0.13 : (mid ? r * 0.08 : r * 0.045)
                ctx.strokeStyle = major ? "rgba(255,255,255,0.9)" : "rgba(255,255,255,0.45)"
                ctx.lineWidth = major ? 1.6 : 1
                ctx.beginPath(); ctx.moveTo(0, -r + 3); ctx.lineTo(0, -r + 3 + len); ctx.stroke()
                if (major) {
                    var label = (d === 0) ? "N" : (d === 90) ? "E" : (d === 180) ? "S" : (d === 270) ? "W" : String(d / 10)
                    var cardinalLabel = (d % 90 === 0)
                    ctx.fillStyle = (d === 0) ? root.accentColor : (cardinalLabel ? "#ffffff" : "rgba(255,255,255,0.7)")
                    ctx.font = (cardinalLabel ? "700 " : "500 ") + Math.round(unit * (cardinalLabel ? 0.62 : 0.48)) + "px " + root._canvasFamily
                    ctx.fillText(label, 0, -r + 3 + r * 0.13 + unit * 0.42)
                }
                ctx.rotate(5 * Math.PI / 180)
            }
            ctx.restore()

            // Fixed lubber line at the top.
            ctx.fillStyle = root.symbolColor
            ctx.beginPath()
            ctx.moveTo(cx, cy - r + r * 0.2)
            ctx.lineTo(cx - r * 0.07, cy - r - 1)
            ctx.lineTo(cx + r * 0.07, cy - r - 1)
            ctx.closePath(); ctx.fill()
        }

        Column {
            anchors.centerIn: parent
            spacing: 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text:           root.valid ? ("00" + Math.round(root._wrap360(root.heading)) % 360).slice(-3) + "°" : "—"
                color:          root.textColor
                font.family:    root.fontFamily
                font.pixelSize: Math.round(unit * 1.05)
                font.weight:    Font.DemiBold
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text:           "HDG"
                color:          root.mutedTextColor
                font.family:    root.fontFamily
                font.pixelSize: Math.round(unit * 0.48)
                font.letterSpacing: unit * 0.08
            }
        }
    }

    // ---- Footer readouts -----------------------------------------------------
    Text {
        x:                  attitude.x
        width:              attitude.width
        y:                  attitude.y + attitude.height + (root._footer - height) / 2
        horizontalAlignment: Text.AlignHCenter
        text:               root.valid ? ("R " + root._fmt(root.roll) + "   P " + root._fmt(root.pitch)) : "R —   P —"
        color:              root.mutedTextColor
        font.family:        root.fontFamily
        font.pixelSize:     Math.round(unit * 0.62)
    }
    Text {
        x:                  rose.x
        width:              rose.width
        y:                  rose.y + rose.height + (root._footer - height) / 2
        horizontalAlignment: Text.AlignHCenter
        text:               root.valid ? root._cardinal(root.heading) : "—"
        color:              root.mutedTextColor
        font.family:        root.fontFamily
        font.pixelSize:     Math.round(unit * 0.62)
        font.weight:        Font.DemiBold
    }

    onRollChanged:      attitude.requestPaint()
    onPitchChanged:     attitude.requestPaint()
    onHeadingChanged:   rose.requestPaint()
    onUnitChanged:      { attitude.requestPaint(); rose.requestPaint() }
    onAccentColorChanged: { attitude.requestPaint(); rose.requestPaint() }
    onSymbolColorChanged: { attitude.requestPaint(); rose.requestPaint() }
}
