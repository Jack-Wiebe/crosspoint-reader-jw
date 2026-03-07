#include "LibraryStore.h"

#include <HalStorage.h>
#include <JsonSettingsIO.h>
#include <Logging.h>

#include <algorithm>

namespace {
constexpr char LIBRARY_FILE_JSON[] = "/.crosspoint/library.json";
}  // namespace

LibraryStore LibraryStore::instance;

bool LibraryStore::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  return JsonSettingsIO::saveLibrary(*this, LIBRARY_FILE_JSON);
}

bool LibraryStore::loadFromFile() {
  return JsonSettingsIO::loadLibrary(*this, LIBRARY_FILE_JSON);
}

void LibraryStore::clear() {
  books.clear();
  bookPaths.clear();
}
