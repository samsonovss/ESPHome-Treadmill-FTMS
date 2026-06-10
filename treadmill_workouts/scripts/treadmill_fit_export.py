#!/usr/bin/env python3
"""Convert treadmill CSV + summary JSON to a minimal Garmin FIT activity file.

The generated FIT is intentionally simple and standards-oriented:
- file_id, event start/stop
- record messages with timestamp, heart_rate, speed, distance, grade/incline
- lap/session/activity summaries
Sport: running, sub_sport: treadmill.
"""
import csv
import json
import math
import os
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

FIT_EPOCH = datetime(1989, 12, 31, tzinfo=timezone.utc)

BASE_TYPES = {
    "enum": (0x00, 1, 0xFF),
    "uint8": (0x02, 1, 0xFF),
    "uint16": (0x84, 2, 0xFFFF),
    "sint16": (0x83, 2, 0x7FFF),
    "uint32": (0x86, 4, 0xFFFFFFFF),
    "uint32z": (0x8C, 4, 0x00000000),
    "sint32": (0x85, 4, 0x7FFFFFFF),
}

CRC_TABLE = [
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
]

def crc_update(crc, byte):
    tmp = CRC_TABLE[crc & 0xF]
    crc = (crc >> 4) & 0x0FFF
    crc ^= tmp ^ CRC_TABLE[byte & 0xF]
    tmp = CRC_TABLE[crc & 0xF]
    crc = (crc >> 4) & 0x0FFF
    crc ^= tmp ^ CRC_TABLE[(byte >> 4) & 0xF]
    return crc & 0xFFFF

def fit_crc(data):
    crc = 0
    for b in data:
        crc = crc_update(crc, b)
    return crc

def parse_dt(value):
    if not value:
        return None
    s = str(value).strip()
    if not s:
        return None
    if s.endswith("Z"):
        s = s[:-1] + "+00:00"
    try:
        dt = datetime.fromisoformat(s)
    except ValueError:
        for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d_%H-%M-%S"):
            try:
                dt = datetime.strptime(s, fmt)
                break
            except ValueError:
                dt = None
        if dt is None:
            return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc)

def fit_time(dt):
    return int((dt.astimezone(timezone.utc) - FIT_EPOCH).total_seconds())

def fnum(v):
    try:
        if v in (None, "", "unknown", "unavailable"):
            return None
        x = float(v)
        if math.isnan(x) or math.isinf(x):
            return None
        return x
    except Exception:
        return None

def pack_value(base, value):
    _, size, invalid = BASE_TYPES[base]
    if value is None:
        value = invalid
    if base in ("enum", "uint8"):
        return struct.pack("<B", int(value) & 0xFF)
    if base == "uint16":
        return struct.pack("<H", int(value) & 0xFFFF)
    if base == "sint16":
        return struct.pack("<h", int(value))
    if base in ("uint32", "uint32z"):
        return struct.pack("<I", int(value) & 0xFFFFFFFF)
    if base == "sint32":
        return struct.pack("<i", int(value))
    raise ValueError(base)

class FitWriter:
    def __init__(self):
        self.data = bytearray()
        self.defs = {}
    def define(self, local, global_num, fields):
        # fields: list of (field_num, base_type)
        self.defs[local] = fields
        b = bytearray()
        b.append(0x40 | local)
        b.append(0)          # reserved
        b.append(0)          # little endian
        b += struct.pack("<H", global_num)
        b.append(len(fields))
        for field_num, base in fields:
            type_id, size, _ = BASE_TYPES[base]
            b += struct.pack("<BBB", field_num, size, type_id)
        self.data += b
    def data_msg(self, local, values):
        fields = self.defs[local]
        b = bytearray([local])
        for field_num, base in fields:
            b += pack_value(base, values.get(field_num))
        self.data += b

