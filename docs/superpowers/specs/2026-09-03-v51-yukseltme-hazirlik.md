# v5.0.8 → v5.1.4 Yükseltme Hazırlık Raporu

**Tarih:** 2026-09-03 · **Paket:** 04 · **Durum:** Rapor — uygulama ertelendi

Bu belge yükseltmeyi **yapmaz**. Yükseltmenin ne gerektirdiğini, neyin kırılacağını
ve hangi sırayla ilerleneceğini ölçer. Yükseltme kararı 2026-09-03'te bilinçli
olarak ertelendi.

## Özet karar

**Şu an yükseltmeyin.** Üç sebep, önem sırasıyla:

1. **Qt 6.11 gerekiyor, sizde 6.8.3 var.** Bu bir tercih değil, sert bir engel —
   v5.1.4 `find_package(Qt6 6.11.0...)` ile başarısız olur.
2. **Aşama A'nın core'a dokunduğu 8 dosya yükseltmede elle taşınacak.** Bu iş,
   yükseltmeden *önce* yapılırsa iki kez yapılır.
3. **Disk.** `/home` %93 dolu, 36 GB boş. Qt 6.11 (~5 GB) + ikinci bir build ağacı
   (~3 GB) + yeni bağımlılıklar sığar ama rahat değil.

## Ölçülen durum

| | Mevcut | v5.1.4 gereksinimi | Durum |
|---|---|---|---|
| QGC | v5.0.8 + 22 commit | v5.1.4 (30 Ağu 2026) | — |
| **Qt** | **6.8.3** | **6.11.0 min / 6.11.1 hedef** | ❌ **Engel** |
| CMake | 4.4.2 | 3.25+ | ✅ |
| GCC | 12.3.0 (Ubuntu 22.04) | — | ✅ |
| Ninja | 1.13.0 | — | ✅ |
| Boş disk (`/home`) | 36 GB (%93 dolu) | ~10 GB gerekir | ⚠️ Dar |

Kaynak: v5.1.4 `.github/build-config.json` → `qt.minimum_version = "6.11.0"`,
`qt.version = "6.11.1"`, `build.cmake_minimum_version = "3.25"`.

v5.1.4'ün istediği Qt modülleri (aynı dosyadan):
`qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport qtimageformats
qtshadertools qtconnectivity qtquick3d qtsensors qtscxml qtwebsockets qthttpserver`

**Dikkat:** `qtgraphs` v5.0.8'de yoktu. Qt 6.11 kurulumunda bu modülü seçmeyi
unutmayın, yoksa configure aşamasında patlarsınız.

## Çakışma yüzeyi: core'a dokunduğumuz her yer

Bu, yükseltmenin gerçek maliyeti. Aşağıdaki dosyalar upstream'e ait ve v5.1.4'te
değişmiş olabilir; her biri elle taşınacak.

| Dosya | Neden dokunduk | Risk |
|---|---|---|
| `src/Comms/LinkConfiguration.h` | `LinkType` enum'una `TypeExternalProtocol` | Düşük — enum'a tek satır |
| `src/Comms/LinkConfiguration.cc` | `createSettings()` + `duplicateSettings()` switch dalları | Düşük |
| `src/Comms/LinkManager.cc` | include, `createConnectedLink()` switch, `loadLinkConfigurationList()` switch, `linkTypeStrings()` | **Orta** — 4 nokta, enum ile string sırası eşleşmeli |
| `src/FactSystem/ParameterManager.cc` | External Protocol link'i parametre yüklemesinden muaf tut **+ upstream hata düzeltmesi** | **Yüksek** — aşağıya bakın |
| `src/Vehicle/ComponentInformation/ComponentInformationManager.cc` | 2 yerde komponent bilgisi isteğini atla | Düşük |
| `src/Vehicle/InitialConnectStateMachine.cc` | `_shouldSkipRequests()` yardımcısı + 7 durumda kullanımı | **Yüksek** — dosyanın yapısı yeniden düzenlendi |
| `src/QmlControls/FlyViewToolBar.qml` | Disconnect butonu (Paket 01) | Düşük |
| `test/CMakeLists.txt`, `test/UnitTestList.cc` | Özel test kaydı | Düşük |

Hepsi `#ifdef QGC_CUSTOM_BUILD` ile korunuyor — **bir istisna dışında**.

### Bilinçli upstream sapması: `ParameterManager.cc`

`refreshAllParameters()` içindeki "bu linkler parametre yüklemez" dalına
koşulsuz bir `return;` eklendi. Öncesinde fonksiyon o dalı çalıştırıp **düşmeye
devam ediyor** ve FTP parametre indirmesi başlatıyordu — kendi yorumunun tam
tersi. Bu gerçek bir upstream hatası.

Ama `return;` `#ifdef` dışında olduğu için **high-latency ve log-replay linklerinin
davranışını da değiştiriyor**. Bizim için doğru davranış; upstream'e göre bir
sapma. Yükseltmede bu satırın hâlâ gerekli olup olmadığını kontrol edin —
upstream aradan düzeltmiş olabilir.

