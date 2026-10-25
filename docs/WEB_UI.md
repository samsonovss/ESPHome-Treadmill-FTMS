# Web interface

[Русская версия](WEB_UI.ru.md) · [Back to README](../README.md) · [**Live demo**](https://samsonovss.github.io/ESPHome-Treadmill-FTMS/)

Open the treadmill's address (`http://treadmill.local` or its IP) in any browser. The page is built into the firmware and works without internet; it adapts to phone, tablet and desktop. Language (RU/EN) and theme are stored in the browser.

## Workout

One chart shows speed, incline and heart rate with the zones in the background. Start, pause and stop, change speed and incline by hand, or follow the running program. During a route, the map and the elevation ahead are shown. After a workout, a summary card shows distance, time, pace, heart rate, time in zones and the load.

## Programs

- **Pulse zone**: pick a zone; the treadmill adjusts speed to keep your heart rate in it, with warm-up and cool-down.
- **HIIT**: work and recovery intervals by time or by heart rate.
- **Fartlek**: random speed and incline surges within limits you set; presets can be saved.
- **Your own programs**: build interval plans in the editor, drag zones and phase edges on the chart. Programs are stored on the board, also those saved by an [AI agent](MCP.md).
- **Routes**: draw a route on the map or pick one from the Zwift catalog; the incline follows its elevation profile by distance.

Programs cannot be changed while the belt is running.

## History

Every workout is recorded on the board (about one sample per second) without Home Assistant. Open a workout to see its charts and summary, download **FIT** (Strava, Garmin Connect and others) or CSV, or move it to the trash; trash can be restored.

## Progress

For a week (Monday-Sunday), a month or a year:

- **Volume**: kilometres, number of workouts and time, compared with the previous period;
- **Load**: training-load points from minutes in heart-rate zones, with a warning when the last 7 days jump well above your usual level;
- **Intensity**: share of easy, moderate and hard time;
- **Fitness**: heart rate at the same speed and incline over time;
- **Consistency**: weeks in a row with training, and which days.

## Settings

Profile and heart-rate zones, heart-rate control, HR strap pairing, speed and belt behaviour, incline calibration, handrail buttons, Zwift, motor board, safety, display, **System** (versions and updates), diagnostics and **Agents (MCP)**.

The header shows a red monitor icon when new display firmware is available and a blue chip icon when new treadmill firmware is available; both open *Settings → System*.
