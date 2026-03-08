#include "LibraryViewerActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "LibraryStore.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"

void LibraryViewerActivity::scanBookPaths() {
  bookPaths.clear();

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
      bookPaths.push_back(path);
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

void LibraryViewerActivity::loadPage(size_t page) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const size_t itemsPerPage = metrics.libraryItemsPerPage;

  size_t start = page * itemsPerPage;
  size_t end = std::min(start + itemsPerPage, bookPaths.size());

  books.clear();

  for (size_t i = start; i < end; i++) {
    const std::string& path = bookPaths[i];

    Epub epub(path, "/.crosspoint");
    bool loaded = epub.load(true, true);

    LibraryBook book;
    book.path = path;
    book.title = StringUtils::getFileNameWithoutExtension(path);
    book.author = loaded ? epub.getAuthor() : "";
    book.coverBmpPath = loaded ? epub.getThumbBmpPath() : "";

    books.push_back(book);
  }

  LIBRARY.setBookPaths(bookPaths);
  LIBRARY.setBooks(books);
}

void LibraryViewerActivity::loadBooks() {
  isLoading = true;
  requestUpdate();

  // Try loading from store first
  if (!LIBRARY.loadFromFile()) {
    // No cache - scan and save paths
    scanBookPaths();
    LIBRARY.setBookPaths(bookPaths);
    LIBRARY.saveToFile();
  } else {
    // Load from cache
    books = LIBRARY.getBooks();
    bookPaths = LIBRARY.getBookPaths();
  }

  // Check if we need to rescan (new books added)
  scanBookPaths();
  if (bookPaths.size() != LIBRARY.getPathCount()) {
    isFirstLoad = true;
  }

  if (isFirstLoad || books.empty()) {
    loadPage(0);
    currentPage = 0;
  }

  selectorIndex = 0;
  isLoading = false;
}

void LibraryViewerActivity::onEnter() {
  Activity::onEnter();

  isFirstLoad = (LIBRARY.getPathCount() == 0);
  loadBooks();

  requestUpdate();
}

void LibraryViewerActivity::onExit() {
  Activity::onExit();
  books.clear();
  bookPaths.clear();
}

void LibraryViewerActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageItems = metrics.libraryItemsPerPage;
  const int totalPages = (bookPaths.size() + pageItems - 1) / pageItems;

  // Handle page navigation
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!books.empty() && selectorIndex < books.size()) {
      onSelectBook(books[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(bookPaths.size());

  buttonNavigator.onNextRelease([this, listSize, pageItems] {
    int newIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    int newPage = newIndex / pageItems;
    if (newPage != currentPage) {
      currentPage = newPage;
      loadPage(currentPage);
    }
    selectorIndex = newIndex;
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize, pageItems] {
    int newIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    int newPage = newIndex / pageItems;
    if (newPage != currentPage) {
      currentPage = newPage;
      loadPage(currentPage);
    }
    selectorIndex = newIndex;
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    int currentAbsIndex = currentPage * pageItems + selectorIndex;
    int newAbsIndex = ButtonNavigator::nextPageIndex(currentAbsIndex, listSize, pageItems);
    int newPage = newAbsIndex / pageItems;
    if (newPage != currentPage) {
      currentPage = newPage;
      loadPage(currentPage);
      selectorIndex = 0;
    } else {
      selectorIndex = newAbsIndex - currentPage * pageItems;
    }
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    int currentAbsIndex = currentPage * pageItems + selectorIndex;
    int newAbsIndex = ButtonNavigator::previousPageIndex(currentAbsIndex, listSize, pageItems);
    int newPage = newAbsIndex / pageItems;
    if (newPage != currentPage) {
      currentPage = newPage;
      loadPage(currentPage);
      selectorIndex = pageItems - 1;
    } else {
      selectorIndex = newAbsIndex - currentPage * pageItems;
    }
    requestUpdate();
  });
}

void LibraryViewerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const int totalPages = (bookPaths.size() + metrics.libraryItemsPerPage - 1) / metrics.libraryItemsPerPage;
  std::string headerTitle = tr(STR_LIBRARY);
  if (totalPages > 1) {
    headerTitle += " (" + std::to_string(currentPage + 1) + "/" + std::to_string(totalPages) + ")";
  }

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (isLoading) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_LOADING_POPUP));
  } else if (books.empty()) {
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