## `custom/` klasörünün port maliyeti

v5.1.4'te `custom-example` şablonu değişmiş:

- `QGCCorePlugin` / `QGCOptions` API'si yeniden yazılmış: `wifiReliableForCalibration()`
  kaldırılmış, sanal metodlar satır içine alınmış, `CustomOptions` ve
  `CustomFlyViewOptions` sınıflarına `Q_OBJECT` eklenmiş.
- `CustomFirmwarePlugin` 119 satır değişmiş.
- `custom-example` şablonuna `PerimeterScanComplexItem` + `PerimeterScanPlanCreator`
  + iki QML editörü eklenmiş (yeni bir örnek özellik; almak zorunda değilsiniz).
- `res/QGCLogoFull.svg` → `res/Images/QGCLogoFull.svg` taşınmış (qrc yollarınız kırılır).

`custom/` içindeki kendi dosyalarımız (Comms köprüsü, CompetitionServerClient,
NoFlyMapSettings, FlyViewCustomLayer, TurretFirmwarePlugin) API değişikliğinden
doğrudan etkilenmiyor; asıl iş `CustomPlugin.{h,cc}` ve
`CustomFirmwarePlugin*` dosyalarında.

## Repo hijyeni — yükseltmeden önce halledin

`mav.tlog` ve `mav.tlog.raw` repoya commit'lenmiş ve **büyümeye devam ediyor**:

| Dosya | Önce | Şimdi |
|---|---|---|
| `mav.tlog` | 51.4 MB | **52.8 MB** |
| `mav.tlog.raw` | 40.2 MB | **41.3 MB** |

Bunlar her `git status`'ta kirli görünüyor, her rebase'de çakışıyor ve depoyu
şişiriyor. Yükseltme bir rebase/merge işi olduğu için bu **doğrudan maliyet**.

Öneri, bu sırayla:
1. `.gitignore`'a ekleyin.
2. `git rm --cached mav.tlog mav.tlog.raw` ile takipten çıkarın (dosyalar diskte kalır).
3. Geçmişten temizlemek isterseniz `git filter-repo` — ama bu geçmişi yeniden
   yazar, `origin`'e force-push gerektirir. Yalnız çalışıyorsanız sorun değil;
   başkası bu depoyu klonladıysa önce onlarla konuşun.

## Uygulama sırası (yükseltmeye karar verdiğinizde)

1. **Disk açın.** En az 15 GB hedefleyin. Eski build ağaçları ve Qt sürümleri ilk bakılacak yer.
2. **Qt 6.11.1 kurun** (Qt Maintenance Tool). Yukarıdaki modül listesini, özellikle
   `qtgraphs`'ı seçin. 6.8.3'ü **silmeyin** — geri dönüş yolunuz.
3. **tlog dosyalarını takipten çıkarın** (yukarıdaki hijyen adımı). Rebase'i belirgin şekilde kolaylaştırır.
4. **Yeni bir dal açın**, `Stable_V5.0`'ı bozmayın:
   `git checkout -b upgrade/v5.1.4 v5.1.4`
5. **`custom/` klasörünü kiraz-toplayın**, merge etmeyin. `custom/` neredeyse tamamen
   bize ait; v5.1.4'ün `custom-example`'ı ile karşılaştırıp API farklarını elle uygulayın.
6. **Core yamalarını yeniden uygulayın** — yukarıdaki tablodaki 8 dosya. Hepsi küçük
   ve `#ifdef`'li; `git log -p` ile her birinin bizim commit'ini bulup elle taşıyın.
7. **Derleyin, testleri çalıştırın.** 84 birim testimiz var; hepsi geçmeli.
   Geçmiyorsa API kayması vardır.
8. **Uçtan uca doğrulayın** — `telemetry_sim.py --mode fake` ile taret bağlantısı,
   ve gerçek PX4 aracıyla normal uçuş akışı.

## Tahmin

| İş | Süre |
|---|---|
| Qt 6.11 kurulumu + disk temizliği | 1-2 saat |
| tlog hijyeni | 15 dakika |
| `custom/` portu (`CustomPlugin`, `CustomFirmwarePlugin`) | Yarım – 1 gün |
| 8 core yamasının yeniden uygulanması | 2-4 saat |
| Derleme + test + düzeltme döngüsü | Yarım – 1 gün |
| **Toplam** | **2-3 gün**, aksilik çıkmazsa |

En büyük belirsizlik `InitialConnectStateMachine.cc` — v5.1.4'te o dosya
yeniden düzenlendiyse yamamız doğrudan uymaz, mantığı yeniden türetmek gerekir.

## Yükseltmeyi ne tetiklemeli

Bunlardan biri olmadan yükseltmeye gerek yok:

- v5.1.x'te ihtiyacınız olan somut bir özellik/düzeltme çıkması
- Yarışma sezonunun bitmesi (yükseltme sırasında sistem bir süre kararsız olur)
- v5.0.x'in güvenlik güncellemesi almayı bırakması