def load_csv(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            dt = parse_dt(r.get("timestamp"))
            if not dt:
                continue
            rows.append({
                "timestamp": dt,
                "heart_rate": fnum(r.get("heart_rate")),
                "speed_kmh": fnum(r.get("speed_kmh")),
                "incline_pct": fnum(r.get("incline_pct")),
                "distance_km": fnum(r.get("distance_km")),
                "calories": fnum(r.get("calories")),
            })
    rows.sort(key=lambda r: r["timestamp"])
    # Remove duplicate timestamps, keep last sample per second.
    dedup = {}
    for r in rows:
        dedup[int(r["timestamp"].timestamp())] = r
    return [dedup[k] for k in sorted(dedup)]

def load_summary(path):
    if path and Path(path).exists():
        return json.loads(Path(path).read_text(encoding="utf-8"))
    return {}

def derive_distance(rows, summary):
    total_km = fnum(summary.get("distance_km"))
    if total_km is None:
        distances = [r["distance_km"] for r in rows if r.get("distance_km") is not None]
        if distances:
            total_km = max(distances)
    # If live distance missing, integrate speed over time and later scale to summary distance when available.
    live_has_dist = any(r.get("distance_km") not in (None, 0) for r in rows)
    accum_m = 0.0
    out = []
    prev = None
    for r in rows:
        if live_has_dist and r.get("distance_km") is not None:
            m = max(0.0, r["distance_km"] * 1000.0)
        else:
            if prev is not None:
                dt = max(0.0, (r["timestamp"] - prev["timestamp"]).total_seconds())
                sp = prev.get("speed_kmh") or r.get("speed_kmh") or 0.0
                accum_m += sp / 3.6 * dt
            m = accum_m
        out.append(m)
        prev = r
    if total_km is not None and out and out[-1] > 0 and not live_has_dist:
        scale = (total_km * 1000.0) / out[-1]
        out = [m * scale for m in out]
    return out, (total_km * 1000.0 if total_km is not None else (out[-1] if out else 0.0))

def build_fit(csv_path, summary_path=None, fit_path=None):
    rows = load_csv(csv_path)
    summary = load_summary(summary_path)
    if not rows:
        raise SystemExit("No timestamped rows in CSV")
    start = parse_dt(summary.get("start")) or rows[0]["timestamp"]
    end = parse_dt(summary.get("end")) or rows[-1]["timestamp"]
    total_timer = max(1, int(round((end - start).total_seconds())))
    distances_m, total_distance_m = derive_distance(rows, summary)
    total_calories = fnum(summary.get("calories"))
    avg_speed_kmh = fnum(summary.get("avg_speed_kmh"))
    avg_speed_ms = (avg_speed_kmh / 3.6) if avg_speed_kmh is not None else (total_distance_m / total_timer if total_timer else None)
    max_speed_ms = max([(r.get("speed_kmh") or 0) / 3.6 for r in rows] + [0])
    avg_hr = fnum(summary.get("avg_heart_rate"))
    if avg_hr is None:
        hrs = [r["heart_rate"] for r in rows if r.get("heart_rate")]
        avg_hr = sum(hrs) / len(hrs) if hrs else None
    max_hr = fnum(summary.get("max_heart_rate"))
    if max_hr is None:
        hrs = [r["heart_rate"] for r in rows if r.get("heart_rate")]
        max_hr = max(hrs) if hrs else None

    fw = FitWriter()
    # local 0 file_id global 0
    fw.define(0, 0, [(0, "enum"), (1, "uint16"), (2, "uint16"), (3, "uint32z"), (4, "uint32")])
    # local 1 event global 21: timestamp, event, event_type, event_group
    fw.define(1, 21, [(253, "uint32"), (0, "enum"), (1, "enum"), (4, "uint8")])
    # local 2 record global 20: timestamp, heart_rate, distance, speed, grade
    # grade field is percent incline, scale 100 (1.23% => 123).
    fw.define(2, 20, [(253, "uint32"), (3, "uint8"), (5, "uint32"), (6, "uint16"), (9, "sint16")])
    # local 3 lap global 19
    fw.define(3, 19, [(253, "uint32"), (2, "uint32"), (7, "uint32"), (8, "uint32"), (9, "uint32"), (10, "uint16"), (11, "uint16"), (12, "uint16"), (13, "uint8"), (14, "uint8"), (15, "uint16"), (24, "enum"), (25, "enum")])
    # local 4 session global 18
    fw.define(4, 18, [(253, "uint32"), (2, "uint32"), (7, "uint32"), (8, "uint32"), (9, "uint32"), (10, "uint16"), (11, "uint16"), (14, "uint8"), (15, "uint8"), (16, "uint16"), (5, "enum"), (6, "enum"), (0, "enum")])
    # local 5 activity global 34: timestamp, total_timer_time, num_sessions, type, event, event_type
    fw.define(5, 34, [(253, "uint32"), (0, "uint32"), (1, "uint16"), (2, "enum"), (3, "enum"), (4, "enum")])

    file_created = fit_time(start)
    fw.data_msg(0, {0: 4, 1: 255, 2: 1, 3: 0x54454E01, 4: file_created})  # activity, development-like manufacturer
    fw.data_msg(1, {253: fit_time(start), 0: 0, 1: 0, 4: 0})              # timer start
    for r, dist_m in zip(rows, distances_m):
        sp_ms = (r.get("speed_kmh") / 3.6) if r.get("speed_kmh") is not None else None
        hr = r.get("heart_rate")
        incline = r.get("incline_pct")
        fw.data_msg(2, {
            253: fit_time(r["timestamp"]),
            3: int(round(hr)) if hr and hr > 0 else None,
            5: int(round(dist_m * 100)),             # m, scale 100
            6: int(round(sp_ms * 1000)) if sp_ms is not None else None,  # m/s, scale 1000
            9: int(round(incline * 100)) if incline is not None else None,  # grade %, scale 100
        })
    fw.data_msg(1, {253: fit_time(end), 0: 0, 1: 4, 4: 0})                # timer stop_all

    common = {
        253: fit_time(end),
        2: fit_time(start),
        7: int(round(total_timer * 1000)),
        8: int(round(total_timer * 1000)),
        9: int(round(total_distance_m * 100)),
        10: int(round((avg_speed_ms or 0) * 1000)),
        11: int(round(max_speed_ms * 1000)),
        12: int(round((total_calories or 0))),
        13: int(round(avg_hr)) if avg_hr else None,
        14: int(round(max_hr)) if max_hr else None,
        15: int(round((total_calories or 0))),
        24: 1,   # running
        25: 6,   # treadmill (widely used sub_sport value)
    }
    fw.data_msg(3, common)
    fw.data_msg(4, {
        253: fit_time(end), 2: fit_time(start), 7: int(round(total_timer * 1000)),
        8: int(round(total_timer * 1000)), 9: int(round(total_distance_m * 100)),
        10: int(round((avg_speed_ms or 0) * 1000)), 11: int(round(max_speed_ms * 1000)),
        14: int(round(avg_hr)) if avg_hr else None, 15: int(round(max_hr)) if max_hr else None,
        16: int(round((total_calories or 0))), 5: 1, 6: 6, 0: 0,
    })
    fw.data_msg(5, {253: fit_time(end), 0: int(round(total_timer * 1000)), 1: 1, 2: 0, 3: 26, 4: 4})

    data = bytes(fw.data)
    header = struct.pack("<BBHI4s", 14, 16, 0, len(data), b".FIT")
    header_crc = fit_crc(header)
    file_without_crc = header + struct.pack("<H", header_crc) + data
    crc = fit_crc(file_without_crc)
    output = file_without_crc + struct.pack("<H", crc)
    if fit_path is None:
        fit_path = str(Path(csv_path).with_suffix(".fit"))
    Path(fit_path).write_bytes(output)
    return fit_path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: treadmill_fit_export.py workout.csv [summary.json] [output.fit]", file=sys.stderr)
        raise SystemExit(2)
    out = build_fit(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None, sys.argv[3] if len(sys.argv) > 3 else None)
    print(out)
