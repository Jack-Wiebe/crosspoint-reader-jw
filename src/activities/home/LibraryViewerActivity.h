#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../../LibraryStore.h"
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class LibraryViewerActivity final : public Activity {
  private:
    static constexpr size_t BATCH_SIZE = 5;
    ButtonNavigator buttonNavigator;

    size_t selectorIndex = 0;
    size_t currentPage = 0;
    size_t displayStart = 0;
    bool isLoading = false;

    size_t loadingEnd = 0;
    size_t loadingIndex = 0;

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
