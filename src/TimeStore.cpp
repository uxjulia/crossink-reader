#include "TimeStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

#include "CrossPointSettings.h"

namespace {
// Binary layout (5 bytes):
//   [0]   version (= 1)
//   [1-4] Unix timestamp  uint32_t LE
static constexpr uint8_t TIME_FILE_VERSION = 1;
static constexpr int TIME_FILE_SIZE = 5;
static constexpr char TIME_FILE[] = "/.crosspoint/last_time.bin";

// Minimum Unix timestamp considered valid: 2024-01-01 00:00:00 UTC.
// Without NTP, the ESP32 starts near epoch 0.
static constexpr uint32_t VALID_EPOCH_MIN = 1704067200UL;
}  // namespace

void TimeStore::restore() {
  FsFile f;
  if (!Storage.openFileForRead("TIME", TIME_FILE, f)) {
    return;
  }
  uint8_t data[TIME_FILE_SIZE];
  const int n = f.read(data, TIME_FILE_SIZE);
  f.close();
  if (n != TIME_FILE_SIZE || data[0] != TIME_FILE_VERSION) {
    return;
  }
  const uint32_t saved = static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
                         (static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);
  if (saved < VALID_EPOCH_MIN) {
    return;
  }
  struct timeval tv = {static_cast<time_t>(saved), 0};
  settimeofday(&tv, nullptr);
  LOG_DBG("TIME", "Restored timestamp: %lu", static_cast<unsigned long>(saved));
}

bool TimeStore::syncAndSave() {
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();

  int retry = 0;
  constexpr int maxRetries = 50;  // 5 seconds max
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < maxRetries) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    retry++;
  }

  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }

  if (retry < maxRetries) {
    LOG_DBG("TIME", "NTP sync successful");
    save();
    return true;
  }

  LOG_DBG("TIME", "NTP sync timeout");
  return false;
}

void TimeStore::applyTimezone() {
  // timezoneIndex: 0 = UTC-12, 12 = UTC+0, 26 = UTC+14
  // POSIX TZ offset sign is inverted: POSIX "UTC-N" = N hours east = UTC+N
  const int posixOffset = 12 - static_cast<int>(SETTINGS.timezoneIndex);
  char tz[12];
  snprintf(tz, sizeof(tz), "UTC%d", posixOffset);
  setenv("TZ", tz, 1);
  tzset();
  LOG_DBG("TIME", "Timezone set: %s (index=%d)", tz, SETTINGS.timezoneIndex);
}

void TimeStore::save() {
  const auto now = static_cast<uint32_t>(time(nullptr));
  if (now < VALID_EPOCH_MIN) {
    return;
  }
  Storage.mkdir("/.crosspoint");
  FsFile f;
  if (!Storage.openFileForWrite("TIME", TIME_FILE, f)) {
    LOG_ERR("TIME", "Could not write last_time.bin");
    return;
  }
  uint8_t data[TIME_FILE_SIZE];
  data[0] = TIME_FILE_VERSION;
  data[1] = now & 0xFF;
  data[2] = (now >> 8) & 0xFF;
  data[3] = (now >> 16) & 0xFF;
  data[4] = (now >> 24) & 0xFF;
  f.write(data, TIME_FILE_SIZE);
  f.close();
  LOG_DBG("TIME", "Saved timestamp: %lu", static_cast<unsigned long>(now));
}
