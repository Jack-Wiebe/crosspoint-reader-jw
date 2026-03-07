#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../../LibraryStore.h"
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class LibraryViewerActivity final : public Activity {
  private:
    ButtonNavigator buttonNavigator;

    size_t selectorIndex = 0;
    size_t currentPage = 0;
    bool isLoading = false;
    bool isFirstLoad = false;

    std::vector<LibraryBook> books;
    std::vector<std::string> bookPaths;

    void scanBookPaths();
    void loadPage(size_t page);
    void loadBooks();

  public:
    explicit LibraryViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
        : Activity("LibraryViewer", renderer, mappedInput) {}
    void onEnter() override;
    void onExit() override;
    void loop() override;
    void render(RenderLock&&) override;
};
