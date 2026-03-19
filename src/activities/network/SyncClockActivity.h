#pragma once
#include "../Activity.h"

// Connects to WiFi, syncs the system clock via NTP, and persists the timestamp
// so that date-aware features (e.g. reading streak) work across reboots.
class SyncClockActivity final : public Activity {
  enum class State { WIFI_SELECTION, SYNCING, DONE, FAILED };
  State state = State::WIFI_SELECTION;

  void onWifiSelectionComplete(bool connected);

 public:
  explicit SyncClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SyncClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::SYNCING; }
};
