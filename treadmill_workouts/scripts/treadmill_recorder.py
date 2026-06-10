#!/usr/bin/env python3
import csv, json, os, signal, sys, time
from datetime import datetime
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError
try:
    from zoneinfo import ZoneInfo
except Exception:
    ZoneInfo = None

BASE_URL = "http://127.0.0.1:8123"
ROOT = Path("/config/treadmill_workouts")
WORKOUT_DIR = ROOT / "workouts"
SCRIPT_DIR = ROOT / "scripts"
LOG_DIR = ROOT / "logs"
STATE_DIR = ROOT / "state"
SECRET_DIR = ROOT / "secrets"
TOKEN_FILE = SECRET_DIR / ".ha_token"
PID_FILE = STATE_DIR / "recorder.pid"
STOP_FILE = STATE_DIR / "recorder.stop"
STATE_FILE = STATE_DIR / "recorder_state.json"
LOG_FILE = LOG_DIR / "recorder.log"
TZ = ZoneInfo("Asia/Yekaterinburg") if ZoneInfo else None

LIVE_ENTITIES = {
    "usage": "binary_sensor.treadmill_treadmill_usage",
    "heart_rate": "sensor.heart_rate",
    "speed_kmh": "sensor.treadmill_treadmill_speed",
    "incline_pct": "sensor.treadmill_treadmill_incline",
    "distance_km": "sensor.treadmill_treadmill_distance",
    "time_s": "sensor.treadmill_treadmill_time",
    "calories": "sensor.treadmill_calories_burned",
    "status": "sensor.status",
    "program": "select.workout_program",
    "zone": "select.heart_rate_zone",
}
SUMMARY_ENTITIES = {
    "distance_km": "sensor.last_distance",
    "calories": "sensor.last_calories",
    "avg_speed_kmh": "sensor.last_avg_speed",
    "avg_incline_pct": "sensor.last_avg_incline",
    "avg_heart_rate": "sensor.treadmill_average_heart_rate",
    "max_heart_rate": "sensor.max_heart_rate",
}
CSV_FIELDS = ["timestamp", "heart_rate", "speed_kmh", "incline_pct", "distance_km", "time_s", "calories", "status", "program", "zone"]

def now_dt():
    return datetime.now(TZ).astimezone() if TZ else datetime.now().astimezone()

def now_iso():
    return now_dt().isoformat(timespec="seconds")

def log(msg):
    ROOT.mkdir(parents=True, exist_ok=True)
    for d in (WORKOUT_DIR, LOG_DIR, STATE_DIR, SECRET_DIR):
        d.mkdir(parents=True, exist_ok=True)
    with LOG_FILE.open("a", encoding="utf-8") as f:
        f.write(f"{now_iso()} {msg}\n")

def token():
    return TOKEN_FILE.read_text(encoding="utf-8").strip()

def state(entity_id):
    req = Request(f"{BASE_URL}/api/states/{entity_id}", headers={"Authorization": "Bearer " + token(), "Content-Type": "application/json"})
    with urlopen(req, timeout=5) as r:
        return json.load(r)

def safe_state(entity_id):
    try:
        return state(entity_id).get("state", "")
    except Exception as e:
        log(f"WARN failed to read {entity_id}: {e}")
        return ""

def fnum(v):
    try:
        if v in (None, "", "unknown", "unavailable"):
            return None
        return float(v)
    except Exception:
        return None

def pid_alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except Exception:
        return False

def read_pid():
    try:
        return int(PID_FILE.read_text().strip())
    except Exception:
        return None

def write_json(path, data):
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(path)

def current_live_row():
    row = {"timestamp": now_iso()}
    for key, eid in LIVE_ENTITIES.items():
        if key == "usage":
            continue
        row[key] = safe_state(eid)
    return row

def current_summary(start_iso, end_iso, csv_path, samples):
    summary = {
        "start": start_iso,
        "end": end_iso,
        "duration_sec": round((datetime.fromisoformat(end_iso) - datetime.fromisoformat(start_iso)).total_seconds(), 1),
        "samples": samples,
        "csv": str(csv_path),
        "source": "Home Assistant treadmill recorder",
        "entities": {"live": LIVE_ENTITIES, "summary": SUMMARY_ENTITIES},
    }
    for key, eid in SUMMARY_ENTITIES.items():
        raw = safe_state(eid)
        num = fnum(raw)
        summary[key] = num if num is not None else raw
    summary["final_status"] = safe_state("sensor.status")
    summary["program"] = safe_state("select.workout_program")
    summary["zone"] = safe_state("select.heart_rate_zone")
    return summary

