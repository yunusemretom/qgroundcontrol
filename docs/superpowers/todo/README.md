# QGC Özelleştirme — Yapılacaklar

TEKNOFEST / Pars Takımı QGroundControl çatalı için planlanan işler.
Başlangıç: 2026-09-03 · Taban sürüm: v5.0.8 (`Stable_V5.0` dalı)

## Durum

| # | Paket | Tip | Durum | Dosya |
|---|-------|-----|-------|-------|
| 1 | Disconnect butonu | Sınırlı | ✅ Bitti | [01-disconnect-butonu.md](01-disconnect-butonu.md) |
| 2 | Menü / palet renk uyumu | Sınırlı | ✅ Bitti | [02-renk-uyumu.md](02-renk-uyumu.md) |
| 3 | Genel protokol köprüsü (Çelik Kubbe) | Mimari | ✅ Aşama A/B/C tamam, testli; donanım denemesi sizde | [03-protokol-koprusu.md](03-protokol-koprusu.md) |
| 4 | v5.1.4 yükseltme hazırlık raporu | Doküman | ✅ Bitti | [04-surum-yukseltme-hazirlik.md](04-surum-yukseltme-hazirlik.md) |

Durum işaretleri: ⬜ başlanmadı · 🔵 devam ediyor · ✅ bitti · ⏸️ beklemede

## Sıra ve gerekçe

1. **Disconnect** önce — dakikalar sürer, anında fayda.
2. **Renk uyumu** ikinci — kök neden bulundu; Paket 3'ün yeni paneli doğru
   renklerle doğsun diye önce palet düzelmeli.
3. **Protokol köprüsü** asıl iş — mimari, kendi tasarım dokümanı ve uygulama
   planı olacak.
4. **Yükseltme hazırlığı** son — kod yok, sadece rapor.

## Kapsam dışı bırakılan kararlar

- **v5.0.8 → v5.1.4 yükseltmesi ertelendi** (2026-09-03 kullanıcı kararı).
  Gerekçe: v5.1.4 Qt 6.11.0 istiyor, makinede Qt 6.8.3 var; ayrıca
  `QGCCorePlugin` API'si değişmiş, `custom/` port gerektiriyor. Paket 4 bunun
  hazırlık raporunu üretecek, uygulama sonraya kalacak.

## Yolda bulunan hatalar — ✅ düzeltildi (2026-11-12)

- `globals.customMaxSquareJson` tanımlı değildi (`MainWindow.qml`); maksimum
  alan özelliği canlı güncellenmiyordu ve `onCustomMaxSquareJsonChanged` hiç
  çalışmıyordu. Özellik eklendi.
- `FlyViewToolStripActionList.qml` `QtQml` import etmiyordu; `QtObject`
  tanınmadığı için **ana pencere hiç yüklenmiyordu**.
- `CompetitionServerClient`: eksik alan `Null` dönüyordu, kontrol `Undefined`
  bekliyordu → kilit durumu her pakette sıfırlanıyordu. Ayrıca satır sonu
  gelmeyen veri sınırsız tamponlanıyordu (NDJSON codec'te de).
- `mav.tlog` / `mav.tlog.raw` artık git tarafından izlenmiyor (`.gitignore`).
  Geçmişten silinmedi — bu geri alınamaz bir işlem, karar sizin.

## Açık kalanlar

- Marka görselleri hâlâ örnek projedeki `dronecode-*.svg`; takım logosu
  repoda yok, eklenince `CustomPlugin::brandImageIndoor/Outdoor` güncellenmeli.
- `TurretFirmwarePlugin` uçuş modu adlarını sabit tutuyor (profille aynı).
  Plugin tüm generic araçlarca paylaşılıyor ve hangi link'ten geldiğini
  bilmiyor; profilden okumak çekirdek API değişikliği ister.
- AppImage paketlemek için sistemde `patchelf` kurulu değil.

## Mobil derlemeler (2026-11-12)

Android ve iOS, fork'taki GitHub Actions'ta `feature/**` dallarına push edilince
derleniyor (Android iş akışı elle de başlatılabilir).

| Platform | Çıktı | Not |
|---|---|---|
| Android | `QGroundControl.apk`, arm64-v8a | `ANDROID_KEYSTORE_PASSWORD` gizli değişkeni yoksa iş içinde üretilen geçici anahtarla imzalanır: kurulabilir, ama her derlemede anahtar değiştiği için güncellemeden önce eski sürüm kaldırılmalı |
| iOS | `Custom-QGroundControl.app` | İmzasız; Apple Developer hesabı ve imzalama olmadan cihaza kurulamaz. GStreamer kapalı (QGC iOS'ta desteklemiyor), seri bağlantı kapalı |

Bu sırada düzeltilen ve normal derlemeyi de etkileyen sorunlar:

- `libevents`, `ulog_cpp`, `zlib`, `SDL_GameControllerDB`, `qmdnsengine` dal
  adıyla (main/master/develop) çekiliyordu → bilinen commit'lere sabitlendi.
  Güncel `libevents` v5.0.8'in MAVLink'iyle derlenmiyordu.
- `custom/android` kaplaması CI'da hiç uygulanmıyordu (`file(COPY)` aynı zaman
  damgalı dosyayı atlıyor) → APK stok simge ve eski manifestle çıkıyordu.
- Qt Android paketleri artık `all_os` altında; GStreamer Android SDK'sı
  indirmesi kararsız → sürdürmeli indirme + SHA-256 + önbellek.

Upstream'in iOS hataları (iOS iş akışı yalnızca elle tetiklendiği için birikmiş):
dağıtım hedefi 12.0 → 16.0, `QGC_NO_SERIAL_LINK` korumasız kod
(`QGCApplication`, `LinkManager`, GPS QML), `BluetoothLink` iOS bloğundaki
`name()` hatası, `.mm` dosyalarının C++ PCH ile derlenmesi, SDL'nin
`SDL_IsIPad` sembolü, yapıt yükleme yolu.

Açık kalan: Android paket kimliği stok QGC ile aynı (`org.mavlink.qgroundcontrol`),
telefonda resmi QGC ile birlikte kurulamaz.
