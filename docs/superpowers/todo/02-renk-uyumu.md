# 02 — Menü / palet renk uyumu

**Tip:** Sınırlı · **Durum:** ✅ Bitti (derlendi, sayfalar yüklenerek doğrulandı)

## Kök neden

`CustomPlugin::paletteOverride` (`custom/src/CustomPlugin.cc:409`) paletin
**47 renginden 31'ini** eziyor. Kalan 16'sı stok QGC değerlerinde kalıyor ve
bunlar tam da Ayarlar sayfalarının kullandığı renkler.

Özel şema: `window #212529`, `windowShade #343a40`, `windowShadeDark #1a1c1f`,
`button #495057` (koyu tema).

| Ezilmemiş renk | Stok değer | Nerede görünüyor |
|---|---|---|
| `groupBorder` | `#bbbbbb` / `#707070` | **Her `SettingsGroupLayout` çerçevesi ve ayracı** |
| `buttonBorder` | `#ffffff`/`#d9d9d9`/`#707070`/`#adadb8` | Buton kenarları |
| `windowShadeLight` | `#909090`/`#828282`/`#707070`/`#626262` | Ara tonlar |
| `toolbarBackground` | stok | Üst araç çubuğu |
| `toolStripFGColor`, `toolStripHoverColor` | stok | Araç şeridi |
| `statusPassedText`, `statusFailedText`, `statusPendingText` | stok | Durum metinleri |
| `colorYellow`, `colorYellowGreen` | stok | Uyarı tonları |
| `mapMissionTrajectory`, `surveyPolygonInterior`, `surveyPolygonTerrainCollision` | stok | Harita (düşük öncelik) |
| `sliderUTMSP`, `switchUTMSP`, `successNotifyUTMSP` | stok | UTMSP (kullanılmıyor) |

Ayrıca `paletteOverride` içinde **`hoverColor`** diye bir renk eziliyor —
`QGCPalette` içinde böyle bir renk **yok**, o blok sessizce hiçbir iş yapmıyor.

## Yapılacaklar

- [x] `groupBorder` ekle — en yüksek etkili tek değişiklik
- [x] `buttonBorder` ekle
- [x] `windowShadeLight` ekle
- [x] `toolbarBackground` ve `toolStripHoverColor` ekle
- [~] `toolStripFGColor` **kasten atlandı** — non-themed bir renk; stok
      değeri `#ffffff` (enabled) ve stok `toolbarBackground` açık temada da
      `#ffffff`. Yani beyaz-üstüne-beyaz sorunu upstream'de zaten var ve
      `ToolStrip` bunu başka yoldan çözüyor olabilir. Kullanıcı araç şeridinde
      sorun bildirmedi; dokunmak riskli, ayrı iş olarak bırakıldı.
