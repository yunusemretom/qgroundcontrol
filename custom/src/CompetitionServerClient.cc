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
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QStringList>
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

void CompetitionServerClient::setCompetitionNumber(const QString& competitionNumber)
{
    const QString trimmed = competitionNumber.trimmed();
    if (_competitionNumber == trimmed) {
        return;
    }

    _competitionNumber = trimmed;
    emit competitionNumberChanged();
}

void CompetitionServerClient::setTeamName(const QString& teamName)
{
    const QString trimmed = teamName.trimmed();
    if (_teamName == trimmed) {
        return;
    }

    _teamName = trimmed;
    emit teamNameChanged();
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
    _sendHelloPacket();
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

    _sendCommandPayloadsIfAny();

    const QJsonObject telemetryObj = _buildTelemetryObject();

    QJsonObject packet;
    packet.insert(QStringLiteral("type"), QStringLiteral("telemetry"));
    packet.insert(QStringLiteral("timestampMs"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    packet.insert(QStringLiteral("telemetry"), telemetryObj);

    // Keep flat fields for backward-compatibility with simple receivers.
    for (auto it = telemetryObj.begin(); it != telemetryObj.end(); ++it) {
        if (!packet.contains(it.key())) {
            packet.insert(it.key(), it.value());
        }
    }

    _sendJsonObject(packet);
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

bool CompetitionServerClient::_sendJsonObject(const QJsonObject& obj, const QString& statusText)
{
    if (!connected() || !_socket) {
        return false;
    }

    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    if (_socket->write(line) < 0) {
        _setLastErrorString(_socket->errorString());
        _setConnectionState(ConnectionState::Error);
        return false;
    }

    if (!statusText.isEmpty()) {
        emit statusTextChanged(statusText);
    }

    return true;
}

void CompetitionServerClient::_sendHelloPacket()
{
    QJsonObject helloObj;
    helloObj.insert(QStringLiteral("type"), QStringLiteral("hello"));
    helloObj.insert(QStringLiteral("timestampMs"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    helloObj.insert(QStringLiteral("sysId"), _sysId);

    if (!_competitionNumber.isEmpty()) {
        helloObj.insert(QStringLiteral("competitionNumber"), _competitionNumber);
        helloObj.insert(QStringLiteral("teamNumber"), _competitionNumber);
    }
    if (!_teamName.isEmpty()) {
        helloObj.insert(QStringLiteral("teamName"), _teamName);
    }

    _sendJsonObject(helloObj, QStringLiteral("Hello payload sent"));
}

void CompetitionServerClient::_sendAuthPacket()
{
    if (!_authEnabled) {
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

    if (!_competitionNumber.isEmpty()) {
        authObj.insert(QStringLiteral("competitionNumber"), _competitionNumber);
        authObj.insert(QStringLiteral("teamNumber"), _competitionNumber);
    }
    if (!_teamName.isEmpty()) {
        authObj.insert(QStringLiteral("teamName"), _teamName);
    }

    _sendJsonObject(authObj, QStringLiteral("Authentication payload sent"));
}

void CompetitionServerClient::_sendCommandPayloadsIfAny()
{
    if (_hasLockPayload) {
        QJsonObject lockObj;
        lockObj.insert(QStringLiteral("type"), QStringLiteral("lockCommand"));
        lockObj.insert(QStringLiteral("command"), QStringLiteral("lock"));
        lockObj.insert(QStringLiteral("timestampMs"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
        lockObj.insert(QStringLiteral("sysId"), _sysId);
        lockObj.insert(QStringLiteral("lockPayload"), QJsonObject::fromVariantMap(_lockPayload));
        lockObj.insert(QStringLiteral("payload"), QJsonObject::fromVariantMap(_lockPayload));

        if (_sendJsonObject(lockObj, QStringLiteral("Lock command sent"))) {
            _hasLockPayload = false;
            _lockPayload.clear();
            emit lockPayloadActiveChanged();
        }
    }

    if (_hasKamikazePayload) {
        QJsonObject kamikazeObj;
        kamikazeObj.insert(QStringLiteral("type"), QStringLiteral("kamikazeCommand"));
        kamikazeObj.insert(QStringLiteral("command"), QStringLiteral("kamikaze"));
        kamikazeObj.insert(QStringLiteral("timestampMs"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
        kamikazeObj.insert(QStringLiteral("sysId"), _sysId);
        kamikazeObj.insert(QStringLiteral("kamikazePayload"), QJsonObject::fromVariantMap(_kamikazePayload));
        kamikazeObj.insert(QStringLiteral("payload"), QJsonObject::fromVariantMap(_kamikazePayload));

        if (_sendJsonObject(kamikazeObj, QStringLiteral("Kamikaze command sent"))) {
            _hasKamikazePayload = false;
            _kamikazePayload.clear();
            emit kamikazePayloadActiveChanged();
        }
    }
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
    const QString messageType = obj.value(QStringLiteral("type")).toString().trimmed().toLower();
    if (messageType == QStringLiteral("ping")) {
        QJsonObject pong;
        pong.insert(QStringLiteral("type"), QStringLiteral("pong"));
        pong.insert(QStringLiteral("timestampMs"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
        pong.insert(QStringLiteral("sysId"), _sysId);
        _sendJsonObject(pong);
        return;
    }

    const QJsonObject dataObj = obj.value(QStringLiteral("data")).isObject()
        ? obj.value(QStringLiteral("data")).toObject()
        : QJsonObject();

    auto valueForKeys = [&](const QStringList& keys) -> QJsonValue {
        for (const QString& key : keys) {
            if (obj.contains(key)) {
                return obj.value(key);
            }
            if (dataObj.contains(key)) {
                return dataObj.value(key);
            }
        }
        return QJsonValue();
    };

    auto valueToLongLong = [](const QJsonValue& value, qint64 fallback) -> qint64 {
        if (value.isDouble()) {
            return static_cast<qint64>(value.toDouble());
        }
        if (value.isString()) {
            bool ok = false;
            const qint64 parsed = value.toString().toLongLong(&ok);
            if (ok) {
                return parsed;
            }
        }
        return fallback;
    };

    auto valueToInt = [](const QJsonValue& value, int fallback) -> int {
        if (value.isDouble()) {
            return static_cast<int>(value.toDouble());
        }
        if (value.isString()) {
            bool ok = false;
            const int parsed = value.toString().toInt(&ok);
            if (ok) {
                return parsed;
            }
        }
        return fallback;
    };

    bool anyListChanged = false;
    bool serverTimeUpdated = false;
    bool lockStatusUpdated = false;

    const QJsonValue rivalsValue = valueForKeys({
        QStringLiteral("rivalList"),
        QStringLiteral("rivals"),
        QStringLiteral("enemyList"),
        QStringLiteral("rakipIhaList")
    });
    if (rivalsValue.isArray()) {
        const QVariantList newList = _jsonArrayToVariantList(rivalsValue.toArray());
        if (newList != _rivalList) {
            _rivalList = newList;
            emit rivalListChanged();
            emit rivalListReceived(_rivalList);
            anyListChanged = true;
        }
    }

    const QJsonValue noFlyValue = valueForKeys({
        QStringLiteral("noFlyZones"),
        QStringLiteral("forbiddenZones"),
        QStringLiteral("yasakliBolgeler")
    });
    if (noFlyValue.isArray()) {
        const QVariantList newList = _jsonArrayToVariantList(noFlyValue.toArray());
        if (newList != _noFlyZones) {
            _noFlyZones = newList;
            emit noFlyZonesChanged();
            emit noFlyZonesReceived(_noFlyZones);
            anyListChanged = true;
        }
    }

    const QJsonValue boundaryValue = valueForKeys({
        QStringLiteral("boundaryPolygon"),
        QStringLiteral("boundary"),
        QStringLiteral("sinirPoligonu")
    });
    if (boundaryValue.isArray()) {
        const QVariantList newList = _jsonArrayToVariantList(boundaryValue.toArray());
        if (newList != _boundaryPolygon) {
            _boundaryPolygon = newList;
            emit boundaryPolygonChanged();
            emit boundaryPolygonReceived(_boundaryPolygon);
            anyListChanged = true;
        }
    }

    const QJsonValue timeValue = valueForKeys({
        QStringLiteral("serverTimeMs"),
        QStringLiteral("serverTimestampMs"),
        QStringLiteral("sunucuSaatiMs"),
        QStringLiteral("timeMs")
    });
    const qint64 newTime = valueToLongLong(timeValue, 0);
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

    bool newLockStatusActive = _lockStatusActive;
    int newLockRemainingMs = _lockRemainingMs;
    bool hasLockStatusField = false;

    const QJsonValue lockStatusValue = valueForKeys({
        QStringLiteral("lockStatus"),
        QStringLiteral("kilitDurumu")
    });
    if (lockStatusValue.isObject()) {
        const QJsonObject lockObj = lockStatusValue.toObject();

        if (lockObj.contains(QStringLiteral("active")) || lockObj.contains(QStringLiteral("isActive")) ||
            lockObj.contains(QStringLiteral("kilitlenmeBasarili"))) {
            newLockStatusActive = lockObj.value(QStringLiteral("active")).toBool(
                lockObj.value(QStringLiteral("isActive")).toBool(
                    lockObj.value(QStringLiteral("kilitlenmeBasarili")).toBool(false)));
            hasLockStatusField = true;
        }

        if (lockObj.contains(QStringLiteral("remainingMs")) || lockObj.contains(QStringLiteral("kalanMs"))) {
            newLockRemainingMs = qMax(0, valueToInt(
                lockObj.contains(QStringLiteral("remainingMs"))
                    ? lockObj.value(QStringLiteral("remainingMs"))
                    : lockObj.value(QStringLiteral("kalanMs")),
                0));
            hasLockStatusField = true;
        } else if (lockObj.contains(QStringLiteral("remainingSec")) || lockObj.contains(QStringLiteral("kalanSn"))) {
            newLockRemainingMs = qMax(0, valueToInt(
                lockObj.contains(QStringLiteral("remainingSec"))
                    ? lockObj.value(QStringLiteral("remainingSec"))
                    : lockObj.value(QStringLiteral("kalanSn")),
                0) * 1000);
            hasLockStatusField = true;
        }
    }

    const QJsonValue lockActiveValue = valueForKeys({
        QStringLiteral("lockStatusActive"),
        QStringLiteral("isLocked"),
        QStringLiteral("kilitAktif")
    });
    if (!lockActiveValue.isUndefined()) {
        newLockStatusActive = lockActiveValue.toBool();
        hasLockStatusField = true;
    }

    const QJsonValue remainingMsValue = valueForKeys({
        QStringLiteral("lockRemainingMs"),
        QStringLiteral("kalanKilitMs")
    });
    if (!remainingMsValue.isUndefined()) {
        newLockRemainingMs = qMax(0, valueToInt(remainingMsValue, 0));
        hasLockStatusField = true;
    }

    const QJsonValue remainingSecValue = valueForKeys({
        QStringLiteral("lockRemainingSec"),
        QStringLiteral("kalanKilitSn")
    });
    if (!remainingSecValue.isUndefined()) {
        newLockRemainingMs = qMax(0, valueToInt(remainingSecValue, 0) * 1000);
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

    if (messageType == QStringLiteral("auth_ok") || messageType == QStringLiteral("authsuccess")) {
        emit statusTextChanged(QStringLiteral("Authentication accepted"));
    } else if (messageType == QStringLiteral("auth_error") || messageType == QStringLiteral("authfailed")) {
        _setLastErrorString(QStringLiteral("Authentication rejected by server"));
        emit statusTextChanged(_lastErrorString);
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
    obj.insert(QStringLiteral("timestampMs"), QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    if (!_competitionNumber.isEmpty()) {
        obj.insert(QStringLiteral("competitionNumber"), _competitionNumber);
        obj.insert(QStringLiteral("teamNumber"), _competitionNumber);
    }
    if (!_teamName.isEmpty()) {
        obj.insert(QStringLiteral("teamName"), _teamName);
    }

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

