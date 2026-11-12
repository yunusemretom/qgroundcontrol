# 03 — Genel protokol köprüsü (Çelik Kubbe taret)

**Tip:** Mimari · **Durum:** ✅ Aşama A/B/C kodu ve testleri tamam; donanımla deneme sizde

Bu paket kendi tasarım dokümanını ve uygulama planını alacak:
`docs/superpowers/specs/2026-09-03-protokol-koprusu-design.md`

## Amaç

QGC'yi MAVLink konuşmayan yarışma donanımlarıyla da çalışır hale getirmek.
İlk hedef Çelik Kubbe taret sistemi; sonrasında **yeni yarışma = yeni JSON
profil dosyası**, C++ değişikliği olmadan.

Karar (2026-09-03): kapsam **tam kontrol** — telemetri + komut + joystick +
video; yani `Celik_Kubbe/YKI/` web uygulamasının yerini alacak.

## Hedef protokol (kaynak: `/home/tom/Documents/Projeler/Celik_Kubbe/YKI/`)

| Yön | Taşıma | Çerçeveleme | Kaynak dosya |
|---|---|---|---|
| Telemetri (in) | UDP :5001 | datagram başına bir JSON | `backend/telemetryBridge.js` |
| Komut (out) | TCP :5000 | satır-sonlu JSON + ACK | `backend/commandBridge.js` |
| Joystick (out) | UDP :5005 → ESP32 | CSV `"pitch,yaw,fire,arm\n"` @50 Hz | `backend/joystickBridge.js` |

## Bileşenler

| Sınıf | Sorumluluk | Bağımlılık |
|---|---|---|
| `ProtocolProfile` | JSON profili yükle + doğrula | yok (saf veri) |
| `ProtocolChannel` (+`Udp`/`Tcp`) | Bayt taşı, bağlan/kop/yeniden bağlan | Qt network |
| `FrameCodec` (+`Ndjson`/`JsonDatagram`/`Csv`) | Bayt ↔ `QJsonObject` | yok (saf dönüşüm) |
| `FieldMapper` | Alan eşle, ölçekle, birim çevir | `ProtocolProfile` |
| `MAVLinkSynthesizer` | Eşlenen alanlardan MAVLink üret | `FieldMapper` |
| `CommandTranslator` | Giden MAVLink → profil şablonu | `ProtocolProfile` |
| `ExternalProtocolLink` | Orkestratör, `LinkInterface` alt sınıfı | yukarıdakiler |

Kalıp `src/Comms/MockLink/` — gerçek trafik taşımak yerine MAVLink **sentezleyen**
bir `LinkInterface`. Repo içinde kanıtlanmış desen.

`FrameCodec` ve `FieldMapper` saf fonksiyon → donanımsız birim testi yazılabilir.

## Yapılacaklar

- [x] Tasarım dokümanını yaz ve kullanıcıya incelet — onaylandı 2026-09-03
- [x] Aşama A uygulama planını yaz →
      [`plans/2026-09-03-protokol-koprusu-asama-a.md`](../plans/2026-09-03-protokol-koprusu-asama-a.md)

### Aşama A — telemetri girişi (✅ Tamamlandı)

- [x] Task 1 · `ProtocolProfile` — profil yükleme + doğrulama (19 birim testi)
- [x] Task 2 · `JsonDatagramCodec` — bayt ↔ JSON (9 birim testi)
- [x] Task 3 · `FieldMapper` — eşleme kuralları (12 birim testi)
- [x] Task 4 · `MAVLinkSynthesizer` — MAVLink üretimi (6 birim testi)
- [x] Task 5 · `UdpProtocolChannel` — UDP alma + zaman aşımı (12 birim testi)
- [x] Task 6 · `ExternalProtocolLink` — orkestratör + QGC kaydı + ayar QML'i
- [x] Task 7 · Çelik Kubbe profili + uçtan uca doğrulama
- [x] Task 8 · Uçuş modu isimleri (`TurretFirmwarePlugin`)

