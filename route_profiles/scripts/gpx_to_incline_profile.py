#!/usr/bin/env python3

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Convert GPX distance/elevation waypoints into an ESPHome "
            "treadmill incline profile."
        )
    )
    parser.add_argument("input", type=Path, help="source GPX file")
    parser.add_argument(
        "--name",
        help="C++ profile name without the incline_profile_ prefix",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="output header path (default: INPUT.profile.h)",
    )
    parser.add_argument(
        "--maximum-level",
        type=int,
        default=15,
        help="maximum treadmill incline level (default: 15)",
    )
    parser.add_argument(
        "--real-grade-at-maximum",
        type=float,
        default=5.0,
        help="measured real grade at maximum level, in percent (default: 5)",
    )
    return parser.parse_args()


def sanitize_identifier(value: str) -> str:
    identifier = re.sub(r"[^A-Za-z0-9_]+", "_", value).strip("_")
    if not identifier:
        raise ValueError("profile name does not contain usable characters")
    if identifier[0].isdigit():
        identifier = f"route_{identifier}"
    return identifier


def parse_distance(name: str) -> float:
    match = re.match(r"\s*(-?\d+(?:[.,]\d+)?)", name)
    if match is None:
        raise ValueError(f"cannot read distance from waypoint name: {name!r}")
    return float(match.group(1).replace(",", "."))


def load_waypoints(path: Path) -> list[tuple[float, float]]:
    root = ET.parse(path).getroot()
    points: list[tuple[float, float]] = []

    for waypoint in root.findall(".//{*}wpt"):
        name_element = waypoint.find("{*}name")
        elevation_element = waypoint.find("{*}ele")
        if (
            name_element is None
            or elevation_element is None
            or not name_element.text
            or not elevation_element.text
            or "distance" not in name_element.text.lower()
        ):
            continue

        points.append(
            (
                parse_distance(name_element.text),
                float(elevation_element.text),
            )
        )

    if len(points) < 2:
        raise ValueError(
            "the GPX file must contain at least two distance/elevation waypoints"
        )

    for previous, current in zip(points, points[1:]):
        if current[0] <= previous[0]:
            raise ValueError("waypoint distances must be strictly increasing")

    return points


def build_profile(
    points: list[tuple[float, float]],
    maximum_level: int,
    real_grade_at_maximum: float,
) -> list[tuple[float, int]]:
    if maximum_level <= 0:
        raise ValueError("maximum level must be greater than zero")
    if real_grade_at_maximum <= 0:
        raise ValueError("real grade at maximum must be greater than zero")

    profile: list[tuple[float, int]] = []
    for (distance, elevation), (next_distance, next_elevation) in zip(
        points, points[1:]
    ):
        grade = (next_elevation - elevation) / (next_distance - distance) * 100.0
        level = round(grade * maximum_level / real_grade_at_maximum)
        level = max(0, min(maximum_level, level))
        profile.append((distance / 1000.0, level))

    profile.append((points[-1][0] / 1000.0, 0))
    return profile


def render_profile(name: str, profile: list[tuple[float, int]]) -> str:
    lines = [
        f"const float incline_profile_{name}[{len(profile)}][2] PROGMEM = {{"
    ]
    for distance, level in profile:
        lines.append(f"  {{{distance:.3f}, {level}}},")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    output = args.output or args.input.with_suffix(".profile.h")
    name = sanitize_identifier(args.name or args.input.stem)

    try:
        points = load_waypoints(args.input)
        profile = build_profile(
            points,
            args.maximum_level,
            args.real_grade_at_maximum,
        )
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(render_profile(name, profile), encoding="utf-8")
    except (OSError, ET.ParseError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"Generated {output} with {len(profile)} profile points")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
