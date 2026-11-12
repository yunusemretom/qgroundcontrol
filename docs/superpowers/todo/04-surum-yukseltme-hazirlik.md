# 04 — v5.1.4 yükseltme hazırlık raporu

**Tip:** Doküman (kod yok) · **Durum:** ✅ Bitti

Yükseltmenin kendisi **ertelendi** (2026-09-03 kullanıcı kararı). Bu paket
sadece, ileride uygulanmak üzere, hazırlık raporunu üretecek.

## Tespit edilen durum (2026-09-03)

| | Mevcut | v5.1.4 gereksinimi |
|---|---|---|
| QGC | v5.0.8 + 13 commit | v5.1.4 (30 Ağu 2026) |
| Qt | **6.8.3** | **6.11.0 min** / 6.11.1 hedef |
| CMake | 4.4.2 ✅ | 3.25+ |
| GCC | 12.3.0 (Ubuntu 22.04) | — |
| Boş disk (`/home`) | 36 GB (%93 dolu) | Qt ~5 GB + build ~15 GB |

Kaynak: `.github/build-config.json` (v5.1.4).

Engeller:
- v5.0.8 → v5.1.4 farkı **4159 dosya**.
- `QGCCorePlugin` / `QGCOptions` API'si değişmiş (`wifiReliableForCalibration`
  kaldırılmış, sanal metodlar satır içine alınmış, `Q_OBJECT` eklenmiş) →
  `custom/` **port** gerektiriyor, düz merge yetmiyor.
- `custom-example` içine `PerimeterScan*` (ComplexMissionItem + PlanCreator)
  eklenmiş — özel yapı iskeleti büyümüş.
- `mav.tlog` (51 MB) ve `mav.tlog.raw` (40 MB) repoya commit'lenmiş ve dirty →
  rebase'i zorlaştırıyor.

Kolaylaştırıcı: core `src/` içindeki 15 değişikliğimizin çoğu zaten upstream'e
girmiş yamalar. Gerçek özel iş `custom/` içinde toplanmış.

## Yapılacaklar

- [x] Qt 6.11.1 kurulum adımlarını yaz (Qt Maintenance Tool, gerekli modüller
      `build-config.json`'daki listeden)
- [x] Disk temizliği planı (şu an 36 GB boş, ~20 GB gerekecek)
- [x] `custom/` port listesi — `QGCCorePlugin`/`QGCOptions` API farkları
      dosya dosya, hangi metodun karşılığı ne
- [x] `mav.tlog` / `mav.tlog.raw` için `.gitignore` + geçmişten çıkarma önerisi
- [x] Yükseltme stratejisi: rebase mi, yeni dalda temiz port mu
- [x] Tahmini süre ve risk değerlendirmesi

## Çıktı

[`specs/2026-09-03-v51-yukseltme-hazirlik.md`](../specs/2026-09-03-v51-yukseltme-hazirlik.md)

Rapor yazıldıktan sonra ölçülen ek gerçekler:

- Çakışma yüzeyi büyüdü: Aşama A core'da **8 dosyaya** dokunuyor (planda 4 öngörülmüştü).
  En riskli ikisi `InitialConnectStateMachine.cc` (yapısı yeniden düzenlendi) ve
  `ParameterManager.cc` (bilinçli upstream sapması içeriyor).
- tlog dosyaları büyümeye devam ediyor: 51.4→52.8 MB ve 40.2→41.3 MB.
- v5.1.4 `qtgraphs` modülünü istiyor — v5.0.8'de yoktu, Qt kurulumunda atlanırsa
  configure patlar.
- `custom-example`'da `res/QGCLogoFull.svg` → `res/Images/QGCLogoFull.svg` taşınmış,
  qrc yollarımızı kırar.