- [x] `statusPassedText`, `statusFailedText`, `statusPendingText` ekle
- [x] `colorYellow`, `colorYellowGreen` ekle
- [x] Ölü `hoverColor` bloğunu sil
- [x] `NoFlyMapSettings.qml:642-661` sabit renklerini palete çevir:
      `#FF3B30`→`colorRed`, `#BF4A4A`→`colorOrange`, `#1589FF`→`colorBlue`,
      `"white"`→`mapWidgetBorderLight`, `Qt.rgba(sabit)` → `Qt.rgba(palet.r, .g, .b, alfa)`
      (QGC'nin mevcut deyimi; `Qt.alpha()` bu repoda hiç kullanılmıyor)
- [x] `qgcPal.text`'in kenarlık rengi olarak kullanıldığı 4 yeri `groupBorder`
      yap — koyu temada saf beyaz 1px çerçeve çiziyorlardı
      (`NoFlyMapSettings.qml` ×3, `CustomOpsSettings.qml` ×1)
- [x] **Yan bulgu:** `CustomOpsSettings.qml` `qgcPal`'ı kullanıyor ama ne
      tanımlıyor ne de `QGroundControl.Palette` import ediyordu; `SettingsPage`
      de sağlamıyor. Mevcut bir hataydı (`ReferenceError`), düzeltildi.
- [x] Ayarlar sayfaları gerçekten yüklenerek doğrulandı — geçici bir sayfa
      gezdirici ile "Yasakli Alanlar", "Custom Ops", "Yarisma" ve
      "Uygulama Resmi" sayfaları açıldı; **benim değişikliklerimden hata yok**.
      Test kodu geri alındı, kaynak ağacı temiz.
- [ ] Renklerin *görsel* doğrulaması (koyu/açık tema karşılaştırması) — bu
      gözle bakmayı gerektiriyor, sizde kaldı

## Doğrulama kanıtı (2026-09-03)

- **Derleme:** 1885/1885 başarılı; `custom/src/CustomPlugin.cc.o` derlendi.
- **Çalışma zamanı:** dört özel Ayarlar sayfası da yüklendi, QML hatası yok.
- **`qgcPal` düzeltmesi kanıtlandı:** düzeltmesiz sürümde `qmllint` `qgcPal`
  satırlarını sorunlu gösteriyordu, düzeltilmişte göstermiyor; "Custom Ops"
  sayfası çalışma zamanında temiz açıldı.
- **Not:** `custom/src/AppSettings/*.qml` dosyaları `custom.qrc`'ye düz kaynak
  olarak gömülüyor, `qt_add_qml_module` ile **önceden derlenmiyorlar**. Bu yüzden
  buradaki hatalar ancak sayfa açılınca ortaya çıkıyor — `qgcPal` hatasının
  fark edilmemesinin sebebi bu. Gelecekte bu klasör için de qmlcachegen
  düşünülebilir.

## Eklenen renkler

Şema kaynağı: mevcut özel paletin Open Color / Bootstrap gri skalası
(`#212529` gray-9 … `#f1f3f5` gray-1) ve mevcut aksanlar
(`colorGreen #27bf89`, `colorRed #e1544c`, `colorOrange #f7b24a`).

| Renk | Koyu (En/Dis) | Açık (En/Dis) | Gerekçe |
|---|---|---|---|
| `groupBorder` | `#495057` / `#343a40` | `#dee2e6` / `#e9ecef` | Ayarlar kutu çerçevesi + ayraç |
| `buttonBorder` | `#5c636a` / `#495057` | `#ced4da` / `#e9ecef` | Buton kenarı |
| `windowShadeLight` | `#495057` / `#343a40` | `#adb5bd` / `#ced4da` | Stok'un ara-ton rolünü korur |
| `toolbarBackground` | `#212529` / `#212529` | `#ffffff` / `#f8f9fa` | `window` ile birebir eşleşsin |
| `toolStripHoverColor` | `#07916d` / `#495057` | `#aeebd0` / `#e4e4e4` | `buttonHighlight` ile tutarlı |
| `statusPassedText` | `#27bf89` / `#777c89` | `#009431` / `#9d9d9d` | `colorGreen` izler |
| `statusFailedText` | `#e1544c` / `#777c89` | `#cc0808` / `#9d9d9d` | `colorRed` / `warningText` izler |
| `statusPendingText` | `#f7b24a` / `#777c89` | `#b95604` / `#9d9d9d` | `colorOrange` izler |
| `colorYellow` | `#fcc419` | `#f08c00` | Stok `#ffff00` diğer aksanlardan çok parlaktı |
| `colorYellowGreen` | `#94d82d` | `#5c940d` | Aynı skala |

Sonuç: ezilen renk sayısı 31 → 40 (ölü `hoverColor` çıkarıldı, 10 gerçek renk eklendi).

## Dokunulan dosyalar

- `custom/src/CustomPlugin.cc`
- `custom/src/AppSettings/NoFlyMapSettings.qml`
- `custom/src/AppSettings/CustomOpsSettings.qml`

## Kapsam dışı

`custom/src/FlyViewCustomLayer.qml` içinde 35 sabit renk var. Kullanıcı sorunu
**Ayarlar sayfalarında** tarif etti; Fly View katmanı ayrı bir iş olarak
bırakıldı. (2026-09-03 kararı.)
