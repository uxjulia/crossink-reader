#pragma once
#include <cstdint>
#include <string>

// Per-book reading statistics, persisted to cachePath/stats.bin.
struct BookReadingStats {
  uint16_t sessionCount = 0;         // Total times this book was opened
  uint32_t totalReadingSeconds = 0;  // Accumulated reading time in seconds
  uint32_t totalPagesTurned = 0;     // Total page-turn actions (forward + backward)
  uint16_t currentStreak = 0;        // Consecutive days this book was read
  uint32_t lastReadTimestamp = 0;    // Unix timestamp of the last streak update

  // Updates the reading streak for today. Must be called once per session open.
  // Only applies if system time is valid (NTP-synced, year >= 2024); otherwise
  // the existing streak value is preserved unchanged.
  void updateStreak();

  // Loads stats from cachePath/stats.bin. Returns default-constructed stats if
  // the file is missing or the version byte does not match.
  static BookReadingStats load(const std::string& cachePath);

  // Saves stats to cachePath/stats.bin.
  void save(const std::string& cachePath) const;

  // Formats a duration in seconds into a human-readable string.
  // Output examples: "< 1 min", "45 min", "2h 30 min"
  static void formatDuration(uint32_t seconds, char* buf, size_t len);
};
