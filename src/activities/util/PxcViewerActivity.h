#pragma once

#include <string>

#include "../Activity.h"
#include "MappedInputManager.h"

class PxcViewerActivity final : public Activity {
 public:
  PxcViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string filePath);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void onScreenshotRequest() override;

 private:
  std::string filePath;
  void renderGrayscaleImage();
};
