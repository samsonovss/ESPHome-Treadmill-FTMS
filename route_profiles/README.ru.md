# Профили маршрутов

Беговые маршруты Zwift для веб-интерфейса дорожки (каталог «Маршруты → Zwift»).

```
zwift/
  index.json          список: slug, название, мир, км, набор высоты (up), круг, закрыт уровнем, e = высота каждые 100 м
  routes/<slug>.json  маршрут с картой и высотой: pts = [[lat, lon, высота_м], …] (≤1500 точек)
  gpx/<slug>.gpx      полный GPX для скачивания/импорта вручную
tools/
  gen-zwift.mjs       пересборка zwift/ (npm i zwift-data && node tools/gen-zwift.mjs)
```

Веб-интерфейс грузит каталог прямо отсюда:
`https://raw.githubusercontent.com/samsonovss/ESPHome-Treadmill-FTMS/main/route_profiles/zwift/`.

Геометрия: статические данные [ZwiftMap](https://zwiftmap.com). Список маршрутов: [zwift-data](https://github.com/andipaetzold/zwift-data) (MIT).
Zwift и названия маршрутов — товарные знаки Zwift, Inc.; проект с Zwift не связан.
