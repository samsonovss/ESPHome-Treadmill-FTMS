#!/usr/bin/env python3
"""CI check: the committed esphome/treadmill_dashboard.h must contain the page built from the current web-ui sources.
Compares the unpacked page, not the gzip bytes (different zlib builds may compress the same text differently)."""
import gzip, re, subprocess, sys
from pathlib import Path
p = Path(__file__).resolve().parent
h = p.parent/'esphome'/'treadmill_dashboard.h'
committed = h.read_text()
subprocess.run([sys.executable, str(p/'generate.py')], check=True, stdout=subprocess.DEVNULL)
built = (p/'generated'/'index.html').read_text()
h.write_text(committed)  # generate.py rewrote the header; keep the committed file untouched
arr = re.search(r'DASHBOARD_GZ\[\] = \{(.*?)\};', committed, re.S).group(1)
page = gzip.decompress(bytes(int(x) for x in arr.replace('\n', '').split(','))).decode('utf-8')
if page != built:
    sys.exit('esphome/treadmill_dashboard.h is out of date: run  cd web-ui && python3 generate.py  and commit the header')
print('treadmill_dashboard.h matches the web-ui sources')
