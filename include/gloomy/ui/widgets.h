#ifndef GLOOMY_UI_WIDGETS_H_
#define GLOOMY_UI_WIDGETS_H_

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"

namespace gloomy::ui {

// Basic widgets, immediate mode: call them every frame; they return what
// happened. Ids come from labels (text after "##" is hidden) under the id
// stack. See docs/design.md section 11.

/// The visible part of a label ("Save##file" shows "Save").
auto DisplayText(std::string_view label) -> std::string_view;

// Containers.

/// Opens a row container; End() closes it.
auto BeginRow(Context &ctx, LayoutSpec layout = {}) -> int;

/// Opens a column container; End() closes it.
auto BeginColumn(Context &ctx, LayoutSpec layout = {}) -> int;

/// Opens a panel: a column with the surface colour, border and padding.
auto BeginPanel(Context &ctx, LayoutSpec layout = {}) -> int;

void End(Context &ctx);

/// Empty space that grows (or has a fixed size if `size` > 0).
void Spacer(Context &ctx, float size = 0.0F);

/// A thin line across the parent (horizontal in a column).
void Separator(Context &ctx, bool vertical = false);

// Text.

struct LabelOptions {
  /// Alpha 0 means the theme's text colour.
  Color color{0, 0, 0, 0};
  bool muted = false;
  bool bold = false;
  bool mono = false;
  /// 0 means the theme's font size.
  float size = 0.0F;
  bool wrap = false;
  TextAlign align = TextAlign::kLeft;
  SizeSpec width = SizeSpec::Fit();
};

void Label(Context &ctx, std::string_view text, const LabelOptions &opt = {});

void Heading(Context &ctx, std::string_view text);

// Controls.

enum class ButtonKind : std::uint8_t { kNormal, kPrimary, kFlat };

/// @return True when activated (click, or Enter/Space while focused).
auto Button(Context &ctx, std::string_view label,
            ButtonKind kind = ButtonKind::kNormal) -> bool;

/// @return True when toggled.
auto Checkbox(Context &ctx, std::string_view label, bool &value) -> bool;

/// One option of a radio group; `value` is set to `option` when chosen.
auto RadioButton(Context &ctx, std::string_view label, int &value, int option)
    -> bool;

/// Horizontal slider. Arrow keys step by 1/100, Page Up/Down by 1/10.
/// @return True when the value changed.
auto Slider(Context &ctx, std::string_view label, float &value, float min,
            float max) -> bool;

/// A bar showing `fraction` (0..1).
void ProgressBar(Context &ctx, float fraction,
                 SizeSpec width = SizeSpec::Grow());

struct TextFieldOptions {
  std::string_view placeholder;
  SizeSpec width = SizeSpec::Fixed(200.0F);
  bool mono = false;
  /// No background or border (for fields embedded in other widgets).
  bool frameless = false;
};

struct TextFieldResult {
  bool changed = false;
  /// Enter was pressed.
  bool submitted = false;
};

/// Single-line text input with selection, clipboard and word movement.
auto TextField(Context &ctx, std::string_view label, std::string &text,
               const TextFieldOptions &opt = {}) -> TextFieldResult;

/// A button showing the selected item that opens a list of `items`.
/// @return True when the selection changed.
auto Dropdown(Context &ctx, std::string_view label, int &selected,
              std::span<const std::string> items) -> bool;

/// A tab bar. Left/Right switch tabs while it has focus.
/// @return True when the selection changed.
auto Tabs(Context &ctx, std::string_view label, int &selected,
          std::span<const std::string> labels) -> bool;

struct SearchFieldOptions {
  std::string_view placeholder = "Search";
  SizeSpec width = SizeSpec::Grow();
  /// Shown at the right end, e.g. "3 of 17" or "481 rows".
  std::string_view status;
  /// Work is running for the current query.
  bool busy = false;
};

struct SearchFieldResult {
  /// The query text changed (search incrementally on every change).
  bool changed = false;
  /// Enter or F3: go to the next match.
  bool next = false;
  /// Shift+Enter or Shift+F3: go to the previous match.
  bool previous = false;
  /// Escape or the clear button emptied the query.
  bool cleared = false;
};

/// A search box: magnifier, query, match status, clear button. Enter/F3
/// and Shift+Enter/Shift+F3 step through matches, Escape clears.
auto SearchField(Context &ctx, std::string_view label, std::string &query,
                 const SearchFieldOptions &opt = {}) -> SearchFieldResult;

/// Id of a search field's text input, to focus it (Ctrl+F, "/").
auto SearchFieldId(Context &ctx, std::string_view label) -> Id;

// Scrolling.

/// Opens a vertically scrolling area with a scroll bar; EndScroll() closes
/// it. Wheel and dragging the bar scroll it.
void BeginScroll(Context &ctx, std::string_view label, LayoutSpec layout = {});

void EndScroll(Context &ctx);

/// Scroll offset of the scroll area `label` (current id stack).
auto ScrollOffset(Context &ctx, std::string_view label) -> double &;

// Lists.

/// Selection and scroll state of a list; owned by the app.
struct ListState {
  int selected = -1;
  /// Index of the first visible row times row height, in pixels.
  double scroll = 0.0;
  // Type-to-jump prefix and when it was last extended.
  std::string prefix;
  double prefix_time = -10.0;
};

/// Returns the label of item i.
using ItemText = std::function<std::string_view(int)>;

struct ListResult {
  bool selection_changed = false;
  /// Enter or double click on the selected item.
  bool activated = false;
};

/// A virtualised, keyboard-first list (X3 style): arrows, Page Up/Down,
/// Home/End, Enter activates, typing jumps to the next item starting with
/// the typed prefix. Only visible rows become nodes.
auto List(Context &ctx, std::string_view label, ListState &state, int count,
          const ItemText &item, LayoutSpec layout = {}) -> ListResult;

// Overlays.

/// Shows `text` near the pointer when node `for_id` has been hovered for a
/// moment.
void Tooltip(Context &ctx, Id for_id, std::string_view text);

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_WIDGETS_H_
