# QGC Özelleştirme — Yapılacaklar

TEKNOFEST / Pars Takımı QGroundControl çatalı için planlanan işler.
Başlangıç: 2026-09-03 · Taban sürüm: v5.0.8 (`Stable_V5.0` dalı)

## Durum

| # | Paket | Tip | Durum | Dosya |
|---|-------|-----|-------|-------|
| 1 | Disconnect butonu | Sınırlı | ✅ Bitti | [01-disconnect-butonu.md](01-disconnect-butonu.md) |
| 2 | Menü / palet renk uyumu | Sınırlı | ✅ Bitti | [02-renk-uyumu.md](02-renk-uyumu.md) |
| 3 | Genel protokol köprüsü (Çelik Kubbe) | Mimari | 🔵 Kod tamam, 84 test geçiyor; GUI doğrulaması sizde | [03-protokol-koprusu.md](03-protokol-koprusu.md) |
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

## Yolda bulunan, kapsam dışı hatalar

Doğrulama sırasında sizin kaydedilmemiş çalışmanızda iki gerçek hata çıktı.
**Dokunmadım** — sizin aktif olarak üzerinde çalıştığınız kod:

1. `globals` nesnesi (`src/UI/MainWindow.qml:85`) `customNoFlyZonesJson`
   tanımlıyor ama **`customMaxSquareJson` tanımlamıyor**. Oysa
   `NoFlyMapSettings.qml` (5 yer) ve `FlyViewCustomLayer.qml` (1 yer) onu
   kullanıyor. Sonuç: yazma denemesi `Cannot assign to non-existent property`
   hatası veriyor, okuma `undefined` dönüyor, yani "maksimum alan" özelliği
   sessizce hep yerel ayara düşüyor ve canlı güncellenmiyor.
   **Tek satırlık düzeltme:** `MainWindow.qml:85` civarına
   `property string customMaxSquareJson: ""` eklemek.

2. `FlyViewCustomLayer.qml:629` — `Connections` içinde
   `onCustomMaxSquareJsonChanged` işleyicisi var ama hedefte o isimde sinyal
   yok, yani hiç çalışmıyor. (1) düzeltilirse bu da düzelir.
