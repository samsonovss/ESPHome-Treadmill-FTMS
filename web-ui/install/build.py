"""Build the Pages USB installer from exact factory assets in one release."""

import json
from pathlib import Path
import re
from shutil import copyfile
import subprocess
import sys


VARIANTS = {
    "nextion": ("manifest.json", "Treadmill — Nextion"),
    "headless": ("manifest-headless.json", "Treadmill — No display"),
}


def select_release(releases):
    for release in sorted(releases, key=lambda r: r.get("published_at") or "", reverse=True):
        tag = release["tag_name"]
        if release.get("draft") or not re.fullmatch(r"fw-v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?", tag):
            continue
        version = tag.removeprefix("fw-v")
        prefix = f"treadmill-fw-{version}"
        assets = {asset["name"] for asset in release["assets"] if asset.get("state") == "uploaded"}
        builds = {variant: f"{prefix}-{variant}.factory.bin" for variant in VARIANTS}
        # The unqualified image in older releases was always the Nextion build.
        if builds["nextion"] not in assets:
            builds["nextion"] = f"{prefix}.factory.bin"
        available = {variant: name for variant, name in builds.items() if name in assets}
        if available:
            return tag, version, available
    return None


def write_json(path, value):
    path.write_text(json.dumps(value) + "\n", encoding="utf-8")


def build(output, releases, download):
    output.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "install.js"):
        copyfile(Path(__file__).with_name(name), output / name)
    release = select_release(releases)
    catalog = {"version": None, "variants": []}
    if release:
        tag, version, available = release
        catalog = {"version": version, "variants": list(available)}
        for variant, asset in available.items():
            download(tag, asset, output / asset)
            manifest, label = VARIANTS[variant]
            write_json(output / manifest, {
                "name": label,
                "version": version,
                "new_install_prompt_erase": True,
                "builds": [{"chipFamily": "ESP32-S3", "parts": [{"path": asset, "offset": 0}]}],
            })
    write_json(output / "variants.json", catalog)


def main(repo, output):
    # gh emits one JSON value per release here, also on CLI versions without --slurp.
    releases = subprocess.check_output([
        "gh", "api", "--paginate", f"repos/{repo}/releases?per_page=100", "--jq", ".[] | @json",
    ], text=True)

    def download(tag, asset, path):
        subprocess.run([
            "gh", "release", "download", tag, "--repo", repo, "--pattern", asset, "--output", str(path),
        ], check=True)

    build(Path(output), (json.loads(line) for line in releases.splitlines() if line.strip()), download)


if __name__ == "__main__":
    main(*sys.argv[1:])
