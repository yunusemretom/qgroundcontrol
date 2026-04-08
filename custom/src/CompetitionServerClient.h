/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Competition server TCP client (Teknofest 2026)
 */

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtNetwork/QAbstractSocket>

class QTcpSocket;
class QJsonArray;
class QJsonObject;

/// TCP client which connects to the competition server and exchanges JSON messages.
/// Message framing: newline-delimited JSON (NDJSON). Each JSON object is a single line.
class CompetitionServerClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool authEnabled READ authEnabled WRITE setAuthEnabled NOTIFY authEnabledChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString competitionNumber READ competitionNumber WRITE setCompetitionNumber NOTIFY competitionNumberChanged)
    Q_PROPERTY(QString teamName READ teamName WRITE setTeamName NOTIFY teamNameChanged)
    Q_PROPERTY(bool autoReconnect READ autoReconnect WRITE setAutoReconnect NOTIFY autoReconnectChanged)
    Q_PROPERTY(int reconnectIntervalMs READ reconnectIntervalMs WRITE setReconnectIntervalMs NOTIFY reconnectIntervalMsChanged)
    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString lastErrorString READ lastErrorString NOTIFY lastErrorStringChanged)

    // Incoming data (QML friendly: list of JS objects)
    Q_PROPERTY(QVariantList rivalList READ rivalList NOTIFY rivalListChanged)
    Q_PROPERTY(QVariantList noFlyZones READ noFlyZones NOTIFY noFlyZonesChanged)
    Q_PROPERTY(QVariantList boundaryPolygon READ boundaryPolygon NOTIFY boundaryPolygonChanged)

    // Server time (ms since epoch or any monotonic server reference)
    Q_PROPERTY(bool serverTimeValid READ serverTimeValid NOTIFY serverTimeValidChanged)
    Q_PROPERTY(qint64 serverTimeBaseMs READ serverTimeBaseMs NOTIFY serverTimeBaseMsChanged)
    Q_PROPERTY(bool lockStatusActive READ lockStatusActive NOTIFY lockStatusActiveChanged)
    Q_PROPERTY(int lockRemainingMs READ lockRemainingMs NOTIFY lockRemainingMsChanged)
    Q_PROPERTY(bool lockPayloadActive READ lockPayloadActive NOTIFY lockPayloadActiveChanged)
    Q_PROPERTY(bool kamikazePayloadActive READ kamikazePayloadActive NOTIFY kamikazePayloadActiveChanged)

    // Outgoing telemetry fields (temporarily settable; Module 5 will bind to Vehicle)
    Q_PROPERTY(int sysId READ sysId WRITE setSysId NOTIFY sysIdChanged)
    Q_PROPERTY(double ownLat READ ownLat WRITE setOwnLat NOTIFY ownTelemetryChanged)
    Q_PROPERTY(double ownLon READ ownLon WRITE setOwnLon NOTIFY ownTelemetryChanged)
    Q_PROPERTY(double ownAlt READ ownAlt WRITE setOwnAlt NOTIFY ownTelemetryChanged)
    Q_PROPERTY(double ownHeading READ ownHeading WRITE setOwnHeading NOTIFY ownTelemetryChanged)
    Q_PROPERTY(QString ownFlightMode READ ownFlightMode WRITE setOwnFlightMode NOTIFY ownTelemetryChanged)

public:
    enum class ConnectionState {
        Disconnected = 0,
        Connecting,
        Connected,
        Error
    };
    Q_ENUM(ConnectionState)

    explicit CompetitionServerClient(QObject* parent = nullptr);
    ~CompetitionServerClient() override;

    QString host() const { return _host; }
    void setHost(const QString& host);

    quint16 port() const { return _port; }
    void setPort(quint16 port);

    bool authEnabled() const { return _authEnabled; }
    void setAuthEnabled(bool enabled);

    QString username() const { return _username; }
    void setUsername(const QString& username);

    QString password() const { return _password; }
    void setPassword(const QString& password);

    QString competitionNumber() const { return _competitionNumber; }
    void setCompetitionNumber(const QString& competitionNumber);

    QString teamName() const { return _teamName; }
    void setTeamName(const QString& teamName);

    bool autoReconnect() const { return _autoReconnect; }
    void setAutoReconnect(bool enabled);

    int reconnectIntervalMs() const { return _reconnectIntervalMs; }
    void setReconnectIntervalMs(int ms);

    ConnectionState connectionState() const { return _connectionState; }
    bool connected() const;
    QString lastErrorString() const { return _lastErrorString; }

    QVariantList rivalList() const { return _rivalList; }
    QVariantList noFlyZones() const { return _noFlyZones; }
    QVariantList boundaryPolygon() const { return _boundaryPolygon; }

    bool serverTimeValid() const { return _serverTimeValid; }
    qint64 serverTimeBaseMs() const { return _serverTimeBaseMs; }
    bool lockStatusActive() const { return _lockStatusActive; }
    int lockRemainingMs() const { return _lockRemainingMs; }
    bool lockPayloadActive() const { return _hasLockPayload; }
    bool kamikazePayloadActive() const { return _hasKamikazePayload; }

    int sysId() const { return _sysId; }
    void setSysId(int sysId);

    double ownLat() const { return _ownLat; }
    void setOwnLat(double v);
    double ownLon() const { return _ownLon; }
    void setOwnLon(double v);
    double ownAlt() const { return _ownAlt; }
    void setOwnAlt(double v);
    double ownHeading() const { return _ownHeading; }
    void setOwnHeading(double v);
    QString ownFlightMode() const { return _ownFlightMode; }
    void setOwnFlightMode(const QString& v);

    /// Returns interpolated server time (base + elapsed since last packet).
    Q_INVOKABLE qint64 currentServerTimeMs() const;

    /// Set optional payload objects (sent as nested JSON object).
    Q_INVOKABLE void setLockPayload(const QVariantMap& payload);
    Q_INVOKABLE void clearLockPayload();
    Q_INVOKABLE void setKamikazePayload(const QVariantMap& payload);
    Q_INVOKABLE void clearKamikazePayload();

