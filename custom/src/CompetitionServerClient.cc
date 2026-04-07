/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Competition server TCP client (Teknofest 2026)
 */

#include "CompetitionServerClient.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QVariantMap>
#include <QtNetwork/QTcpSocket>

CompetitionServerClient::CompetitionServerClient(QObject* parent)
    : QObject(parent)
{
    _socket = new QTcpSocket(this);

    _reconnectTimer.setSingleShot(true);
    connect(&_reconnectTimer, &QTimer::timeout, this, &CompetitionServerClient::_tryReconnect);

    _txTimer.setInterval(1000);
    _txTimer.setSingleShot(false);
    connect(&_txTimer, &QTimer::timeout, this, &CompetitionServerClient::_sendTelemetryTick);

    connect(_socket, &QTcpSocket::connected, this, &CompetitionServerClient::_onSocketConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &CompetitionServerClient::_onSocketDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &CompetitionServerClient::_onSocketReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &CompetitionServerClient::_onSocketErrorOccurred);
}

CompetitionServerClient::~CompetitionServerClient()
{
    disconnectFromServer();
}

void CompetitionServerClient::setHost(const QString& host)
{
    if (_host == host) {
        return;
    }
    _host = host;
    emit hostChanged();

    if (_autoReconnect && !_host.isEmpty() && _port != 0 && !connected()) {
        _scheduleReconnect();
    }
}

void CompetitionServerClient::setPort(quint16 port)
{
    if (_port == port) {
        return;
    }
    _port = port;
    emit portChanged();

    if (_autoReconnect && !_host.isEmpty() && _port != 0 && !connected()) {
        _scheduleReconnect();
    }
}

void CompetitionServerClient::setAuthEnabled(bool enabled)
{
    if (_authEnabled == enabled) {
        return;
    }

    _authEnabled = enabled;
    emit authEnabledChanged();

    if (connected() && _authEnabled) {
        _sendAuthPacket();
    }
}

void CompetitionServerClient::setUsername(const QString& username)
{
    const QString trimmedUsername = username.trimmed();
    if (_username == trimmedUsername) {
        return;
    }

    _username = trimmedUsername;
    emit usernameChanged();

    if (connected() && _authEnabled) {
        _sendAuthPacket();
    }
}

void CompetitionServerClient::setPassword(const QString& password)
{
    if (_password == password) {
        return;
    }

    _password = password;
    emit passwordChanged();

    if (connected() && _authEnabled) {
        _sendAuthPacket();
    }
}

void CompetitionServerClient::setAutoReconnect(bool enabled)
{
    if (_autoReconnect == enabled) {
        return;
    }
    _autoReconnect = enabled;
    emit autoReconnectChanged();

    if (!_autoReconnect) {
        _cancelReconnect();
    } else if (!connected() && !_host.isEmpty() && _port != 0) {
        _scheduleReconnect();
    }
}

void CompetitionServerClient::setReconnectIntervalMs(int ms)
{
    ms = qBound(250, ms, 60 * 1000);
    if (_reconnectIntervalMs == ms) {
        return;
    }
    _reconnectIntervalMs = ms;
    emit reconnectIntervalMsChanged();
}

bool CompetitionServerClient::connected() const
{
    return _socket && _socket->state() == QAbstractSocket::ConnectedState;
}

void CompetitionServerClient::setSysId(int sysId)
{
    if (_sysId == sysId) {
        return;
    }
    _sysId = sysId;
    emit sysIdChanged();
}

void CompetitionServerClient::setOwnLat(double v)
{
    if (qFuzzyCompare(_ownLat, v)) {
        return;
    }
    _ownLat = v;
    emit ownTelemetryChanged();
}

void CompetitionServerClient::setOwnLon(double v)
{
    if (qFuzzyCompare(_ownLon, v)) {
        return;
    }
    _ownLon = v;
    emit ownTelemetryChanged();
}

void CompetitionServerClient::setOwnAlt(double v)
{
    if (qFuzzyCompare(_ownAlt, v)) {
        return;
    }
    _ownAlt = v;
    emit ownTelemetryChanged();
}

void CompetitionServerClient::setOwnHeading(double v)
{
    if (qFuzzyCompare(_ownHeading, v)) {
        return;
    }
    _ownHeading = v;
    emit ownTelemetryChanged();
}

void CompetitionServerClient::setOwnFlightMode(const QString& v)
{
    if (_ownFlightMode == v) {
        return;
    }
    _ownFlightMode = v;
    emit ownTelemetryChanged();
}

