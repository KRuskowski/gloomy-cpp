#ifndef GLOOMY_UI_LIST_ROWS_H_
#define GLOOMY_UI_LIST_ROWS_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/summary.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

// Rows for short lists that are not worth a DataGrid: a handful of items,
// each a line of cells in fixed columns with a few actions at the end
// (a fleet, a build queue), and rows of an ordered queue the user edits
// (move up, down, run now, remove). Columns line up between a header and
// every row, whatever the texts: a cell cuts its text with an ellipsis.

/// Horizontal alignment of a cell's text.
enum class CellAlign : std::uint8_t { kLeft, kRight };

/// One column of a cell row.
struct CellColumn {
  /// Shown by CellHeader().
  std::string_view title;
  /// Width in pixels at the default font size (scaled with the theme,
  /// CellScale()); 0 shares the width left over with the other 0 columns.
  float width = 0.0F;
  CellAlign align = CellAlign::kLeft;
  /// Tooltip of the header; empty for none.
  std::string_view why;
};

/// One cell's content.
struct Cell {
  std::string_view text;
  /// Colours the text (neutral: the theme's text, or muted).
  Tone tone = Tone::kNeutral;
  /// An explicit colour wins over the tone; alpha 0 for none.
  Color color{0.0F, 0.0F, 0.0F, 0.0F};
  bool bold = false;
  bool muted = false;
  /// Tooltip; empty for none. Shown on the whole row when the text is cut.
  std::string_view why;
};

/// A button at the end of a row.
struct RowAction {
  std::string_view label;
  /// Not empty: the button is disabled and says why.
  std::string_view disabled;
  ButtonKind kind = ButtonKind::kFlat;
};

/// How a cell row looks and what it offers.
struct CellRowOptions {
  /// Marked with the theme's selection colour and accent bar.
  bool selected = false;
  /// A coloured bar at the left edge (severity); neutral for none.
  Tone status = Tone::kNeutral;
  /// Buttons after the cells, in this order.
  std::span<const RowAction> actions;
  /// Space kept for actions so that rows with fewer buttons, and the
  /// header, still line up: the width of these labels as buttons. Empty:
  /// `actions` itself.
  std::span<const RowAction> action_slots;
};

/// What the user did with a row this frame.
struct CellRowResult {
  /// Clicked on the row outside its buttons (selects it).
  bool clicked = false;
  /// Enter while the row has focus, or a double click.
  bool activated = false;
  /// Index into CellRowOptions::actions of the button pressed; -1 none.
  int action = -1;
};

/// The factor fixed column widths are scaled with: the theme's font size
/// over the default 13 px, so that a UI scale setting widens them too.
[[nodiscard]] auto CellScale(const Theme &theme) -> float;

/// Width of `actions` drawn as buttons with `gap` between them.
[[nodiscard]] auto RowActionsWidth(Context &ctx,
                                   std::span<const RowAction> actions,
                                   float gap) -> float;

/// The column titles, muted and bold, lined up with the rows below;
/// `action_slots` keeps the same room for the rows' buttons.
void CellHeader(Context &ctx, std::string_view label,
                std::span<const CellColumn> columns,
                std::span<const RowAction> action_slots = {});

/// One row: `cells` in `columns` (a missing cell is empty), then the
/// actions. The row is focusable: Enter activates it.
auto CellRow(Context &ctx, std::string_view label,
             std::span<const CellColumn> columns, std::span<const Cell> cells,
             const CellRowOptions &options = {}) -> CellRowResult;

/// An edit of an ordered queue.
enum class QueueEdit : std::uint8_t {
  kNone,
  /// Move the item one place earlier.
  kUp,
  /// Move the item one place later.
  kDown,
  /// Run this item now (skip to it).
  kGo,
  kRemove,
};

/// One item of a queue the user edits.
struct QueueRowSpec {
  /// Position in the queue (0 first) and the queue's length.
  int index = 0;
  int count = 1;
  std::string_view text;
  /// The item running now: marked, bold, no "Go".
  bool current = false;
  Tone tone = Tone::kNeutral;
  /// Tooltip of the text; empty for none.
  std::string_view why;
  /// Labels of the buttons.
  std::string_view up = "Up";
  std::string_view down = "Down";
  std::string_view go = "Go";
  std::string_view remove = "Remove";
};

/// A queue item: its number ("2."), the text and Up / Down / Go / Remove.
/// A button that cannot act is disabled and says why, so the buttons stay
/// in their columns. With the row focused: Alt+Up and Alt+Down move it,
/// Enter runs it, Delete removes it.
/// @return The edit chosen this frame.
auto QueueRow(Context &ctx, std::string_view label, const QueueRowSpec &spec)
    -> QueueEdit;

/// Applies `edit` of the item at `index` to `items`: Up and Down swap it
/// with its neighbour, Remove erases it, Go and None change nothing.
/// @return Where the item is now; -1 if removed or `index` is out of range.
template <typename T>
auto ApplyQueueEdit(std::vector<T> &items, int index, QueueEdit edit) -> int {
  const int n = static_cast<int>(items.size());
  if (index < 0 || index >= n) {
    return -1;
  }
  const auto at = [&items](int i) -> T & {
    return items[static_cast<std::size_t>(i)];
  };
  switch (edit) {
    case QueueEdit::kUp:
      if (index == 0) {
        return index;
      }
      std::swap(at(index), at(index - 1));
      return index - 1;
    case QueueEdit::kDown:
      if (index + 1 >= n) {
        return index;
      }
      std::swap(at(index), at(index + 1));
      return index + 1;
    case QueueEdit::kRemove:
      items.erase(items.begin() + index);
      return -1;
    case QueueEdit::kGo:
    case QueueEdit::kNone:
      break;
  }
  return index;
}

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_LIST_ROWS_H_
