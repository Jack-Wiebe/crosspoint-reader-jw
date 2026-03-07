#include "LibraryViewerActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"

void LibraryViewerActivity::loadBooks() {
  books.clear();

  //TODO: Investigate using a store instead of grabbing from storage everytime
  auto root = Storage.open("/");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  root.rewindDirectory();

  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));

    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      file.close();
      continue;
    }

    if (!file.isDirectory() && StringUtils::checkFileExtension(std::string(name), ".epub")) {
      std::string path = "/" + std::string(name);

      Epub epub(path, "/.crosspoint");
      bool loaded = epub.load(false, true);

      // Generate thumbnail for cover if it doesn't exist
      if (loaded && !epub.getCoverBmpPath().empty()) {
        epub.generateThumbBmp(70); // 70px height for library cover
      }

      LibraryBook book;
      book.path = path;
      book.title = loaded ? epub.getTitle() : StringUtils::getFileNameWithoutExtension(name);
      book.author = loaded ? epub.getAuthor() : "NO AUTHOR FOUND";
      book.coverBmpPath = loaded ? epub.getThumbBmpPath() : "";

      books.push_back(book);
    }
    file.close();
  }
  root.close();

  std::sort(books.begin(), books.end(), [](const LibraryBook& a, const LibraryBook& b) {
    if (a.author != b.author)
        return a.author < b.author;
    return a.title < b.title;
  });
}

void LibraryViewerActivity::onEnter() {
  Activity::onEnter();

  loadBooks();
  selectorIndex = 0;

  requestUpdate();
}

void LibraryViewerActivity::onExit() {
  Activity::onExit();
  books.clear();
}

void LibraryViewerActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageItems = metrics.libraryItemsPerPage;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!books.empty() && selectorIndex < books.size()) {
      onSelectBook(books[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(books.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void LibraryViewerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_LIBRARY));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (books.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOKS_FOUND));
  } else {
    GUI.drawListWithCover(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, books.size(), selectorIndex,
        [this](int index) { return books[index].title; },
        [this](int index) { return books[index].author; },
        [this](int index) { return books[index].coverBmpPath; });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
