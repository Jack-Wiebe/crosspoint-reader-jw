#include "Lyra3CoversTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cstdint>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"
#include "components/icons/book.h"
#include "fontIds.h"

// Internal constants
namespace {
constexpr int hPaddingInSelection = 8;
constexpr int cornerRadius = 6;
}  // namespace

void Lyra3CoversTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                           bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const int tileWidth = (rect.width - 2 * Lyra3CoversMetrics::values.contentSidePadding) / 3;
  const int tileY = rect.y;
  const bool hasContinueReading = !recentBooks.empty();

  // Draw book card regardless, fill with message based on `hasContinueReading`
  // Draw cover image as background if available (inside the box)
  // Only load from SD on first render, then use stored buffer
  if (hasContinueReading) {
    if (!coverRendered) {
      for (int i = 0;
           i < std::min(static_cast<int>(recentBooks.size()), Lyra3CoversMetrics::values.homeRecentBooksCount); i++) {
        std::string coverPath = recentBooks[i].coverBmpPath;
        bool hasCover = true;
        int tileX = Lyra3CoversMetrics::values.contentSidePadding + tileWidth * i;
        if (coverPath.empty()) {
          hasCover = false;
        } else {
          const std::string coverBmpPath =
              UITheme::getCoverThumbPath(coverPath, Lyra3CoversMetrics::values.homeCoverHeight);

          // First time: load cover from SD and render
          FsFile file;
          if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
            Bitmap bitmap(file);
            if (bitmap.parseHeaders() == BmpReaderError::Ok) {
              float coverHeight = static_cast<float>(bitmap.getHeight());
              float coverWidth = static_cast<float>(bitmap.getWidth());
              float ratio = coverWidth / coverHeight;
              const float tileRatio = static_cast<float>(tileWidth - 2 * hPaddingInSelection) /
                                      static_cast<float>(Lyra3CoversMetrics::values.homeCoverHeight);
              float cropX = 1.0f - (tileRatio / ratio);

              renderer.drawBitmap(bitmap, tileX + hPaddingInSelection, tileY + hPaddingInSelection,
                                  tileWidth - 2 * hPaddingInSelection, Lyra3CoversMetrics::values.homeCoverHeight,
                                  cropX);
            } else {
              hasCover = false;
            }
            file.close();
          }
        }
        // Draw either way
        renderer.drawRect(tileX + hPaddingInSelection, tileY + hPaddingInSelection, tileWidth - 2 * hPaddingInSelection,
                          Lyra3CoversMetrics::values.homeCoverHeight, true);

        if (!hasCover) {
          // Render empty cover
          renderer.fillRect(tileX + hPaddingInSelection,
                            tileY + hPaddingInSelection + (Lyra3CoversMetrics::values.homeCoverHeight / 3),
                            tileWidth - 2 * hPaddingInSelection, 2 * Lyra3CoversMetrics::values.homeCoverHeight / 3,
                            true);
          renderer.drawIcon(CoverIcon, tileX + hPaddingInSelection + 24, tileY + hPaddingInSelection + 24, 32, 32);
        }
      }

      coverBufferStored = storeCoverBuffer();
      coverRendered = coverBufferStored;  // Only consider it rendered if we successfully stored the buffer
    }

    for (int i = 0; i < std::min(static_cast<int>(recentBooks.size()), Lyra3CoversMetrics::values.homeRecentBooksCount);
         i++) {
      bool bookSelected = (selectorIndex == i);

      int tileX = Lyra3CoversMetrics::values.contentSidePadding + tileWidth * i;

      const int maxLineWidth = tileWidth - 2 * hPaddingInSelection;

      auto titleLines = renderer.wrappedText(SMALL_FONT_ID, recentBooks[i].title.c_str(), maxLineWidth, 3);

      const int titleLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
      const int dynamicBlockHeight = static_cast<int>(titleLines.size()) * titleLineHeight;
      // Add a little padding below the text inside the selection box just like the top padding (5 + hPaddingSelection)
      const int dynamicTitleBoxHeight = dynamicBlockHeight + hPaddingInSelection + 5;

      if (bookSelected) {
        // Draw selection box
        renderer.fillRoundedRect(tileX, tileY, tileWidth, hPaddingInSelection, cornerRadius, true, true, false, false,
                                 Color::LightGray);
        renderer.fillRectDither(tileX, tileY + hPaddingInSelection, hPaddingInSelection,
                                Lyra3CoversMetrics::values.homeCoverHeight, Color::LightGray);
        renderer.fillRectDither(tileX + tileWidth - hPaddingInSelection, tileY + hPaddingInSelection,
                                hPaddingInSelection, Lyra3CoversMetrics::values.homeCoverHeight, Color::LightGray);
        renderer.fillRoundedRect(tileX, tileY + Lyra3CoversMetrics::values.homeCoverHeight + hPaddingInSelection,
                                 tileWidth, dynamicTitleBoxHeight, cornerRadius, false, false, true, true,
                                 Color::LightGray);
      }

      int currentY = tileY + Lyra3CoversMetrics::values.homeCoverHeight + hPaddingInSelection + 5;
      for (const auto& line : titleLines) {
        renderer.drawText(SMALL_FONT_ID, tileX + hPaddingInSelection, currentY, line.c_str(), true);
        currentY += titleLineHeight;
      }
    }
  } else {
    drawEmptyRecents(renderer, rect);
  }
}

