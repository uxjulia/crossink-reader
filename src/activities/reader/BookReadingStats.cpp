#include "BookReadingStats.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <time.h>

namespace {
// Binary layout version 2 (23 bytes):
//   [0]     version (= 2)
//   [1-2]   sessionCount        uint16_t LE
//   [3-6]   totalReadingSeconds uint32_t LE
//   [7-10]  totalPagesTurned    uint32_t LE
//   [11-14] lastReadUnixDay     uint32_t LE
//   [15-18] currentStreak       uint32_t LE
//   [19-22] maxStreak           uint32_t LE
static constexpr uint8_t STATS_FILE_VERSION = 2;
static constexpr int STATS_FILE_SIZE = 23;

// Minimum unix timestamp for a valid clock (2024-01-01 UTC).
static constexpr uint32_t VALID_EPOCH_MIN = 1704067200UL;
}  // namespace

BookReadingStats BookReadingStats::load(const std::string& cachePath) {
  BookReadingStats stats;
  FsFile f;
  if (!Storage.openFileForRead("STATS", cachePath + "/stats.bin", f)) {
    return stats;
  }
  uint8_t data[STATS_FILE_SIZE];
  const int n = f.read(data, STATS_FILE_SIZE);
  f.close();
  if (n != STATS_FILE_SIZE || data[0] != STATS_FILE_VERSION) {
    LOG_DBG("STATS", "Stats missing or version mismatch, starting fresh");
    return stats;
  }
  stats.sessionCount = static_cast<uint16_t>(data[1]) | (static_cast<uint16_t>(data[2]) << 8);
  stats.totalReadingSeconds = static_cast<uint32_t>(data[3]) | (static_cast<uint32_t>(data[4]) << 8) |
                              (static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 24);
  stats.totalPagesTurned = static_cast<uint32_t>(data[7]) | (static_cast<uint32_t>(data[8]) << 8) |
                           (static_cast<uint32_t>(data[9]) << 16) | (static_cast<uint32_t>(data[10]) << 24);
  stats.lastReadUnixDay = static_cast<uint32_t>(data[11]) | (static_cast<uint32_t>(data[12]) << 8) |
                          (static_cast<uint32_t>(data[13]) << 16) | (static_cast<uint32_t>(data[14]) << 24);
  stats.currentStreak = static_cast<uint32_t>(data[15]) | (static_cast<uint32_t>(data[16]) << 8) |
                        (static_cast<uint32_t>(data[17]) << 16) | (static_cast<uint32_t>(data[18]) << 24);
  stats.maxStreak = static_cast<uint32_t>(data[19]) | (static_cast<uint32_t>(data[20]) << 8) |
                    (static_cast<uint32_t>(data[21]) << 16) | (static_cast<uint32_t>(data[22]) << 24);
  return stats;
}

void BookReadingStats::updateStreak() {
  const time_t now = time(nullptr);
  if (static_cast<uint32_t>(now) < VALID_EPOCH_MIN) {
    return;  // Clock not valid — no streak tracking until time is synced
  }
  const uint32_t today = static_cast<uint32_t>(now / 86400);

  if (lastReadUnixDay == 0) {
    // First session with a valid clock
    currentStreak = 1;
    lastReadUnixDay = today;
  } else if (today == lastReadUnixDay) {
    // Same day, streak unchanged
  } else if (today == lastReadUnixDay + 1) {
    // Consecutive day
    currentStreak++;
    lastReadUnixDay = today;
  } else {
    // Streak broken (gap of 2+ days)
    currentStreak = 1;
    lastReadUnixDay = today;
  }

  if (currentStreak > maxStreak) {
    maxStreak = currentStreak;
  }
}

void BookReadingStats::formatDuration(uint32_t seconds, char* buf, size_t len) {
  if (seconds < 60) {
    snprintf(buf, len, "%s", tr(STR_STATS_LESS_THAN_MIN));
    return;
  }
  const uint32_t hours = seconds / 3600;
  const uint32_t minutes = (seconds % 3600) / 60;
  if (hours == 0) {
    snprintf(buf, len, "%lu min", static_cast<unsigned long>(minutes));
  } else {
    snprintf(buf, len, "%luh %lu min", static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes));
  }
}

void BookReadingStats::save(const std::string& cachePath) const {
  FsFile f;
  if (!Storage.openFileForWrite("STATS", cachePath + "/stats.bin", f)) {
    LOG_ERR("STATS", "Could not write stats.bin");
    return;
  }
  uint8_t data[STATS_FILE_SIZE];
  data[0] = STATS_FILE_VERSION;
  data[1] = sessionCount & 0xFF;
  data[2] = (sessionCount >> 8) & 0xFF;
  data[3] = totalReadingSeconds & 0xFF;
  data[4] = (totalReadingSeconds >> 8) & 0xFF;
  data[5] = (totalReadingSeconds >> 16) & 0xFF;
  data[6] = (totalReadingSeconds >> 24) & 0xFF;
  data[7] = totalPagesTurned & 0xFF;
  data[8] = (totalPagesTurned >> 8) & 0xFF;
  data[9] = (totalPagesTurned >> 16) & 0xFF;
  data[10] = (totalPagesTurned >> 24) & 0xFF;
  data[11] = lastReadUnixDay & 0xFF;
  data[12] = (lastReadUnixDay >> 8) & 0xFF;
  data[13] = (lastReadUnixDay >> 16) & 0xFF;
  data[14] = (lastReadUnixDay >> 24) & 0xFF;
  data[15] = currentStreak & 0xFF;
  data[16] = (currentStreak >> 8) & 0xFF;
  data[17] = (currentStreak >> 16) & 0xFF;
  data[18] = (currentStreak >> 24) & 0xFF;
  data[19] = maxStreak & 0xFF;
  data[20] = (maxStreak >> 8) & 0xFF;
  data[21] = (maxStreak >> 16) & 0xFF;
  data[22] = (maxStreak >> 24) & 0xFF;
  f.write(data, STATS_FILE_SIZE);
  f.close();
}
