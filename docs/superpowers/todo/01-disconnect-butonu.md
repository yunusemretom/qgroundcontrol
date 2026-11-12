# 01 — Disconnect butonu

**Tip:** Sınırlı · **Durum:** ✅ Bitti (derlendi ve çalıştırıldı)

## Sorun

`src/QmlControls/FlyViewToolBar.qml:79` içinde bir Disconnect butonu zaten var,
ama iki kusuru yüzünden işe yaramıyor:

1. `visible: _activeVehicle && _communicationLost` — sadece **bağlantı zaten
   koptuğunda** görünüyor. Yani bilerek bağlantı kesmek için kullanılamıyor.
2. `_activeVehicle.closeVehicle()` yalnızca aracın link referanslarını siliyor
   (`VehicleLinkManager::closeVehicle`, `src/Vehicle/VehicleLinkManager.cc:356`).
   **Link açık kalıyor**, bir sonraki heartbeat'te araç geri geliyor.

Ek olarak: autoconnect link'leri (`_addUDPAutoConnectLink`,
`src/Comms/LinkManager.cc:386`) `dynamic` işaretli olduğu için Comm Links
sayfasında hiç görünmüyor — yani şu an bu link'ler arayüzden **hiçbir şekilde**
kesilemiyor. Kesilse bile `_portListTimer` bir sonraki turda geri ekliyor.

## Yapılacaklar

- [x] Butonu her zaman görünür yap: `visible: _activeVehicle`
- [x] Onay diyaloğu ekle (`mainWindow.showMessageDialog`, `Dialog.Yes | Dialog.No`)
      — uçuş sırasında yanlışlıkla basmaya karşı
- [x] Diyalog metni kesilecek link'in adını göstersin
      (`_activeVehicle.vehicleLinkManager.linkNames`)
- [x] Onayda gerçekten kes: aracın kullandığı link'leri isimle eşleyip
      `QGroundControl.linkManager.linkConfigurations` üzerinden
      `config.link.disconnect()` çağır, ardından `closeVehicle()`
- [x] Autoconnect durumunu ele al: kesilen link `isAutoConnect` ise kullanıcıyı
      uyar, çünkü aksi halde birkaç saniye içinde geri gelir
- [x] Buton rengi palete uysun (uyarı tonu, `primaryButton` değil)
- [x] Derle ve çalıştırıp doğrula — Paket 02 ile birlikte tek derlemede

## Yapılan doğrulama (2026-09-03)

- `qmlformat` ile parse: **OK**
- `qmllint` karşılaştırması (aynı dizinde, öncesi/sonrası):
  `import` 15→15, `missing-property` 7→7 değişmedi;
  `unqualified` 16→18 (ikisi de dosyanın mevcut deyimi: `QGroundControl.`
  singleton'ı ve `mainWindow` global'i — satır 28 ve 71'de zaten var);
  `unused-imports` 2→1 (**iyileşme**: `QtQuick.Dialogs` artık kullanılıyor).
- **Derleme: 1885/1885 başarılı**, `Debug/Custom-QGroundControl` bağlandı.
- `FlyViewToolBar.qml` qmlcachegen ile **AOT derlendi** — sözdizimi ve
  derlenebilir bağlamalar derleme zamanında doğrulandı.
- Uygulama offscreen çalıştırıldı, bu dosyadan hata çıkmadı.
- Kalan: butonun görsel doğrulaması ve gerçek araçla disconnect denemesi
  sizin elinizde — offscreen'de araç bağlı olmadığı için buton görünmüyor.

## Dokunulan dosyalar

- `src/QmlControls/FlyViewToolBar.qml`

## Doğrulama

- Bağlıyken buton görünüyor mu?
- Basınca onay çıkıyor mu? Hayır deyince hiçbir şey olmuyor mu?
- Evet deyince araç **ve** link gerçekten düşüyor mu, geri gelmiyor mu?