void Lyra3CoversTheme::drawListWithCover(GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                                         const std::function<std::string(int index)>& rowTitle,
                                         const std::function<std::string(int index)>& rowAuthor,
                                         const std::function<std::string(int index)>& rowCoverPath) const {
  const int pageItems = Lyra3CoversMetrics::values.libraryItemsPerPage;
  const int thumbHeight = Lyra3CoversMetrics::values.coverHeight;
  const int rowHeight = rect.height / pageItems;
  const int coverMargin = (rowHeight / thumbHeight) / 2;

  const int totalPages = (itemCount + pageItems - 1) / pageItems;
  if (totalPages > 1) {
    constexpr int indicatorWidth = 20;
    constexpr int arrowSize = 6;
    constexpr int margin = 15;

    const int centerX = rect.x + rect.width - indicatorWidth / 2 - margin;
    const int indicatorTop = rect.y;
    const int indicatorBottom = rect.y + rect.height - arrowSize;

    for (int i = 0; i < arrowSize; ++i) {
      const int lineWidth = 1 + i * 2;
      const int startX = centerX - i;
      renderer.drawLine(startX, indicatorTop + i, startX + lineWidth - 1, indicatorTop + i);
    }

    for (int i = 0; i < arrowSize; ++i) {
      const int lineWidth = 1 + (arrowSize - 1 - i) * 2;
      const int startX = centerX - (arrowSize - 1 - i);
      renderer.drawLine(startX, indicatorBottom - arrowSize + 1 + i, startX + lineWidth - 1,
                        indicatorBottom - arrowSize + 1 + i);
    }
  }

  const int coverHeight = rowHeight - 16;
  const int coverWidth = coverHeight * 0.6;
  const int coverX = Lyra3CoversMetrics::values.contentSidePadding;
  const int textX = coverX + coverWidth + 12;

  if (selectedIndex >= 0) {
    renderer.fillRect(0, rect.y + selectedIndex % pageItems * rowHeight - 2, rect.width, rowHeight);
  }

  const auto pageStartIndex = selectedIndex / pageItems * pageItems;
  for (int i = pageStartIndex; i < itemCount && i < pageStartIndex + pageItems; i++) {
    const int itemY = rect.y + (i % pageItems) * rowHeight;
    const bool isSelected = (i == selectedIndex);

    std::string coverPath = rowCoverPath(i);

    if (!coverPath.empty()) {
      std::string coverBmpPath = UITheme::getCoverThumbPath(coverPath, thumbHeight);
      FsFile file;
      if (Storage.openFileForRead("LIST", coverBmpPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderer.drawBitmap(bitmap, coverX, itemY + coverMargin, coverWidth, coverHeight);
        }
        file.close();
      }
    } else {
      renderer.drawIcon(BookIcon, coverX + 12, itemY + 26, 24, 24);
    }

    int textY = itemY + 20;
    auto titleText = renderer.truncatedText(UI_12_FONT_ID, rowTitle(i).c_str(), rect.width - textX - Lyra3CoversMetrics::values.contentSidePadding);
    renderer.drawText(UI_12_FONT_ID, textX, textY, titleText.c_str(), !isSelected);

    textY += 40;
    auto authorText = renderer.truncatedText(UI_10_FONT_ID, rowAuthor(i).c_str(), rect.width - textX - Lyra3CoversMetrics::values.contentSidePadding);
    renderer.drawText(UI_10_FONT_ID, textX, textY, authorText.c_str(), !isSelected);
  }
}