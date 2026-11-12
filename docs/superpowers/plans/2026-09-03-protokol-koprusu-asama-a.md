# Protokol Köprüsü — Aşama A Uygulama Planı

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Çelik Kubbe taretinin UDP/JSON telemetrisini, JSON profil dosyasıyla yapılandırılan genel bir köprü üzerinden MAVLink'e çevirip QGroundControl'de normal bir araç olarak göstermek.

**Architecture:** `LinkInterface` alt sınıfı olan `ExternalProtocolLink`, gerçek MAVLink trafiği taşımak yerine gelen JSON telemetriden MAVLink mesajları **sentezler** (`src/Comms/MockLink/` içindeki kanıtlanmış desen). Zincir: UDP soketi → çerçeve çözücü → alan eşleyici → MAVLink sentezleyici → `emit bytesReceived()`. Eşleme kuralları C++'ta sabit değil, JSON profil dosyasında; yeni yarışma = yeni profil dosyası.

**Tech Stack:** C++17, Qt 6.8.3 (Core, Network, Test), CMake + Ninja, QGC MAVLink v2 (`MAVLinkLib.h`)

**Spec:** `docs/superpowers/specs/2026-09-03-protokol-koprusu-design.md`

## Kapsam: neden yalnızca Aşama A

Spec üç aşama tanımlıyor. Bu plan **yalnızca Aşama A**'yı kapsıyor, çünkü A tek başına çalışan ve test edilebilir yazılım üretiyor: taret QGC'de araç olarak görünür, harita konumu, HUD, batarya, uçuş modu çalışır. Aşama B (komut kanalı) ve Aşama C (joystick) kendi planlarını alacak.

## Global Constraints

Spec'ten alınan, her göreve örtük olarak dahil olan kısıtlar:

