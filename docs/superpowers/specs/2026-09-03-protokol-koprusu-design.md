# Genel Protokol Köprüsü — Tasarım

**Tarih:** 2026-09-03 · **Paket:** 03 · **Durum:** İnceleme bekliyor

## Amaç

QGroundControl'ü MAVLink konuşmayan yarışma donanımlarıyla da çalışır hale
getirmek. İlk hedef TEKNOFEST Çelik Kubbe taret sistemi. Tasarım kısıtı:
**yeni yarışma = yeni JSON profil dosyası, C++ derlemesi yok.**

İkincil sonuç: `Celik_Kubbe/YKI/` web yer kontrol istasyonu gereksiz hale gelir.

Kapsam kararı (2026-09-03): **tam kontrol** — telemetri, komut, joystick ve
video. Bunlardan **video için yeni kod yazılmayacak**: QGC'nin mevcut
RTSP/UDP/MJPEG video altyapısı taretin yayınını olduğu gibi oynatıyor, sadece
Ayarlar'dan URL girilmesi yeterli. Yani bu tasarımın kod ürettiği üç kanal var:
telemetri (in), komut (out), joystick (out).

## Hedef protokol

Kaynak: `/home/tom/Documents/Projeler/Celik_Kubbe/YKI/`

| Yön | Taşıma | Çerçeveleme | Referans |
|---|---|---|---|
| Telemetri (in) | UDP :5001 | datagram başına bir JSON nesnesi | `backend/telemetryBridge.js` |
| Komut (out) | TCP :5000 | satır-sonlu JSON, `{ack,status}` ile onaylanır | `backend/commandBridge.js` |
| Joystick (out) | UDP :5005 → ESP32 | CSV satırı `"pitch,yaw,fire,arm\n"`, ≤50 Hz | `backend/joystickBridge.js` |
| Video | RTSP / V4L2 / MJPEG | — | `backend/videoRelay.js` |

### Telemetri paketinin gerçek şekli

`telemetry_sim.py` çıktısından doğrulandı:

```jsonc
{
  "seq": 1234, "timestamp": 1700000000000, "source": "sim",
  "battery": 85.5,        // %
  "altitude": 150.3,      // m, hedefin kestirilen irtifası
  "speed": 12.1,          // °/s, taret dönüş hızı
  "rssi": -65,            // dBm
  "pitch": 12.0, "roll": 0.0, "yaw": 47.5, "heading": 47.5,   // derece
  "mode": "TAKIP",        // ARAMA | TAKIP | KILITLI
  "tracking": true,
  "gps": { "lat": 39.925533, "lon": 32.866287, "fix": true, "satellites": 12 },
  "fps": 28.4, "frame": 5120, "locked": false,
  "fire": false, "shots": 3, "detections": 2,
  "turret": { "yaw": .., "pitch": .., "yaw_rate": .., "pitch_rate": ..,
              "yaw_cmd": .., "pitch_cmd": .., "state": "TAKIP" },
  "target": null | { "label": "..", "faction": "..", "track_id": 7,
                     "range_m": 120.4, "in_range_window": true,
                     "yaw_error": .., "pitch_error": .., "cx": .., "cy": ..,
                     "score": 0.87 }
}
```

`target` **null olabilir** — eşleyici bunu tolere etmeli.
Açılar **derece**, MAVLink `ATTITUDE` **radyan** istiyor.

## Mimari

```
                         custom/src/Comms/
Taret ──UDP:5001──►  UdpChannel ─► JsonDatagramCodec ─┐
                                                      ├─► FieldMapper ─┬─► MAVLinkSynthesizer ─► QGC Vehicle
                     ProtocolProfile (JSON) ──────────┘                │   harita · HUD · tlog · disconnect
                                                                       └─► QVariantMap ─► QML taret paneli
                                                                           target · detections · shots
QGC arm/mod/guided ─► CommandTranslator ─► NdjsonCodec ─► TcpChannel ──TCP:5000──► Taret
QGC Joystick ──────► CsvCodec @50 Hz ────────────────────► UdpChannel ──UDP:5005──► ESP32
```

### Neden sanal MAVLink aracı

