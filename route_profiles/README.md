# Route Profiles

[Русская версия](README.ru.md)

This directory contains source route data and development tools used to prepare
autonomous treadmill incline profiles.

It is **not required for a normal ESPHome installation**. The profiles used by
the firmware are already stored in [`esphome/incline_profiles.h`](../esphome/incline_profiles.h).

## Directory Layout

```text
route_profiles/
├── gpx_files/       # Source GPX routes with distance and elevation waypoints
├── scripts/         # GPX conversion tools
└── zwift_profiles/  # Saved Zwift route profiles and generated C++ snippets
```

## Preparing a Route

1. Create or export a route as KML or GPX.
2. If elevation data is missing, process it with
   [GPS Visualizer](https://www.gpsvisualizer.com/convert_input):
   - output format: `GPX`;
   - DEM elevation data: `Best available source`;
   - equal-interval waypoints: `30 meters`.
3. Save the result in `route_profiles/gpx_files/`.
4. Run the converter from the repository root:

   ```sh
   python3 route_profiles/scripts/gpx_to_incline_profile.py \
     route_profiles/gpx_files/example.gpx \
     --name example_route
   ```

By default, the generated C++ profile is written next to the input file as
`example.profile.h`. Use `--output` to choose another path.

The generated file contains a two-dimensional ESPHome profile:

```cpp
const float incline_profile_example_route[][2] PROGMEM = {
  {0.000, 0},
  {0.030, 3},
};
```

The first value is route distance in kilometres. The second value is the
treadmill incline level.

## Calibration

The default conversion assumes:

```text
maximum treadmill level = 15
real grade at maximum = 5%
```

Override these values when your treadmill uses different limits:

```sh
python3 route_profiles/scripts/gpx_to_incline_profile.py \
  route_profiles/gpx_files/example.gpx \
  --name example_route \
  --maximum-level 15 \
  --real-grade-at-maximum 5
```

Review the generated profile before copying it into
`esphome/incline_profiles.h`. The converter intentionally never overwrites the
firmware profile file automatically.

## GPX Requirements

The converter expects GPX waypoints containing:

- an `<ele>` elevation value;
- a `<name>` beginning with distance in metres and containing the word
  `distance`.

GPS Visualizer can create waypoints in this format.