qint64 CompetitionServerClient::currentServerTimeMs() const
{
    if (!_serverTimeValid) {
        return 0;
    }
    const qint64 elapsed = _serverTimeElapsed.isValid() ? _serverTimeElapsed.elapsed() : 0;
    return _serverTimeBaseMs + elapsed;
}

void CompetitionServerClient::setLockPayload(const QVariantMap& payload)
{
    const bool wasActive = _hasLockPayload;
    _lockPayload = payload;
    _hasLockPayload = true;
    if (!wasActive) {
        emit lockPayloadActiveChanged();
    }
}

void CompetitionServerClient::clearLockPayload()
{
    const bool wasActive = _hasLockPayload;
    _lockPayload.clear();
    _hasLockPayload = false;
    if (wasActive) {
        emit lockPayloadActiveChanged();
    }
}

void CompetitionServerClient::setKamikazePayload(const QVariantMap& payload)
{
    const bool wasActive = _hasKamikazePayload;
    _kamikazePayload = payload;
    _hasKamikazePayload = true;
    if (!wasActive) {
        emit kamikazePayloadActiveChanged();
    }
}

void CompetitionServerClient::clearKamikazePayload()
{
    const bool wasActive = _hasKamikazePayload;
    _kamikazePayload.clear();
    _hasKamikazePayload = false;
    if (wasActive) {
        emit kamikazePayloadActiveChanged();
    }
}

void CompetitionServerClient::connectToServer()
{
    _manualDisconnectRequested = false;
    _cancelReconnect();

    if (_host.isEmpty() || _port == 0) {
        _setLastErrorString(QStringLiteral("Host/port not set"));
        _setConnectionState(ConnectionState::Error);
        return;
    }

    if (_socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }

    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->abort();
    }

    _rxBuffer.clear();
    _setLastErrorString(QString());
    _setConnectionState(ConnectionState::Connecting);
    emit statusTextChanged(QStringLiteral("Connecting to %1:%2").arg(_host).arg(_port));

    _socket->connectToHost(_host, _port);
}

void CompetitionServerClient::disconnectFromServer()
{
    _manualDisconnectRequested = true;
    _cancelReconnect();
    _txTimer.stop();

    if (_socket->state() == QAbstractSocket::UnconnectedState) {
        _setConnectionState(ConnectionState::Disconnected);
        return;
    }

    _socket->disconnectFromHost();
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->abort();
    }

    _setConnectionState(ConnectionState::Disconnected);
    emit statusTextChanged(QStringLiteral("Disconnected"));
}

void CompetitionServerClient::_onSocketConnected()
{
    _manualDisconnectRequested = false;
    _setConnectionState(ConnectionState::Connected);
    _setLastErrorString(QString());
    _sendAuthPacket();
    _txTimer.start();
    emit statusTextChanged(QStringLiteral("Connected"));
}

void CompetitionServerClient::_onSocketDisconnected()
{
    _txTimer.stop();

    const bool hadErrorState = (_connectionState == ConnectionState::Error);
    _setConnectionState(hadErrorState ? ConnectionState::Error : ConnectionState::Disconnected);
    emit statusTextChanged(QStringLiteral("Disconnected"));

    if (!_manualDisconnectRequested && _autoReconnect && !_host.isEmpty() && _port != 0) {
        _scheduleReconnect();
    }
}

void CompetitionServerClient::_onSocketReadyRead()
{
    _rxBuffer.append(_socket->readAll());

    while (true) {
        const int newlineIndex = _rxBuffer.indexOf('\n');
        if (newlineIndex < 0) {
            break;
        }

        QByteArray line = _rxBuffer.left(newlineIndex);
        _rxBuffer.remove(0, newlineIndex + 1);

        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }

        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        _handleJsonLine(line);
    }
}

void CompetitionServerClient::_onSocketErrorOccurred(QAbstractSocket::SocketError)
{
    _txTimer.stop();

    _setLastErrorString(_socket->errorString());
    _setConnectionState(ConnectionState::Error);
    emit statusTextChanged(QStringLiteral("Socket error: %1").arg(_lastErrorString));

    if (!_manualDisconnectRequested && _autoReconnect && !_host.isEmpty() && _port != 0) {
        _scheduleReconnect();
    }
}

void CompetitionServerClient::_tryReconnect()
{
    if (!_autoReconnect) {
        return;
    }
    if (connected() || _socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }
    connectToServer();
}

void CompetitionServerClient::_sendTelemetryTick()
{
    if (!connected()) {
        return;
    }

    const QJsonObject obj = _buildTelemetryObject();
    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    _socket->write(line);
}

