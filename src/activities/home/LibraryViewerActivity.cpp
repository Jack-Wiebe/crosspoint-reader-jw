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

  std::sort(bookPaths.begin(), bookPaths.end());
}

void LibraryViewerActivity::loadPage(size_t page) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const size_t itemsPerPage = metrics.libraryItemsPerPage;

  displayStart = page * itemsPerPage;
  currentPage = page;
  selectorIndex = 0;
  requestUpdate();
}

void LibraryViewerActivity::loadBooks() {
  books.clear();
  bookPaths.clear();

  scanBookPaths();

  if (LIBRARY.loadFromFile()) {
    const auto& cachedPaths = LIBRARY.getBookPaths();

    bool cacheValid = (bookPaths.size() == cachedPaths.size());
    if (cacheValid) {
      for (size_t i = 0; i < bookPaths.size(); i++) {
        if (bookPaths[i] != cachedPaths[i]) {
          cacheValid = false;
          break;
        }
      }
    }

    if (cacheValid) {
      books = LIBRARY.getBooks();
      std::sort(books.begin(), books.end(), [](const LibraryBook& a, const LibraryBook& b) {
        if (a.author != b.author) return a.author < b.author;
        return a.title < b.title;
      });
      currentPage = 0;
      displayStart = 0;
      selectorIndex = 0;
      return;
    }
  }

  loadingIndex = 0;
  loadingEnd = bookPaths.size();
  isLoading = true;
  requestUpdate();
}

void LibraryViewerActivity::onEnter() {
  Activity::onEnter();

  loadBooks();

  requestUpdate();
}

void LibraryViewerActivity::onExit() {
  Activity::onExit();

  isLoading = false;
  books.clear();
  bookPaths.clear();
}

void LibraryViewerActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageItems = metrics.libraryItemsPerPage;
  const int totalPages = (books.size() + pageItems - 1) / pageItems;

  // Progressive loading: load one book per loop iteration
  if (isLoading && loadingIndex < loadingEnd) {
    const std::string& path = bookPaths[loadingIndex];

    Epub epub(path, "/.crosspoint");
    bool loaded = epub.load(true, true);

    if (loaded && !epub.getCoverBmpPath().empty()) {
      epub.generateThumbBmp(100);
    }

    LibraryBook book;
    book.path = path;
    book.title = loaded ? epub.getTitle() : StringUtils::getFileNameWithoutExtension(path);
    book.author = loaded ? epub.getAuthor() : "";
    book.coverBmpPath = loaded ? epub.getThumbBmpPath() : "";

    books.push_back(book);

    loadingIndex++;
    requestUpdate();
    return;
  }

  // Finished loading
  if (isLoading && loadingIndex >= loadingEnd) {
    std::sort(books.begin(), books.end(), [](const LibraryBook& a, const LibraryBook& b) {
      if (a.author != b.author) return a.author < b.author;
      return a.title < b.title;
    });

    LIBRARY.setBookPaths(bookPaths);
    LIBRARY.setBooks(books);
    isLoading = false;
  }

  // Handle page navigation
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!books.empty() && selectorIndex < books.size()) {
      onSelectBook(books[displayStart + selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(books.size());

  buttonNavigator.onNextRelease([this, pageItems, listSize, totalPages] {
    int currentPageItems = std::min(pageItems, static_cast<int>(listSize - currentPage * pageItems));
    int newIndex = ButtonNavigator::nextIndex(selectorIndex, currentPageItems);

    if (newIndex < selectorIndex) {
      if (currentPage + 1 >= totalPages){
        currentPage = 0;
        newIndex = 0;
      }else{
        currentPage++;
      }
      loadPage(currentPage);
    }
    selectorIndex = newIndex;
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, pageItems, listSize, totalPages] {
    int currentPageItems = std::min(pageItems, static_cast<int>(listSize - currentPage * pageItems));
    int newIndex = ButtonNavigator::previousIndex(selectorIndex, currentPageItems);

    // If wrapped (newIndex > selectorIndex), we moved to previous page
    if (newIndex > selectorIndex) {
      if (currentPage <= 0){
        currentPage = totalPages-1;
        newIndex = (listSize-1) % pageItems;
      }else{
        currentPage--;
      }
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
      selectorIndex = (listSize-1) % pageItems;
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

  const int totalPages = (books.size() + metrics.libraryItemsPerPage - 1) / metrics.libraryItemsPerPage;
  std::string headerTitle = tr(STR_LIBRARY);
  if (totalPages > 1) {
    headerTitle += " (" + std::to_string(currentPage + 1) + "/" + std::to_string(totalPages) + ")";
  }

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (isLoading) {
    std::string loadingMsg = tr(STR_LOADING_POPUP);
    if (loadingEnd > 0) {
      loadingMsg += " " + std::to_string(loadingIndex + 1) + "/" + std::to_string(loadingEnd);
    }
    GUI.drawPopup(renderer, loadingMsg.c_str());
  } else if (books.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOKS_FOUND));
  } else {
    size_t displayEnd = std::min(displayStart + metrics.libraryItemsPerPage, books.size());
    size_t pageBookCount = displayEnd - displayStart;
    GUI.drawListWithCover(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, pageBookCount, selectorIndex,
        [this](int index) { return books[displayStart + index].title; },
        [this](int index) { return books[displayStart + index].author; },
        [this](int index) { return books[displayStart + index].coverBmpPath; });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
