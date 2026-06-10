# Zwift Connection and Automatic Incline

[Русская версия](ZWIFT.ru.md) | [Back to README](../../README.md)

This guide describes the native Zwift connection and the conversion of real route grade into a treadmill-specific incline level.

## What Zwift Sends

Zwift uses real route grade:

- `3%` means a 3 m rise over 100 m of travel;
- `0%` means flat terrain;
- a negative value means downhill.

Incline levels on many consumer treadmills do not equal real percentages. For example, a maximum level of `15` may physically produce only about a `5%` grade.

## Pairing

1. Turn on the treadmill and wait for ESPHome to start.
2. Open the Zwift device-pairing screen.
3. Select `KICKR RUN` as the running-speed source.
4. Select the same device under controllable incline when Zwift offers that option.
5. Confirm that the BLE status in Home Assistant changes to connected.
6. Verify that `Zwift Requested Incline` changes while moving along the route.

Zwift may use separate BLE connections for speed and the controllable device. The configuration allows two BLE server clients.

## Home Assistant Settings

### Zwift Auto Incline

Allows the mapped value to control the physical incline mechanism.

When disabled:

- Zwift commands are still received;
- diagnostic sensors continue to update;
- the physical incline does not change.

Keep it disabled during initial pairing and calibration.

### Treadmill Maximum Incline Level

The maximum value accepted by the treadmill controller. The current implementation supports `1-15`.

### Treadmill Real Grade At Maximum

The measured physical belt grade at the treadmill's maximum incline level.

### Zwift Incline Intensity

Effect scaling:

- `100%` uses the full calculated value;
- `50%` uses half;
- `0%` always produces zero incline.

### Diagnostic Sensors

- `Zwift Requested Incline` is the original Zwift grade;
- `Zwift Mapped Treadmill Incline` is the calculated treadmill target.

## Measuring Real Grade

1. Stop the belt.
2. Set the treadmill to its maximum incline level.
3. Measure the vertical belt rise relative to the flat position.
4. Measure the horizontal length used for the measurement.
5. Calculate:

```text
grade, % = rise / horizontal length * 100
```

Example:

```text
rise = 6 cm
length = 120 cm
real grade = 6 / 120 * 100 = 5%
```

Use:

```text
Treadmill Maximum Incline Level = 15
Treadmill Real Grade At Maximum = 5
```

## Mapping Formula

```text
treadmill level =
    Zwift grade
    * treadmill maximum level
    / real grade at maximum
    * intensity / 100
```

Example with maximum level `15`, real maximum grade `5%`, and intensity `100%`:

| Zwift grade | Treadmill level |
|---:|---:|
| -3% | 0 |
| 1% | 3 |
| 2% | 6 |
| 3% | 9 |
| 5% | 15 |
| 8% | 15, limited |

Negative values are mapped to `0` because a typical treadmill cannot lower its front below the flat position.

## Safe Validation

1. Disable `Zwift Auto Incline`.
2. Start a route and verify `Zwift Requested Incline`.
3. Compare it with `Zwift Mapped Treadmill Incline`.
4. Set a low intensity such as `20%`.
5. Step off the belt and enable automatic incline.
6. Verify mechanism direction and travel limits.
7. Increase intensity gradually.

Do not enable native automatic incline until direction and hardware limits have been verified.

## Recommended Documentation Media

Three screenshots are sufficient for the README:

1. `KICKR RUN` on the Zwift pairing screen;
2. the controllable-incline selection;
3. the Home Assistant card with settings and diagnostic sensors.

A short video should show the Zwift grade, diagnostic sensors, and physical incline changing together. Use a small preview linked to the video instead of committing another large MOV file to Git.
