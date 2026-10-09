# Route profiles

Zwift running routes for the treadmill web UI («Routes → Zwift» catalog).

```
zwift/
  index.json          route list: slug, name, world, km, elevation gain (up), lap, locked, e = elevation every 100 m
  routes/<slug>.json  route with map and elevation: pts = [[lat, lon, ele_m], …] (≤1500 points)
  gpx/<slug>.gpx      full-resolution GPX for manual download/import
tools/
  gen-zwift.mjs       regenerates zwift/ (npm i zwift-data && node tools/gen-zwift.mjs)
```

The web UI loads the catalog directly from
`https://raw.githubusercontent.com/samsonovss/ESPHome-Treadmill-FTMS/main/route_profiles/zwift/`.

Geometry: [ZwiftMap](https://zwiftmap.com) static streams. Route list: [zwift-data](https://github.com/andipaetzold/zwift-data) (MIT).
Zwift and route names are trademarks of Zwift, Inc.; this project is not affiliated with Zwift.