void CompetitionServerClient::_setConnectionState(ConnectionState state)
{
    if (_connectionState == state) {
        return;
    }
    _connectionState = state;
    emit connectionStateChanged();
    emit connectedChanged();
    emit connectionStatusChanged(_connectionState, _lastErrorString);
}

void CompetitionServerClient::_setLastErrorString(const QString& s)
{
    if (_lastErrorString == s) {
        return;
    }
    _lastErrorString = s;
    emit lastErrorStringChanged();
    emit connectionStatusChanged(_connectionState, _lastErrorString);
}

void CompetitionServerClient::_sendAuthPacket()
{
    if (!_authEnabled || !connected() || !_socket) {
        return;
    }

    if (_username.isEmpty()) {
        emit statusTextChanged(QStringLiteral("Authentication enabled but username is empty"));
        return;
    }

    QJsonObject authObj;
    authObj.insert(QStringLiteral("type"), QStringLiteral("auth"));
    authObj.insert(QStringLiteral("username"), _username);
    authObj.insert(QStringLiteral("password"), _password);
    authObj.insert(QStringLiteral("user"), _username);
    authObj.insert(QStringLiteral("pass"), _password);
    authObj.insert(QStringLiteral("sysId"), _sysId);

    const QByteArray line = QJsonDocument(authObj).toJson(QJsonDocument::Compact) + '\n';
    _socket->write(line);
    emit statusTextChanged(QStringLiteral("Authentication payload sent"));
}

void CompetitionServerClient::_scheduleReconnect()
{
    if (!_autoReconnect) {
        return;
    }
    if (_reconnectTimer.isActive()) {
        return;
    }
    _reconnectTimer.start(_reconnectIntervalMs);
}

void CompetitionServerClient::_cancelReconnect()
{
    _reconnectTimer.stop();
}

void CompetitionServerClient::_handleJsonLine(const QByteArray& line)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        _setLastErrorString(QStringLiteral("JSON parse error: %1").arg(err.errorString()));
        emit statusTextChanged(_lastErrorString);
        return;
    }

    _handleMessageObject(doc.object());
}

