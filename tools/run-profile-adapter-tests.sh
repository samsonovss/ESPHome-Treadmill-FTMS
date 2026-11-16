#!/bin/sh
# Host-only harness. Pass ArduinoJson src directory as argument; no firmware build.
set -eu
json_src=${1:?ArduinoJson src directory required}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
work=$(mktemp -d /tmp/oc-profile-harness.XXXXXX)
# Keep bounded artifacts for inspection; do not remove other test runs.
mkdir -p "$work/stubs/esphome/components/json"
printf '#pragma once\n' > "$work/stubs/esphome/components/json/json_util.h"
printf '#pragma once\ninline unsigned esp_random(){return 1234567;}\n' > "$work/stubs/esp_random.h"
g++ -std=c++17 -Wall -Wextra -Werror "$root/tools/profile-core.test.cpp" -o "$work/core"
"$work/core"
g++ -std=c++20 -Wall -Wextra -Werror -pthread -I"$work/stubs" -I"$json_src" "$root/tools/profile-adapter.test.cpp" -o "$work/adapter"
"$work/adapter"
printf 'Host harness artifacts: %s\n' "$work"
