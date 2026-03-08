#pragma once

#include <functional>

#include "activities/Activity.h"

class ClearLibraryCacheActivity final : public Activity {
 public:
  explicit ClearLibraryCacheActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ClearLibraryCache", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { WARNING, CLEARING, SUCCESS, FAILED };

  State state = WARNING;

  void goBack() { finish(); }

  void clearLibraryCache();
};
