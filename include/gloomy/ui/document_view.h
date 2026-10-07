#ifndef GLOOMY_UI_DOCUMENT_VIEW_H_
#define GLOOMY_UI_DOCUMENT_VIEW_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gloomy/data/document.h"
#include "gloomy/text/rich_layout.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// Scroll position, search and layout caches of a DocumentView; owned by
/// the app.
struct DocumentViewState {
  /// Search string; matches are found as it changes.
  std::string search;
  std::vector<data::DocMatch> matches;
  /// Index into `matches` of the current match, -1 if none.
  int current = -1;
  /// Scroll offset in pixels.
  double scroll = 0.0;

  // Internal.
  std::string searched;
  std::uint64_t searched_version = 0;
  float width = -1.0F;
  std::uint64_t doc_version = 0;
  std::vector<float> heights;
  std::vector<std::uint8_t> exact;
  std::vector<double> fenwick;
  std::unordered_map<std::uint32_t, text::RichLayout> layouts;
  std::unordered_map<std::uint32_t, std::vector<text::TextRun>> runs;
  std::optional<std::uint32_t> scroll_to_block;
  bool scroll_to_match = false;
};

struct DocumentResult {
  /// Target of a link that was clicked.
  std::optional<std::string> link;
};

/// A long rich-text document (contracts): headings, paragraphs, lists,
/// quotes, code, rules, inline styles and links; virtualised so only
/// visible blocks are laid out. Search terms are highlighted, the current
/// match more strongly; F3 / Shift+F3 step through matches. Keys while
/// focused: arrows, Page Up/Down, Home/End scroll.
auto DocumentView(Context &ctx, std::string_view label,
                  const data::Document &doc, DocumentViewState &state,
                  LayoutSpec layout = {}) -> DocumentResult;

/// Moves to the next (or previous) match and scrolls it into view.
void GoToMatch(DocumentViewState &state, bool backwards);

/// Scrolls block `block` to the top on the next frame (outline jumps).
void ScrollToBlock(DocumentViewState &state, std::uint32_t block);

/// "3 of 17", "No matches" or empty without a search.
auto MatchStatus(const DocumentViewState &state) -> std::string;

/// Index of the first block visible at the top.
auto TopBlock(const DocumentViewState &state) -> std::uint32_t;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_DOCUMENT_VIEW_H_
