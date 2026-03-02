#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct LibraryBook {
  std::string path;
  std::string title;
  std::string author;
  std::string coverBmpPath;
};

class LibraryViewerActivity final : public Activity {
  private:
    ButtonNavigator buttonNavigator;

    size_t selectorIndex = 0;

    std::vector<LibraryBook> books;

    void loadBooks();

  public:
    explicit LibraryViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
        : Activity("LibraryViewer", renderer, mappedInput) {}
    void onEnter() override;
    void onExit() override;
    void loop() override;
    void render(RenderLock&&) override;
};