`src/Comms/MockLink/` repo içinde kanıtlanmış desen: `LinkInterface` alt sınıfı,
gerçek trafik taşımak yerine MAVLink **sentezliyor**:

```cpp
mavlink_msg_heartbeat_pack_chan(sysId, compId, mavlinkChannel(), &msg, ...);
// -> mavlink_msg_to_send_buffer -> emit bytesReceived(this, bytes);
```

Giden yön `_writeBytes(const QByteArray&)` override'ı ile yakalanıyor.

Taret bu yolla QGC'ye normal bir araç gibi görünür; harita konumu, HUD, uçuş
modu göstergesi, tlog kaydı, ve Paket 01'deki disconnect butonu **hiç ek kod
yazmadan** çalışır. Alternatif (bağımsız panel) bunların hepsini elde yazmayı
gerektirirdi.

### Bileşenler

| Sınıf | Sorumluluk | Bağımlılık | Test edilebilirlik |
|---|---|---|---|
| `ProtocolProfile` | JSON profili yükle + doğrula, erişimci sun | yok | saf veri — doğrudan test |
| `ProtocolChannel` (soyut) | Bağlan/kop, bayt gönder/al, yeniden bağlan | Qt network | sahte soketle test |
| ├ `UdpProtocolChannel` | UDP bind / send | `QUdpSocket` | |
| └ `TcpProtocolChannel` | TCP connect / send | `QTcpSocket` | |
| `FrameCodec` (soyut) | Bayt ↔ `QJsonObject` | yok | **saf fonksiyon** |
| ├ `JsonDatagramCodec` | Bir datagram = bir JSON | | |
| ├ `NdjsonCodec` | `\n` ile ayrılmış JSON | | |
| └ `CsvCodec` | Şablonlu CSV satırı | | |
| `FieldMapper` | Profil kurallarıyla eşle, ölçekle, birim çevir | `ProtocolProfile` | **saf fonksiyon** |
| `MAVLinkSynthesizer` | Eşlenen alanlardan MAVLink mesajı üret | `FieldMapper` | üretilen mesaj denetlenebilir |
| `CommandTranslator` | Giden MAVLink → profil komut şablonu | `ProtocolProfile` | **saf fonksiyon** |
| `ExternalProtocolLink` | Orkestratör, `LinkInterface` alt sınıfı | yukarıdakiler | entegrasyon |
| `ExternalProtocolSettings` | Fact tabanlı ayarlar + QML sayfası | `SettingsManager` | — |

`FrameCodec`, `FieldMapper` ve `CommandTranslator` saf dönüşüm; donanım, soket
veya QGC çalışır durumda olmadan birim testi yazılabilir. Ağır iş bu üçünde
olduğu için test kapsamı yüksek tutulabilir.

## Profil dosyası şeması

```json
{
  "name": "Çelik Kubbe Taret",
  "schemaVersion": 1,
  "vehicle": { "mavType": "MAV_TYPE_GENERIC", "sysId": 1, "compId": 1,
               "heartbeatHz": 1 },

  "channels": {
    "telemetry": { "dir": "in",  "transport": "udp",
                   "bind": "0.0.0.0:5001", "codec": "json-datagram",
                   "timeoutMs": 3000 },
    "command":   { "dir": "out", "transport": "tcp",
                   "connect": "192.168.1.100:5000", "codec": "ndjson",
                   "ack": { "field": "ack", "matches": "id" },
                   "reconnectMs": 5000 },
    "joystick":  { "dir": "out", "transport": "udp",
                   "connect": "celikkubbe.local:5005", "codec": "csv",
                   "rateHz": 50,
                   "template": "{pitch:.3f},{yaw:.3f},{fire:.0f},{arm:.0f}",
                   "failsafe": "0.000,0.000,0,0" }
  },

  "telemetry": {
    "source": "telemetry",
    "mavlink": {
      "GLOBAL_POSITION_INT.lat":          { "from": "gps.lat",  "scale": 1e7 },
      "GLOBAL_POSITION_INT.lon":          { "from": "gps.lon",  "scale": 1e7 },
      "GLOBAL_POSITION_INT.relative_alt": { "from": "altitude", "scale": 1000 },
      "GLOBAL_POSITION_INT.hdg":          { "from": "heading",  "scale": 100 },
      "ATTITUDE.roll":                    { "from": "roll",  "unit": "deg2rad" },
      "ATTITUDE.pitch":                   { "from": "pitch", "unit": "deg2rad" },
      "ATTITUDE.yaw":                     { "from": "yaw",   "unit": "deg2rad" },
      "SYS_STATUS.battery_remaining":     { "from": "battery" },
      "GPS_RAW_INT.satellites_visible":   { "from": "gps.satellites" },
      "GPS_RAW_INT.fix_type":             { "from": "gps.fix", "map": { "true": 3, "false": 0 } },
      "VFR_HUD.groundspeed":              { "from": "speed" },
      "RADIO_STATUS.rssi":                { "from": "rssi" }
    },
    "flightMode": { "from": "mode",
                    "values": { "ARAMA": 0, "TAKIP": 1, "KILITLI": 2 } },
    "passthrough": ["tracking", "fps", "frame", "locked", "fire", "shots",
                    "detections", "turret", "target", "seq", "source"]
  },

  "commands": {
    "arm":     { "channel": "command", "template": { "cmd": "arm",      "params": {} } },
    "disarm":  { "channel": "command", "template": { "cmd": "disarm",   "params": {} } },
    "setMode": { "channel": "command", "template": { "cmd": "set_mode", "params": { "mode": "{value}" } } }
  }
}
```