- **Açı birimi:** Taret **derece** gönderir, MAVLink `ATTITUDE` **radyan** ister. Dönüşüm eşleme kuralının `"unit": "deg2rad"` alanıyla yapılır, C++'ta sabit kodlanmaz.
- **`target` alanı null olabilir.** Eşleyici ve pass-through bunu çökmeden tolere etmeli.
- **Eşleme kuralı alanları:** `from` (zorunlu, nokta ile ayrılmış yol), `scale`, `unit`, `map`, `default`. **`scale`, `unit` ve `map` birbirini dışlar** — bir kuralda en fazla biri bulunur.
- **Eksik alan davranışı:** Kaynak alan yok ve `default` verilmemişse o MAVLink alanı önceki değerinde kalır. Tek eksik alan tüm mesajı düşürmez.
- **`HEARTBEAT` telemetriden bağımsızdır.** Profilin `vehicle.heartbeatHz` hızında, gelen paket olsun olmasın üretilir; telemetri `timeoutMs` süresince gelmezse durur (QGC'nin bağlantı-koptu tespiti buna dayanır).
- **Bozuk paket asla çökertmez.** Sayaç artır, `CustomLog` kategorisine yaz, devam et.
- **Profil yok/bozuk ise özellik kapalıdır.** QGC normal çalışmaya devam eder.
- **Desteklenen `unit` değerleri:** `deg2rad`, `rad2deg`, `m2cm`, `cm2m`.
- Kod ve yorumlar **İngilizce** (repo geneli böyle). Kullanıcıya görünen metinler `tr()` ile.

## Core dosyalara dokunma maliyeti — okuyun

QGC'de yeni bir link tipi eklemek **core dosyalarda 4 nokta** gerektiriyor; `custom/` içinde kalmak mümkün değil. Bunlar upstream güncellemelerinde çakışacak:

| Dosya | Ne | Görev |
|---|---|---|
| `src/Comms/LinkConfiguration.h` | `LinkType` enum'una `TypeExternalProtocol` | 6 |
| `src/Comms/LinkManager.cc:123` | `createConnectedLink` switch dalı | 6 |
| `src/Comms/LinkManager.cc:338` | `createConfiguration` switch dalı | 6 |
| `src/Comms/LinkManager.cc:547` | `linkTypeStrings()` listesi (enum ile **aynı sırada** olmalı) | 6 |
| `test/CMakeLists.txt` | `add_subdirectory` + `add_qgc_test` | 1 |
| `test/UnitTestList.cc` | `#include` + `UT_REGISTER_TEST` | 1 |

**Hepsi `#ifdef QGC_CUSTOM_BUILD` ile korunacak** (bu makro `custom/CMakeLists.txt:5` tarafından tanımlanıyor). Böylece hem stok yapı etkilenmez hem de upstream merge'de çakışma yüzeyi tek satırlık bloklara iner. Bu değişiklikler Paket 04'ün yükseltme raporuna kaydedilecek.

## Spec'ten bilinçli sapma

Spec `ExternalProtocolSettings`'i "Fact tabanlı ayarlar + QML sayfası" olarak tarif
ediyor. Plan bunu **`LinkConfiguration` özelliği + QML düzenleme paneli** olarak
uyguluyor (Task 6), ayrı bir Fact grubu olarak değil.

Gerekçe: bu ayarın tek içeriği "hangi profil dosyası" ve bu, link'e ait bir ayar —
QGC'nin bir link'e ait ayarları koyduğu yer `LinkConfiguration`'dır
(`loadSettings`/`saveSettings` ile kalıcı olur, Comm Links sayfasında düzenlenir).
Ayrı bir Fact grubu ikinci bir doğruluk kaynağı yaratır ve iki link aynı anda farklı
profillerle çalışamaz hale gelir. Aşama B/C ek ayar getirirse Fact grubu yeniden
değerlendirilir.

## Dosya yapısı

Yeni dosyalar `custom/src/Comms/` altında. Her dosyanın tek bir sorumluluğu var:

| Dosya | Sorumluluk |
|---|---|
| `ProtocolProfile.h/.cc` | JSON profilini yükle, doğrula, erişimci sun. Ağ bilmez. |
| `FrameCodec.h` | Soyut arayüz: bayt ↔ `QJsonObject` |
| `JsonDatagramCodec.h/.cc` | Bir datagram = bir JSON nesnesi |
| `FieldMapper.h/.cc` | Eşleme kurallarını uygula. Saf dönüşüm. |
| `MAVLinkSynthesizer.h/.cc` | Eşlenmiş değerlerden MAVLink mesajı üret |
| `ProtocolChannel.h` | Soyut arayüz: bağlan/kop/gönder/al |
| `UdpProtocolChannel.h/.cc` | UDP bind + datagram alma |
| `ExternalProtocolConfiguration.h/.cc` | `LinkConfiguration` alt sınıfı |
| `ExternalProtocolLink.h/.cc` | Orkestratör, `LinkInterface` alt sınıfı |

Testler `test/Custom/` altında (core test altyapısı buna zorluyor).

---

### Task 1: ProtocolProfile — profil yükleme ve doğrulama

Profil dosyasını okuyup doğrulayan saf veri sınıfı. Ağ, MAVLink veya QGC bağımlılığı yok — bu yüzden ilk görev ve en kolay test edilen.

**Files:**
- Create: `custom/src/Comms/ProtocolProfile.h`
- Create: `custom/src/Comms/ProtocolProfile.cc`
- Create: `test/Custom/ProtocolProfileTest.h`
- Create: `test/Custom/ProtocolProfileTest.cc`
- Create: `test/Custom/CMakeLists.txt`
- Modify: `test/CMakeLists.txt` (dosya sonuna, `add_subdirectory(Vehicle)` bloğundan sonra)
- Modify: `test/UnitTestList.cc` (include bloğuna ve kayıt bloğuna)
- Modify: `custom/CMakeLists.txt` (`CUSTOM_SOURCES` ve `CUSTOM_INCLUDE_DIRECTORIES` listelerine)

**Interfaces:**
- Consumes: hiçbir şey (ilk görev)
- Produces:
  - `struct MappingRule { QString from; enum Kind { None, Scale, Unit, Map } kind; double scale; QString unit; QVariantMap valueMap; QVariant defaultValue; bool hasDefault; }`
  - `struct ChannelConfig { QString name; QString direction; QString transport; QString host; quint16 port; QString codec; int timeoutMs; int reconnectMs; }`
  - `class ProtocolProfile` — `bool loadFromJson(const QByteArray&, QString* errorString)`, `bool isValid() const`, `QString name() const`, `int heartbeatHz() const`, `int sysId() const`, `int compId() const`, `QString mavTypeName() const`, `ChannelConfig channel(const QString&) const`, `QStringList channelNames() const`, `QHash<QString, MappingRule> mavlinkRules() const`, `QStringList passthroughFields() const`, `QString flightModeField() const`, `QMap<QString,int> flightModeValues() const`

- [ ] **Step 1: Write the failing test**

`test/Custom/ProtocolProfileTest.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

class ProtocolProfileTest : public UnitTest
{
    Q_OBJECT

public:
    ProtocolProfileTest() = default;

private slots:
    void _loadsValidProfile_test();
    void _rejectsMalformedJson_test();
    void _rejectsProfileWithoutName_test();
    void _rejectsRuleWithBothScaleAndUnit_test();
    void _parsesChannelEndpoint_test();
    void _parsesMappingRuleKinds_test();
    void _parsesFlightModeValues_test();
    void _missingOptionalSectionsAreEmpty_test();

private:
    static QByteArray _minimalProfile();
};
```

`test/Custom/ProtocolProfileTest.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ProtocolProfileTest.h"
#include "ProtocolProfile.h"

#include <QtTest/QTest>

QByteArray ProtocolProfileTest::_minimalProfile()
{
    return R"({
        "name": "Test Profile",
        "schemaVersion": 1,
        "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 42, "compId": 1, "heartbeatHz": 2 },
        "channels": {
            "telemetry": { "dir": "in", "transport": "udp", "bind": "0.0.0.0:5001",
                           "codec": "json-datagram", "timeoutMs": 3000 }
        },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat": { "from": "gps.lat", "scale": 1e7 },
                "ATTITUDE.yaw":            { "from": "yaw", "unit": "deg2rad" },
                "GPS_RAW_INT.fix_type":    { "from": "gps.fix", "map": { "true": 3, "false": 0 } },
                "VFR_HUD.groundspeed":     { "from": "speed", "default": 0 }
            },
            "flightMode": { "from": "mode", "values": { "ARAMA": 0, "TAKIP": 1, "KILITLI": 2 } },
            "passthrough": [ "tracking", "shots" ]
        }
    })";
}

void ProtocolProfileTest::_loadsValidProfile_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(profile.loadFromJson(_minimalProfile(), &error));
    QVERIFY(error.isEmpty());
    QVERIFY(profile.isValid());
    QCOMPARE(profile.name(), QStringLiteral("Test Profile"));
    QCOMPARE(profile.sysId(), 42);
    QCOMPARE(profile.compId(), 1);
    QCOMPARE(profile.heartbeatHz(), 2);
}

void ProtocolProfileTest::_rejectsMalformedJson_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(QByteArrayLiteral("{ not json"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!profile.isValid());
}

void ProtocolProfileTest::_rejectsProfileWithoutName_test()
{
    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(QByteArrayLiteral(R"({"schemaVersion": 1})"), &error));
    QVERIFY(error.contains(QStringLiteral("name")));
}

void ProtocolProfileTest::_rejectsRuleWithBothScaleAndUnit_test()
{
    const QByteArray json = R"({
        "name": "Bad", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": { "source": "telemetry",
                       "mavlink": { "ATTITUDE.yaw": { "from": "yaw", "scale": 2, "unit": "deg2rad" } } }
    })";

    ProtocolProfile profile;
    QString error;

    QVERIFY(!profile.loadFromJson(json, &error));
    QVERIFY(!error.isEmpty());
}

void ProtocolProfileTest::_parsesChannelEndpoint_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));

    const ChannelConfig channel = profile.channel(QStringLiteral("telemetry"));
    QCOMPARE(channel.name,      QStringLiteral("telemetry"));
    QCOMPARE(channel.direction, QStringLiteral("in"));
    QCOMPARE(channel.transport, QStringLiteral("udp"));
    QCOMPARE(channel.host,      QStringLiteral("0.0.0.0"));
    QCOMPARE(channel.port,      static_cast<quint16>(5001));
    QCOMPARE(channel.codec,     QStringLiteral("json-datagram"));
    QCOMPARE(channel.timeoutMs, 3000);

    QCOMPARE(profile.channelNames().size(), 1);
}

void ProtocolProfileTest::_parsesMappingRuleKinds_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));

    const QHash<QString, MappingRule> rules = profile.mavlinkRules();
    QCOMPARE(rules.size(), 4);

    const MappingRule scaleRule = rules.value(QStringLiteral("GLOBAL_POSITION_INT.lat"));
    QCOMPARE(scaleRule.from,  QStringLiteral("gps.lat"));
    QCOMPARE(scaleRule.kind,  MappingRule::Scale);
    QCOMPARE(scaleRule.scale, 1e7);

    const MappingRule unitRule = rules.value(QStringLiteral("ATTITUDE.yaw"));
    QCOMPARE(unitRule.kind, MappingRule::Unit);
    QCOMPARE(unitRule.unit, QStringLiteral("deg2rad"));

    const MappingRule mapRule = rules.value(QStringLiteral("GPS_RAW_INT.fix_type"));
    QCOMPARE(mapRule.kind, MappingRule::Map);
    QCOMPARE(mapRule.valueMap.value(QStringLiteral("true")).toInt(), 3);

    const MappingRule defaultRule = rules.value(QStringLiteral("VFR_HUD.groundspeed"));
    QCOMPARE(defaultRule.kind, MappingRule::None);
    QVERIFY(defaultRule.hasDefault);
    QCOMPARE(defaultRule.defaultValue.toInt(), 0);
}

void ProtocolProfileTest::_parsesFlightModeValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_minimalProfile(), nullptr));

    QCOMPARE(profile.flightModeField(), QStringLiteral("mode"));

    const QMap<QString, int> modes = profile.flightModeValues();
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.value(QStringLiteral("TAKIP")), 1);
}

void ProtocolProfileTest::_missingOptionalSectionsAreEmpty_test()
{
    const QByteArray json = R"({
        "name": "Sparse", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": { "source": "telemetry", "mavlink": {} }
    })";

    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(json, nullptr));
    QVERIFY(profile.passthroughFields().isEmpty());
    QVERIFY(profile.flightModeField().isEmpty());
    QVERIFY(profile.flightModeValues().isEmpty());
}
```

`test/Custom/CMakeLists.txt`:

```cmake
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        ProtocolProfileTest.cc
        ProtocolProfileTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

`test/CMakeLists.txt` — dosyanın sonuna ekle:

```cmake
if(QGC_CUSTOM_BUILD)
    add_subdirectory(Custom)
    add_qgc_test(ProtocolProfileTest)
endif()
```

`test/UnitTestList.cc` — include bloğunun sonuna:

```cpp
#ifdef QGC_CUSTOM_BUILD
// Custom
#include "ProtocolProfileTest.h"
#endif
```

ve kayıt bloğunun sonuna (son `UT_REGISTER_TEST` satırından sonra):

```cpp
#ifdef QGC_CUSTOM_BUILD
    UT_REGISTER_TEST(ProtocolProfileTest)
#endif
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd build && ninja
```

Expected: FAIL — derleme hatası, `ProtocolProfile.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`custom/src/Comms/ProtocolProfile.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Declarative description of a non-MAVLink protocol, loaded from JSON.
 */

#pragma once

#include <QtCore/QHash>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

Q_DECLARE_LOGGING_CATEGORY(ProtocolProfileLog)

/// One telemetry field mapping: where the value comes from and how it is transformed.
/// Scale, Unit and Map are mutually exclusive; a rule carries at most one of them.
struct MappingRule
{
    enum Kind {
        None,   ///< value used as-is
        Scale,  ///< value multiplied by `scale`
        Unit,   ///< named conversion, see `unit`
        Map     ///< discrete lookup through `valueMap`
    };

    QString     from;                   ///< dot-separated source path, e.g. "gps.lat"
    Kind        kind        = None;
    double      scale       = 1.0;
    QString     unit;                   ///< deg2rad | rad2deg | m2cm | cm2m
    QVariantMap valueMap;
    QVariant    defaultValue;
    bool        hasDefault  = false;
};

/// One transport endpoint declared by the profile.
struct ChannelConfig
{
    QString name;
    QString direction;                  ///< "in" or "out"
    QString transport;                  ///< "udp" or "tcp"
    QString host;
    quint16 port        = 0;
    QString codec;
    int     timeoutMs   = 0;
    int     reconnectMs = 0;
    bool    valid       = false;
};

/// Parsed, validated protocol profile. Pure data: knows nothing about sockets or MAVLink.
class ProtocolProfile
{
public:
    ProtocolProfile() = default;

    /// Parses and validates `json`. On failure returns false, leaves the profile invalid,
    /// and writes a human-readable reason to `errorString` when it is non-null.
    bool loadFromJson(const QByteArray &json, QString *errorString);

    bool isValid() const { return _valid; }

    QString name() const { return _name; }
    int     schemaVersion() const { return _schemaVersion; }

    QString mavTypeName() const { return _mavTypeName; }
    int     sysId() const { return _sysId; }
    int     compId() const { return _compId; }
    int     heartbeatHz() const { return _heartbeatHz; }

    QStringList   channelNames() const { return _channels.keys(); }
    ChannelConfig channel(const QString &name) const { return _channels.value(name); }

    QString telemetrySourceChannel() const { return _telemetrySource; }
    QHash<QString, MappingRule> mavlinkRules() const { return _mavlinkRules; }
    QStringList passthroughFields() const { return _passthrough; }

    QString          flightModeField() const { return _flightModeField; }
    QMap<QString,int> flightModeValues() const { return _flightModeValues; }

private:
    bool _fail(QString *errorString, const QString &reason);

    bool    _valid          = false;
    QString _name;
    int     _schemaVersion  = 0;
    QString _mavTypeName    = QStringLiteral("MAV_TYPE_GENERIC");
    int     _sysId          = 1;
    int     _compId         = 1;
    int     _heartbeatHz    = 1;

    QMap<QString, ChannelConfig> _channels;
    QString                      _telemetrySource;
    QHash<QString, MappingRule>  _mavlinkRules;
    QStringList                  _passthrough;
    QString                      _flightModeField;
    QMap<QString, int>           _flightModeValues;
};
```

`custom/src/Comms/ProtocolProfile.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ProtocolProfile.h"

#include "QGCLoggingCategory.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

QGC_LOGGING_CATEGORY(ProtocolProfileLog, "custom.comms.protocolprofile")

bool ProtocolProfile::_fail(QString *errorString, const QString &reason)
{
    _valid = false;
    if (errorString) {
        *errorString = reason;
    }
    qCWarning(ProtocolProfileLog) << "Profile rejected:" << reason;
    return false;
}

/// Splits "host:port" into its parts. Returns false when the port is missing or out of range.
static bool parseEndpoint(const QString &endpoint, QString &host, quint16 &port)
{
    const int separator = endpoint.lastIndexOf(QLatin1Char(':'));
    if (separator <= 0) {
        return false;
    }

    bool ok = false;
    const uint parsed = QStringView(endpoint).mid(separator + 1).toUInt(&ok);
    if (!ok || (parsed == 0) || (parsed > 65535)) {
        return false;
    }

    host = endpoint.left(separator);
    port = static_cast<quint16>(parsed);
    return true;
}

bool ProtocolProfile::loadFromJson(const QByteArray &json, QString *errorString)
{
    *this = ProtocolProfile();

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return _fail(errorString, QStringLiteral("JSON parse error: %1").arg(parseError.errorString()));
    }
    if (!document.isObject()) {
        return _fail(errorString, QStringLiteral("Profile root must be a JSON object"));
    }

    const QJsonObject root = document.object();

    _name = root.value(QStringLiteral("name")).toString();
    if (_name.isEmpty()) {
        return _fail(errorString, QStringLiteral("Profile is missing a non-empty \"name\""));
    }
    _schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);

    const QJsonObject vehicle = root.value(QStringLiteral("vehicle")).toObject();
    _mavTypeName = vehicle.value(QStringLiteral("mavType")).toString(QStringLiteral("MAV_TYPE_GENERIC"));
    _sysId       = vehicle.value(QStringLiteral("sysId")).toInt(1);
    _compId      = vehicle.value(QStringLiteral("compId")).toInt(1);
    _heartbeatHz = vehicle.value(QStringLiteral("heartbeatHz")).toInt(1);
    if (_heartbeatHz <= 0) {
        return _fail(errorString, QStringLiteral("vehicle.heartbeatHz must be greater than zero"));
    }

    const QJsonObject channels = root.value(QStringLiteral("channels")).toObject();
    if (channels.isEmpty()) {
        return _fail(errorString, QStringLiteral("Profile declares no channels"));
    }
    for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();

        ChannelConfig channel;
        channel.name        = it.key();
        channel.direction   = source.value(QStringLiteral("dir")).toString();
        channel.transport   = source.value(QStringLiteral("transport")).toString();
        channel.codec       = source.value(QStringLiteral("codec")).toString();
        channel.timeoutMs   = source.value(QStringLiteral("timeoutMs")).toInt(0);
        channel.reconnectMs = source.value(QStringLiteral("reconnectMs")).toInt(0);

        if ((channel.direction != QLatin1String("in")) && (channel.direction != QLatin1String("out"))) {
            return _fail(errorString, QStringLiteral("Channel \"%1\": dir must be \"in\" or \"out\"").arg(it.key()));
        }

        const QString endpoint = (channel.direction == QLatin1String("in"))
                ? source.value(QStringLiteral("bind")).toString()
                : source.value(QStringLiteral("connect")).toString();
        if (!parseEndpoint(endpoint, channel.host, channel.port)) {
            return _fail(errorString, QStringLiteral("Channel \"%1\": invalid endpoint \"%2\"").arg(it.key(), endpoint));
        }

        channel.valid = true;
        _channels.insert(channel.name, channel);
    }

    const QJsonObject telemetry = root.value(QStringLiteral("telemetry")).toObject();
    _telemetrySource = telemetry.value(QStringLiteral("source")).toString();
    if (!_channels.contains(_telemetrySource)) {
        return _fail(errorString, QStringLiteral("telemetry.source \"%1\" names no declared channel").arg(_telemetrySource));
    }

    const QJsonObject mavlink = telemetry.value(QStringLiteral("mavlink")).toObject();
    for (auto it = mavlink.constBegin(); it != mavlink.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();

        MappingRule rule;
        rule.from = source.value(QStringLiteral("from")).toString();
        if (rule.from.isEmpty()) {
            return _fail(errorString, QStringLiteral("Mapping \"%1\" is missing \"from\"").arg(it.key()));
        }

        const bool hasScale = source.contains(QStringLiteral("scale"));
        const bool hasUnit  = source.contains(QStringLiteral("unit"));
        const bool hasMap   = source.contains(QStringLiteral("map"));
        if ((hasScale + hasUnit + hasMap) > 1) {
            return _fail(errorString, QStringLiteral("Mapping \"%1\": scale, unit and map are mutually exclusive").arg(it.key()));
        }

        if (hasScale) {
            rule.kind  = MappingRule::Scale;
            rule.scale = source.value(QStringLiteral("scale")).toDouble(1.0);
        } else if (hasUnit) {
            rule.kind = MappingRule::Unit;
            rule.unit = source.value(QStringLiteral("unit")).toString();

            static const QStringList supportedUnits = {
                QStringLiteral("deg2rad"), QStringLiteral("rad2deg"),
                QStringLiteral("m2cm"),    QStringLiteral("cm2m")
            };
            if (!supportedUnits.contains(rule.unit)) {
                return _fail(errorString, QStringLiteral("Mapping \"%1\": unsupported unit \"%2\"").arg(it.key(), rule.unit));
            }
        } else if (hasMap) {
            rule.kind     = MappingRule::Map;
            rule.valueMap = source.value(QStringLiteral("map")).toObject().toVariantMap();
        }

        if (source.contains(QStringLiteral("default"))) {
            rule.hasDefault   = true;
            rule.defaultValue = source.value(QStringLiteral("default")).toVariant();
        }

        _mavlinkRules.insert(it.key(), rule);
    }

    const QJsonArray passthrough = telemetry.value(QStringLiteral("passthrough")).toArray();
    for (const QJsonValue &field : passthrough) {
        _passthrough.append(field.toString());
    }

    const QJsonObject flightMode = telemetry.value(QStringLiteral("flightMode")).toObject();
    _flightModeField = flightMode.value(QStringLiteral("from")).toString();
    const QJsonObject modeValues = flightMode.value(QStringLiteral("values")).toObject();
    for (auto it = modeValues.constBegin(); it != modeValues.constEnd(); ++it) {
        _flightModeValues.insert(it.key(), it.value().toInt());
    }

    _valid = true;
    if (errorString) {
        errorString->clear();
    }
    return true;
}
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine, `CustomPlugin.h` satırından sonra ekle:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ProtocolProfile.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ProtocolProfile.h
```

ve `CUSTOM_INCLUDE_DIRECTORIES` listesine:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:ProtocolProfileTest
```

Expected: PASS — 8 test slotu geçer, `Totals: 8 passed, 0 failed`.

- [ ] **Step 5: Commit**

```bash
git add custom/src/Comms/ProtocolProfile.h custom/src/Comms/ProtocolProfile.cc custom/CMakeLists.txt test/Custom/ test/CMakeLists.txt test/UnitTestList.cc
git commit -m "$(cat <<'EOF'
feat(custom): add ProtocolProfile for declarative protocol descriptions

Loads and validates a JSON profile describing a non-MAVLink protocol:
channels, telemetry field mappings, flight mode values and passthrough
fields. Pure data with no network or MAVLink dependency.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: JsonDatagramCodec — bayt ↔ JSON çevirimi

Bir UDP datagramını `QJsonObject`'e çeviren saf dönüşüm. Bozuk girdi asla istisna fırlatmaz.

**Files:**
- Create: `custom/src/Comms/FrameCodec.h`
- Create: `custom/src/Comms/JsonDatagramCodec.h`
- Create: `custom/src/Comms/JsonDatagramCodec.cc`
- Create: `test/Custom/JsonDatagramCodecTest.h`
- Create: `test/Custom/JsonDatagramCodecTest.cc`
- Modify: `test/Custom/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`
- Modify: `test/UnitTestList.cc`
- Modify: `custom/CMakeLists.txt`

**Interfaces:**
- Consumes: hiçbir şey (Task 1'den bağımsız)
- Produces:
  - `class FrameCodec` — soyut: `virtual QList<QJsonObject> decode(const QByteArray &bytes) = 0`, `virtual QByteArray encode(const QJsonObject &object) = 0`, `virtual ~FrameCodec()`
  - `class JsonDatagramCodec : public FrameCodec` — `decode()` tek elemanlı liste veya boş liste döner; `encode()` kompakt JSON döner
  - `int JsonDatagramCodec::malformedCount() const`

- [ ] **Step 1: Write the failing test**

`test/Custom/JsonDatagramCodecTest.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

class JsonDatagramCodecTest : public UnitTest
{
    Q_OBJECT

public:
    JsonDatagramCodecTest() = default;

private slots:
    void _decodesSingleObject_test();
    void _decodesNestedObject_test();
    void _malformedInputYieldsNothing_test();
    void _malformedInputIsCounted_test();
    void _nonObjectRootYieldsNothing_test();
    void _emptyInputYieldsNothing_test();
    void _encodeRoundTrips_test();
};
```

`test/Custom/JsonDatagramCodecTest.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "JsonDatagramCodecTest.h"
#include "JsonDatagramCodec.h"

#include <QtCore/QJsonObject>
#include <QtTest/QTest>

void JsonDatagramCodecTest::_decodesSingleObject_test()
{
    JsonDatagramCodec codec;

    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral(R"({"seq": 7, "battery": 85.5})"));

    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first().value(QStringLiteral("seq")).toInt(), 7);
    QCOMPARE(frames.first().value(QStringLiteral("battery")).toDouble(), 85.5);
}

void JsonDatagramCodecTest::_decodesNestedObject_test()
{
    JsonDatagramCodec codec;

    const QList<QJsonObject> frames = codec.decode(QByteArrayLiteral(R"({"gps": {"lat": 39.9, "fix": true}})"));

    QCOMPARE(frames.size(), 1);
    const QJsonObject gps = frames.first().value(QStringLiteral("gps")).toObject();
    QCOMPARE(gps.value(QStringLiteral("lat")).toDouble(), 39.9);
    QCOMPARE(gps.value(QStringLiteral("fix")).toBool(), true);
}

void JsonDatagramCodecTest::_malformedInputYieldsNothing_test()
{
    JsonDatagramCodec codec;

    QVERIFY(codec.decode(QByteArrayLiteral("{ this is not json")).isEmpty());
    QVERIFY(codec.decode(QByteArrayLiteral("\x01\x02\x03")).isEmpty());
}

void JsonDatagramCodecTest::_malformedInputIsCounted_test()
{
    JsonDatagramCodec codec;
    QCOMPARE(codec.malformedCount(), 0);

    (void) codec.decode(QByteArrayLiteral("{ bad"));
    (void) codec.decode(QByteArrayLiteral("{ also bad"));
    QCOMPARE(codec.malformedCount(), 2);

    (void) codec.decode(QByteArrayLiteral(R"({"ok": 1})"));
    QCOMPARE(codec.malformedCount(), 2);
}

void JsonDatagramCodecTest::_nonObjectRootYieldsNothing_test()
{
    JsonDatagramCodec codec;

    QVERIFY(codec.decode(QByteArrayLiteral("[1, 2, 3]")).isEmpty());
    QVERIFY(codec.decode(QByteArrayLiteral("42")).isEmpty());
}

void JsonDatagramCodecTest::_emptyInputYieldsNothing_test()
{
    JsonDatagramCodec codec;

    QVERIFY(codec.decode(QByteArray()).isEmpty());
    QCOMPARE(codec.malformedCount(), 0); // empty is not malformed, just nothing
}

void JsonDatagramCodecTest::_encodeRoundTrips_test()
{
    JsonDatagramCodec codec;

    QJsonObject original;
    original.insert(QStringLiteral("cmd"), QStringLiteral("arm"));
    original.insert(QStringLiteral("id"), 3);

    const QList<QJsonObject> frames = codec.decode(codec.encode(original));

    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first(), original);
}
```

`test/Custom/CMakeLists.txt` — `target_sources` bloğuna ekle:

```cmake
        JsonDatagramCodecTest.cc
        JsonDatagramCodecTest.h
```

`test/CMakeLists.txt` — `if(QGC_CUSTOM_BUILD)` bloğuna ekle:

```cmake
    add_qgc_test(JsonDatagramCodecTest)
```

`test/UnitTestList.cc` — include bloğuna `#include "JsonDatagramCodecTest.h"` ve kayıt bloğuna `UT_REGISTER_TEST(JsonDatagramCodecTest)` ekle (ikisi de mevcut `#ifdef QGC_CUSTOM_BUILD` bloklarının içine).

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd build && ninja
```

Expected: FAIL — `JsonDatagramCodec.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`custom/src/Comms/FrameCodec.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Framing strategy: turns raw bytes into JSON objects and back.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QList>

/// Converts between the wire's byte framing and JSON objects.
///
/// decode() takes whatever bytes arrived and returns however many complete frames
/// they contained: zero for a malformed datagram, one for a JSON datagram, many for
/// a stream carrying several newline-delimited objects. Implementations keep any
/// partial trailing frame internally.
///
/// Neither method throws. Malformed input is dropped and counted.
class FrameCodec
{
public:
    virtual ~FrameCodec() = default;

    virtual QList<QJsonObject> decode(const QByteArray &bytes) = 0;
    virtual QByteArray encode(const QJsonObject &object) = 0;

    /// Number of frames dropped as malformed since construction.
    virtual int malformedCount() const = 0;
};
```

`custom/src/Comms/JsonDatagramCodec.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "FrameCodec.h"

#include <QtCore/QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(JsonDatagramCodecLog)

/// One datagram carries exactly one JSON object. There is no partial-frame state:
/// a datagram either parses or it does not.
class JsonDatagramCodec : public FrameCodec
{
public:
    JsonDatagramCodec() = default;

    QList<QJsonObject> decode(const QByteArray &bytes) final;
    QByteArray encode(const QJsonObject &object) final;
    int malformedCount() const final { return _malformedCount; }

private:
    int _malformedCount = 0;
};
```

`custom/src/Comms/JsonDatagramCodec.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "JsonDatagramCodec.h"

#include "QGCLoggingCategory.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>

QGC_LOGGING_CATEGORY(JsonDatagramCodecLog, "custom.comms.jsondatagramcodec")

QList<QJsonObject> JsonDatagramCodec::decode(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return {};
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);

    if ((parseError.error != QJsonParseError::NoError) || !document.isObject()) {
        _malformedCount++;
        qCDebug(JsonDatagramCodecLog) << "Dropped malformed datagram, total:" << _malformedCount
                                      << "reason:" << parseError.errorString();
        return {};
    }

    return { document.object() };
}

QByteArray JsonDatagramCodec::encode(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine ekle:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/FrameCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/JsonDatagramCodec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/JsonDatagramCodec.h
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:JsonDatagramCodecTest
```

Expected: PASS — 7 test slotu geçer.

- [ ] **Step 5: Commit**

```bash
git add custom/src/Comms/FrameCodec.h custom/src/Comms/JsonDatagramCodec.h custom/src/Comms/JsonDatagramCodec.cc custom/CMakeLists.txt test/Custom/ test/CMakeLists.txt test/UnitTestList.cc
git commit -m "$(cat <<'EOF'
feat(custom): add FrameCodec interface and JSON datagram codec

FrameCodec abstracts wire framing; JsonDatagramCodec implements the
one-datagram-one-object case used by the turret telemetry channel.
Malformed input is dropped and counted, never thrown.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: FieldMapper — eşleme kurallarının uygulanması

Profil kurallarını gelen JSON'a uygulayan saf dönüşüm. Aşama A'nın en yoğun mantığı burada, bu yüzden test kapsamı en geniş görev bu.

**Files:**
- Create: `custom/src/Comms/FieldMapper.h`
- Create: `custom/src/Comms/FieldMapper.cc`
- Create: `test/Custom/FieldMapperTest.h`
- Create: `test/Custom/FieldMapperTest.cc`
- Modify: `test/Custom/CMakeLists.txt`, `test/CMakeLists.txt`, `test/UnitTestList.cc`, `custom/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1'den `ProtocolProfile`, `MappingRule`
- Produces:
  - `class FieldMapper` — `explicit FieldMapper(const ProtocolProfile &profile)`
  - `QHash<QString, QVariant> mapTelemetry(const QJsonObject &packet) const` — anahtarlar `"MESSAGE.field"` biçimi
  - `QVariantMap passthrough(const QJsonObject &packet) const`
  - `int flightModeValue(const QJsonObject &packet, bool *found) const`
  - `static QVariant valueAtPath(const QJsonObject &object, const QString &dottedPath)` — yol yoksa geçersiz `QVariant`

- [ ] **Step 1: Write the failing test**

`test/Custom/FieldMapperTest.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

class FieldMapperTest : public UnitTest
{
    Q_OBJECT

public:
    FieldMapperTest() = default;

private slots:
    void _valueAtPathFindsNestedValue_test();
    void _valueAtPathMissingReturnsInvalid_test();
    void _valueAtPathThroughNullReturnsInvalid_test();
    void _appliesScale_test();
    void _appliesDegToRad_test();
    void _appliesDiscreteMap_test();
    void _usesDefaultWhenSourceMissing_test();
    void _omitsFieldWhenSourceMissingAndNoDefault_test();
    void _nullTargetDoesNotCrash_test();
    void _passthroughCopiesDeclaredFields_test();
    void _flightModeMapsKnownValue_test();
    void _flightModeUnknownValueReportsNotFound_test();

private:
    static QByteArray _profileJson();
};
```

`test/Custom/FieldMapperTest.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "FieldMapperTest.h"
#include "FieldMapper.h"
#include "ProtocolProfile.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtMath>
#include <QtTest/QTest>

QByteArray FieldMapperTest::_profileJson()
{
    return R"({
        "name": "Mapper Test", "schemaVersion": 1,
        "vehicle": { "sysId": 1, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat": { "from": "gps.lat", "scale": 1e7 },
                "ATTITUDE.yaw":            { "from": "yaw", "unit": "deg2rad" },
                "GPS_RAW_INT.fix_type":    { "from": "gps.fix", "map": { "true": 3, "false": 0 } },
                "VFR_HUD.groundspeed":     { "from": "speed", "default": 0 },
                "SYS_STATUS.battery_remaining": { "from": "battery" }
            },
            "flightMode": { "from": "mode", "values": { "ARAMA": 0, "TAKIP": 1, "KILITLI": 2 } },
            "passthrough": [ "tracking", "shots", "target" ]
        }
    })";
}

static QJsonObject packetFrom(const char *json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

void FieldMapperTest::_valueAtPathFindsNestedValue_test()
{
    const QJsonObject packet = packetFrom(R"({"gps": {"lat": 39.925533}})");

    QCOMPARE(FieldMapper::valueAtPath(packet, QStringLiteral("gps.lat")).toDouble(), 39.925533);
}

void FieldMapperTest::_valueAtPathMissingReturnsInvalid_test()
{
    const QJsonObject packet = packetFrom(R"({"gps": {"lat": 1.0}})");

    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("gps.lon")).isValid());
    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("nope")).isValid());
    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("gps.lat.deeper")).isValid());
}

void FieldMapperTest::_valueAtPathThroughNullReturnsInvalid_test()
{
    const QJsonObject packet = packetFrom(R"({"target": null})");

    QVERIFY(!FieldMapper::valueAtPath(packet, QStringLiteral("target.range_m")).isValid());
}

void FieldMapperTest::_appliesScale_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"gps": {"lat": 39.925533}})"));

    QVERIFY(mapped.contains(QStringLiteral("GLOBAL_POSITION_INT.lat")));
    QCOMPARE(mapped.value(QStringLiteral("GLOBAL_POSITION_INT.lat")).toDouble(), 39.925533 * 1e7);
}

void FieldMapperTest::_appliesDegToRad_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"yaw": 180.0})"));

    QVERIFY(qFuzzyCompare(mapped.value(QStringLiteral("ATTITUDE.yaw")).toDouble() + 1.0, M_PI + 1.0));
}

void FieldMapperTest::_appliesDiscreteMap_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    QCOMPARE(mapper.mapTelemetry(packetFrom(R"({"gps": {"fix": true}})"))
                .value(QStringLiteral("GPS_RAW_INT.fix_type")).toInt(), 3);
    QCOMPARE(mapper.mapTelemetry(packetFrom(R"({"gps": {"fix": false}})"))
                .value(QStringLiteral("GPS_RAW_INT.fix_type")).toInt(), 0);
}

void FieldMapperTest::_usesDefaultWhenSourceMissing_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"battery": 50})"));

    QVERIFY(mapped.contains(QStringLiteral("VFR_HUD.groundspeed")));
    QCOMPARE(mapped.value(QStringLiteral("VFR_HUD.groundspeed")).toInt(), 0);
}

void FieldMapperTest::_omitsFieldWhenSourceMissingAndNoDefault_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    // "battery" has no default; a packet without it must not produce the key at all,
    // so the synthesizer leaves the previous value in place.
    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packetFrom(R"({"yaw": 0.0})"));

    QVERIFY(!mapped.contains(QStringLiteral("SYS_STATUS.battery_remaining")));
}

void FieldMapperTest::_nullTargetDoesNotCrash_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QJsonObject packet = packetFrom(R"({"target": null, "tracking": false, "shots": 0})");

    const QHash<QString, QVariant> mapped = mapper.mapTelemetry(packet);
    Q_UNUSED(mapped);

    const QVariantMap through = mapper.passthrough(packet);
    QVERIFY(through.contains(QStringLiteral("target")));
    QVERIFY(through.value(QStringLiteral("target")).isNull());
}

void FieldMapperTest::_passthroughCopiesDeclaredFields_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    const QVariantMap through = mapper.passthrough(
        packetFrom(R"({"tracking": true, "shots": 3, "fps": 30, "target": {"track_id": 7}})"));

    QCOMPARE(through.value(QStringLiteral("tracking")).toBool(), true);
    QCOMPARE(through.value(QStringLiteral("shots")).toInt(), 3);
    QCOMPARE(through.value(QStringLiteral("target")).toMap().value(QStringLiteral("track_id")).toInt(), 7);
    // "fps" is not declared in passthrough, so it must not leak through
    QVERIFY(!through.contains(QStringLiteral("fps")));
}

void FieldMapperTest::_flightModeMapsKnownValue_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    bool found = false;
    QCOMPARE(mapper.flightModeValue(packetFrom(R"({"mode": "KILITLI"})"), &found), 2);
    QVERIFY(found);
}

void FieldMapperTest::_flightModeUnknownValueReportsNotFound_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    const FieldMapper mapper(profile);

    bool found = true;
    (void) mapper.flightModeValue(packetFrom(R"({"mode": "BILINMEYEN"})"), &found);
    QVERIFY(!found);

    found = true;
    (void) mapper.flightModeValue(packetFrom(R"({"battery": 1})"), &found);
    QVERIFY(!found);
}
```

Test kayıtları: `test/Custom/CMakeLists.txt`'e `FieldMapperTest.cc` / `FieldMapperTest.h`, `test/CMakeLists.txt`'e `add_qgc_test(FieldMapperTest)`, `test/UnitTestList.cc`'ye `#include "FieldMapperTest.h"` ve `UT_REGISTER_TEST(FieldMapperTest)`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd build && ninja
```

Expected: FAIL — `FieldMapper.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`custom/src/Comms/FieldMapper.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Applies a ProtocolProfile's mapping rules to an incoming telemetry packet.
 */

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>

/// Turns a raw telemetry object into the values the synthesizer needs.
/// Pure transformation: no sockets, no MAVLink, no Qt object lifetime.
class FieldMapper
{
public:
    explicit FieldMapper(const ProtocolProfile &profile) : _profile(profile) {}

    /// Keys are "MESSAGE.field" exactly as written in the profile. A rule whose source
    /// field is absent and which declares no default produces no entry, so the caller
    /// keeps whatever value it already had.
    QHash<QString, QVariant> mapTelemetry(const QJsonObject &packet) const;

    /// Copies the profile's declared passthrough fields verbatim. Undeclared fields
    /// never leak through, so the QML side sees a stable, documented surface.
    QVariantMap passthrough(const QJsonObject &packet) const;

    /// Resolves the profile's flight-mode field to its numeric custom_mode value.
    /// Sets *found to false when the field is absent or its value is not in the map.
    int flightModeValue(const QJsonObject &packet, bool *found) const;

    /// Walks a dot-separated path. Returns an invalid QVariant when any segment is
    /// missing, null, or not an object where the path expects to descend.
    static QVariant valueAtPath(const QJsonObject &object, const QString &dottedPath);

private:
    static QVariant _applyRule(const MappingRule &rule, const QVariant &raw);

    ProtocolProfile _profile;
};
```

`custom/src/Comms/FieldMapper.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "FieldMapper.h"

#include <QtCore/QJsonValue>
#include <QtCore/QStringList>
#include <QtCore/QtMath>

QVariant FieldMapper::valueAtPath(const QJsonObject &object, const QString &dottedPath)
{
    if (dottedPath.isEmpty()) {
        return QVariant();
    }

    const QStringList segments = dottedPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return QVariant();
    }

    QJsonValue current = object;
    for (const QString &segment : segments) {
        if (!current.isObject()) {
            return QVariant();
        }

        const QJsonObject currentObject = current.toObject();
        if (!currentObject.contains(segment)) {
            return QVariant();
        }
        current = currentObject.value(segment);
    }

    // A JSON null carries no value; treat it as absent so callers fall back cleanly.
    if (current.isNull() || current.isUndefined()) {
        return QVariant();
    }

    return current.toVariant();
}

QVariant FieldMapper::_applyRule(const MappingRule &rule, const QVariant &raw)
{
    switch (rule.kind) {
    case MappingRule::Scale:
        return raw.toDouble() * rule.scale;

    case MappingRule::Unit:
        if (rule.unit == QLatin1String("deg2rad")) {
            return qDegreesToRadians(raw.toDouble());
        }
        if (rule.unit == QLatin1String("rad2deg")) {
            return qRadiansToDegrees(raw.toDouble());
        }
        if (rule.unit == QLatin1String("m2cm")) {
            return raw.toDouble() * 100.0;
        }
        if (rule.unit == QLatin1String("cm2m")) {
            return raw.toDouble() / 100.0;
        }
        // ProtocolProfile rejects unknown units at load time, so this is unreachable
        // in practice; passing the value through is the safe fallback.
        return raw;

    case MappingRule::Map: {
        // JSON object keys are strings, so booleans and numbers are looked up by their
        // string form: true -> "true", 3 -> "3".
        QString key = raw.toString();
        if (raw.typeId() == QMetaType::Bool) {
            key = raw.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        return rule.valueMap.value(key);
    }

    case MappingRule::None:
    default:
        return raw;
    }
}

QHash<QString, QVariant> FieldMapper::mapTelemetry(const QJsonObject &packet) const
{
    QHash<QString, QVariant> mapped;

    const QHash<QString, MappingRule> rules = _profile.mavlinkRules();
    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
        const MappingRule &rule = it.value();

        const QVariant raw = valueAtPath(packet, rule.from);
        if (!raw.isValid()) {
            if (rule.hasDefault) {
                mapped.insert(it.key(), rule.defaultValue);
            }
            // No default: leave the key out entirely so the synthesizer keeps its
            // previous value rather than writing a zero.
            continue;
        }

        const QVariant converted = _applyRule(rule, raw);
        if (converted.isValid()) {
            mapped.insert(it.key(), converted);
        }
    }

    return mapped;
}

QVariantMap FieldMapper::passthrough(const QJsonObject &packet) const
{
    QVariantMap through;

    const QStringList fields = _profile.passthroughFields();
    for (const QString &field : fields) {
        if (packet.contains(field)) {
            through.insert(field, packet.value(field).toVariant());
        }
    }

    return through;
}

int FieldMapper::flightModeValue(const QJsonObject &packet, bool *found) const
{
    if (found) {
        *found = false;
    }

    const QString field = _profile.flightModeField();
    if (field.isEmpty()) {
        return 0;
    }

    const QVariant raw = valueAtPath(packet, field);
    if (!raw.isValid()) {
        return 0;
    }

    const QMap<QString, int> values = _profile.flightModeValues();
    const QString key = raw.toString();
    if (!values.contains(key)) {
        return 0;
    }

    if (found) {
        *found = true;
    }
    return values.value(key);
}
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine `FieldMapper.cc` ve `FieldMapper.h` ekle.

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:FieldMapperTest
```

Expected: PASS — 12 test slotu geçer.

- [ ] **Step 5: Commit**

```bash
git add custom/src/Comms/FieldMapper.h custom/src/Comms/FieldMapper.cc custom/CMakeLists.txt test/Custom/ test/CMakeLists.txt test/UnitTestList.cc
git commit -m "$(cat <<'EOF'
feat(custom): add FieldMapper for profile-driven telemetry mapping

Applies a profile's mapping rules to an incoming packet: dotted-path
lookup, scale/unit/discrete-map conversion, defaults, passthrough fields
and flight mode resolution. Absent sources without a default produce no
entry so the synthesizer keeps its previous value.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: MAVLinkSynthesizer — MAVLink mesajı üretimi

Eşlenmiş değerlerden MAVLink mesajları üretir. Durum tutar: eksik alanlar önceki değerinde kalır.

**Files:**
- Create: `custom/src/Comms/MAVLinkSynthesizer.h`
- Create: `custom/src/Comms/MAVLinkSynthesizer.cc`
- Create: `test/Custom/MAVLinkSynthesizerTest.h`
- Create: `test/Custom/MAVLinkSynthesizerTest.cc`
- Modify: `test/Custom/CMakeLists.txt`, `test/CMakeLists.txt`, `test/UnitTestList.cc`, `custom/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3'ten `FieldMapper`, Task 1'den `ProtocolProfile`
- Produces:
  - `class MAVLinkSynthesizer` — `MAVLinkSynthesizer(const ProtocolProfile &profile, uint8_t mavlinkChannel)`
  - `QList<mavlink_message_t> buildTelemetryMessages(const QHash<QString, QVariant> &mapped)`
  - `mavlink_message_t buildHeartbeat(int customMode) const`

  Sentezleyici uçuş modunu **saklamaz**; çağıran (Task 6) `_customMode`'u kendi tutar
  ve her heartbeat'te parametre olarak geçer.

- [ ] **Step 1: Write the failing test**

`test/Custom/MAVLinkSynthesizerTest.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

class MAVLinkSynthesizerTest : public UnitTest
{
    Q_OBJECT

public:
    MAVLinkSynthesizerTest() = default;

private slots:
    void _heartbeatCarriesProfileIdentity_test();
    void _heartbeatCarriesCustomMode_test();
    void _buildsGlobalPositionFromMappedValues_test();
    void _buildsAttitudeFromMappedValues_test();
    void _omittedFieldKeepsPreviousValue_test();
    void _emptyMappingProducesNoMessages_test();

private:
    static QByteArray _profileJson();
};
```

`test/Custom/MAVLinkSynthesizerTest.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MAVLinkSynthesizerTest.h"
#include "MAVLinkSynthesizer.h"
#include "ProtocolProfile.h"

#include <QtTest/QTest>

QByteArray MAVLinkSynthesizerTest::_profileJson()
{
    return R"({
        "name": "Synth Test", "schemaVersion": 1,
        "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 42, "compId": 1, "heartbeatHz": 1 },
        "channels": { "telemetry": { "dir": "in", "transport": "udp",
                                     "bind": "0.0.0.0:5001", "codec": "json-datagram" } },
        "telemetry": {
            "source": "telemetry",
            "mavlink": {
                "GLOBAL_POSITION_INT.lat":          { "from": "gps.lat", "scale": 1e7 },
                "GLOBAL_POSITION_INT.lon":          { "from": "gps.lon", "scale": 1e7 },
                "GLOBAL_POSITION_INT.relative_alt": { "from": "altitude", "scale": 1000 },
                "ATTITUDE.yaw":                     { "from": "yaw", "unit": "deg2rad" },
                "ATTITUDE.pitch":                   { "from": "pitch", "unit": "deg2rad" }
            }
        }
    })";
}

/// Finds the first message with the given id, or returns false.
static bool findMessage(const QList<mavlink_message_t> &messages, uint32_t msgId, mavlink_message_t &out)
{
    for (const mavlink_message_t &message : messages) {
        if (message.msgid == msgId) {
            out = message;
            return true;
        }
    }
    return false;
}

void MAVLinkSynthesizerTest::_heartbeatCarriesProfileIdentity_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    const mavlink_message_t heartbeat = synthesizer.buildHeartbeat(0);

    QCOMPARE(heartbeat.msgid, static_cast<uint32_t>(MAVLINK_MSG_ID_HEARTBEAT));
    QCOMPARE(static_cast<int>(heartbeat.sysid), 42);
    QCOMPARE(static_cast<int>(heartbeat.compid), 1);
}

void MAVLinkSynthesizerTest::_heartbeatCarriesCustomMode_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    const mavlink_message_t heartbeat = synthesizer.buildHeartbeat(2);

    mavlink_heartbeat_t decoded{};
    mavlink_msg_heartbeat_decode(&heartbeat, &decoded);
    QCOMPARE(decoded.custom_mode, static_cast<uint32_t>(2));
}

void MAVLinkSynthesizerTest::_buildsGlobalPositionFromMappedValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.lat"), 39.925533 * 1e7);
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.lon"), 32.866287 * 1e7);
    mapped.insert(QStringLiteral("GLOBAL_POSITION_INT.relative_alt"), 150.3 * 1000);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(mapped), MAVLINK_MSG_ID_GLOBAL_POSITION_INT, message));

    mavlink_global_position_int_t decoded{};
    mavlink_msg_global_position_int_decode(&message, &decoded);
    QCOMPARE(decoded.lat, static_cast<int32_t>(39.925533 * 1e7));
    QCOMPARE(decoded.lon, static_cast<int32_t>(32.866287 * 1e7));
    QCOMPARE(decoded.relative_alt, static_cast<int32_t>(150.3 * 1000));
}

void MAVLinkSynthesizerTest::_buildsAttitudeFromMappedValues_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> mapped;
    mapped.insert(QStringLiteral("ATTITUDE.yaw"), 1.5);
    mapped.insert(QStringLiteral("ATTITUDE.pitch"), 0.25);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(mapped), MAVLINK_MSG_ID_ATTITUDE, message));

    mavlink_attitude_t decoded{};
    mavlink_msg_attitude_decode(&message, &decoded);
    QVERIFY(qFuzzyCompare(decoded.yaw, 1.5f));
    QVERIFY(qFuzzyCompare(decoded.pitch, 0.25f));
}

void MAVLinkSynthesizerTest::_omittedFieldKeepsPreviousValue_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QHash<QString, QVariant> first;
    first.insert(QStringLiteral("ATTITUDE.yaw"), 1.5);
    first.insert(QStringLiteral("ATTITUDE.pitch"), 0.25);
    (void) synthesizer.buildTelemetryMessages(first);

    // Second packet carries only yaw; pitch must survive from the first.
    QHash<QString, QVariant> second;
    second.insert(QStringLiteral("ATTITUDE.yaw"), 2.0);

    mavlink_message_t message{};
    QVERIFY(findMessage(synthesizer.buildTelemetryMessages(second), MAVLINK_MSG_ID_ATTITUDE, message));

    mavlink_attitude_t decoded{};
    mavlink_msg_attitude_decode(&message, &decoded);
    QVERIFY(qFuzzyCompare(decoded.yaw, 2.0f));
    QVERIFY(qFuzzyCompare(decoded.pitch, 0.25f));
}

void MAVLinkSynthesizerTest::_emptyMappingProducesNoMessages_test()
{
    ProtocolProfile profile;
    QVERIFY(profile.loadFromJson(_profileJson(), nullptr));
    MAVLinkSynthesizer synthesizer(profile, 0);

    QVERIFY(synthesizer.buildTelemetryMessages(QHash<QString, QVariant>()).isEmpty());
}
```

Test kayıtları: Task 2 ve 3'teki üç dosyaya `MAVLinkSynthesizerTest` girdilerini ekle.

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd build && ninja
```

Expected: FAIL — `MAVLinkSynthesizer.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`custom/src/Comms/MAVLinkSynthesizer.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Builds MAVLink messages out of mapped telemetry values.
 */

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QVariant>

#include "MAVLinkLib.h"

/// Produces MAVLink messages from the values FieldMapper extracted.
///
/// Holds the last known value of every field it has ever seen, so a packet that
/// omits a field yields a message carrying the previous value rather than a zero.
/// Only messages whose fields the profile actually maps are produced.
class MAVLinkSynthesizer
{
public:
    MAVLinkSynthesizer(const ProtocolProfile &profile, uint8_t mavlinkChannel);

    /// Merges `mapped` into the running state and returns one message per
    /// MAVLink message the profile mentions. Empty when the profile maps nothing.
    QList<mavlink_message_t> buildTelemetryMessages(const QHash<QString, QVariant> &mapped);

    mavlink_message_t buildHeartbeat(int customMode) const;

private:
    mavlink_message_t _buildGlobalPositionInt() const;
    mavlink_message_t _buildAttitude() const;
    mavlink_message_t _buildSysStatus() const;
    mavlink_message_t _buildGpsRawInt() const;
    mavlink_message_t _buildVfrHud() const;

    double _value(const QString &key, double fallback = 0.0) const;

    ProtocolProfile          _profile;
    uint8_t                  _channel;
    QHash<QString, QVariant> _state;      ///< last known value per "MESSAGE.field" key
    QSet<QString>            _messageIds; ///< MESSAGE names the profile maps
};
```

`custom/src/Comms/MAVLinkSynthesizer.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MAVLinkSynthesizer.h"

#include <QtCore/QDateTime>
#include <QtCore/QSet>

MAVLinkSynthesizer::MAVLinkSynthesizer(const ProtocolProfile &profile, uint8_t mavlinkChannel)
    : _profile(profile)
    , _channel(mavlinkChannel)
{
    // Remember which MAVLink messages this profile actually feeds, so we never emit
    // a message the operator did not ask for.
    const QHash<QString, MappingRule> rules = _profile.mavlinkRules();
    for (auto it = rules.constBegin(); it != rules.constEnd(); ++it) {
        _messageIds.insert(it.key().section(QLatin1Char('.'), 0, 0));
    }
}

double MAVLinkSynthesizer::_value(const QString &key, double fallback) const
{
    return _state.contains(key) ? _state.value(key).toDouble() : fallback;
}

QList<mavlink_message_t> MAVLinkSynthesizer::buildTelemetryMessages(const QHash<QString, QVariant> &mapped)
{
    for (auto it = mapped.constBegin(); it != mapped.constEnd(); ++it) {
        _state.insert(it.key(), it.value());
    }

    QList<mavlink_message_t> messages;

    if (_messageIds.contains(QStringLiteral("GLOBAL_POSITION_INT"))) {
        messages.append(_buildGlobalPositionInt());
    }
    if (_messageIds.contains(QStringLiteral("ATTITUDE"))) {
        messages.append(_buildAttitude());
    }
    if (_messageIds.contains(QStringLiteral("SYS_STATUS"))) {
        messages.append(_buildSysStatus());
    }
    if (_messageIds.contains(QStringLiteral("GPS_RAW_INT"))) {
        messages.append(_buildGpsRawInt());
    }
    if (_messageIds.contains(QStringLiteral("VFR_HUD"))) {
        messages.append(_buildVfrHud());
    }

    return messages;
}

mavlink_message_t MAVLinkSynthesizer::buildHeartbeat(int customMode) const
{
    mavlink_message_t message{};

    (void) mavlink_msg_heartbeat_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        MAV_TYPE_GENERIC,
        MAV_AUTOPILOT_GENERIC,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        static_cast<uint32_t>(customMode),
        MAV_STATE_ACTIVE);

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildGlobalPositionInt() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_global_position_int_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch()),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lat"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lon"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.alt"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.relative_alt"))),
        static_cast<int16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.vx"))),
        static_cast<int16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.vy"))),
        static_cast<int16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.vz"))),
        static_cast<uint16_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.hdg"), UINT16_MAX)));

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildAttitude() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_attitude_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch()),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.roll"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.pitch"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.yaw"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.rollspeed"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.pitchspeed"))),
        static_cast<float>(_value(QStringLiteral("ATTITUDE.yawspeed"))));

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildSysStatus() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_sys_status_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        0, 0, 0,                            // sensors present / enabled / health
        0,                                  // load
        static_cast<uint16_t>(_value(QStringLiteral("SYS_STATUS.voltage_battery"), UINT16_MAX)),
        -1,                                 // current_battery, -1 = unknown
        static_cast<int8_t>(_value(QStringLiteral("SYS_STATUS.battery_remaining"), -1)),
        0, 0, 0, 0, 0, 0,                   // comm drop/error counters
        0, 0, 0);                           // extended sensor fields

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildGpsRawInt() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_gps_raw_int_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000,
        static_cast<uint8_t>(_value(QStringLiteral("GPS_RAW_INT.fix_type"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lat"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.lon"))),
        static_cast<int32_t>(_value(QStringLiteral("GLOBAL_POSITION_INT.alt"))),
        UINT16_MAX, UINT16_MAX,             // eph, epv unknown
        UINT16_MAX,                         // vel unknown
        UINT16_MAX,                         // cog unknown
        static_cast<uint8_t>(_value(QStringLiteral("GPS_RAW_INT.satellites_visible"))),
        0, 0, 0, 0, 0, 0);                  // alt_ellipsoid, accuracies, yaw

    return message;
}

mavlink_message_t MAVLinkSynthesizer::_buildVfrHud() const
{
    mavlink_message_t message{};

    (void) mavlink_msg_vfr_hud_pack_chan(
        static_cast<uint8_t>(_profile.sysId()),
        static_cast<uint8_t>(_profile.compId()),
        _channel,
        &message,
        static_cast<float>(_value(QStringLiteral("VFR_HUD.airspeed"))),
        static_cast<float>(_value(QStringLiteral("VFR_HUD.groundspeed"))),
        static_cast<int16_t>(_value(QStringLiteral("VFR_HUD.heading"))),
        static_cast<uint16_t>(_value(QStringLiteral("VFR_HUD.throttle"))),
        static_cast<float>(_value(QStringLiteral("VFR_HUD.alt"))),
        static_cast<float>(_value(QStringLiteral("VFR_HUD.climb"))));

    return message;
}
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine `MAVLinkSynthesizer.cc` ve `MAVLinkSynthesizer.h` ekle.

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:MAVLinkSynthesizerTest
```

Expected: PASS — 6 test slotu geçer.

> **Not:** `mavlink_msg_*_pack_chan` imzaları MAVLink sürümüne göre alan sayısında değişebilir. Derleme "too few arguments" verirse, doğru imzayı `build/_deps/` altındaki üretilmiş `mavlink_msg_<name>.h` dosyasından okuyup eksik alanları 0 ile doldurun; test beklentileri değişmez.

- [ ] **Step 5: Commit**

```bash
git add custom/src/Comms/MAVLinkSynthesizer.h custom/src/Comms/MAVLinkSynthesizer.cc custom/CMakeLists.txt test/Custom/ test/CMakeLists.txt test/UnitTestList.cc
git commit -m "$(cat <<'EOF'
feat(custom): add MAVLinkSynthesizer for mapped telemetry

Builds HEARTBEAT plus the position, attitude, status, GPS and HUD
messages the profile maps. Keeps the last known value of every field so
a packet that omits one does not zero it out.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: UdpProtocolChannel — UDP datagram alma

Profilden gelen bind adresine bağlanıp datagram yayan kanal. Zaman aşımı takibi burada.

**Files:**
- Create: `custom/src/Comms/ProtocolChannel.h`
- Create: `custom/src/Comms/UdpProtocolChannel.h`
- Create: `custom/src/Comms/UdpProtocolChannel.cc`
- Create: `test/Custom/UdpProtocolChannelTest.h`
- Create: `test/Custom/UdpProtocolChannelTest.cc`
- Modify: `test/Custom/CMakeLists.txt`, `test/CMakeLists.txt`, `test/UnitTestList.cc`, `custom/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1'den `ChannelConfig`
- Produces:
  - `class ProtocolChannel : public QObject` — soyut: `virtual bool open() = 0`, `virtual void close() = 0`, `virtual bool isOpen() const = 0`, `virtual bool send(const QByteArray&) = 0`; sinyaller `void framesReceived(const QByteArray &bytes)`, `void timedOut()`, `void errorOccurred(const QString &message)`
  - `class UdpProtocolChannel : public ProtocolChannel` — `explicit UdpProtocolChannel(const ChannelConfig &config, QObject *parent = nullptr)`, `quint64 packetCount() const`

- [ ] **Step 1: Write the failing test**

`test/Custom/UdpProtocolChannelTest.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

class UdpProtocolChannelTest : public UnitTest
{
    Q_OBJECT

public:
    UdpProtocolChannelTest() = default;

private slots:
    void _opensAndBindsPort_test();
    void _receivesDatagram_test();
    void _rejectsInvalidPort_test();
    void _closeStopsReceiving_test();
    void _emitsTimeoutWhenSilent_test();
};
```

`test/Custom/UdpProtocolChannelTest.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "UdpProtocolChannelTest.h"
#include "UdpProtocolChannel.h"

#include <QtNetwork/QUdpSocket>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

/// Binds an ephemeral port by asking the OS, so parallel test runs never collide.
static quint16 freePort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress::LocalHost, 0);
    const quint16 port = probe.localPort();
    probe.close();
    return port;
}

static ChannelConfig makeConfig(quint16 port, int timeoutMs = 0)
{
    ChannelConfig config;
    config.name      = QStringLiteral("telemetry");
    config.direction = QStringLiteral("in");
    config.transport = QStringLiteral("udp");
    config.host      = QStringLiteral("127.0.0.1");
    config.port      = port;
    config.codec     = QStringLiteral("json-datagram");
    config.timeoutMs = timeoutMs;
    config.valid     = true;
    return config;
}

void UdpProtocolChannelTest::_opensAndBindsPort_test()
{
    UdpProtocolChannel channel(makeConfig(freePort()));

    QVERIFY(channel.open());
    QVERIFY(channel.isOpen());

    channel.close();
    QVERIFY(!channel.isOpen());
}

void UdpProtocolChannelTest::_receivesDatagram_test()
{
    const quint16 port = freePort();
    UdpProtocolChannel channel(makeConfig(port));
    QVERIFY(channel.open());

    QSignalSpy spy(&channel, &ProtocolChannel::framesReceived);

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"seq": 1})"), QHostAddress::LocalHost, port);

    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toByteArray(), QByteArrayLiteral(R"({"seq": 1})"));
    QCOMPARE(channel.packetCount(), static_cast<quint64>(1));
}

void UdpProtocolChannelTest::_rejectsInvalidPort_test()
{
    ChannelConfig config = makeConfig(0);
    config.valid = false;

    UdpProtocolChannel channel(config);
    QVERIFY(!channel.open());
    QVERIFY(!channel.isOpen());
}

void UdpProtocolChannelTest::_closeStopsReceiving_test()
{
    const quint16 port = freePort();
    UdpProtocolChannel channel(makeConfig(port));
    QVERIFY(channel.open());
    channel.close();

    QSignalSpy spy(&channel, &ProtocolChannel::framesReceived);

    QUdpSocket sender;
    sender.writeDatagram(QByteArrayLiteral(R"({"seq": 2})"), QHostAddress::LocalHost, port);

    QVERIFY(!spy.wait(500));
    QCOMPARE(spy.count(), 0);
}

void UdpProtocolChannelTest::_emitsTimeoutWhenSilent_test()
{
    UdpProtocolChannel channel(makeConfig(freePort(), 300));
    QVERIFY(channel.open());

    QSignalSpy spy(&channel, &ProtocolChannel::timedOut);

    QVERIFY(spy.wait(3000));
    QVERIFY(spy.count() >= 1);
}
```

Test kayıtları: üç dosyaya `UdpProtocolChannelTest` girdilerini ekle. `test/Custom/CMakeLists.txt`'e ayrıca Network bağımlılığı gerekmez — QGC zaten `Qt6::Network` linkliyor.

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd build && ninja
```

Expected: FAIL — `UdpProtocolChannel.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`custom/src/Comms/ProtocolChannel.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief One transport endpoint declared by a ProtocolProfile.
 */

#pragma once

#include "ProtocolProfile.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>

/// Moves bytes to and from one endpoint. Knows nothing about their meaning:
/// framing is the codec's job, interpretation is the mapper's.
class ProtocolChannel : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolChannel(const ChannelConfig &config, QObject *parent = nullptr)
        : QObject(parent), _config(config) {}

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool send(const QByteArray &bytes) = 0;

    ChannelConfig config() const { return _config; }

signals:
    /// Raw bytes as they arrived. One emission per datagram for packet transports,
    /// one per readable chunk for stream transports.
    void framesReceived(const QByteArray &bytes);

    /// No data for `config().timeoutMs`. Only emitted when timeoutMs is greater than zero.
    void timedOut();

    void errorOccurred(const QString &message);

protected:
    ChannelConfig _config;
};
```

`custom/src/Comms/UdpProtocolChannel.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "ProtocolChannel.h"

#include <QtCore/QLoggingCategory>

class QTimer;
class QUdpSocket;

Q_DECLARE_LOGGING_CATEGORY(UdpProtocolChannelLog)

/// Binds a UDP port and emits each datagram as it arrives. For an "out" channel it
/// sends to the configured host instead.
class UdpProtocolChannel : public ProtocolChannel
{
    Q_OBJECT

public:
    explicit UdpProtocolChannel(const ChannelConfig &config, QObject *parent = nullptr);
    ~UdpProtocolChannel() override;

    bool open() final;
    void close() final;
    bool isOpen() const final;
    bool send(const QByteArray &bytes) final;

    quint64 packetCount() const { return _packetCount; }

private slots:
    void _readPendingDatagrams();
    void _checkTimeout();

private:
    QUdpSocket *_socket       = nullptr;
    QTimer     *_timeoutTimer = nullptr;
    qint64      _lastRxMs     = 0;
    quint64     _packetCount  = 0;
    bool        _timedOut     = false;
};
```

`custom/src/Comms/UdpProtocolChannel.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "UdpProtocolChannel.h"

#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtNetwork/QUdpSocket>

QGC_LOGGING_CATEGORY(UdpProtocolChannelLog, "custom.comms.udpprotocolchannel")

UdpProtocolChannel::UdpProtocolChannel(const ChannelConfig &config, QObject *parent)
    : ProtocolChannel(config, parent)
{
}

UdpProtocolChannel::~UdpProtocolChannel()
{
    close();
}

bool UdpProtocolChannel::open()
{
    if (!_config.valid || (_config.port == 0)) {
        qCWarning(UdpProtocolChannelLog) << "Refusing to open channel with invalid config:" << _config.name;
        emit errorOccurred(tr("Channel \"%1\" has an invalid endpoint").arg(_config.name));
        return false;
    }

    if (_socket) {
        return true;
    }

    _socket = new QUdpSocket(this);

    if (_config.direction == QLatin1String("in")) {
        const QHostAddress bindAddress = (_config.host == QLatin1String("0.0.0.0"))
                ? QHostAddress(QHostAddress::AnyIPv4)
                : QHostAddress(_config.host);

        if (!_socket->bind(bindAddress, _config.port, QUdpSocket::ShareAddress)) {
            const QString reason = _socket->errorString();
            qCWarning(UdpProtocolChannelLog) << "Bind failed on" << _config.port << reason;
            emit errorOccurred(tr("Could not bind UDP port %1: %2").arg(_config.port).arg(reason));
            delete _socket;
            _socket = nullptr;
            return false;
        }

        (void) connect(_socket, &QUdpSocket::readyRead, this, &UdpProtocolChannel::_readPendingDatagrams);
    }

    _lastRxMs = QDateTime::currentMSecsSinceEpoch();
    _timedOut = false;

    if (_config.timeoutMs > 0) {
        _timeoutTimer = new QTimer(this);
        // Check several times per timeout window so the signal is reasonably prompt.
        _timeoutTimer->setInterval(qMax(100, _config.timeoutMs / 3));
        (void) connect(_timeoutTimer, &QTimer::timeout, this, &UdpProtocolChannel::_checkTimeout);
        _timeoutTimer->start();
    }

    qCDebug(UdpProtocolChannelLog) << "Opened channel" << _config.name << _config.host << _config.port;
    return true;
}

void UdpProtocolChannel::close()
{
    if (_timeoutTimer) {
        _timeoutTimer->stop();
        delete _timeoutTimer;
        _timeoutTimer = nullptr;
    }

    if (_socket) {
        _socket->close();
        delete _socket;
        _socket = nullptr;
    }
}

bool UdpProtocolChannel::isOpen() const
{
    return (_socket != nullptr);
}

bool UdpProtocolChannel::send(const QByteArray &bytes)
{
    if (!_socket) {
        return false;
    }

    const qint64 written = _socket->writeDatagram(bytes, QHostAddress(_config.host), _config.port);
    return (written == bytes.size());
}

void UdpProtocolChannel::_readPendingDatagrams()
{
    while (_socket && _socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(_socket->pendingDatagramSize()));
        const qint64 read = _socket->readDatagram(datagram.data(), datagram.size());
        if (read < 0) {
            continue;
        }
        datagram.truncate(static_cast<int>(read));

        _packetCount++;
        _lastRxMs = QDateTime::currentMSecsSinceEpoch();
        _timedOut = false;

        emit framesReceived(datagram);
    }
}

void UdpProtocolChannel::_checkTimeout()
{
    if (_timedOut || (_config.timeoutMs <= 0)) {
        return;
    }

    if ((QDateTime::currentMSecsSinceEpoch() - _lastRxMs) > _config.timeoutMs) {
        _timedOut = true;
        qCDebug(UdpProtocolChannelLog) << "Channel timed out:" << _config.name;
        emit timedOut();
    }
}
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine `ProtocolChannel.h`, `UdpProtocolChannel.cc`, `UdpProtocolChannel.h` ekle.

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:UdpProtocolChannelTest
```

Expected: PASS — 5 test slotu geçer.

- [ ] **Step 5: Commit**

```bash
git add custom/src/Comms/ProtocolChannel.h custom/src/Comms/UdpProtocolChannel.h custom/src/Comms/UdpProtocolChannel.cc custom/CMakeLists.txt test/Custom/ test/CMakeLists.txt test/UnitTestList.cc
git commit -m "$(cat <<'EOF'
feat(custom): add ProtocolChannel interface and UDP implementation

Binds the profile's telemetry port, emits each datagram as it arrives and
reports a timeout when the far end goes quiet.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: ExternalProtocolLink — orkestratör ve QGC kaydı

Zinciri birleştirip QGC'nin link yığınına takar. Core dosyalara dokunan tek görev bu.

**Files:**
- Create: `custom/src/Comms/ExternalProtocolConfiguration.h`
- Create: `custom/src/Comms/ExternalProtocolConfiguration.cc`
- Create: `custom/src/Comms/ExternalProtocolLink.h`
- Create: `custom/src/Comms/ExternalProtocolLink.cc`
- Create: `custom/src/AppSettings/ExternalProtocolSettings.qml`
- Modify: `custom/custom.qrc` (`/Custom/qml` bloğuna)
- Modify: `src/Comms/LinkConfiguration.h` (`LinkType` enum, `TypeLogReplay` satırından hemen önce)
- Modify: `src/Comms/LinkManager.cc` (3 nokta: satır ~123 switch, ~338 switch, ~547 `linkTypeStrings`)
- Modify: `custom/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 `ProtocolProfile`, Task 2 `JsonDatagramCodec`, Task 3 `FieldMapper`, Task 4 `MAVLinkSynthesizer`, Task 5 `UdpProtocolChannel`
- Produces:
  - `class ExternalProtocolConfiguration : public LinkConfiguration` — `QString profilePath() const` / `void setProfilePath(const QString&)`
  - `class ExternalProtocolLink : public LinkInterface` — `QVariantMap passthroughData() const`, sinyal `void passthroughDataChanged(const QVariantMap&)`

- [ ] **Step 1: Write the implementation**

> Bu görevde TDD döngüsü yok: `LinkInterface` alt sınıfı `LinkManager`'ın canlı olmasını gerektiriyor, bu yüzden birim testi yerine Task 7'nin uçtan uca doğrulaması geçerli. Önceki beş görevin tamamı birim testli; buradaki iş yalnızca onları birbirine bağlamak.

`custom/src/Comms/ExternalProtocolConfiguration.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "LinkConfiguration.h"

/// Link configuration whose only setting is which protocol profile to load.
/// Everything else — ports, mappings, message set — comes from that file.
class ExternalProtocolConfiguration : public LinkConfiguration
{
    Q_OBJECT
    Q_PROPERTY(QString profilePath READ profilePath WRITE setProfilePath NOTIFY profilePathChanged)

public:
    explicit ExternalProtocolConfiguration(const QString &name, QObject *parent = nullptr);
    explicit ExternalProtocolConfiguration(const ExternalProtocolConfiguration *copy, QObject *parent = nullptr);

    LinkType type() const final { return LinkConfiguration::TypeExternalProtocol; }
    void copyFrom(const LinkConfiguration *source) final;
    void loadSettings(QSettings &settings, const QString &root) final;
    void saveSettings(QSettings &settings, const QString &root) const final;
    QString settingsURL() const final { return QStringLiteral("ExternalProtocolSettings.qml"); }
    QString settingsTitle() const final { return tr("External Protocol Settings"); }

    QString profilePath() const { return _profilePath; }
    void setProfilePath(const QString &path);

signals:
    void profilePathChanged();

private:
    QString _profilePath = QStringLiteral(":/Custom/protocols/celik-kubbe.json");
};
```

`custom/src/Comms/ExternalProtocolConfiguration.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ExternalProtocolConfiguration.h"

#include <QtCore/QSettings>

ExternalProtocolConfiguration::ExternalProtocolConfiguration(const QString &name, QObject *parent)
    : LinkConfiguration(name, parent)
{
}

ExternalProtocolConfiguration::ExternalProtocolConfiguration(const ExternalProtocolConfiguration *copy, QObject *parent)
    : LinkConfiguration(copy, parent)
    , _profilePath(copy->_profilePath)
{
}

void ExternalProtocolConfiguration::copyFrom(const LinkConfiguration *source)
{
    LinkConfiguration::copyFrom(source);

    const ExternalProtocolConfiguration *const typedSource =
            qobject_cast<const ExternalProtocolConfiguration*>(source);
    if (typedSource) {
        setProfilePath(typedSource->profilePath());
    }
}

void ExternalProtocolConfiguration::setProfilePath(const QString &path)
{
    if (_profilePath != path) {
        _profilePath = path;
        emit profilePathChanged();
    }
}

void ExternalProtocolConfiguration::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    setProfilePath(settings.value(QStringLiteral("profilePath"), _profilePath).toString());
    settings.endGroup();
}

void ExternalProtocolConfiguration::saveSettings(QSettings &settings, const QString &root) const
{
    settings.beginGroup(root);
    settings.setValue(QStringLiteral("profilePath"), _profilePath);
    settings.endGroup();
}
```

`custom/src/Comms/ExternalProtocolLink.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Presents a non-MAVLink device to QGC as an ordinary MAVLink vehicle.
 */

#pragma once

#include "ExternalProtocolConfiguration.h"
#include "FieldMapper.h"
#include "JsonDatagramCodec.h"
#include "LinkInterface.h"
#include "MAVLinkSynthesizer.h"
#include "ProtocolProfile.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariantMap>

class QTimer;
class UdpProtocolChannel;

Q_DECLARE_LOGGING_CATEGORY(ExternalProtocolLinkLog)

/// Reads the profile's telemetry channel, maps each packet and feeds the resulting
/// MAVLink messages into QGC as if they had arrived over the wire. Follows the
/// synthesizing-link pattern established by src/Comms/MockLink.
///
/// Stage A is receive-only: _writeBytes() drops outgoing MAVLink. Stage B adds the
/// command channel.
class ExternalProtocolLink : public LinkInterface
{
    Q_OBJECT

public:
    explicit ExternalProtocolLink(SharedLinkConfigurationPtr &config, QObject *parent = nullptr);
    ~ExternalProtocolLink() override;

    void disconnect() final;
    bool isConnected() const final;

    /// Profile fields not mapped to MAVLink, exposed for the turret QML panel.
    QVariantMap passthroughData() const { return _passthrough; }

signals:
    void passthroughDataChanged(const QVariantMap &data);

private slots:
    void _onFramesReceived(const QByteArray &bytes);
    void _onChannelTimedOut();
    void _sendHeartbeat();

private:
    bool _connect() final;
    void _writeBytes(const QByteArray &bytes) final;

    bool _loadProfile();
    void _respondWithMavlinkMessage(const mavlink_message_t &message);

    ProtocolProfile                     _profile;
    QScopedPointer<FieldMapper>         _mapper;
    QScopedPointer<MAVLinkSynthesizer>  _synthesizer;
    JsonDatagramCodec                   _codec;
    UdpProtocolChannel                 *_telemetryChannel = nullptr;
    QTimer                             *_heartbeatTimer   = nullptr;

    QVariantMap _passthrough;
    int         _customMode  = 0;
    bool        _connected   = false;
    quint64     _packetCount = 0;
};
```

`custom/src/Comms/ExternalProtocolLink.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ExternalProtocolLink.h"
#include "QGCLoggingCategory.h"
#include "UdpProtocolChannel.h"

#include <QtCore/QFile>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>

QGC_LOGGING_CATEGORY(ExternalProtocolLinkLog, "custom.comms.externalprotocollink")

ExternalProtocolLink::ExternalProtocolLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
{
}

ExternalProtocolLink::~ExternalProtocolLink()
{
    ExternalProtocolLink::disconnect();
}

bool ExternalProtocolLink::_loadProfile()
{
    // linkConfiguration() returns std::shared_ptr<LinkConfiguration>; qobject_cast needs
    // the raw pointer.
    const ExternalProtocolConfiguration *const config =
            qobject_cast<const ExternalProtocolConfiguration*>(linkConfiguration().get());
    if (!config) {
        qCWarning(ExternalProtocolLinkLog) << "Link created without an ExternalProtocolConfiguration";
        return false;
    }

    QFile file(config->profilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(ExternalProtocolLinkLog) << "Cannot open profile" << config->profilePath() << file.errorString();
        return false;
    }

    QString error;
    if (!_profile.loadFromJson(file.readAll(), &error)) {
        qCWarning(ExternalProtocolLinkLog) << "Invalid profile" << config->profilePath() << error;
        return false;
    }

    qCDebug(ExternalProtocolLinkLog) << "Loaded profile" << _profile.name();
    return true;
}

bool ExternalProtocolLink::_connect()
{
    if (!_loadProfile()) {
        // A missing or broken profile disables the feature; QGC carries on normally.
        return false;
    }

    if (!_allocateMavlinkChannel()) {
        qCWarning(ExternalProtocolLinkLog) << "No free MAVLink channel";
        return false;
    }

    _mapper.reset(new FieldMapper(_profile));
    _synthesizer.reset(new MAVLinkSynthesizer(_profile, mavlinkChannel()));

    const ChannelConfig telemetryConfig = _profile.channel(_profile.telemetrySourceChannel());
    _telemetryChannel = new UdpProtocolChannel(telemetryConfig, this);

    (void) connect(_telemetryChannel, &ProtocolChannel::framesReceived,
                   this, &ExternalProtocolLink::_onFramesReceived);
    (void) connect(_telemetryChannel, &ProtocolChannel::timedOut,
                   this, &ExternalProtocolLink::_onChannelTimedOut);

    if (!_telemetryChannel->open()) {
        delete _telemetryChannel;
        _telemetryChannel = nullptr;
        _freeMavlinkChannel();
        return false;
    }

    // The heartbeat is independent of incoming telemetry: QGC's comm-lost detection
    // relies on it stopping when the far end goes quiet, not on packet arrival.
    _heartbeatTimer = new QTimer(this);
    _heartbeatTimer->setInterval(1000 / _profile.heartbeatHz());
    (void) connect(_heartbeatTimer, &QTimer::timeout, this, &ExternalProtocolLink::_sendHeartbeat);
    _heartbeatTimer->start();

    _connected = true;
    emit connected();
    return true;
}

void ExternalProtocolLink::disconnect()
{
    if (_heartbeatTimer) {
        _heartbeatTimer->stop();
        delete _heartbeatTimer;
        _heartbeatTimer = nullptr;
    }

    if (_telemetryChannel) {
        _telemetryChannel->close();
        delete _telemetryChannel;
        _telemetryChannel = nullptr;
    }

    if (_connected) {
        _connected = false;
        _freeMavlinkChannel();
        emit disconnected();
    }
}

bool ExternalProtocolLink::isConnected() const
{
    return _connected;
}

void ExternalProtocolLink::_writeBytes(const QByteArray &bytes)
{
    // Stage A is receive-only. Stage B parses these frames and translates them into
    // the profile's command templates.
    Q_UNUSED(bytes);
}

void ExternalProtocolLink::_respondWithMavlinkMessage(const mavlink_message_t &message)
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN]{};
    const int length = mavlink_msg_to_send_buffer(buffer, &message);
    emit bytesReceived(this, QByteArray(reinterpret_cast<char*>(buffer), length));
}

void ExternalProtocolLink::_sendHeartbeat()
{
    if (_synthesizer) {
        _respondWithMavlinkMessage(_synthesizer->buildHeartbeat(_customMode));
    }
}

void ExternalProtocolLink::_onFramesReceived(const QByteArray &bytes)
{
    const QList<QJsonObject> packets = _codec.decode(bytes);
    for (const QJsonObject &packet : packets) {
        _packetCount++;

        bool modeFound = false;
        const int mode = _mapper->flightModeValue(packet, &modeFound);
        if (modeFound) {
            _customMode = mode;
        }

        const QList<mavlink_message_t> messages =
                _synthesizer->buildTelemetryMessages(_mapper->mapTelemetry(packet));
        for (const mavlink_message_t &message : messages) {
            _respondWithMavlinkMessage(message);
        }

        const QVariantMap through = _mapper->passthrough(packet);
        if (through != _passthrough) {
            _passthrough = through;
            emit passthroughDataChanged(_passthrough);
        }
    }
}

void ExternalProtocolLink::_onChannelTimedOut()
{
    // Stop the heartbeat so QGC's own comm-lost logic takes over rather than
    // inventing a second notion of "disconnected" here.
    if (_heartbeatTimer) {
        _heartbeatTimer->stop();
    }
    qCDebug(ExternalProtocolLinkLog) << "Telemetry timed out, heartbeat stopped";
}
```

`src/Comms/LinkConfiguration.h` — `LinkType` enum'una, `TypeLogReplay` satırından **hemen önce** ekle:

```cpp
#ifdef QGC_CUSTOM_BUILD
        TypeExternalProtocol,   ///< Profile-driven non-MAVLink protocol bridge
#endif
```

`src/Comms/LinkManager.cc` — `createConnectedLink` switch'ine (satır ~151, `TypeMock` bloğundan sonra):

```cpp
#ifdef QGC_CUSTOM_BUILD
    case LinkConfiguration::TypeExternalProtocol:
        link = std::make_shared<ExternalProtocolLink>(config);
        break;
#endif
```

`src/Comms/LinkManager.cc` — `createConfiguration` switch'ine (satır ~344 civarı, aynı sırada):

```cpp
#ifdef QGC_CUSTOM_BUILD
            case LinkConfiguration::TypeExternalProtocol:
                link = new ExternalProtocolConfiguration(name);
                break;
#endif
```

`src/Comms/LinkManager.cc` — `linkTypeStrings()` içine, **enum ile aynı sırada** (`Mock Link` satırından sonra, `AirLink` satırından önce):

```cpp
#ifdef QGC_CUSTOM_BUILD
    list += tr("External Protocol");
#endif
```

`src/Comms/LinkManager.cc` — include bloğuna:

```cpp
#ifdef QGC_CUSTOM_BUILD
#include "ExternalProtocolLink.h"
#endif
```

`custom/src/AppSettings/ExternalProtocolSettings.qml` — `settingsURL()` bu dosyayı adlandırıyor; olmadan Comm Links düzenleme paneli boş yüklenir:

```qml
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl.Controls
import QGroundControl.ScreenTools

ColumnLayout {
    spacing: ScreenTools.defaultFontPixelHeight / 2

    property var subEditConfig: null

    QGCLabel {
        Layout.fillWidth:   true
        wrapMode:           Text.WordWrap
        text:               qsTr("Ports, field mappings and commands all come from the profile file. Point this at a different profile to talk to different hardware.")
    }

    LabelledTextField {
        Layout.fillWidth:   true
        label:              qsTr("Profile file")
        textFieldPreferredWidth: ScreenTools.defaultFontPixelWidth * 30
        text:               subEditConfig ? subEditConfig.profilePath : ""

        onEditingFinished: {
            if (subEditConfig) {
                subEditConfig.profilePath = text
            }
        }
    }
}
```

`custom/custom.qrc` — `/Custom/qml` bloğuna ekle:

```xml
        <file alias="QGroundControl/AppSettings/ExternalProtocolSettings.qml">src/AppSettings/ExternalProtocolSettings.qml</file>
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine dört yeni C++ dosyasını ekle.

- [ ] **Step 2: Verify it builds and the existing tests still pass**

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:ProtocolProfileTest && ./Debug/Custom-QGroundControl --unittest:FieldMapperTest && ./Debug/Custom-QGroundControl --unittest:MAVLinkSynthesizerTest && ./Debug/Custom-QGroundControl --unittest:UdpProtocolChannelTest && ./Debug/Custom-QGroundControl --unittest:JsonDatagramCodecTest
```

Expected: Derleme başarılı, beş testin tamamı PASS.

- [ ] **Step 3: Verify the stock build is untouched**

```bash
grep -c "QGC_CUSTOM_BUILD" src/Comms/LinkManager.cc src/Comms/LinkConfiguration.h
```

Expected: `LinkManager.cc` en az 4, `LinkConfiguration.h` en az 1 — yani her core değişikliği koruma altında.

- [ ] **Step 4: Commit**

```bash
git add custom/src/Comms/ExternalProtocolConfiguration.h custom/src/Comms/ExternalProtocolConfiguration.cc custom/src/Comms/ExternalProtocolLink.h custom/src/Comms/ExternalProtocolLink.cc custom/CMakeLists.txt src/Comms/LinkConfiguration.h src/Comms/LinkManager.cc
git commit -m "$(cat <<'EOF'
feat(custom): add ExternalProtocolLink bridging JSON telemetry to MAVLink

Wires channel, codec, mapper and synthesizer into a LinkInterface that
presents a non-MAVLink device to QGC as an ordinary vehicle, following
the MockLink synthesizing-link pattern. Receive-only for now.

Core LinkConfiguration/LinkManager changes are guarded by
QGC_CUSTOM_BUILD so the stock build is unaffected.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Çelik Kubbe profili ve uçtan uca doğrulama

Gerçek profili kaynak olarak gömer ve `telemetry_sim.py` ile canlı doğrulama yapar.

**Files:**
- Create: `custom/res/protocols/celik-kubbe.json`
- Modify: `custom/custom.qrc`
- Modify: `custom/CMakeLists.txt` (gerekiyorsa; `custom.qrc` zaten `CUSTOM_RESOURCES` içinde)

**Interfaces:**
- Consumes: Task 6'dan `ExternalProtocolConfiguration::profilePath()` varsayılanı `:/Custom/protocols/celik-kubbe.json`
- Produces: çalışan uçtan uca köprü

- [ ] **Step 1: Write the profile**

`custom/res/protocols/celik-kubbe.json`:

```json
{
  "name": "Çelik Kubbe Taret",
  "schemaVersion": 1,
  "vehicle": {
    "mavType": "MAV_TYPE_GENERIC",
    "sysId": 1,
    "compId": 1,
    "heartbeatHz": 1
  },
  "channels": {
    "telemetry": {
      "dir": "in",
      "transport": "udp",
      "bind": "0.0.0.0:5001",
      "codec": "json-datagram",
      "timeoutMs": 3000
    }
  },
  "telemetry": {
    "source": "telemetry",
    "mavlink": {
      "GLOBAL_POSITION_INT.lat":          { "from": "gps.lat",  "scale": 10000000 },
      "GLOBAL_POSITION_INT.lon":          { "from": "gps.lon",  "scale": 10000000 },
      "GLOBAL_POSITION_INT.relative_alt": { "from": "altitude", "scale": 1000 },
      "GLOBAL_POSITION_INT.hdg":          { "from": "heading",  "scale": 100 },
      "ATTITUDE.roll":                    { "from": "roll",  "unit": "deg2rad" },
      "ATTITUDE.pitch":                   { "from": "pitch", "unit": "deg2rad" },
      "ATTITUDE.yaw":                     { "from": "yaw",   "unit": "deg2rad" },
      "SYS_STATUS.battery_remaining":     { "from": "battery" },
      "GPS_RAW_INT.satellites_visible":   { "from": "gps.satellites" },
      "GPS_RAW_INT.fix_type":             { "from": "gps.fix", "map": { "true": 3, "false": 0 } },
      "VFR_HUD.groundspeed":              { "from": "speed", "default": 0 }
    },
    "flightMode": {
      "from": "mode",
      "values": { "ARAMA": 0, "TAKIP": 1, "KILITLI": 2 }
    },
    "passthrough": [
      "tracking", "fps", "frame", "locked", "fire", "shots",
      "detections", "turret", "target", "seq", "source", "rssi"
    ]
  }
}
```

`custom/custom.qrc` — mevcut prefix'ler `/custom/img`, `/Custom/qmlimages`, `/Custom/qml`, `/json`, `/Custom/res`. Hiçbiri uymuyor, bu yüzden **yeni bir blok** ekle (son `</qresource>` ile `</RCC>` arasına):

```xml
    <qresource prefix="/Custom/protocols">
        <file alias="celik-kubbe.json">res/protocols/celik-kubbe.json</file>
    </qresource>
```

Bu, kaynağı tam olarak `:/Custom/protocols/celik-kubbe.json` yoluna yerleştirir — `ExternalProtocolConfiguration`'ın varsayılanı bu.

- [ ] **Step 2: Build and verify the profile is embedded**

```bash
cd build && ninja
```

Expected: Derleme başarılı.

- [ ] **Step 3: Verify the profile loads and parses**

Profilin gerçekten kaynağa gömüldüğünü ve `ProtocolProfile` tarafından kabul edildiğini doğrulayan bir test ekleyin.

`test/Custom/ProtocolProfileTest.h` — `private slots:` bloğuna ekle:

```cpp
    void _shippedCelikKubbeProfileIsValid_test();
```

`test/Custom/ProtocolProfileTest.cc` — ekle (dosyanın başına `#include <QtCore/QFile>` gerekir):

```cpp
void ProtocolProfileTest::_shippedCelikKubbeProfileIsValid_test()
{
    QFile file(QStringLiteral(":/Custom/protocols/celik-kubbe.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), "Shipped profile is missing from the Qt resources");

    ProtocolProfile profile;
    QString error;
    QVERIFY2(profile.loadFromJson(file.readAll(), &error), qPrintable(error));

    QCOMPARE(profile.channelNames().size(), 1);
    QCOMPARE(profile.channel(QStringLiteral("telemetry")).port, static_cast<quint16>(5001));
    QCOMPARE(profile.mavlinkRules().size(), 11);
    QCOMPARE(profile.flightModeValues().size(), 3);
    QCOMPARE(profile.passthroughFields().size(), 12);
}
```

```bash
cd build && ninja && ./Debug/Custom-QGroundControl --unittest:ProtocolProfileTest
```

Expected: PASS — 9 test slotu geçer.

- [ ] **Step 4: End-to-end verification against the simulator**

İki terminal gerekiyor.

Terminal 1 — simülatörü başlat:

```bash
python3 /home/tom/Documents/Projeler/Celik_Kubbe/YKI/telemetry_sim.py --mode fake
```

Terminal 2 — QGC'yi başlat:

```bash
cd /home/tom/Documents/Projeler/qgroundcontrol/build && ./Debug/Custom-QGroundControl
```

QGC içinde: **Application Settings → Comm Links → Add**, tip olarak **External Protocol** seç, kaydet ve **Connect**.

Doğrulanacaklar:
- Araç bağlanıyor ve araç göstergesi haritada Ankara koordinatlarında (39.9255, 32.8663) beliriyor
- Batarya göstergesi simülatörün düşen batarya değerini izliyor
- HUD'daki yaw/pitch taret açılarıyla değişiyor
- Uçuş modunun **sayısal** `custom_mode` değeri değişiyor — **Analyze → MAVLink Inspector →
  HEARTBEAT** altından bakın; `ARAMA`=0, `TAKIP`=1, `KILITLI`=2. Toolbar'da bu sayının
  **isim** olarak görünmesi Task 8'in işi; bu adımda henüz beklenmiyor.
- Simülatörü durdurunca (Ctrl-C) QGC ~3 saniye sonra bağlantı kaybı gösteriyor
- Paket 01'deki **Disconnect** butonu bu link'i düzgün kesiyor

- [ ] **Step 5: Commit**

```bash
git add custom/res/protocols/celik-kubbe.json custom/custom.qrc test/Custom/ProtocolProfileTest.h test/Custom/ProtocolProfileTest.cc
git commit -m "$(cat <<'EOF'
feat(custom): ship the Çelik Kubbe turret protocol profile

Maps the turret's UDP JSON telemetry onto position, attitude, status,
GPS and HUD messages, with the tracker states as flight modes and the
turret-specific fields passed through to QML.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Uçuş modu isimleri

Spec'in 2. kararı: taretin `ARAMA`/`TAKIP`/`KILITLI` durumları QGC'nin mod göstergesinde
**isim** olarak görünmeli. Task 4 bunları `custom_mode` sayısına çeviriyor; sayıyı isme
çeviren yer `FirmwarePlugin`.

**Files:**
- Create: `custom/src/FirmwarePlugin/TurretFirmwarePlugin.h`
- Create: `custom/src/FirmwarePlugin/TurretFirmwarePlugin.cc`
- Modify: `custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.h`
- Modify: `custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc`
- Modify: `custom/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 4'ün ürettiği `HEARTBEAT.custom_mode` değerleri (0/1/2), Task 7'nin
  profilindeki `telemetry.flightMode.values` haritası
- Produces: `class TurretFirmwarePlugin : public FirmwarePlugin` — `void updateAvailableFlightModes(FlightModeList&) final`

**Neden yeni bir plugin, mevcut `CustomFirmwarePlugin` değil**

`CustomFirmwarePluginFactory::firmwarePluginForAutopilot`
(`custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc`) yalnızca
`MAV_AUTOPILOT_PX4` için plugin döndürüyor; `supportedFirmwareClasses()` da sadece
`FirmwareClassPX4` bildiriyor. Taret ise `MAV_AUTOPILOT_GENERIC` gönderiyor (Task 4,
`buildHeartbeat`). Yani şu an taret için jenerik `FirmwarePlugin` seçiliyor ve
`custom_mode` isme çevrilmiyor.

`CustomFirmwarePlugin`'i GENERIC'i de kapsayacak şekilde genişletmek yanlış olur:
o sınıf PX4 mod numaralarını (`PX4CustomMode::Mode`) yorumluyor, taretin 0/1/2
değerleri orada bambaşka modlara denk düşer. Ayrı bir sınıf doğru ayrım.

**Kabul edilen ödünleşim:** Fabrika `MAV_AUTOPILOT_GENERIC` bildiren **her** aracı
`TurretFirmwarePlugin`'e yönlendirecek. Bu özel yapı yalnızca yarışma dronu (PX4) ve
taretle konuştuğu için kabul edilebilir. İleride GENERIC bildiren başka bir cihaz
eklenirse ayrım `MAV_TYPE` üzerinden yapılmalı.

- [ ] **Step 1: Write the failing verification**

Bu görevin doğrulaması uçtan uca; birim testi yok çünkü `FirmwarePlugin` seçimi
`Vehicle` ve `FirmwarePluginManager`'ın canlı olmasını gerektiriyor.

Önce mevcut davranışı kaydedin — simülatör ve QGC çalışırken toolbar'daki mod
göstergesine bakın:

```bash
python3 /home/tom/Documents/Projeler/Celik_Kubbe/YKI/telemetry_sim.py --mode fake
```

Expected (düzeltmeden önce): mod göstergesi boş, `Unknown` ya da ham sayı gösteriyor —
`ARAMA`/`TAKIP`/`KILITLI` **görünmüyor**. Gördüğünüzü not edin.

- [ ] **Step 2: Write the implementation**

`custom/src/FirmwarePlugin/TurretFirmwarePlugin.h`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Flight mode names for the profile-driven turret bridge.
 */

#pragma once

#include "FirmwarePlugin.h"

/// Names the turret's tracker states so QGC's mode indicator and mode selector show
/// ARAMA / TAKIP / KILITLI instead of a bare custom_mode number.
///
/// The numbers must match telemetry.flightMode.values in the shipped profile
/// (custom/res/protocols/celik-kubbe.json).
class TurretFirmwarePlugin : public FirmwarePlugin
{
    Q_OBJECT

public:
    explicit TurretFirmwarePlugin(QObject *parent = nullptr);

    void updateAvailableFlightModes(FlightModeList &flightModeList) final;
};
```

`custom/src/FirmwarePlugin/TurretFirmwarePlugin.cc`:

```cpp
/****************************************************************************
 *
 * (c) 2026
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "TurretFirmwarePlugin.h"

TurretFirmwarePlugin::TurretFirmwarePlugin(QObject *parent)
    : FirmwarePlugin(parent)
{
}

void TurretFirmwarePlugin::updateAvailableFlightModes(FlightModeList &flightModeList)
{
    flightModeList.clear();

    // Keep these in step with telemetry.flightMode.values in celik-kubbe.json.
    // canBeSet is false: Stage A is receive-only, so the operator cannot command a
    // mode change yet. Stage B flips these on once the command channel exists.
    static const struct {
        const char *name;
        uint32_t    customMode;
    } turretModes[] = {
        { "ARAMA",   0 },
        { "TAKIP",   1 },
        { "KILITLI", 2 },
    };

    for (const auto &turretMode : turretModes) {
        // FirmwareFlightMode(name, custom_mode, canBeSet = false, advanced = false)
        // — see src/FirmwarePlugin/FirmwarePlugin.h:32
        flightModeList.append(FirmwareFlightMode(tr(turretMode.name), turretMode.customMode));
    }
}
```

`custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.h` — sınıfa ekle:

```cpp
    FirmwarePlugin *_turretPluginInstance = nullptr;
```

`custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc` — include ekle ve iki metodu güncelle:

```cpp
#include "TurretFirmwarePlugin.h"
```

```cpp
QList<QGCMAVLink::FirmwareClass_t> CustomFirmwarePluginFactory::supportedFirmwareClasses() const
{
    QList<QGCMAVLink::FirmwareClass_t> firmwareClasses;
    firmwareClasses.append(QGCMAVLink::FirmwareClassPX4);
    firmwareClasses.append(QGCMAVLink::FirmwareClassGeneric);
    return firmwareClasses;
}
```

```cpp
FirmwarePlugin* CustomFirmwarePluginFactory::firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE /*vehicleType*/)
{
    if (autopilotType == MAV_AUTOPILOT_PX4) {
        if (!_pluginInstance) {
            _pluginInstance = new CustomFirmwarePlugin;
        }
        return _pluginInstance;
    }

    // The turret bridge reports MAV_AUTOPILOT_GENERIC. This custom build only ever
    // talks to the competition drone (PX4) and the turret, so claiming GENERIC here
    // is safe; revisit via MAV_TYPE if another generic device is ever added.
    if (autopilotType == MAV_AUTOPILOT_GENERIC) {
        if (!_turretPluginInstance) {
            _turretPluginInstance = new TurretFirmwarePlugin;
        }
        return _turretPluginInstance;
    }

    return nullptr;
}
```

`custom/CMakeLists.txt` — `CUSTOM_SOURCES` listesine `TurretFirmwarePlugin.cc` ve `TurretFirmwarePlugin.h` ekle.

- [ ] **Step 3: Build and verify the mode names appear**

```bash
cd build && ninja
```

Simülatör çalışırken QGC'yi başlatıp bağlanın.

Expected: Toolbar'daki mod göstergesi artık `ARAMA`, `TAKIP` ve `KILITLI` yazıyor ve
simülatör durum değiştirdikçe güncelleniyor. Step 1'de not ettiğiniz davranış değişmiş olmalı.

- [ ] **Step 4: Verify the PX4 vehicle path still works**

Taret için GENERIC'i sahiplenmek yarışma dronunu bozmamalı.

```bash
cd build && ./Debug/Custom-QGroundControl --unittest:FactSystemTestPX4
```

Expected: PASS. Ayrıca mümkünse gerçek PX4 aracıyla bağlanıp mod listesinin eskisi gibi
geldiğini doğrulayın.

- [ ] **Step 5: Commit**

```bash
git add custom/src/FirmwarePlugin/TurretFirmwarePlugin.h custom/src/FirmwarePlugin/TurretFirmwarePlugin.cc custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.h custom/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc custom/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(custom): name the turret tracker states as flight modes

QGC selected the generic FirmwarePlugin for the bridge, so the turret's
custom_mode showed as a bare number. TurretFirmwarePlugin names 0/1/2 as
ARAMA/TAKIP/KILITLI, matching the shipped profile. Modes are read-only
until the Stage B command channel lands.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Aşama A tamamlandığında

Çalışır durumda olanlar: taret QGC'de normal bir araç; harita konumu, HUD, batarya, GPS, isimlendirilmiş uçuş modu, tlog kaydı ve disconnect butonu. `telemetry_sim.py` ile donanımsız test edilebilir.

Sonraki planlar:
- **Aşama B** — `TcpProtocolChannel`, `NdjsonCodec`, `CommandTranslator`, ACK işleme; arm/disarm ve mod değiştirme
- **Aşama C** — `CsvCodec`, joystick hız sınırlama ve failsafe

`docs/superpowers/todo/03-protokol-koprusu.md` dosyasındaki maddeleri bitirdikçe işaretleyin.
