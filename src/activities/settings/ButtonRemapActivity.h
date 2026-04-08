#pragma once

#include <functional>
#include <string>

#include "activities/Activity.h"

class ButtonRemapActivity final : public Activity {
 public:
  // isReaderMode = true  → saves to reader-specific front button fields
  // isReaderMode = false → saves to system-wide front button fields (default)
  explicit ButtonRemapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool isReaderMode = false)
      : Activity("ButtonRemap", renderer, mappedInput), readerMode(isReaderMode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool readerMode;

  // Index of the logical role currently awaiting input.
  uint8_t currentStep = 0;
  // Temporary mapping from logical role -> hardware button index.
  uint8_t tempMapping[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  // Error banner timing (used when reassigning duplicate buttons).
  unsigned long errorUntil = 0;
  std::string errorMessage;

  // Commit temporary mapping to settings.
  void applyTempMapping();
  // Returns false if a hardware button is already assigned to a different role.
  bool validateUnassigned(uint8_t pressedButton);
  // Labels for UI display.
  const char* getRoleName(uint8_t roleIndex) const;
  const char* getHardwareName(uint8_t buttonIndex) const;
};
