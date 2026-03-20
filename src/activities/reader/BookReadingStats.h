#pragma once
#include <cstdint>
#include <string>

// Per-book reading statistics, persisted to cachePath/stats.bin.
struct BookReadingStats {
  uint16_t sessionCount = 0;         // Total times this book was opened
  uint32_t totalReadingSeconds = 0;  // Accumulated reading time in seconds
  uint32_t totalPagesTurned = 0;     // Total page-turn actions (forward + backward)

  // Reading streak fields (require a valid system clock — see TimeStore).
  // lastReadUnixDay is time(nullptr)/86400 for the last day this book was opened.
  // Zero means the streak has never been recorded (no valid clock on those sessions).
  uint32_t lastReadUnixDay = 0;  // Unix day of last session
  uint32_t currentStreak = 0;   // Consecutive days read (including today)
  uint32_t maxStreak = 0;       // All-time best streak

  // Loads stats from cachePath/stats.bin. Returns default-constructed stats if
  // the file is missing or the version byte does not match.
  static BookReadingStats load(const std::string& cachePath);

  // Saves stats to cachePath/stats.bin.
  void save(const std::string& cachePath) const;

  // Updates the reading streak based on the current date. No-op if the system
  // clock is not valid (time < 2024-01-01). Call once per session on book open.
  void updateStreak();

  // Formats a duration in seconds into a human-readable string.
  // Output examples: "< 1 min", "45 min", "2h 30 min"
  static void formatDuration(uint32_t seconds, char* buf, size_t len);
};
