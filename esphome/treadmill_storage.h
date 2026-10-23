#pragma once
// LittleFS foundation only: no recorder, history, editor, HTTP handler or autoformat.
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace treadmill_storage {
inline constexpr const char *TAG = "treadmill_storage";
inline constexpr const char *PARTITION = "workouts_archive";
inline constexpr const char *BASE_PATH = "/workouts";
inline constexpr const char *PROBE_PATH = "/workouts/.persistence-v1";
inline constexpr const char PROBE[] = "treadmill-littlefs-persistence-v1\n";
inline constexpr size_t ARCHIVE_BYTES = 0x700000;
inline bool mounted = false;
inline std::string status = "not_mounted";
inline std::string self_test_status = "not_run";
inline size_t total_bytes = 0, used_bytes = 0;
inline bool capacity_valid = false;

inline bool refresh_capacity() {
  capacity_valid = mounted &&
      esp_littlefs_info(PARTITION, &total_bytes, &used_bytes) == ESP_OK &&
      used_bytes <= total_bytes;
  if (!capacity_valid) total_bytes = used_bytes = 0;
  return capacity_valid;
}
inline float total() { return refresh_capacity() ? float(total_bytes) : NAN; }
inline float used() { return refresh_capacity() ? float(used_bytes) : NAN; }
inline float free() { return refresh_capacity() ? float(total_bytes - used_bytes) : NAN; }

// Fixed, tiny file. Never repair/overwrite an existing damaged or partial probe.
// Existing file is read once per boot. A new file is created exclusively once.
inline bool verify_probe() {
  FILE *file = fopen(PROBE_PATH, "rb");
  if (!file) { self_test_status = "read_open_failed"; return false; }
  char data[sizeof(PROBE)]{};
  const size_t count = fread(data, 1, sizeof(data), file);
  const bool ok = count == sizeof(PROBE) - 1 &&
                  memcmp(data, PROBE, sizeof(PROBE) - 1) == 0 && !ferror(file);
  const bool closed = fclose(file) == 0;
  self_test_status = ok && closed ? "retained_verified" : "verify_failed_no_rewrite";
  return ok && closed;
}
inline void persistence_self_test() {
  if (!mounted) { self_test_status = "not_mounted"; return; }
  struct stat st{};
  if (stat(PROBE_PATH, &st) == 0) { verify_probe(); return; }
  if (errno != ENOENT) { self_test_status = "stat_failed_no_write"; return; }
  const int fd = open(PROBE_PATH, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) { self_test_status = "create_failed"; return; }
  const bool written = write(fd, PROBE, sizeof(PROBE) - 1) == ssize_t(sizeof(PROBE) - 1);
  const bool synced = fsync(fd) == 0;
  const bool closed = close(fd) == 0;
  if (!written || !synced || !closed) {
    self_test_status = "write_failed_no_rewrite";
    return;
  }
  if (verify_probe()) self_test_status = "created_verified_reboot_to_prove";
}
inline bool mount() {
  if (mounted) return true;
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, PARTITION);
  if (!partition) { status = "partition_missing_usb_factory_required"; return false; }
  if (partition->size != ARCHIVE_BYTES) { status = "partition_size_mismatch"; return false; }
  esp_vfs_littlefs_conf_t config{};
  config.base_path = BASE_PATH;
  config.partition_label = PARTITION;
  config.format_if_mount_failed = false; // Never destroy data on a mount failure.
  config.dont_mount = false;
  const esp_err_t result = esp_vfs_littlefs_register(&config);
  if (result != ESP_OK) {
    status = "mount_failed_init_blank_only_or_recover:" + std::string(esp_err_to_name(result));
    ESP_LOGW(TAG, "%s; no automatic format", status.c_str());
    return false;
  }
  mounted = true;
  status = "mounted";
  persistence_self_test();
  if (refresh_capacity())
    ESP_LOGI(TAG, "mounted total=%u used=%u free=%u test=%s",
             unsigned(total_bytes), unsigned(used_bytes), unsigned(total_bytes-used_bytes),
             self_test_status.c_str());
  else ESP_LOGW(TAG, "mounted_capacity_unavailable");
  return true;
}

// Only the deliberate diagnostic button calls this. STOP guard is supplied from
// real firmware globals. Full partition inspection, not a misleading sample.
// Nonblank/corrupt/previously formatted partitions can NEVER be formatted here.
inline bool initialize_blank(bool stopped) {
  if (!stopped) { status = "init_refused_not_stopped"; return false; }
  if (mounted) { status = "mounted_init_not_needed"; return false; }
  if (mount()) return true; // Valid old filesystem always wins, without erase.
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, PARTITION);
  if (!partition || partition->size != ARCHIVE_BYTES) return false;
  uint8_t chunk[4096];
  for (size_t offset = 0; offset < partition->size; offset += sizeof(chunk)) {
    if (esp_partition_read(partition, offset, chunk, sizeof(chunk)) != ESP_OK) {
      status = "init_refused_partition_read_failed"; return false;
    }
    for (uint8_t byte : chunk) {
      if (byte != 0xff) { status = "init_refused_nonblank_recover_do_not_erase"; return false; }
    }
    esphome::App.feed_wdt();
    if ((offset / sizeof(chunk)) % 16 == 0) vTaskDelay(1);
  }
  // Explicit operation, only after all 7 MiB have been verified erased.
  const esp_err_t result = esp_littlefs_format(PARTITION);
  if (result != ESP_OK) {
    status = "init_format_failed:" + std::string(esp_err_to_name(result)); return false;
  }
  return mount();
}
} // namespace treadmill_storage
