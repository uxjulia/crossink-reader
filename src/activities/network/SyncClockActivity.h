#pragma once

#include "activities/Activity.h"

class SyncClockActivity : public Activity {
 public:
  SyncClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SyncClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { IDLE, SYNCING, DONE, FAILED };
  State state = State::IDLE;

  void onWifiSelectionComplete(bool connected);
};
