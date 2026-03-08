#include "ClearLibraryCacheActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ClearLibraryCacheActivity::onEnter() {
  Activity::onEnter();

  state = WARNING;
  requestUpdate();
}

void ClearLibraryCacheActivity::onExit() { Activity::onExit(); }

void ClearLibraryCacheActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                tr(STR_CLEAR_LIBRARY_CACHE));

  if (state == WARNING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 50, tr(STR_CLEAR_LIBRARY_WARNING_1), true);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CLEAR_LIBRARY_WARNING_2), true);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CLEAR_LIBRARY_WARNING_3), true);

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_CLEAR_BUTTON), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == CLEARING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CLEARING_CACHE));
    renderer.displayBuffer();
    return;
  }

  if (state == SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_LIBRARY_CACHE_CLEARED), true,
                             EpdFontFamily::BOLD);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CLEAR_CACHE_FAILED), true,
                             EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_CHECK_SERIAL_OUTPUT));

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

namespace {
constexpr char LIBRARY_FILE_JSON[] = "/.crosspoint/library.json";
}  // namespace

void ClearLibraryCacheActivity::clearLibraryCache() {
  LOG_DBG("CLEAR_LIBRARY", "Clearing library cache...");

  // Check if file exists first
  if (!Storage.exists(LIBRARY_FILE_JSON)) {
    LOG_DBG("CLEAR_LIBRARY", "Library cache file doesn't exist, treating as success");
    state = SUCCESS;
    requestUpdate();
    return;
  }

  // Try to remove the file
  bool result = Storage.remove(LIBRARY_FILE_JSON);

  // Verify removal with Storage.exists()
  if (result && !Storage.exists(LIBRARY_FILE_JSON)) {
    LOG_DBG("CLEAR_LIBRARY", "Library cache cleared successfully");
    state = SUCCESS;
  } else {
    LOG_ERR("CLEAR_LIBRARY", "Failed to clear library cache");
    state = FAILED;
  }

  requestUpdate();
}

void ClearLibraryCacheActivity::loop() {
  if (state == WARNING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      LOG_DBG("CLEAR_LIBRARY", "User confirmed, starting cache clear");
      {
        RenderLock lock(*this);
        state = CLEARING;
      }
      requestUpdateAndWait();

      clearLibraryCache();
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      LOG_DBG("CLEAR_LIBRARY", "User cancelled");
      goBack();
    }
    return;
  }

  if (state == SUCCESS || state == FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      goBack();
    }
    return;
  }
}
