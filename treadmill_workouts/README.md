# Treadmill Workout Recorder

This optional Home Assistant companion records treadmill workouts to CSV and exports a Garmin FIT activity after each workout.

## How It Works

1. A Home Assistant automation starts `treadmill_recorder.py` when `binary_sensor.treadmill_treadmill_usage` turns on.
2. The recorder reads treadmill entities from the local Home Assistant REST API once per second.
3. Samples are written to a timestamped CSV file.
4. When the treadmill stops, the recorder writes a summary JSON file.
5. `treadmill_fit_export.py` converts the CSV and summary into a FIT running activity with treadmill sub-sport.

The FIT exporter uses only the Python standard library. It writes timestamps, heart rate, speed, distance, incline/grade, calories, lap, session, and activity summaries.

## Directory Layout

```text
/config/treadmill_workouts/
├── scripts/
│   ├── treadmill_recorder.py
│   └── treadmill_fit_export.py
├── secrets/
│   └── .ha_token
├── workouts/
├── logs/
└── state/
```

The repository contains empty runtime directories. Tokens, logs, state files, and personal workout exports are excluded by `.gitignore`.

## Installation

1. Copy `treadmill_workouts/` to `/config/treadmill_workouts/` in Home Assistant.
2. Make the scripts executable:

   ```sh
   chmod 755 /config/treadmill_workouts/scripts/*.py
   ```

3. In the Home Assistant user profile, create a long-lived access token.
4. Save only the token value to:

   ```text
   /config/treadmill_workouts/secrets/.ha_token
   ```

5. Restrict access to the token:

   ```sh
   chmod 600 /config/treadmill_workouts/secrets/.ha_token
   ```

6. Merge [`examples/configuration.yaml`](examples/configuration.yaml) into `configuration.yaml`.
7. Add [`examples/automations.yaml`](examples/automations.yaml) through YAML or recreate the three automations in the Home Assistant UI.
8. Check the configuration and restart Home Assistant.

## Configuration

Edit these constants in `scripts/treadmill_recorder.py` when your installation uses different values:

- `BASE_URL` - local Home Assistant URL;
- `ROOT` - recorder directory;
- `TZ` - workout timezone;
- `LIVE_ENTITIES` - entities sampled every second;
- `SUMMARY_ENTITIES` - final workout statistics.

The supplied entity IDs match this treadmill project.

## Output

Each workout creates:

```text
YYYY-MM-DD_HH-MM-SS.csv
YYYY-MM-DD_HH-MM-SS.summary.json
YYYY-MM-DD_HH-MM-SS.fit
```

CSV columns:

```text
timestamp, heart_rate, speed_kmh, incline_pct, distance_km,
time_s, calories, status, program, zone
```

The FIT file can be imported into applications that accept standard FIT running activities.

## Manual Use

```sh
python3 /config/treadmill_workouts/scripts/treadmill_recorder.py start
python3 /config/treadmill_workouts/scripts/treadmill_recorder.py stop
python3 /config/treadmill_workouts/scripts/treadmill_recorder.py status
```

Convert an existing CSV manually:

```sh
python3 \
  /config/treadmill_workouts/scripts/treadmill_fit_export.py \
  workout.csv workout.summary.json workout.fit
```

Logs are written to `/config/treadmill_workouts/logs/recorder.log`.

## Privacy

Do not commit `.ha_token`, CSV, FIT, summary, state, or log files. Workout exports may contain timestamps, heart rate, speed, distance, and other personal activity data.
