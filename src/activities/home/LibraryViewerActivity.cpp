#include "LibraryViewerActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <unordered_map>

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

  std::vector<LibraryBook> cachedBooks;
  bool haveCache = LIBRARY.loadFromFile();
  if (haveCache) {
    cachedBooks = LIBRARY.getBooks();
  }

  // Build map of cached books by path
  std::unordered_map<std::string, LibraryBook> cachedMap;
  cachedMap.clear();
  for (const auto& b : cachedBooks) {
    cachedMap[b.path] = b;
  }

  // Pre-allocate and populate with cached entries where possible
  books.assign(bookPaths.size(), LibraryBook());
  itemCached.assign(bookPaths.size(), false);
  size_t unprocessedCount = 0;
  for (size_t i = 0; i < bookPaths.size(); i++) {
    auto it = cachedMap.find(bookPaths[i]);
    if (it != cachedMap.end()) {
      books[i] = it->second;
      itemCached[i] = true;
    } else {
      itemCached[i] = false;
      unprocessedCount++;
    }
  }

  generatingThumbs = false;
  thumbGenIndex = 0;

  if (unprocessedCount == 0) {
    // All books are cached - start thumbnail generation check in background if needed
    std::sort(books.begin(), books.end(), [](const LibraryBook& a, const LibraryBook& b) {
      if (a.author != b.author) return a.author < b.author;
      return a.title < b.title;
    });

    LIBRARY.setBookPaths(bookPaths);
    LIBRARY.setBooks(books);
    LIBRARY.saveToFile();

    isLoading = false;
    generatingThumbs = true;
    thumbGenIndex = 0;
    requestUpdate();
    return;
  }

  // Some books need processing
  loadingIndex = 0;
  loadingEnd = bookPaths.size();
  nextUnprocessed = 0;
  isLoading = true;
  generatingThumbs = false;
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
  generatingThumbs = false;
  books.clear();
  bookPaths.clear();
  itemCached.clear();
}

void LibraryViewerActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageItems = metrics.libraryItemsPerPage;
  const int totalPages = (books.size() + pageItems - 1) / pageItems;

  // Progressive loading: load batch of unprocessed books per loop iteration
  if (isLoading) {
    size_t processed = 0;
    while (nextUnprocessed < bookPaths.size() && processed < BATCH_SIZE) {
      if (itemCached[nextUnprocessed]) {
        nextUnprocessed++;
        continue;
      }

      const std::string& path = bookPaths[nextUnprocessed];

      Epub epub(path, "/.crosspoint");
      bool loaded = epub.load(true, true);

      LibraryBook book;
      book.path = path;
      book.title = loaded ? epub.getTitle() : StringUtils::getFileNameWithoutExtension(path);
      book.author = loaded ? epub.getAuthor() : "";
      if (loaded) {
        book.coverBmpPath = epub.getThumbBmpPath(100);
      } else {
        book.coverBmpPath = "";
      }

      books[nextUnprocessed] = book;
      nextUnprocessed++;
      processed++;
    }

    LIBRARY.setBookPaths(bookPaths);
    LIBRARY.setBooks(books);
    LIBRARY.saveToFile();

    if (nextUnprocessed >= bookPaths.size()) {
      std::sort(books.begin(), books.end(), [](const LibraryBook& a, const LibraryBook& b) {
        if (a.author != b.author) return a.author < b.author;
        return a.title < b.title;
      });

      LIBRARY.setBookPaths(bookPaths);
      LIBRARY.setBooks(books);
      LIBRARY.saveToFile();

      isLoading = false;
      generatingThumbs = true;
      thumbGenIndex = 0;
    }

    requestUpdate();
    return;
  }

  // Background thumbnail generation
  if (generatingThumbs) {
    size_t processed = 0;
    while (thumbGenIndex < books.size() && processed < BATCH_SIZE) {
      LibraryBook& book = books[thumbGenIndex];
      if (!book.coverBmpPath.empty()) {
        Epub epub(book.path, "/.crosspoint");
        if (epub.load(true, true)) {
          epub.generateThumbBmp(100);
        }
      }
      thumbGenIndex++;
      processed++;
    }

    if (thumbGenIndex >= books.size()) {
      generatingThumbs = false;
    }
    requestUpdate();
    return;
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
    if (bookPaths.size() > 0) {
      size_t done = nextUnprocessed;
      if (done > bookPaths.size()) done = bookPaths.size();
      loadingMsg += " " + std::to_string(done) + "/" + std::to_string(bookPaths.size());
    }
    GUI.drawPopup(renderer, loadingMsg.c_str());
  } else if (generatingThumbs) {
    std::string loadingMsg = tr(STR_LOADING_POPUP);
    if (books.size() > 0) {
      size_t done = thumbGenIndex;
      if (done > books.size()) done = books.size();
      loadingMsg += " " + std::to_string(done) + "/" + std::to_string(books.size());
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

  if (isLoading || generatingThumbs) {
    const auto labels = mappedInput.mapLabels(tr(STR_HOME), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
