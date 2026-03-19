#pragma once

// Persists and restores the system clock across reboots via the SD card.
// This allows date-aware features (e.g. reading streak) to work correctly
// without requiring a fresh NTP sync on every boot.
//
// Usage:
//   - Call TimeStore::restore() once at boot after the SD card is ready.
//   - Call TimeStore::save() after a successful NTP sync.
namespace TimeStore {

// Restores the system clock from the last saved timestamp.
// No-op if no saved time exists or the saved time predates 2024.
void restore();

// Saves the current system time to /.crosspoint/last_time.bin.
// No-op if the current time is not valid (year < 2024 — not yet NTP-synced).
void save();

// Synchronizes the system clock via NTP (pool.ntp.org) and, on success,
// persists the timestamp with save(). Requires an active WiFi connection.
// Blocks for up to 5 seconds waiting for sync. Returns true on success.
bool syncAndSave();

// Applies the user's timezone offset from SETTINGS.timezoneIndex to the
// system TZ environment variable and calls tzset(). Call at boot (after
// settings are loaded) and after the timezone setting changes.
void applyTimezone();

}  // namespace TimeStore