Profil `custom/res/protocols/celik-kubbe.json` altında kaynak olarak gömülür;
kullanıcı Ayarlar'dan kendi dosyasını da seçebilir.

### Eşleme kuralı alanları

Bir eşleme kuralı şu anahtarları kabul eder; **`scale`, `unit` ve `map`
birbirini dışlar**, bir kuralda en fazla biri bulunur:

| Anahtar | Anlamı |
|---|---|
| `from` | Kaynak JSON yolu, nokta ile ayrılmış (`gps.lat`, `turret.yaw_rate`). **Zorunlu.** |
| `scale` | Sayısal çarpan. `değer * scale`, sonra hedef alanın tamsayı tipine yuvarlanır. |
| `unit` | Adlandırılmış dönüşüm. Desteklenen: `deg2rad`, `rad2deg`, `m2cm`, `cm2m`. |
| `map` | Ayrık değer sözlüğü. Anahtarlar JSON değerinin dize karşılığı (`"true"`, `"ARAMA"`), değerler hedef sayı. |
| `default` | Kaynak alan yok ya da null ise kullanılacak değer. Verilmezse alan hiç yazılmaz. |

Kaynak alan eksikse ve `default` yoksa o MAVLink alanı önceki değerinde kalır —
tek bir eksik alan tüm mesajı düşürmez.

## Veri akışı ve durum

### Gelen telemetri
1. `UdpProtocolChannel` datagram alır → `JsonDatagramCodec` → `QJsonObject`
2. `FieldMapper` profildeki `telemetry.mavlink` kurallarını uygular
3. `MAVLinkSynthesizer` `GLOBAL_POSITION_INT`, `ATTITUDE`, `SYS_STATUS`,
   `GPS_RAW_INT`, `VFR_HUD` üretir; ayrıca profil `heartbeatHz` hızında
   `HEARTBEAT` yayar (`flightMode` eşlemesi `custom_mode` alanına gider)
4. `passthrough` alanları `QVariantMap` olarak QML'e açılır

`HEARTBEAT` telemetriden bağımsız, sabit hızda üretilir — böylece QGC'nin
bağlantı-koptu tespiti taret paketleri kesildiğinde doğru çalışır.

### Giden komut
1. QGC `MAV_CMD_COMPONENT_ARM_DISARM` veya mod değişikliği gönderir
2. `ExternalProtocolLink::_writeBytes` MAVLink çerçevesini ayrıştırır
3. `CommandTranslator` profildeki şablonla eşler
4. `NdjsonCodec` → `TcpProtocolChannel` → taret
5. Gelen `{ack, status}` eşlenir; başarısızsa `COMMAND_ACK` ile `MAV_RESULT_FAILED`

### Joystick
QGC'nin `MANUAL_CONTROL` mesajı yakalanır, profil şablonuyla CSV'ye çevrilir,
`rateHz` ile sınırlandırılıp UDP'den ESP32'ye gider.