public slots:
    void connectToServer();
    void disconnectFromServer();

signals:
    void hostChanged();
    void portChanged();
    void authEnabledChanged();
    void usernameChanged();
    void passwordChanged();
    void competitionNumberChanged();
    void teamNameChanged();
    void autoReconnectChanged();
    void reconnectIntervalMsChanged();
    void connectionStateChanged();
    void connectedChanged();
    void lastErrorStringChanged();

    void rivalListChanged();
    void noFlyZonesChanged();
    void boundaryPolygonChanged();

    void serverTimeValidChanged();
    void serverTimeBaseMsChanged();
    void lockStatusActiveChanged();
    void lockRemainingMsChanged();
    void lockPayloadActiveChanged();
    void kamikazePayloadActiveChanged();

    void sysIdChanged();
    void ownTelemetryChanged();

    // Explicit publish signals expected by competition data consumers.
    void rivalListReceived(const QVariantList& rivalList);
    void noFlyZonesReceived(const QVariantList& noFlyZones);
    void boundaryPolygonReceived(const QVariantList& boundaryPolygon);
    void serverTimeReceived(qint64 serverTimeMs);
    void competitionPacketReceived(const QVariantList& rivalList,
                                   const QVariantList& noFlyZones,
                                   const QVariantList& boundaryPolygon,
                                   qint64 serverTimeMs);
    void lockStatusReceived(bool active, int remainingMs);

    void connectionStatusChanged(ConnectionState state, const QString& lastErrorString);

    /// High-level status useful for UI (single place to listen).
    void statusTextChanged(const QString& status);

private slots:
    void _onSocketConnected();
    void _onSocketDisconnected();
    void _onSocketReadyRead();
    void _onSocketErrorOccurred(QAbstractSocket::SocketError socketError);
    void _tryReconnect();
    void _sendTelemetryTick();

private:
    void _setConnectionState(ConnectionState state);
    void _setLastErrorString(const QString& s);
    bool _sendJsonObject(const QJsonObject& obj, const QString& statusText = QString());
    void _sendHelloPacket();
    void _sendAuthPacket();
    void _sendCommandPayloadsIfAny();

    void _scheduleReconnect();
    void _cancelReconnect();

    void _handleJsonLine(const QByteArray& line);
    void _handleMessageObject(const QJsonObject& obj);

    static QVariantList _jsonArrayToVariantList(const QJsonArray& array);
    static QVariantMap _jsonObjectToVariantMap(const QJsonObject& obj);

    QJsonObject _buildTelemetryObject() const;

private:
    QString         _host;
    quint16         _port = 0;
    bool            _authEnabled = false;
    QString         _username;
    QString         _password;
    QString         _competitionNumber;
    QString         _teamName;
    bool            _autoReconnect = true;
    int             _reconnectIntervalMs = 1000;
    bool            _manualDisconnectRequested = false;

    ConnectionState _connectionState = ConnectionState::Disconnected;
    QString         _lastErrorString;

    QTcpSocket*     _socket = nullptr;
    QByteArray      _rxBuffer;

    QTimer          _reconnectTimer;
    QTimer          _txTimer;

    QVariantList    _rivalList;
    QVariantList    _noFlyZones;
    QVariantList    _boundaryPolygon;

    bool            _serverTimeValid = false;
    qint64          _serverTimeBaseMs = 0;
    QElapsedTimer   _serverTimeElapsed;
    bool            _lockStatusActive = false;
    int             _lockRemainingMs = 0;

    int             _sysId = 0;
    double          _ownLat = qQNaN();
    double          _ownLon = qQNaN();
    double          _ownAlt = qQNaN();
    double          _ownHeading = qQNaN();
    QString         _ownFlightMode;

    bool            _hasLockPayload = false;
    QVariantMap     _lockPayload;
    bool            _hasKamikazePayload = false;
    QVariantMap     _kamikazePayload;
};