void CompetitionServerClient::_handleMessageObject(const QJsonObject& obj)
{
    bool anyListChanged = false;
    bool serverTimeUpdated = false;
    bool lockStatusUpdated = false;

    if (obj.contains(QStringLiteral("rivalList")) && obj.value(QStringLiteral("rivalList")).isArray()) {
        const QVariantList newList = _jsonArrayToVariantList(obj.value(QStringLiteral("rivalList")).toArray());
        if (newList != _rivalList) {
            _rivalList = newList;
            emit rivalListChanged();
            emit rivalListReceived(_rivalList);
            anyListChanged = true;
        }
    }

    if (obj.contains(QStringLiteral("noFlyZones")) && obj.value(QStringLiteral("noFlyZones")).isArray()) {
        const QVariantList newList = _jsonArrayToVariantList(obj.value(QStringLiteral("noFlyZones")).toArray());
        if (newList != _noFlyZones) {
            _noFlyZones = newList;
            emit noFlyZonesChanged();
            emit noFlyZonesReceived(_noFlyZones);
            anyListChanged = true;
        }
    }

    if (obj.contains(QStringLiteral("boundaryPolygon")) && obj.value(QStringLiteral("boundaryPolygon")).isArray()) {
        const QVariantList newList = _jsonArrayToVariantList(obj.value(QStringLiteral("boundaryPolygon")).toArray());
        if (newList != _boundaryPolygon) {
            _boundaryPolygon = newList;
            emit boundaryPolygonChanged();
            emit boundaryPolygonReceived(_boundaryPolygon);
            anyListChanged = true;
        }
    }

    if (obj.contains(QStringLiteral("serverTimeMs"))) {
        const QJsonValue v = obj.value(QStringLiteral("serverTimeMs"));
        const qint64 newTime = v.isDouble() ? static_cast<qint64>(v.toDouble()) : v.toVariant().toLongLong();
        if (newTime > 0) {
            const bool wasValid = _serverTimeValid;
            _serverTimeValid = true;
            if (!wasValid) {
                emit serverTimeValidChanged();
            }

            if (_serverTimeBaseMs != newTime) {
                _serverTimeBaseMs = newTime;
                emit serverTimeBaseMsChanged();
            }
            _serverTimeElapsed.restart();
            serverTimeUpdated = true;
            emit serverTimeReceived(_serverTimeBaseMs);
        }
    }

    bool newLockStatusActive = _lockStatusActive;
    int newLockRemainingMs = _lockRemainingMs;
    bool hasLockStatusField = false;

    if (obj.contains(QStringLiteral("lockStatus")) && obj.value(QStringLiteral("lockStatus")).isObject()) {
        const QJsonObject lockObj = obj.value(QStringLiteral("lockStatus")).toObject();
        if (lockObj.contains(QStringLiteral("active"))) {
            newLockStatusActive = lockObj.value(QStringLiteral("active")).toBool();
            hasLockStatusField = true;
        }
        if (lockObj.contains(QStringLiteral("remainingMs"))) {
            newLockRemainingMs = qMax(0, lockObj.value(QStringLiteral("remainingMs")).toVariant().toInt());
            hasLockStatusField = true;
        } else if (lockObj.contains(QStringLiteral("remainingSec"))) {
            newLockRemainingMs = qMax(0, lockObj.value(QStringLiteral("remainingSec")).toVariant().toInt() * 1000);
            hasLockStatusField = true;
        }
    }

    if (obj.contains(QStringLiteral("lockStatusActive"))) {
        newLockStatusActive = obj.value(QStringLiteral("lockStatusActive")).toBool();
        hasLockStatusField = true;
    }
    if (obj.contains(QStringLiteral("lockRemainingMs"))) {
        newLockRemainingMs = qMax(0, obj.value(QStringLiteral("lockRemainingMs")).toVariant().toInt());
        hasLockStatusField = true;
    }
    if (obj.contains(QStringLiteral("lockRemainingSec"))) {
        newLockRemainingMs = qMax(0, obj.value(QStringLiteral("lockRemainingSec")).toVariant().toInt() * 1000);
        hasLockStatusField = true;
    }

    if (hasLockStatusField) {
        if (_lockStatusActive != newLockStatusActive) {
            _lockStatusActive = newLockStatusActive;
            emit lockStatusActiveChanged();
            lockStatusUpdated = true;
        }
        if (_lockRemainingMs != newLockRemainingMs) {
            _lockRemainingMs = newLockRemainingMs;
            emit lockRemainingMsChanged();
            lockStatusUpdated = true;
        }

        if (lockStatusUpdated) {
            emit lockStatusReceived(_lockStatusActive, _lockRemainingMs);
        }
    }

    if (anyListChanged || serverTimeUpdated) {
        emit competitionPacketReceived(_rivalList, _noFlyZones, _boundaryPolygon, currentServerTimeMs());
    }

    if (anyListChanged) {
        emit statusTextChanged(QStringLiteral("Competition data updated"));
    }
}

QVariantList CompetitionServerClient::_jsonArrayToVariantList(const QJsonArray& array)
{
    QVariantList list;
    list.reserve(array.size());
    for (const QJsonValue& v : array) {
        if (v.isObject()) {
            list.append(_jsonObjectToVariantMap(v.toObject()));
        } else {
            list.append(v.toVariant());
        }
    }
    return list;
}

QVariantMap CompetitionServerClient::_jsonObjectToVariantMap(const QJsonObject& obj)
{
    QVariantMap map;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isObject()) {
            map.insert(it.key(), _jsonObjectToVariantMap(it.value().toObject()));
        } else if (it.value().isArray()) {
            map.insert(it.key(), _jsonArrayToVariantList(it.value().toArray()));
        } else {
            map.insert(it.key(), it.value().toVariant());
        }
    }
    return map;
}

QJsonObject CompetitionServerClient::_buildTelemetryObject() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("sysId"), _sysId);

    if (!qIsNaN(_ownLat))     obj.insert(QStringLiteral("lat"), _ownLat);
    if (!qIsNaN(_ownLon))     obj.insert(QStringLiteral("lon"), _ownLon);
    if (!qIsNaN(_ownAlt))     obj.insert(QStringLiteral("alt"), _ownAlt);
    if (!qIsNaN(_ownHeading)) obj.insert(QStringLiteral("heading"), _ownHeading);

    if (!_ownFlightMode.isEmpty()) {
        obj.insert(QStringLiteral("flightMode"), _ownFlightMode);
    }

    if (_hasLockPayload) {
        obj.insert(QStringLiteral("lockPayload"), QJsonObject::fromVariantMap(_lockPayload));
    }
    if (_hasKamikazePayload) {
        obj.insert(QStringLiteral("kamikazePayload"), QJsonObject::fromVariantMap(_kamikazePayload));
    }

    return obj;
}

