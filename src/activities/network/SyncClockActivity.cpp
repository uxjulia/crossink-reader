#include "SyncClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "TimeStore.h"
#include "WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SyncClockActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();

  if (WiFi.status() != WL_CONNECTED) {
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                           [this](const ActivityResult& result) {
                             onWifiSelectionComplete(!result.isCancelled);
                           });
  } else {
    onWifiSelectionComplete(true);
  }
}

void SyncClockActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    finish();
    return;
  }

  {
    RenderLock lock(*this);
    state = State::SYNCING;
  }
  requestUpdate(true);

  state = TimeStore::syncAndSave() ? State::DONE : State::FAILED;
  requestUpdate(true);
}

void SyncClockActivity::loop() {
  if (state == State::DONE || state == State::FAILED) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      finish();
    }
  }
}

void SyncClockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, w, metrics.headerHeight}, tr(STR_SYNC_CLOCK));

  const char* msg = "";
  switch (state) {
    case State::SYNCING:
      msg = tr(STR_SYNCING_TIME);
      break;
    case State::DONE:
      msg = tr(STR_CLOCK_SYNCED);
      break;
    case State::FAILED:
      msg = tr(STR_SYNC_FAILED_MSG);
      break;
    default:
      break;
  }

  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int msgW = renderer.getTextWidth(UI_12_FONT_ID, msg);
  renderer.drawText(UI_12_FONT_ID, (w - msgW) / 2, (h - lineH) / 2, msg, true);

  if (state == State::DONE || state == State::FAILED) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
