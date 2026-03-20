#include "TimeStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

// esp_clk_rtc_time() returns microseconds since original boot, continuing through deep sleep.
// The header (esp_private/esp_clk.h) is not exposed by Arduino-ESP32, so we declare it directly.
extern "C" uint64_t esp_clk_rtc_time(void);

#include "CrossPointSettings.h"

// RTC_NOINIT_ATTR: persists across deep sleep without being zeroed on reset.
// These are written by saveBeforeSleep() and read by restore() on wake.
RTC_NOINIT_ATTR static uint32_t s_unixBeforeSleep;   // Unix timestamp at sleep entry
RTC_NOINIT_ATTR static uint64_t s_rtcUsBeforeSleep;  // LP timer counter at sleep entry

namespace {
// Binary layout of /.crosspoint/last_time.bin (5 bytes):
//   [0]   version (= 1)
//   [1-4] Unix timestamp uint32_t LE
static constexpr uint8_t TIME_FILE_VERSION = 1;
static constexpr int TIME_FILE_SIZE = 5;
static constexpr char TIME_FILE[] = "/.crosspoint/last_time.bin";

// Minimum unix timestamp considered valid: 2024-01-01 00:00:00 UTC.
// Without NTP, the ESP32 starts near epoch 0.
static constexpr uint32_t VALID_EPOCH_MIN = 1704067200UL;
}  // namespace

namespace TimeStore {

bool clockApproximate = false;

void restore() {
  // --- Tier 1: LP timer delta (deep sleep wake) ---
  // esp_clk_rtc_time() counts microseconds since original boot and keeps
  // ticking through deep sleep. On wake we compute the exact elapsed time.
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP && s_unixBeforeSleep >= VALID_EPOCH_MIN) {
    const uint64_t elapsedUs = esp_clk_rtc_time() - s_rtcUsBeforeSleep;
    const uint32_t newUnix = s_unixBeforeSleep + static_cast<uint32_t>(elapsedUs / 1000000ULL);
    struct timeval tv = {static_cast<time_t>(newUnix), 0};
    settimeofday(&tv, nullptr);
    clockApproximate = true;
    LOG_DBG("TIME", "LP timer restore: base=%lu elapsed=%llus -> %lu", s_unixBeforeSleep,
            elapsedUs / 1000000ULL, newUnix);
    return;
  }

  // --- Tier 2: SD card backup (power-off recovery / first boot after NTP) ---
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
  clockApproximate = true;
  LOG_DBG("TIME", "SD backup restore: %lu", static_cast<unsigned long>(saved));
}

void saveBeforeSleep() {
  s_unixBeforeSleep = static_cast<uint32_t>(time(nullptr));
  s_rtcUsBeforeSleep = esp_clk_rtc_time();
  save();  // Also persist to SD so power-off is recoverable
  LOG_DBG("TIME", "Saved before sleep: unix=%lu rtc_us=%llu", s_unixBeforeSleep, s_rtcUsBeforeSleep);
}

void save() {
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
  LOG_DBG("TIME", "Saved to SD: %lu", static_cast<unsigned long>(now));
}

bool syncAndSave() {
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
    clockApproximate = false;
    save();
    return true;
  }

  LOG_DBG("TIME", "NTP sync timeout");
  return false;
}

void applyTimezone() {
  // SETTINGS.timezoneIndex: 0 = UTC-12, 12 = UTC+0, 26 = UTC+14 (whole hours only)
  // POSIX TZ sign is inverted from ISO 8601: "UTC-N" means N hours east of UTC (= UTC+N).
  const int posixOffset = 12 - static_cast<int>(SETTINGS.timezoneIndex);
  char tz[12];
  snprintf(tz, sizeof(tz), "UTC%d", posixOffset);
  setenv("TZ", tz, 1);
  tzset();
  LOG_DBG("TIME", "Timezone: %s (index=%d)", tz, SETTINGS.timezoneIndex);
}

}  // namespace TimeStore
