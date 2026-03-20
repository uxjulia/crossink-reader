#pragma once

// Persists and restores the system clock across reboots and deep sleep.
//
// Restoration priority:
//   1. LP timer delta (RTC_NOINIT_ATTR) — exact elapsed time after deep sleep wake,
//      no SD card needed. Accurate to ~1 second.
//   2. SD card backup (/.crosspoint/last_time.bin) — survives full power-off.
//      Restores to the last saved time (stale by however long the device was off).
//
// clockApproximate is true until a successful NTP sync confirms the time is accurate.
// Use this flag to show a "~" prefix before the clock in the UI.
//
// Usage:
//   Boot:        TimeStore::restore()       — after SD is ready
//                TimeStore::applyTimezone() — after SETTINGS are loaded
//   Deep sleep:  TimeStore::saveBeforeSleep()
//   NTP sync:    TimeStore::syncAndSave()

namespace TimeStore {

// True until a successful NTP sync confirms accuracy. Initially false (no time at all).
// Set to true after restore() if time was recovered (approximate). Cleared by syncAndSave().
extern bool clockApproximate;

// Restores the system clock. Tries LP timer delta first (deep sleep wake),
// then falls back to the SD card backup. No-op if neither source is valid.
// Call after Storage.begin() so the SD fallback path is available.
void restore();

// Saves the current unix timestamp + LP timer counter to RTC memory, and also
// persists the timestamp to /.crosspoint/last_time.bin on the SD card.
// Call just before esp_deep_sleep_start().
void saveBeforeSleep();

// Saves only the current unix timestamp to SD (no RTC memory update).
// Used after a successful NTP sync to update the SD backup.
void save();

// Synchronises the system clock via NTP (pool.ntp.org) and, on success,
// persists the timestamp with save() and clears clockApproximate.
// Blocks for up to 5 seconds. Requires an active WiFi connection.
// Returns true on success.
bool syncAndSave();

// Applies the user's timezone offset (SETTINGS.timezoneIndex) to the system
// TZ environment variable and calls tzset(). Call after settings are loaded
// and whenever the timezone setting changes.
void applyTimezone();

}  // namespace TimeStore