## Hata yönetimi

| Durum | Davranış |
|---|---|
| Profil dosyası yok / bozuk | Özellik kapalı, QGC normal çalışır, kullanıcıya tek uyarı |
| Bozuk paket | Sayaç artır, `CustomLog`'a yaz, **asla çökme** |
| Kanal kopması | Otomatik yeniden bağlanma (`reconnectMs`), durum QML'e yansır |
| Telemetri zaman aşımı | `timeoutMs` sonrası `HEARTBEAT` durur → QGC bağlantı-koptu gösterir |
| **Joystick kopması** | Tek bir `failsafe` paketi gönder (`0,0,0,0`) — YKI'deki `sendStop()` davranışı. ESP32 watchdog'una güvenilmez. |
| Eşlenemeyen alan | Sessizce atla, tek seferlik debug logu |
| `target: null` | Eşleyici tolere eder, `passthrough`'da null olarak geçer |

## Test stratejisi

| Katman | Yöntem |
|---|---|
| `FrameCodec` | Birim testi: kayıtlı bayt dizileri ↔ beklenen JSON, bozuk girdi dahil |
| `FieldMapper` | Birim testi: örnek telemetri paketi → beklenen eşlenmiş değerler; ölçek, birim, null, eksik alan |
| `CommandTranslator` | Birim testi: MAVLink komutu → beklenen JSON şablonu |
| `MAVLinkSynthesizer` | Üretilen `mavlink_message_t` alanları doğrulanır |
| Uçtan uca | `telemetry_sim.py --mode fake` → QGC; harita konumu, HUD, batarya doğru mu |

`telemetry_sim.py` donanımsız gerçek trafik ürettiği için entegrasyon testi
taret olmadan yapılabilir.

## Kapsam dışı (bu pakette yok)

- **Video:** QGC'nin mevcut RTSP/UDP video altyapısı zaten var; taret için ayrı
  kod gerekmez, sadece Ayarlar'dan URL girilir. Yeni bileşen yazılmayacak.
- **Yarışma sunucusu:** `CompetitionServerClient` yerinde kalır. İleride bu
  motora bir profil olarak taşınabilir, ama bu pakette değil.
- **Taret'e özel Fly View paneli:** `passthrough` verisi QML'e açılır; paneli
  çizmek ayrı bir iş.

## Alınan kararlar

1. **ARM/DISARM MAVLink üzerinden.** Taret'e özel bir QML butonu değil,
   QGC'nin mevcut arm/disarm akışı kullanılacak: `MAV_CMD_COMPONENT_ARM_DISARM`
   `_writeBytes`'ta yakalanıp profildeki `arm`/`disarm` şablonuna çevrilecek.
   Gerekçe: mevcut buton, onay diyaloğu ve arm göstergesi bedava gelir.

2. **Uçuş modu isimleri `CustomFirmwarePlugin` üzerinden.** `ARAMA`/`TAKIP`/
   `KILITLI` profilin `telemetry.flightMode.values` haritasıyla `custom_mode`
   sayısına çevrilecek; `CustomFirmwarePlugin` aynı isimleri mod listesi olarak
   sunacak. Böylece toolbar'daki mod göstergesi ve mod seçici çalışır.

## Uygulama sırası

Tek planda üç aşama; her aşama kendi başına çalışır durumda bitmeli:

| Aşama | İçerik | Bitince ne çalışır |
|---|---|---|
| **A** | `ProtocolProfile`, `FrameCodec`, `FieldMapper`, `MAVLinkSynthesizer`, `UdpProtocolChannel`, `ExternalProtocolLink` (yalnız gelen yön) | Taret QGC'de araç olarak görünür; harita, HUD, batarya, mod çalışır. `telemetry_sim.py` ile uçtan uca test edilebilir. |
| **B** | `TcpProtocolChannel`, `CommandTranslator`, ACK işleme | Arm/disarm ve mod değiştirme çalışır |
| **C** | `CsvCodec`, joystick hız sınırlama + failsafe | Taret QGC joystick'iyle sürülür |

Aşama A tek başına YKI'nin telemetri ve gösterge işlevini karşılar; B ve C
kontrolü ekler.
