import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from build import build, select_release


def release(version, suffixes, *, date="2026-11-20", draft=False):
    return {
        "tag_name": f"fw-v{version}", "published_at": date, "draft": draft,
        "assets": [{"name": f"treadmill-fw-{version}{suffix}", "state": "uploaded"} for suffix in suffixes],
    }


class InstallerTests(unittest.TestCase):
    def test_two_variants_use_exact_factory_assets(self):
        latest = release("1.0.0-beta.13", ["-nextion.factory.bin", "-headless.factory.bin", "-nextion.ota.bin", "-headless.ota.bin"])
        old = release("1.0.0-beta.12", [".factory.bin"], date="2026-11-19")
        with TemporaryDirectory() as directory:
            output = Path(directory)
            downloads = []

            def download(tag, name, path):
                downloads.append((tag, name))
                path.write_text(name)

            build(output, [old, latest], download)
            catalog = json.loads((output / "variants.json").read_text())
            self.assertEqual(catalog, {"version": "1.0.0-beta.13", "variants": ["nextion", "headless"]})
            html = (output / "index.html").read_text()
            for variant, path in [("nextion", "manifest.json"), ("headless", "manifest-headless.json")]:
                manifest = json.loads((output / path).read_text())
                filename = f"treadmill-fw-1.0.0-beta.13-{variant}.factory.bin"
                self.assertIn(f'manifest="{path}"', html)
                self.assertEqual(manifest["builds"], [{"chipFamily": "ESP32-S3", "parts": [{"path": filename, "offset": 0}]}])
                self.assertEqual((output / filename).read_text(), filename)
            self.assertEqual(downloads, [("fw-v1.0.0-beta.13", f"treadmill-fw-1.0.0-beta.13-{variant}.factory.bin") for variant in ("nextion", "headless")])

    def test_legacy_release_and_ineligible_assets(self):
        legacy = release("1.0.0-beta.12", [".factory.bin"], date="2026-11-18")
        draft = release("1.0.0-beta.14", ["-nextion.factory.bin"], draft=True)
        other = release("1.0.0-beta.13", ["-unknown.factory.bin", "-headless.ota.bin"])
        incomplete = release("1.0.0-beta.13", ["-headless.factory.bin"])
        incomplete["assets"][0]["state"] = "starter"
        self.assertEqual(select_release([draft, other, legacy, incomplete]), (
            "fw-v1.0.0-beta.12", "1.0.0-beta.12", {"nextion": "treadmill-fw-1.0.0-beta.12.factory.bin"},
        ))
        self.assertIsNone(select_release([draft, other, incomplete]))

    def test_never_mix_releases(self):
        latest = release("1.0.0-beta.13", ["-headless.factory.bin"])
        old = release("1.0.0-beta.12", [".factory.bin"], date="2026-11-19")
        self.assertEqual(select_release([latest, old])[2], {"headless": "treadmill-fw-1.0.0-beta.13-headless.factory.bin"})


if __name__ == "__main__":
    unittest.main()