def start_recording():
    ROOT.mkdir(parents=True, exist_ok=True)
    for d in (WORKOUT_DIR, LOG_DIR, STATE_DIR, SECRET_DIR):
        d.mkdir(parents=True, exist_ok=True)
    pid = read_pid()
    if pid and pid_alive(pid):
        log(f"start ignored: recorder already running pid={pid}")
        return 0
    try:
        PID_FILE.unlink(missing_ok=True)
        STOP_FILE.unlink(missing_ok=True)
    except Exception:
        pass
    start = now_dt()
    start_iso = start.isoformat(timespec="seconds")
    stem = start.strftime("%Y-%m-%d_%H-%M-%S")
    csv_path = WORKOUT_DIR / f"{stem}.csv"
    summary_path = WORKOUT_DIR / f"{stem}.summary.json"
    PID_FILE.write_text(str(os.getpid()), encoding="utf-8")
    write_json(STATE_FILE, {"running": True, "pid": os.getpid(), "start": start_iso, "csv": str(csv_path), "summary": str(summary_path), "samples": 0})
    log(f"START pid={os.getpid()} csv={csv_path}")
    samples = 0
    off_seen = 0
    try:
        with csv_path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
            writer.writeheader()
            while True:
                if STOP_FILE.exists():
                    log("stop file seen")
                    break
                usage = safe_state(LIVE_ENTITIES["usage"])
                if usage != "on":
                    off_seen += 1
                else:
                    off_seen = 0
                # wait for 3 consecutive off samples, so a tiny sensor hiccup does not cut the workout
                if off_seen >= 3 and samples > 0:
                    log("usage off confirmed")
                    break
                row = current_live_row()
                writer.writerow({k: row.get(k, "") for k in CSV_FIELDS})
                f.flush()
                samples += 1
                if samples % 30 == 0:
                    write_json(STATE_FILE, {"running": True, "pid": os.getpid(), "start": start_iso, "csv": str(csv_path), "summary": str(summary_path), "samples": samples, "last_sample": row["timestamp"]})
                time.sleep(1)
    except Exception as e:
        log(f"ERROR recording failed: {e}")
        raise
    finally:
        end_iso = now_iso()
        try:
            summary = current_summary(start_iso, end_iso, csv_path, samples)
            write_json(summary_path, summary)
            fit_path = csv_path.with_suffix(".fit")
            try:
                import subprocess
                subprocess.run([sys.executable, str(ROOT / "scripts" / "treadmill_fit_export.py"), str(csv_path), str(summary_path), str(fit_path)], check=True, timeout=30)
                summary["fit"] = str(fit_path)
                write_json(summary_path, summary)
                log(f"FIT generated {fit_path}")
            except Exception as e:
                summary["fit_error"] = str(e)
                write_json(summary_path, summary)
                log(f"WARN FIT generation failed: {e}")
            write_json(STATE_FILE, {"running": False, "pid": os.getpid(), "start": start_iso, "end": end_iso, "csv": str(csv_path), "summary": str(summary_path), "samples": samples})
            log(f"END samples={samples} summary={summary_path}")
        finally:
            try: PID_FILE.unlink(missing_ok=True)
            except Exception: pass
            try: STOP_FILE.unlink(missing_ok=True)
            except Exception: pass
    return 0

def stop_recording():
    ROOT.mkdir(parents=True, exist_ok=True)
    for d in (WORKOUT_DIR, LOG_DIR, STATE_DIR, SECRET_DIR):
        d.mkdir(parents=True, exist_ok=True)
    STOP_FILE.write_text(now_iso(), encoding="utf-8")
    pid = read_pid()
    if pid and pid_alive(pid):
        log(f"STOP requested pid={pid}")
        for _ in range(12):
            if not pid_alive(pid):
                return 0
            time.sleep(1)
        log(f"STOP timeout pid still alive={pid}")
    else:
        log("STOP requested but no active pid")
    return 0

def status():
    print(STATE_FILE.read_text(encoding="utf-8") if STATE_FILE.exists() else "{}")
    return 0

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
    if cmd == "start":
        raise SystemExit(start_recording())
    if cmd == "stop":
        raise SystemExit(stop_recording())
    if cmd == "status":
        raise SystemExit(status())
    print("usage: treadmill_recorder.py start|stop|status", file=sys.stderr)
    raise SystemExit(2)
