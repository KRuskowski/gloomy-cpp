#ifndef GLOOMY_UI_DATA_GRID_H_
#define GLOOMY_UI_DATA_GRID_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "gloomy/data/display_rows.h"
#include "gloomy/data/query.h"
#include "gloomy/data/table_source.h"
#include "gloomy/data/table_view.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// How a cell is drawn (docs/design.md section 11.2).
enum class CellRenderer : std::uint8_t {
  /// Formatted text; numbers right-aligned.
  kText,
  /// Text over a bar showing value / bar_max.
  kBar,
  /// Text whose colour fades with an age column (knowledge age).
  kAge,
  /// The enum label in a coloured pill.
  kBadge,
};

/// A column of the grid: which source column, and how it looks.
struct GridColumn {
  int column = 0;
  float width = 120.0F;
  float min_width = 36.0F;
  bool visible = true;
  /// Pinned columns stay at the left while the rest scrolls sideways.
  bool pinned = false;
  CellRenderer renderer = CellRenderer::kText;
  /// kBar: the value that fills the bar.
  double bar_max = 1.0;
  /// kAge: source column holding the age, and ages at which the text is
  /// still fully bright and fully faded.
  int age_column = -1;
  double age_fresh = 0.0;
  double age_stale = 1.0;
};

/// Selected rows as RowKeys. Selecting everything is O(1): `all` inverts
/// the meaning of `keys`.
struct GridSelection {
  bool all = false;
  std::unordered_set<std::uint64_t> keys;

  auto Contains(std::uint64_t key) const -> bool {
    return all != keys.contains(key);
  }

  void Set(std::uint64_t key, bool selected) {
    if (selected != all) {
      keys.insert(key);
    } else {
      keys.erase(key);
    }
  }

  void Clear() {
    all = false;
    keys.clear();
  }

  void SelectAll() {
    all = true;
    keys.clear();
  }

  auto Empty() const -> bool { return !all && keys.empty(); }
};

/// Cursor keys of group header lines carry this bit (RowKeys should not).
inline constexpr std::uint64_t kGroupKeyBit = 1ULL << 63U;

/// Everything about a grid the app may keep, save and restore; the query
/// and columns are what a view preset stores (docs/design.md section 2.4).
struct GridState {
  std::vector<GridColumn> columns;
  data::Query query;
  GridSelection selection;
  /// The cursor row (RowKey) and display column.
  std::optional<std::uint64_t> cursor;
  int cursor_col = 0;
  std::optional<std::uint64_t> anchor;
  /// Vertical scroll in pixels (double: a million rows are 24 million px).
  double scroll_y = 0.0;
  float scroll_x = 0.0F;
  /// Set to scroll the cursor row into view once the view shows it (after
  /// setting `cursor` from outside, e.g. a search hit).
  bool reveal = false;
  /// Ids (RowGroup::id) of collapsed groups.
  std::unordered_set<std::uint64_t> collapsed;

  // Internal: display lines, cursor position cache, interaction,
  // type-to-jump. Positions are display lines (group headers included).
  data::DisplayRows display;
  std::uint64_t display_generation = ~0ULL;
  std::uint64_t display_version = 0;
  bool display_dirty = true;
  std::int64_t cursor_pos = -1;
  std::int64_t anchor_pos = -1;
  std::uint64_t cursor_generation = ~0ULL;
  bool pressing = false;
  int resizing = -1;
  float resize_start = 0.0F;
  int dragging = -1;
  bool drag_moved = false;
  int menu_column = -1;
  std::string jump_prefix;
  double jump_time = -10.0;
  std::int64_t hover_pos = -1;
  int hover_col = -1;
  double hover_since = 0.0;
};

/// What happened in the grid this frame.
struct GridResult {
  /// Enter or double click on a row; `row` is the source row.
  bool activated = false;
  std::optional<data::RowIndex> row;
  bool selection_changed = false;
  /// Sort, filters or search changed through the grid's UI.
  bool query_changed = false;
  /// Right click on a cell: source row and source column, for context
  /// menus (see GridCellMenu()).
  std::optional<data::RowIndex> context_row;
  int context_column = -1;
};

/// One GridColumn per source column, sized by type.
auto DefaultColumns(const data::TableSource &source) -> std::vector<GridColumn>;

/// Display rows (in the view) of the selection, cursor row if empty.
auto SelectedRows(const data::TableView &view, const GridState &state)
    -> std::vector<data::RowIndex>;

/// The selection (or cursor row) as tab-separated text with a header line.
auto CopyAsTsv(const data::TableView &view, const GridState &state)
    -> std::string;

/// A virtualised data grid over `view`: sorting (click; Shift+click adds
/// keys), column resize (drag edges, double click autosizes), reorder
/// (drag headers, Alt+Left/Right), hide and pin (header menu, right
/// click), multi-select (Ctrl/Shift click, Shift+arrows, Ctrl+A), keyboard
/// navigation, type to jump, Ctrl+C as TSV, search highlighting, cell
/// renderers and explanations as tooltips. Only visible cells are
/// formatted and drawn; selection and cursor survive re-queries.
auto DataGrid(Context &ctx, std::string_view label, data::TableView &view,
              GridState &state, LayoutSpec layout = {}) -> GridResult;

/// The standard cell context menu: copy the cell or the selected rows,
/// show only or hide rows with this value, clear filters. Call after
/// DataGrid() with its result every frame.
/// @return True if the query changed.
auto GridCellMenu(Context &ctx, std::string_view label,
                  const data::TableView &view, GridState &state,
                  const GridResult &result) -> bool;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_DATA_GRID_H_
