# Protokol Köprüsü — Aşama B ve C Uygulama Planı

**Goal:** Çelik Kubbe taretine giden komut kanalı (Aşama B - TCP :5000) ve joystick kanalı (Aşama C - UDP :5005) bileşenlerini tamamlayıp çift yönlü kontrol sağlamak.

## Aşama B Bileşenleri (Komut Kanalı)
1. `TcpProtocolChannel`: TCP soket istemcisi, yeniden bağlanma mantığı (`reconnectMs`).
2. `NdjsonCodec`: `\n` ile ayrılmış JSON çerçeveleme/çözümleme.
3. `CommandTranslator`: MAVLink komutlarını (`ARM/DISARM`, mod değiştirme) profil JSON şablonlarına çevirme ve ACK eşleme.

## Aşama C Bileşenleri (Joystick Kanalı)
1. `CsvCodec`: MAVLink `MANUAL_CONTROL` verisini profil şablonuna göre CSV satırına çevirme.
2. `ExternalProtocolLink` entegrasyonu: 50 Hz hız sınırlama ve bağlantı kopmasında failsafe `0,0,0,0` paketi.
3. `TurretFirmwarePlugin`: Modların QML'den tetiklenebilmesi için `canBeSet = true` yapılması.
