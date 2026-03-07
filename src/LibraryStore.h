#pragma once
#include <string>
#include <vector>

struct LibraryBook {
  std::string path;
  std::string title;
  std::string author;
  std::string coverBmpPath;

  bool operator==(const LibraryBook& other) const { return path == other.path; }
};

class LibraryStore;
namespace JsonSettingsIO {
bool loadLibrary(LibraryStore& store, const char* json);
}  // namespace JsonSettingsIO

class LibraryStore {
  static LibraryStore instance;

  std::vector<LibraryBook> books;
  std::vector<std::string> bookPaths;

  friend bool JsonSettingsIO::loadLibrary(LibraryStore&, const char*);

 public:
  ~LibraryStore() = default;

  static LibraryStore& getInstance() { return instance; }

  const std::vector<LibraryBook>& getBooks() const { return books; }
  const std::vector<std::string>& getBookPaths() const { return bookPaths; }

  int getCount() const { return static_cast<int>(books.size()); }
  int getPathCount() const { return static_cast<int>(bookPaths.size()); }

  void setBooks(const std::vector<LibraryBook>& newBooks) { books = newBooks; }
  void setBookPaths(const std::vector<std::string>& paths) { bookPaths = paths; }

  bool saveToFile() const;
  bool loadFromFile();
  void clear();
};

#define LIBRARY LibraryStore::getInstance()