### Aşama B — komut kanalı (✅ Tamamlandı)

- [x] `TcpProtocolChannel`, `NdjsonCodec`, `CommandTranslator`, ACK işleme
- [x] Arm/disarm ve mod değiştirme (`SET_MODE` ve `MAV_CMD_DO_SET_MODE`)
- [x] Komutlar profilin `commands` şablonlarından; `{value}` yerleştirme,
      `id`/`timestamp` ekleme, ACK'i hem `id` hem `cmd` ile eşleme
- [x] İletilemeyen komuta QGC'ye anında ACK (UNSUPPORTED / FAILED / DENIED)

### Aşama C — joystick (✅ Tamamlandı)

- [x] `CsvCodec`, profil `template` / `failsafe` / `rateHz`
- [x] Hız sınırı en yeni kareyi bekletip gönderiyor — "kol ortada" karesi
      hiçbir zaman düşmüyor
- [x] Link kapanırken profilin failsafe karesi gönderiliyor

### Revizyon (2026-11-12)

Kod incelemesinde bulunup düzeltilenler, her biri testli:

- Telemetri zaman aşımından sonra heartbeat geri gelmiyordu.
- Sentezlenen MAVLink, QGC'nin kanalını paylaşıyordu → sahte paket kaybı.
  Artık MockLink gibi ayrı bir yardımcı kanal kullanılıyor.
- QGC `bytesReceived` içinde senkron yanıt verince yayınlar iç içe giriyor ve
  sonraki dinleyicilere ters sırada ulaşıyordu → FIFO kuyruğu.
- `TcpProtocolChannel`: `close()` sonrası kendiliğinden yeniden bağlanma,
  kapanırken GUI'yi 500 ms dondurma, zaman aşımı sinyalinin sürekli tekrarı.
- Profildeki `mavType` artık heartbeat'te; bilinmeyen tip profil hatası.

## Planlama sırasında çıkan önemli bulgu

QGC'de **yeni link tipi eklemek core dosyalara dokunmayı zorunlu kılıyor** —
`custom/` içinde kalmak mümkün değil:

| Dosya | Ne |
|---|---|
| `src/Comms/LinkConfiguration.h` | `LinkType` enum'una yeni değer |
| `src/Comms/LinkManager.cc` | 3 nokta: iki switch + `linkTypeStrings()` |
| `test/CMakeLists.txt` | özel test dizini kaydı |
| `test/UnitTestList.cc` | test kaydı |

Hepsi `#ifdef QGC_CUSTOM_BUILD` ile korunacak; böylece stok yapı etkilenmez ve
upstream merge'de çakışma yüzeyi tek satırlık bloklara iner. Bu değişiklikler
Paket 04'ün yükseltme raporuna girmeli.

## Test altyapısı

`telemetry_sim.py --mode fake` donanımsız gerçek trafik üretiyor — entegrasyon
testi için hazır. Codec + mapper için kayıtlı paketlerle birim testi.

## Uygulama sırasında bulunan ve düzeltilen hata

`InitialConnectStateMachine.cc`'deki `_shouldSkipRequests()` refactor'ünde
`_stateRequestMission`, `_stateRequestGeoFence` ve `_stateRequestRallyPoints`
atlama dalları hem `_first*LoadComplete()` hem `advance()` çağırıyordu. Üç
`_first*LoadComplete()` fonksiyonunun **hepsi kendi içinde `advance()` çağırıyor**
(`Vehicle.cc:1829/1835/1843`), yani durum makinesi tek durum için iki kez
ilerliyordu. Şu an zararsız — `_stateSignalInitialConnectComplete` yığın geri
sarılmadan `_active`'i false yapıyor ve `advance()` başta dönüyor — ama bu bir
tesadüf; senkron tamamlanmayan bir durum eklenirse durumlar sessizce atlanır.
Üç fazladan `advance()` çağrısı kaldırıldı.

## Bağımlılık

Paket 02 (renk uyumu) önce bitmeli — yeni taret paneli doğru palet renkleriyle
doğsun diye.
