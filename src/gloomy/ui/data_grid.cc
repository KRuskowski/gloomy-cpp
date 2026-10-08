#include "gloomy/ui/data_grid.h"

#include <algorithm>
#include <cmath>

#include "gloomy/core/utf8.h"
#include "gloomy/data/search.h"
#include "gloomy/ui/overlays.h"
#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

namespace {

using data::ColumnInfo;
using data::ColumnType;
using data::RowIndex;
using data::SortDirection;

constexpr float kEdgeGrab = 4.0F;
constexpr float kCellPad = 6.0F;
constexpr float kDragThreshold = 6.0F;
constexpr double kJumpTimeout = 1.0;
constexpr double kHoverDelay = 0.5;

// A visible column on screen.
struct ColSpan {
  int index = 0;
  float x0 = 0.0F;
  float x1 = 0.0F;
  bool pinned = false;
};

// Where everything is, derived from the state and the body rectangle. The
// interaction code and the painter use the same geometry.
struct Geometry {
  Rect body;
  /// Column group band above the headers; empty without groups.
  Rect band;
  Rect header;
  Rect rows;
  Rect status;
  float row_h = 22.0F;
  float pinned_w = 0.0F;
  float content_w = 0.0F;
  std::vector<ColSpan> cols;

  // Region non-pinned columns are drawn in.
  auto ScrollRegion() const -> Rect {
    return {body.x0 + pinned_w, body.y0, body.x1, body.y1};
  }
};

auto MakeGeometry(const GridState &state, const Rect &body, const Theme &t)
    -> Geometry {
  Geometry g;
  g.body = body;
  g.row_h = t.metrics.row_height;
  const float header_h = g.row_h + 4.0F;
  const float status_h = g.row_h;
  const bool groups = std::any_of(
      state.columns.begin(), state.columns.end(), [](const GridColumn &c) {
        return c.visible && !c.group.empty();
      });
  const float band_h = groups ? g.row_h : 0.0F;
  g.band = {body.x0, body.y0, body.x1, body.y0 + band_h};
  g.header = {body.x0, g.band.y1, body.x1, g.band.y1 + header_h};
  g.status = {body.x0, body.y1 - status_h, body.x1, body.y1};
  g.rows = {body.x0, g.header.y1, body.x1, g.status.y0};
  float pinned_x = body.x0;
  for (const GridColumn &c : state.columns) {
    if (c.visible && c.pinned) {
      pinned_x += c.width;
    }
  }
  g.pinned_w = pinned_x - body.x0;
  float px = body.x0;
  float sx = pinned_x - state.scroll_x;
  for (std::size_t i = 0; i < state.columns.size(); ++i) {
    const GridColumn &c = state.columns[i];
    if (!c.visible) {
      continue;
    }
    ColSpan span;
    span.index = static_cast<int>(i);
    span.pinned = c.pinned;
    if (c.pinned) {
      span.x0 = px;
      px += c.width;
    } else {
      span.x0 = sx;
      sx += c.width;
    }
    span.x1 = span.x0 + c.width;
    g.content_w += c.width;
    g.cols.push_back(span);
  }
  return g;
}

struct Hit {
  enum class Kind { kNone, kHeader, kHeaderEdge, kCell };
  Kind kind = Kind::kNone;
  int col = -1;
  std::int64_t pos = -1;
};

auto HitAt(const Geometry &g, Vec2 p, double scroll_y, std::int64_t rows)
    -> Hit {
  Hit hit;
  if (!g.body.Contains(p) || g.band.Contains(p)) {
    return hit;
  }
  for (const ColSpan &c : g.cols) {
    // Scrolled columns hidden under the pinned ones do not count.
    if (!c.pinned && p.x < g.body.x0 + g.pinned_w) {
      continue;
    }
    if (g.header.Contains(p) && std::fabs(p.x - c.x1) <= kEdgeGrab) {
      hit.kind = Hit::Kind::kHeaderEdge;
      hit.col = c.index;
      return hit;
    }
  }
  for (const ColSpan &c : g.cols) {
    if (!c.pinned && p.x < g.body.x0 + g.pinned_w) {
      continue;
    }
    if (p.x >= c.x0 && p.x < c.x1) {
      hit.col = c.index;
      break;
    }
  }
  if (g.header.Contains(p)) {
    hit.kind = hit.col >= 0 ? Hit::Kind::kHeader : Hit::Kind::kNone;
    return hit;
  }
  if (g.rows.Contains(p)) {
    const auto pos = static_cast<std::int64_t>(
        std::floor((p.y - g.rows.y0 + scroll_y) / g.row_h));
    if (pos >= 0 && pos < rows) {
      hit.kind = Hit::Kind::kCell;
      hit.pos = pos;
    }
  }
  return hit;
}

auto IsRightAligned(const ColumnInfo &info) -> bool {
  return info.IsNumeric();
}

auto Fold(std::string_view s) -> std::string {
  std::string out;
  FoldCaseUtf8(s, out);
  return out;
}

// Sort cycle on a header click: ascending, descending, off. With `add`,
// the column becomes an extra key instead of the only one.
void ToggleSort(std::vector<data::SortKey> &sort, int column, bool add) {
  auto it = std::find_if(sort.begin(), sort.end(), [&](const data::SortKey &k) {
    return k.column == column;
  });
  if (!add) {
    if (sort.size() == 1 && it == sort.begin()) {
      if (it->direction == SortDirection::kAscending) {
        it->direction = SortDirection::kDescending;
      } else {
        sort.clear();
      }
    } else {
      sort = {{column, SortDirection::kAscending}};
    }
    return;
  }
  if (it == sort.end()) {
    sort.push_back({column, SortDirection::kAscending});
  } else if (it->direction == SortDirection::kAscending) {
    it->direction = SortDirection::kDescending;
  } else {
    sort.erase(it);
  }
}

auto BadgeColor(const Palette &p, std::int64_t v) -> Color {
  const Color colors[] = {
      p.primary, p.accent, p.success, p.warning, p.info, p.error};
  return colors[static_cast<std::size_t>(std::abs(v)) % 6].WithAlpha(0.28F);
}

// Rebuilds the display lines when the view or the collapsed set changed.
void SyncDisplay(const data::TableView &view, GridState &state) {
  if (state.display_generation == view.generation() && !state.display_dirty) {
    return;
  }
  state.display.Build(view.view(), state.collapsed, &view.source());
  state.display_generation = view.generation();
  state.display_dirty = false;
  ++state.display_version;
}

auto LineCount(const GridState &state) -> std::int64_t {
  return static_cast<std::int64_t>(state.display.size());
}

// The source row shown on display line `line`, or nothing for a group
// header.
auto LineRow(const data::TableView &view, const GridState &state,
             std::int64_t line) -> std::optional<RowIndex> {
  const data::DisplayRows::Line l =
      state.display.At(static_cast<std::size_t>(line));
  if (l.group >= 0) {
    return std::nullopt;
  }
  return view.rows()[l.pos];
}

// Cursor key of a line: the RowKey, or the group id with kGroupKeyBit.
auto LineKey(const data::TableView &view, const GridState &state,
             std::int64_t line) -> std::uint64_t {
  const data::DisplayRows::Line l =
      state.display.At(static_cast<std::size_t>(line));
  if (l.group >= 0) {
    return view.view().groups[static_cast<std::size_t>(l.group)].id |
           kGroupKeyBit;
  }
  return view.source().RowKey(view.rows()[l.pos]);
}

// Display line of a cursor key, -1 if not shown.
auto LineOfKey(const data::TableView &view, const GridState &state,
               std::uint64_t key) -> std::int64_t {
  if ((key & kGroupKeyBit) != 0) {
    const auto &groups = view.view().groups;
    for (std::size_t g = 0; g < groups.size(); ++g) {
      if ((groups[g].id | kGroupKeyBit) == key) {
        return state.display.LineOfGroup(static_cast<std::int32_t>(g));
      }
    }
    return -1;
  }
  const std::int64_t pos = view.PositionOfKey(key);
  return pos < 0 ? -1
                 : state.display.LineOfPos(static_cast<std::uint32_t>(pos));
}

// Fixes up the cached cursor position after the lines changed.
void SyncCursor(const data::TableView &view, GridState &state) {
  if (state.cursor_generation == state.display_version) {
    return;
  }
  state.cursor_generation = state.display_version;
  state.cursor_pos =
      state.cursor ? LineOfKey(view, state, *state.cursor) : std::int64_t{-1};
  if (state.anchor == state.cursor) {
    state.anchor_pos = state.cursor_pos;
  } else {
    state.anchor_pos =
        state.anchor ? LineOfKey(view, state, *state.anchor) : std::int64_t{-1};
  }
}

// Makes the cursor row the anchor of range selections.
void SetAnchor(GridState &state) {
  state.anchor = state.cursor;
  state.anchor_pos = state.cursor_pos;
}

void SetCursor(const data::TableView &view, GridState &state,
               std::int64_t line) {
  const std::int64_t n = LineCount(state);
  if (n == 0) {
    state.cursor.reset();
    state.cursor_pos = -1;
    return;
  }
  line = std::clamp<std::int64_t>(line, 0, n - 1);
  state.cursor_pos = line;
  state.cursor = LineKey(view, state, line);
}

auto CursorOnData(const GridState &state) -> bool {
  return state.cursor && (*state.cursor & kGroupKeyBit) == 0;
}

// Selects the data rows on lines a..b.
void SelectRange(const data::TableView &view, GridState &state, std::int64_t a,
                 std::int64_t b) {
  state.selection.Clear();
  if (a > b) {
    std::swap(a, b);
  }
  for (std::int64_t line = std::max<std::int64_t>(0, a);
       line <= b && line < LineCount(state);
       ++line) {
    if (const auto row = LineRow(view, state, line)) {
      state.selection.Set(view.source().RowKey(*row), true);
    }
  }
}

// Tree data: depth and flags of the row on `line` (data lines only).
struct TreeInfo {
  bool tree = false;
  std::uint16_t depth = 0;
  std::uint8_t flags = 0;
};

auto TreeAt(const data::TableView &view, const GridState &state,
            std::int64_t line) -> TreeInfo {
  const data::RowView &v = view.view();
  if (v.depth.empty() || line < 0 || line >= LineCount(state)) {
    return {};
  }
  const data::DisplayRows::Line l =
      state.display.At(static_cast<std::size_t>(line));
  if (l.group >= 0) {
    return {};
  }
  return {true, v.depth[l.pos], v.tree_flags[l.pos]};
}

// Collapses (or expands) the tree row on `line`.
void ToggleTree(const data::TableView &view, GridState &state,
                std::int64_t line, std::optional<bool> collapse = {}) {
  const auto row = LineRow(view, state, line);
  if (!row) {
    return;
  }
  const std::uint64_t key = view.source().RowKey(*row);
  const bool now = collapse.value_or(!state.collapsed.contains(key));
  if (now) {
    state.collapsed.insert(key);
  } else {
    state.collapsed.erase(key);
  }
  state.display_dirty = true;
}

// Collapses (or expands) the group on `line`; toggles without `collapse`.
void ToggleGroup(const data::TableView &view, GridState &state,
                 std::int64_t line, std::optional<bool> collapse = {}) {
  const data::DisplayRows::Line l =
      state.display.At(static_cast<std::size_t>(line));
  if (l.group < 0) {
    return;
  }
  const std::uint64_t id =
      view.view().groups[static_cast<std::size_t>(l.group)].id;
  const bool now = collapse.value_or(!state.collapsed.contains(id));
  if (now) {
    state.collapsed.insert(id);
  } else {
    state.collapsed.erase(id);
  }
  state.display_dirty = true;
}

// Width needed for the header and the visible rows of column `index`.
auto AutoWidth(Context &ctx, const data::TableView &view,
               const GridState &state, int index, std::int64_t first,
               std::int64_t count) -> float {
  const Theme &t = ctx.theme();
  const GridColumn &c = state.columns[static_cast<std::size_t>(index)];
  const ColumnInfo &info = view.source().Columns()[c.column];
  float w = ctx.text().Measure(
                info.title, t.fonts.bold, t.metrics.small_font_size + 1) +
            3 * kCellPad + 14;
  std::string cell;
  for (std::int64_t p = first; p < first + count && p < LineCount(state); ++p) {
    const auto row = LineRow(view, state, p);
    if (!row) {
      continue;
    }
    view.source().FormatCell(*row, c.column, cell);
    w = std::max(w,
                 ctx.text().Measure(cell, t.fonts.sans, t.metrics.font_size) +
                     2 * kCellPad + 2);
  }
  return std::clamp(std::ceil(w), c.min_width, 640.0F);
}

// Type-to-jump: the next row whose jump column starts with `prefix`.
auto FindPrefix(const data::TableView &view, const GridState &state,
                int source_col, std::string_view prefix, std::int64_t from)
    -> std::int64_t {
  const auto rows = view.rows();
  const std::int64_t n = LineCount(state);
  if (n == 0) {
    return -1;
  }
  const data::TableSource &src = view.source();
  const ColumnInfo &info = src.Columns()[source_col];
  // Sorted ascending by this text column (and not grouped): binary search.
  if (!state.display.grouped() && !state.query.sort.empty() &&
      state.query.sort[0].column == source_col &&
      state.query.sort[0].direction == SortDirection::kAscending &&
      info.type == ColumnType::kText) {
    const auto it = std::lower_bound(
        rows.begin(), rows.end(), prefix, [&](RowIndex r, std::string_view p) {
          return Fold(src.GetText(source_col, r)) < p;
        });
    if (it != rows.end() &&
        Fold(src.GetText(source_col, *it)).starts_with(prefix)) {
      return it - rows.begin();
    }
    return -1;
  }
  std::string cell;
  for (std::int64_t k = 0; k < n; ++k) {
    const std::int64_t p = (from + k) % n;
    const auto row = LineRow(view, state, p);
    if (!row) {
      continue;
    }
    src.FormatCell(*row, source_col, cell);
    if (Fold(cell).starts_with(prefix)) {
      return p;
    }
  }
  return -1;
}

// Keys while the grid has focus.
// @return True if the cursor moved and should be scrolled into view.
auto HandleKeys(Context &ctx, data::TableView &view, GridState &state,
                const std::vector<int> &visible, int page, GridResult &result)
    -> bool {
  bool reveal_cursor = false;
  const std::int64_t n = LineCount(state);
  auto move = [&](std::int64_t pos, std::uint8_t mods) {
    if (n == 0) {
      return;
    }
    const std::int64_t anchor_pos =
        state.anchor ? state.anchor_pos : state.cursor_pos;
    SetCursor(view, state, pos);
    reveal_cursor = true;
    if ((mods & kModShift) != 0) {
      SelectRange(view,
                  state,
                  anchor_pos < 0 ? state.cursor_pos : anchor_pos,
                  state.cursor_pos);
      if (!state.anchor) {
        SetAnchor(state);
      }
    } else if ((mods & kModCtrl) == 0) {
      state.selection.Clear();
      if (CursorOnData(state)) {
        state.selection.Set(*state.cursor, true);
      }
      SetAnchor(state);
    }
  };
  const bool on_group =
      state.cursor && !CursorOnData(state) && state.cursor_pos >= 0;
  const std::int64_t cur = std::max<std::int64_t>(state.cursor_pos, -1);
  std::uint8_t mods = 0;
  while (ctx.ConsumeKeyAnyMods(Key::kDown, mods)) {
    move(cur + 1, mods);
  }
  while (ctx.ConsumeKeyAnyMods(Key::kUp, mods)) {
    move(std::max<std::int64_t>(0, cur - 1), mods);
  }
  while (ctx.ConsumeKeyAnyMods(Key::kPageDown, mods)) {
    move(cur + page, mods);
  }
  while (ctx.ConsumeKeyAnyMods(Key::kPageUp, mods)) {
    move(std::max<std::int64_t>(0, cur - page), mods);
  }
  while (ctx.ConsumeKeyAnyMods(Key::kHome, mods)) {
    move(0, mods);
  }
  while (ctx.ConsumeKeyAnyMods(Key::kEnd, mods)) {
    move(n - 1, mods);
  }
  if (ctx.ConsumeKey(Key::kLeft, kModAlt)) {
    // Move the cursor column left.
    if (state.cursor_col > 0) {
      std::swap(state.columns[static_cast<std::size_t>(
                    visible[static_cast<std::size_t>(state.cursor_col)])],
                state.columns[static_cast<std::size_t>(
                    visible[static_cast<std::size_t>(state.cursor_col) - 1])]);
      --state.cursor_col;
    }
  }
  if (ctx.ConsumeKey(Key::kRight, kModAlt)) {
    if (state.cursor_col + 1 < static_cast<int>(visible.size())) {
      std::swap(state.columns[static_cast<std::size_t>(
                    visible[static_cast<std::size_t>(state.cursor_col)])],
                state.columns[static_cast<std::size_t>(
                    visible[static_cast<std::size_t>(state.cursor_col) + 1])]);
      ++state.cursor_col;
    }
  }
  // Tree rows: Left collapses or goes to the parent, Right expands or goes
  // to the first child.
  const TreeInfo tree = TreeAt(view, state, state.cursor_pos);
  if (tree.tree && state.cursor) {
    const bool has_children = (tree.flags & data::kTreeHasChildren) != 0;
    const bool collapsed = state.collapsed.contains(*state.cursor);
    if (ctx.ConsumeKey(Key::kLeft)) {
      if (has_children && !collapsed) {
        ToggleTree(view, state, state.cursor_pos, true);
      } else {
        for (std::int64_t l = state.cursor_pos - 1; l >= 0; --l) {
          if (TreeAt(view, state, l).depth < tree.depth) {
            move(l, kModNone);
            break;
          }
        }
      }
    }
    if (ctx.ConsumeKey(Key::kRight)) {
      if (has_children && collapsed) {
        ToggleTree(view, state, state.cursor_pos, false);
      } else if (has_children) {
        move(state.cursor_pos + 1, kModNone);
      }
    }
  }
  // On a group header, Left collapses and Right expands.
  if (on_group && ctx.ConsumeKey(Key::kLeft)) {
    ToggleGroup(view, state, state.cursor_pos, true);
  }
  if (on_group && ctx.ConsumeKey(Key::kRight)) {
    ToggleGroup(view, state, state.cursor_pos, false);
  }
  if (ctx.ConsumeKey(Key::kLeft)) {
    state.cursor_col = std::max(0, state.cursor_col - 1);
    reveal_cursor = true;
  }
  if (ctx.ConsumeKey(Key::kRight)) {
    state.cursor_col =
        std::min(static_cast<int>(visible.size()) - 1, state.cursor_col + 1);
    reveal_cursor = true;
  }
  if (ctx.ConsumeKey(Key::kA, kModCtrl)) {
    state.selection.SelectAll();
  }
  if (ctx.ConsumeKey(Key::kC, kModCtrl)) {
    ctx.clipboard().set(CopyAsTsv(view, state));
  }
  if (!state.selection.Empty() && ctx.ConsumeKey(Key::kEscape)) {
    state.selection.Clear();
  }
  if (state.cursor && ctx.ConsumeKey(Key::kSpace, kModNone, true)) {
    if (on_group) {
      ToggleGroup(view, state, state.cursor_pos);
    } else {
      state.selection.Set(*state.cursor,
                          !state.selection.Contains(*state.cursor));
    }
  }
  if (ctx.ConsumeKey(Key::kEnter) && state.cursor_pos >= 0 &&
      state.cursor_pos < n) {
    if (on_group) {
      ToggleGroup(view, state, state.cursor_pos);
    } else {
      result.activated = true;
      result.row = LineRow(view, state, state.cursor_pos);
    }
  }
  // Type to jump on the primary sort column, else the cursor column.
  const std::string typed = ctx.TakeText();
  if (!typed.empty() && typed != " " && !visible.empty()) {
    int jump_col = state
                       .columns[static_cast<std::size_t>(
                           visible[static_cast<std::size_t>(state.cursor_col)])]
                       .column;
    if (!state.query.sort.empty()) {
      jump_col = state.query.sort[0].column;
    }
    if (ctx.time() - state.jump_time > kJumpTimeout) {
      state.jump_prefix.clear();
    }
    const std::string folded = Fold(typed);
    const bool repeat =
        state.jump_prefix.empty() || state.jump_prefix == folded;
    state.jump_prefix = repeat ? folded : state.jump_prefix + folded;
    state.jump_time = ctx.time();
    const std::int64_t found = FindPrefix(
        view,
        state,
        jump_col,
        state.jump_prefix,
        std::max<std::int64_t>(0, state.cursor_pos + (repeat ? 1 : 0)));
    if (found >= 0) {
      move(found, kModNone);
    }
  }
  return reveal_cursor;
}

// Pointer input: selection, sorting, resizing, reordering, menus.
// @return True if the cursor moved and should be scrolled into view.
auto HandlePointer(Context &ctx, data::TableView &view, GridState &state,
                   const Interaction &it, const Geometry &g,
                   const std::vector<int> &visible, Id id, Id menu_id,
                   GridResult &result) -> bool {
  bool reveal_cursor = false;
  const std::int64_t n = LineCount(state);
  const data::TableSource &src = view.source();
  const double viewport = std::max(0.0F, g.rows.Height());
  const Vec2 mouse = ctx.mouse();
  // A press and release within one frame still counts as a press.
  if ((it.pressed || it.clicked) && !state.pressing) {
    state.pressing = true;
    const Vec2 press = ctx.press_pos();
    const Hit hit = HitAt(g, press, state.scroll_y, n);
    ctx.SetFocus(id, false);
    state.drag_moved = false;
    if (hit.kind == Hit::Kind::kHeaderEdge) {
      state.resizing = hit.col;
      state.resize_start =
          state.columns[static_cast<std::size_t>(hit.col)].width;
    } else if (hit.kind == Hit::Kind::kHeader) {
      state.dragging = hit.col;
    } else if (hit.kind == Hit::Kind::kCell && !LineRow(view, state, hit.pos)) {
      // A group header: the cursor goes there and the group toggles.
      SetCursor(view, state, hit.pos);
      ToggleGroup(view, state, hit.pos);
    } else if (const TreeInfo tree = hit.kind == Hit::Kind::kCell
                                         ? TreeAt(view, state, hit.pos)
                                         : TreeInfo{};
               tree.tree && (tree.flags & data::kTreeHasChildren) != 0 &&
               !g.cols.empty() && hit.col == g.cols.front().index &&
               press.x >= g.cols.front().x0 + kCellPad + 14.0F * tree.depth &&
               press.x <
                   g.cols.front().x0 + kCellPad + 14.0F * tree.depth + 16.0F) {
      // The chevron of a tree row.
      SetCursor(view, state, hit.pos);
      ToggleTree(view, state, hit.pos);
    } else if (hit.kind == Hit::Kind::kCell) {
      const std::uint8_t mods = ctx.press_mods();
      const std::int64_t anchor_pos = state.anchor ? state.anchor_pos : -1;
      SetCursor(view, state, hit.pos);
      for (std::size_t v = 0; v < visible.size(); ++v) {
        if (visible[v] == hit.col) {
          state.cursor_col = static_cast<int>(v);
        }
      }
      if ((mods & kModShift) != 0 && anchor_pos >= 0) {
        SelectRange(view, state, anchor_pos, hit.pos);
      } else if ((mods & kModCtrl) != 0) {
        state.selection.Set(*state.cursor,
                            !state.selection.Contains(*state.cursor));
        SetAnchor(state);
      } else {
        state.selection.Clear();
        state.selection.Set(*state.cursor, true);
        SetAnchor(state);
      }
    }
  } else if (it.pressed) {
    if (state.resizing >= 0) {
      GridColumn &c = state.columns[static_cast<std::size_t>(state.resizing)];
      c.width = std::max(c.min_width, state.resize_start + it.drag.x);
    } else if (state.dragging >= 0) {
      if (std::fabs(it.drag.x) > kDragThreshold) {
        state.drag_moved = true;
      }
      if (state.drag_moved) {
        // Swap with the neighbour once the pointer passes its middle.
        for (const ColSpan &c : g.cols) {
          if (c.index == state.dragging ||
              c.pinned !=
                  state.columns[static_cast<std::size_t>(state.dragging)]
                      .pinned) {
            continue;
          }
          const float mid = (c.x0 + c.x1) * 0.5F;
          const bool right = c.index > state.dragging && mouse.x > mid;
          const bool left = c.index < state.dragging && mouse.x < mid;
          if (right || left) {
            auto &cols = state.columns;
            const GridColumn moved =
                cols[static_cast<std::size_t>(state.dragging)];
            cols.erase(cols.begin() + state.dragging);
            cols.insert(cols.begin() + c.index, moved);
            state.dragging = c.index;
            break;
          }
        }
      }
    } else if (state.cursor && std::fabs(it.drag.y) > kDragThreshold) {
      // Drag-select.
      const Hit hit = HitAt(g,
                            {std::clamp(mouse.x, g.rows.x0, g.rows.x1 - 1),
                             std::clamp(mouse.y, g.rows.y0, g.rows.y1 - 1)},
                            state.scroll_y,
                            n);
      if (hit.kind == Hit::Kind::kCell && state.anchor) {
        const std::int64_t anchor_pos = state.anchor_pos;
        SetCursor(view, state, hit.pos);
        SelectRange(view, state, anchor_pos, hit.pos);
        reveal_cursor = true;
      }
    }
  }
  if (!it.pressed) {
    state.pressing = false;
  }
  if (it.clicked && state.dragging >= 0 && !state.drag_moved) {
    ToggleSort(state.query.sort,
               state.columns[static_cast<std::size_t>(state.dragging)].column,
               (ctx.press_mods() & kModShift) != 0);
  }
  if (it.double_clicked) {
    const Hit hit = HitAt(g, mouse, state.scroll_y, n);
    if (hit.kind == Hit::Kind::kHeaderEdge) {
      const auto first = static_cast<std::int64_t>(state.scroll_y / g.row_h);
      state.columns[static_cast<std::size_t>(hit.col)].width =
          AutoWidth(ctx,
                    view,
                    state,
                    hit.col,
                    first,
                    static_cast<std::int64_t>(viewport / g.row_h) + 1);
    } else if (hit.kind == Hit::Kind::kCell) {
      if (const auto row = LineRow(view, state, hit.pos)) {
        result.activated = true;
        result.row = row;
      }
    }
  }
  if (!it.pressed) {
    state.resizing = -1;
    state.dragging = -1;
  }
  if (it.right_clicked) {
    const Hit hit = HitAt(g, mouse, state.scroll_y, n);
    if (hit.kind == Hit::Kind::kHeader || hit.kind == Hit::Kind::kHeaderEdge) {
      state.menu_column = hit.col;
      ctx.OpenPopup(menu_id);
    } else if (const auto line_row = hit.kind == Hit::Kind::kCell
                                         ? LineRow(view, state, hit.pos)
                                         : std::nullopt) {
      const RowIndex r = *line_row;
      if (!state.selection.Contains(src.RowKey(r))) {
        state.selection.Clear();
        state.selection.Set(src.RowKey(r), true);
      }
      SetCursor(view, state, hit.pos);
      result.context_row = r;
      result.context_column =
          state.columns[static_cast<std::size_t>(hit.col)].column;
    }
  }
  return reveal_cursor;
}

// Draws a group header line: chevron, key and count in the first column,
// aggregates under their columns.
void PaintGroupLine(PaintContext &pc, const Geometry &geo,
                    const GridState &state, const data::TableView &view,
                    const data::RowGroup &group, const Rect &row_rect,
                    const std::vector<data::Aggregate> &aggregates,
                    bool cursor) {
  const Theme &th = pc.theme;
  const Palette &c = th.colors;
  gfx::DrawList &list = pc.list;
  const auto infos = view.source().Columns();
  const float small = th.metrics.small_font_size + 1;
  list.FillRect(row_rect, Mix(c.surface_raised, c.primary, 0.06F));
  list.FillRect({row_rect.x0, row_rect.y1 - 1, row_rect.x1, row_rect.y1},
                c.divider);
  const bool collapsed = state.collapsed.contains(group.id);
  const float level = 14.0F * group.level;
  const float x = geo.body.x0 + kCellPad + level;
  const float cy = std::round(row_rect.Center().y);
  // Chevron: right when collapsed, down when open.
  if (collapsed) {
    list.Line({x + 3, cy - 4}, {x + 7, cy}, 1.5F, c.text_muted);
    list.Line({x + 7, cy}, {x + 3, cy + 4}, 1.5F, c.text_muted);
  } else {
    list.Line({x + 1, cy - 2}, {x + 5, cy + 2}, 1.5F, c.text_muted);
    list.Line({x + 5, cy + 2}, {x + 9, cy - 2}, 1.5F, c.text_muted);
  }
  std::string label = group.key.empty() ? "(blank)" : group.key;
  label += "  ";
  std::string n;
  data::FormatInt(group.count, true, n);
  label += "(" + n + ")";
  // The label may run across columns; aggregates paint over it only where
  // there is one.
  float label_right = geo.body.x1 - kCellPad;
  for (const ColSpan &span : geo.cols) {
    const GridColumn &col = state.columns[static_cast<std::size_t>(span.index)];
    for (const data::Aggregate &a : aggregates) {
      if (a.column == col.column && span.x0 > x + 40) {
        label_right = std::min(label_right, span.x0);
      }
    }
  }
  list.PushClip(row_rect);
  pc.text.DrawFitted(list,
                     label,
                     th.fonts.bold,
                     th.metrics.font_size,
                     {x + 16, row_rect.y0, label_right - kCellPad, row_rect.y1},
                     c.text);
  for (std::size_t i = 0; i < aggregates.size() && i < group.values.size();
       ++i) {
    const data::Aggregate &a = aggregates[i];
    const double v = group.values[i];
    if (std::isnan(v)) {
      continue;
    }
    for (const ColSpan &span : geo.cols) {
      const GridColumn &col =
          state.columns[static_cast<std::size_t>(span.index)];
      if (col.column != a.column) {
        continue;
      }
      const ColumnInfo &info = infos[col.column];
      std::string text;
      switch (a.op) {
        case data::AggregateOp::kSum:
          text = "\xCE\xA3 ";
          break;
        case data::AggregateOp::kMean:
          text = "avg ";
          break;
        case data::AggregateOp::kMin:
          text = "min ";
          break;
        case data::AggregateOp::kMax:
          text = "max ";
          break;
        case data::AggregateOp::kCount:
          text = "n ";
          break;
      }
      if (info.type == ColumnType::kDouble ||
          a.op == data::AggregateOp::kMean) {
        data::FormatDouble(v, info.decimals, info.group_digits, text);
      } else {
        data::FormatInt(static_cast<std::int64_t>(std::llround(v)),
                        info.group_digits,
                        text);
      }
      if (a.op != data::AggregateOp::kCount) {
        text += info.unit;
      }
      const Rect region = span.pinned ? Rect{geo.body.x0,
                                             geo.body.y0,
                                             geo.body.x0 + geo.pinned_w,
                                             geo.body.y1}
                                      : geo.ScrollRegion();
      list.PushClip(
          Rect{span.x0, row_rect.y0, span.x1, row_rect.y1}.Intersect(region));
      pc.text.DrawFitted(
          list,
          text,
          th.fonts.bold,
          small,
          {span.x0 + kCellPad, row_rect.y0, span.x1 - kCellPad, row_rect.y1},
          c.text_muted,
          text::HAlign::kRight);
      list.PopClip();
    }
  }
  list.PopClip();
  if (cursor) {
    list.StrokeRect(row_rect.Inset(0.5F), c.focus, 1.0F);
  }
}

// Draws the grid: rows, cells, header, status bar.
void PaintGrid(PaintContext &pc, const Rect &r, const data::TableView &view,
               const GridState &state, bool focused, bool busy) {
  const Theme &th = pc.theme;
  const Palette &c = th.colors;
  const data::TableSource &s = view.source();
  const auto infos2 = s.Columns();
  const auto rows2 = view.rows();
  const std::int64_t count = LineCount(state);
  const Geometry geo = MakeGeometry(state, r, th);
  const auto &groups = view.view().groups;
  const auto &aggregates = view.view().query.aggregates;
  const bool tree = !view.view().depth.empty();
  const float indent =
      tree ? 0.0F
           : 14.0F * static_cast<float>(view.view().query.group_by.size());
  gfx::DrawList &list = pc.list;
  const float font = th.metrics.font_size;
  const float small = th.metrics.small_font_size + 1;
  const std::vector<std::string> &terms = view.terms();
  std::string cell;
  std::vector<data::MatchRange> matches;

  auto region_for = [&](const ColSpan &span) {
    return span.pinned ? Rect{geo.body.x0,
                              geo.body.y0,
                              geo.body.x0 + geo.pinned_w,
                              geo.body.y1}
                       : geo.ScrollRegion();
  };

  // Rows.
  const auto first = static_cast<std::int64_t>(state.scroll_y / geo.row_h);
  const float offset = static_cast<float>(
      state.scroll_y - static_cast<double>(first) * geo.row_h);
  list.PushClip(geo.rows);
  float y = geo.rows.y0 - offset;
  data::Explanation scratch;
  for (std::int64_t p = first; p < count && y < geo.rows.y1;
       ++p, y += geo.row_h) {
    const data::DisplayRows::Line line =
        state.display.At(static_cast<std::size_t>(p));
    const Rect row_rect{geo.body.x0, y, geo.body.x1, y + geo.row_h};
    if (line.group >= 0) {
      const data::RowGroup &group =
          groups[static_cast<std::size_t>(line.group)];
      PaintGroupLine(pc,
                     geo,
                     state,
                     view,
                     group,
                     row_rect,
                     aggregates,
                     focused && state.cursor &&
                         *state.cursor == (group.id | kGroupKeyBit));
      continue;
    }
    const RowIndex row = rows2[line.pos];
    const std::uint64_t key = s.RowKey(row);
    if (state.selection.Contains(key)) {
      list.FillRect(
          row_rect,
          focused ? c.selection : c.selection.WithAlpha(c.selection.a * 0.6F));
    } else if (p % 2 == 1) {
      list.FillRect(row_rect, c.surface_alt);
    }
    for (const ColSpan &span : geo.cols) {
      const GridColumn &col =
          state.columns[static_cast<std::size_t>(span.index)];
      const ColumnInfo &info = infos2[col.column];
      const Rect region = region_for(span);
      const Rect cell_rect =
          Rect{span.x0, y, span.x1, y + geo.row_h}.Intersect(region);
      if (cell_rect.Empty()) {
        continue;
      }
      list.PushClip(cell_rect);
      s.FormatCell(row, col.column, cell);
      Color fg = c.text;
      // Grouped rows indent their first column under the group headers;
      // tree rows by depth, after a chevron.
      const bool first_col = span.index == geo.cols.front().index;
      float in = first_col ? indent : 0.0F;
      if (tree && first_col) {
        const float level = 14.0F * view.view().depth[line.pos];
        in = level + 16.0F;
        if ((view.view().tree_flags[line.pos] & data::kTreeHasChildren) != 0) {
          const float cx = span.x0 + kCellPad + level;
          const float cy = std::round(y + geo.row_h * 0.5F);
          if (state.collapsed.contains(key)) {
            list.Line({cx + 3, cy - 4}, {cx + 7, cy}, 1.5F, c.text_muted);
            list.Line({cx + 7, cy}, {cx + 3, cy + 4}, 1.5F, c.text_muted);
          } else {
            list.Line({cx + 1, cy - 2}, {cx + 5, cy + 2}, 1.5F, c.text_muted);
            list.Line({cx + 5, cy + 2}, {cx + 9, cy - 2}, 1.5F, c.text_muted);
          }
        }
      }
      if (tree &&
          (view.view().tree_flags[line.pos] & data::kTreeContext) != 0) {
        // Ancestors shown only for context are dimmed.
        fg = c.text_disabled;
      }
      Rect text_box{
          span.x0 + kCellPad + in, y, span.x1 - kCellPad, y + geo.row_h};
      data::CellIcon cell_icon;
      Rect icon_box;
      const bool has_icon = col.renderer != CellRenderer::kSparkline &&
                            pc.icons != nullptr &&
                            s.GetIcon(row, col.column, cell_icon) &&
                            cell_icon.icon != icon::kNoIcon;
      if (has_icon) {
        // Icons sit in front of the text, as tall as the text line.
        const float side = std::max(10.0F, std::floor(geo.row_h - 6.0F));
        const float top = std::round(y + 0.5F * (geo.row_h - side));
        icon_box = {text_box.x0, top, text_box.x0 + side, top + side};
        text_box.x0 += side + 4.0F;
      }
      switch (col.renderer) {
        case CellRenderer::kBar: {
          const double v = s.GetDouble(col.column, row);
          const double f =
              col.bar_max > 0 ? std::clamp(v / col.bar_max, 0.0, 1.0) : 0;
          if (!std::isnan(f)) {
            list.FillRect(
                {span.x0 + 2,
                 y + 3,
                 span.x0 + 2 + static_cast<float>(f) * (span.x1 - span.x0 - 4),
                 y + geo.row_h - 3},
                c.primary.WithAlpha(0.3F),
                gfx::CornerRadii::All(2));
          }
          break;
        }
        case CellRenderer::kAge: {
          if (col.age_column >= 0) {
            const double age = s.GetDouble(col.age_column, row);
            const double span_age =
                std::max(1e-9, col.age_stale - col.age_fresh);
            const float f = static_cast<float>(
                std::clamp((age - col.age_fresh) / span_age, 0.0, 1.0));
            fg = Mix(c.text, c.text_disabled, f);
          }
          break;
        }
        case CellRenderer::kBadge: {
          if (!cell.empty()) {
            const float w = pc.text.Measure(cell, th.fonts.sans, small) + 12;
            list.FillRect({text_box.x0 - 2,
                           y + 3,
                           std::min(text_box.x0 - 2 + w, span.x1 - 2),
                           y + geo.row_h - 3},
                          BadgeColor(c, s.GetInt64(col.column, row)),
                          gfx::CornerRadii::All((geo.row_h - 6) * 0.5F));
          }
          break;
        }
        case CellRenderer::kSparkline: {
          static thread_local std::vector<float> series;
          series.clear();
          if (s.GetSeries(col.column, row, series) && series.size() >= 2) {
            const auto [lo_it, hi_it] =
                std::minmax_element(series.begin(), series.end());
            const float lo = *lo_it;
            const float range = std::max(1e-6F, *hi_it - lo);
            const Rect box{span.x0 + kCellPad,
                           y + 4,
                           span.x1 - kCellPad,
                           y + geo.row_h - 4};
            auto point = [&](std::size_t k) -> Vec2 {
              return {box.x0 + box.Width() * static_cast<float>(k) /
                                   static_cast<float>(series.size() - 1),
                      box.y1 - box.Height() * (series[k] - lo) / range};
            };
            const Color trend =
                series.back() >= series.front() ? c.success : c.error;
            for (std::size_t k = 1; k < series.size(); ++k) {
              list.Line(point(k - 1), point(k), 1.3F, trend);
            }
          }
          cell.clear();
          break;
        }
        case CellRenderer::kText:
          break;
      }
      if (has_icon) {
        pc.icons->DrawInRect(list,
                             cell_icon.icon,
                             icon_box,
                             cell_icon.color.a > 0.0F ? cell_icon.color : fg);
      }
      const text::HAlign align =
          IsRightAligned(info) && col.renderer != CellRenderer::kBadge
              ? text::HAlign::kRight
              : text::HAlign::kLeft;
      const Rect box =
          col.renderer == CellRenderer::kBadge
              ? Rect{text_box.x0 + 4, y, text_box.x1, y + geo.row_h}
              : text_box;
      const float size = col.renderer == CellRenderer::kBadge ? small : font;
      // Search highlights behind the text.
      if (!terms.empty() && info.searchable) {
        matches.clear();
        data::FindMatches(cell, terms, matches);
        if (!matches.empty()) {
          const text::ShapedText &shaped =
              pc.text.Shape(cell, th.fonts.sans, size);
          const float x0 = align == text::HAlign::kRight
                               ? std::round(box.x1 - shaped.width)
                               : box.x0;
          for (const data::MatchRange &m : matches) {
            list.FillRect({x0 + shaped.CaretX(m.begin),
                           y + 3,
                           x0 + shaped.CaretX(m.end),
                           y + geo.row_h - 3},
                          c.highlight,
                          gfx::CornerRadii::All(2));
          }
        }
      }
      pc.text.DrawFitted(list, cell, th.fonts.sans, size, box, fg, align);
      if (info.explained && s.Explain(row, col.column, scratch)) {
        // A small corner tick marks cells with an explanation.
        list.FillRect({span.x1 - 5, y + 1, span.x1 - 1, y + 3}, c.accent);
        list.FillRect({span.x1 - 3, y + 1, span.x1 - 1, y + 5}, c.accent);
      }
      list.PopClip();
    }
    // The cursor.
    if (state.cursor && *state.cursor == key) {
      list.StrokeRect(row_rect.Inset(0.5F), focused ? c.focus : c.border, 1.0F);
    }
  }
  if (count == 0) {
    const bool filtered =
        !view.view().query.search.empty() || !view.view().query.filters.empty();
    pc.text.DrawFitted(list,
                       busy ? "Searching\xE2\x80\xA6"
                            : (filtered ? "No rows match" : "No rows"),
                       th.fonts.sans,
                       font,
                       {geo.rows.x0,
                        geo.rows.y0 + 12,
                        geo.rows.x1,
                        geo.rows.y0 + 12 + geo.row_h},
                       c.text_muted,
                       text::HAlign::kCenter);
  }
  list.PopClip();

  // Column group band: one label over each run of columns sharing a group.
  if (!geo.band.Empty()) {
    list.FillRect(geo.band, Mix(c.surface_raised, c.primary, 0.05F));
    list.FillRect({geo.band.x0, geo.band.y1 - 1, geo.band.x1, geo.band.y1},
                  c.divider);
    std::size_t i = 0;
    while (i < geo.cols.size()) {
      const GridColumn &run =
          state.columns[static_cast<std::size_t>(geo.cols[i].index)];
      std::size_t j = i + 1;
      while (j < geo.cols.size()) {
        const GridColumn &next =
            state.columns[static_cast<std::size_t>(geo.cols[j].index)];
        if (next.group != run.group || next.pinned != run.pinned) {
          break;
        }
        ++j;
      }
      const Rect span{
          geo.cols[i].x0, geo.band.y0, geo.cols[j - 1].x1, geo.band.y1 - 1};
      const Rect clip = span.Intersect(region_for(geo.cols[i]));
      if (!run.group.empty() && !clip.Empty()) {
        list.PushClip(clip);
        pc.text.DrawFitted(list,
                           run.group,
                           th.fonts.bold,
                           small,
                           span.Inset(kCellPad),
                           c.text_muted,
                           text::HAlign::kCenter);
        list.PopClip();
        list.FillRect({span.x1 - 1, span.y0 + 4, span.x1, span.y1 - 3},
                      c.divider);
      }
      i = j;
    }
  }
  // Header.
  list.FillRect(geo.header, c.surface_raised);
  list.FillRect(
      {geo.header.x0, geo.header.y1 - 1, geo.header.x1, geo.header.y1},
      c.border);
  for (const ColSpan &span : geo.cols) {
    const GridColumn &col = state.columns[static_cast<std::size_t>(span.index)];
    const ColumnInfo &info = infos2[col.column];
    const Rect region = region_for(span);
    const Rect head =
        Rect{span.x0, geo.header.y0, span.x1, geo.header.y1 - 1}.Intersect(
            region);
    if (head.Empty()) {
      continue;
    }
    list.PushClip(head);
    if (state.dragging == span.index && state.drag_moved) {
      list.FillRect(head, c.active);
    }
    // Sort indicator (with the key's rank when sorting by several).
    float right = span.x1 - kCellPad;
    const auto &sort = state.query.sort;
    for (std::size_t k = 0; k < sort.size(); ++k) {
      if (sort[k].column != col.column) {
        continue;
      }
      const float cx = right - 4;
      const float cy = std::round(head.Center().y);
      const bool up = sort[k].direction == SortDirection::kAscending;
      const float d = up ? -1.0F : 1.0F;
      list.Line({cx - 4, cy - 2 * d}, {cx, cy + 2 * d}, 1.5F, c.primary);
      list.Line({cx, cy + 2 * d}, {cx + 4, cy - 2 * d}, 1.5F, c.primary);
      right -= 12;
      if (sort.size() > 1) {
        const std::string rank = std::to_string(k + 1);
        pc.text.DrawFitted(list,
                           rank,
                           th.fonts.sans,
                           small - 2,
                           {right - 8, head.y0, right, head.y1},
                           c.primary,
                           text::HAlign::kRight);
        right -= 10;
      }
    }
    // Filter marker.
    for (const data::Filter &f : state.query.filters) {
      if (f.column == col.column) {
        list.FillCircle({right - 3, std::round(head.Center().y)}, 3, c.accent);
        right -= 10;
        break;
      }
    }
    const text::HAlign align =
        IsRightAligned(info) ? text::HAlign::kRight : text::HAlign::kLeft;
    pc.text.DrawFitted(list,
                       col.title.empty() ? info.title : col.title,
                       th.fonts.bold,
                       small,
                       {span.x0 + kCellPad, head.y0, right, head.y1},
                       c.text_muted,
                       align);
    list.PopClip();
    // Column edge.
    if (span.x1 > region.x0 && span.x1 <= region.x1) {
      list.FillRect(
          {span.x1 - 1, geo.header.y0 + 5, span.x1, geo.header.y1 - 5},
          c.divider);
    }
  }
  if (geo.pinned_w > 0) {
    list.FillRect({geo.body.x0 + geo.pinned_w - 1,
                   geo.body.y0,
                   geo.body.x0 + geo.pinned_w,
                   geo.status.y0},
                  c.border);
  }

  // Status bar: counts and progress.
  list.FillRect(geo.status, c.surface_raised);
  list.FillRect(
      {geo.status.x0, geo.status.y0, geo.status.x1, geo.status.y0 + 1},
      c.divider);
  std::string status;
  data::FormatInt(static_cast<std::int64_t>(rows2.size()), true, status);
  if (rows2.size() != s.RowCount()) {
    status += " of ";
    data::FormatInt(s.RowCount(), true, status);
  }
  if (state.display.grouped()) {
    // Lines include group headers; count data rows and groups instead.
    status.clear();
    data::FormatInt(static_cast<std::int64_t>(rows2.size()), true, status);
    status += rows2.size() == 1 ? " row in " : " rows in ";
    data::FormatInt(static_cast<std::int64_t>(groups.size()), true, status);
    status += groups.size() == 1 ? " group" : " groups";
  } else {
    status += count == 1 ? " row" : " rows";
  }
  if (!state.selection.Empty()) {
    status += "  \xC2\xB7  ";
    if (state.selection.all) {
      status += "all";
    } else {
      data::FormatInt(
          static_cast<std::int64_t>(state.selection.keys.size()), true, status);
    }
    status += " selected";
  }
  if (busy) {
    status += "  \xC2\xB7  updating\xE2\x80\xA6";
  }
  pc.text.DrawFitted(list,
                     status,
                     th.fonts.sans,
                     small,
                     {geo.status.x0 + kCellPad,
                      geo.status.y0,
                      geo.status.x1 - kCellPad,
                      geo.status.y1},
                     c.text_muted);
  // Horizontal position indicator.
  const float avail = geo.body.Width() - geo.pinned_w;
  if (geo.content_w - geo.pinned_w > avail && avail > 0) {
    const float scroll_w = geo.content_w - geo.pinned_w;
    const float w = std::max(24.0F, avail * avail / scroll_w);
    const float x = geo.body.x0 + geo.pinned_w +
                    (avail - w) * state.scroll_x / (scroll_w - avail);
    list.FillRect({x, geo.status.y1 - 4, x + w, geo.status.y1 - 1},
                  c.text_disabled,
                  gfx::CornerRadii::All(1.5F));
  }
}

// The header menu of a column: sort, filter, pin, hide, autosize.
void HeaderMenu(Context &ctx, data::TableView &view, GridState &state,
                const Geometry &g, Id menu_id, std::size_t visible_count) {
  const Theme &t = ctx.theme();
  const auto infos = view.source().Columns();
  const Vec2 mouse = ctx.mouse();
  const double viewport = std::max(0.0F, g.rows.Height());
  if (ctx.IsPopupOpen(menu_id) && state.menu_column >= 0 &&
      state.menu_column < static_cast<int>(state.columns.size())) {
    GridColumn &col =
        state.columns[static_cast<std::size_t>(state.menu_column)];
    const ColumnInfo &info = infos[col.column];
    Rect anchor{mouse.x, g.header.y1, mouse.x + 1, g.header.y1};
    for (const ColSpan &span : g.cols) {
      if (span.index == state.menu_column) {
        anchor = {span.x0, g.header.y0, span.x1, g.header.y1};
      }
    }
    ctx.BeginLayer(LayerKind::kPopup, menu_id, anchor);
    ctx.PushId(menu_id);
    auto item = [&](std::string_view text) {
      return Button(ctx, text, ButtonKind::kFlat);
    };
    bool close = false;
    if (item("Sort ascending")) {
      state.query.sort = {{col.column, SortDirection::kAscending}};
      close = true;
    }
    if (item("Sort descending")) {
      state.query.sort = {{col.column, SortDirection::kDescending}};
      close = true;
    }
    if (!state.query.sort.empty() && item("Clear sort")) {
      state.query.sort.clear();
      close = true;
    }
    Separator(ctx);
    // Grouping and group totals.
    auto &group_by = state.query.group_by;
    const bool grouped_here =
        std::find(group_by.begin(), group_by.end(), col.column) !=
        group_by.end();
    if (!grouped_here && item("Group by this column")) {
      group_by.push_back(col.column);
      close = true;
    }
    if (grouped_here && item("Ungroup this column")) {
      std::erase(group_by, col.column);
      close = true;
    }
    if (!group_by.empty() && item("Remove all grouping")) {
      group_by.clear();
      state.collapsed.clear();
      close = true;
    }
    if (!group_by.empty() && info.IsNumeric()) {
      static const std::vector<std::string> kTotals{
          "No total", "Sum", "Average", "Minimum", "Maximum"};
      static const data::AggregateOp kOps[] = {data::AggregateOp::kSum,
                                               data::AggregateOp::kSum,
                                               data::AggregateOp::kMean,
                                               data::AggregateOp::kMin,
                                               data::AggregateOp::kMax};
      auto &aggs = state.query.aggregates;
      auto found =
          std::find_if(aggs.begin(), aggs.end(), [&](const data::Aggregate &a) {
            return a.column == col.column;
          });
      int choice = 0;
      if (found != aggs.end()) {
        for (int k = 1; k < 5; ++k) {
          if (kOps[k] == found->op) {
            choice = k;
          }
        }
      }
      LayoutSpec totals_row;
      totals_row.gap = 6;
      totals_row.align = Align::kCenter;
      BeginRow(ctx, totals_row);
      LabelOptions totals_label;
      totals_label.muted = true;
      Label(ctx, "Group total", totals_label);
      if (Dropdown(ctx, "##total", choice, kTotals)) {
        std::erase_if(aggs, [&](const data::Aggregate &a) {
          return a.column == col.column;
        });
        if (choice > 0) {
          aggs.push_back({col.column, kOps[choice]});
        }
      }
      End(ctx);
    }
    Separator(ctx);
    // Filter editor for this column.
    const auto existing = std::find_if(
        state.query.filters.begin(),
        state.query.filters.end(),
        [&](const data::Filter &f) { return f.column == col.column; });
    const bool had_filter = existing != state.query.filters.end();

    struct FilterEdit {
      int column = -1;
      int op = 0;
      std::string value;
    };

    FilterEdit &edit = ctx.State<FilterEdit>(HashCombine(menu_id, 0x66ULL));
    if (edit.column != col.column) {
      edit = FilterEdit{};
      edit.column = col.column;
      if (had_filter) {
        if (info.IsNumeric()) {
          data::FormatDouble(existing->a, info.decimals, false, edit.value);
        } else {
          edit.value = existing->text;
        }
      }
    }
    static const std::vector<std::string> kNumericOps{
        "\xE2\x89\xA5", "\xE2\x89\xA4", "=", "\xE2\x89\xA0", ">", "<"};
    static const data::FilterOp kNumericOpValues[] = {
        data::FilterOp::kGreaterEqual,
        data::FilterOp::kLessEqual,
        data::FilterOp::kEqual,
        data::FilterOp::kNotEqual,
        data::FilterOp::kGreater,
        data::FilterOp::kLess};
    LabelOptions muted;
    muted.muted = true;
    muted.size = t.metrics.small_font_size;
    if (info.type == ColumnType::kEnum) {
      Label(ctx, "Show", muted);
      std::vector<std::int64_t> set;
      if (had_filter) {
        set = existing->set;
      } else {
        for (std::size_t v = 0; v < info.enum_labels.size(); ++v) {
          set.push_back(static_cast<std::int64_t>(v));
        }
      }
      bool changed = false;
      for (std::size_t v = 0; v < info.enum_labels.size(); ++v) {
        bool on =
            std::find(set.begin(), set.end(), static_cast<std::int64_t>(v)) !=
            set.end();
        if (Checkbox(ctx, info.enum_labels[v], on)) {
          changed = true;
          if (on) {
            set.push_back(static_cast<std::int64_t>(v));
          } else {
            std::erase(set, static_cast<std::int64_t>(v));
          }
        }
      }
      if (changed) {
        std::erase_if(state.query.filters, [&](const data::Filter &f) {
          return f.column == col.column;
        });
        if (set.size() != info.enum_labels.size()) {
          std::sort(set.begin(), set.end());
          data::Filter f;
          f.column = col.column;
          f.op = data::FilterOp::kInSet;
          f.set = set;
          state.query.filters.push_back(std::move(f));
        }
      }
    } else {
      Label(ctx, info.IsNumeric() ? "Filter" : "Contains", muted);
      LayoutSpec row;
      row.gap = 4;
      row.align = Align::kCenter;
      BeginRow(ctx, row);
      if (info.IsNumeric()) {
        Dropdown(ctx, "##op", edit.op, kNumericOps);
      }
      TextFieldOptions field;
      field.width = SizeSpec::Fixed(110);
      const TextFieldResult r = TextField(ctx, "##value", edit.value, field);
      End(ctx);
      if (r.submitted || item("Apply filter")) {
        std::erase_if(state.query.filters, [&](const data::Filter &f) {
          return f.column == col.column;
        });
        if (!edit.value.empty()) {
          data::Filter f;
          f.column = col.column;
          if (info.IsNumeric()) {
            f.op = kNumericOpValues[std::clamp(edit.op, 0, 5)];
            std::string number;
            for (const char ch : edit.value) {
              if (ch != ',' && ch != ' ') {
                number.push_back(ch);
              }
            }
            f.a = std::strtod(number.c_str(), nullptr);
          } else {
            f.op = data::FilterOp::kContains;
            f.text = edit.value;
          }
          state.query.filters.push_back(f);
        }
        close = true;
      }
    }
    if (had_filter && item("Clear filter")) {
      std::erase_if(state.query.filters, [&](const data::Filter &f) {
        return f.column == col.column;
      });
      edit.value.clear();
      close = true;
    }
    Separator(ctx);
    if (item(col.pinned ? "Unpin column" : "Pin column")) {
      col.pinned = !col.pinned;
      // Keep pinned columns first.
      std::stable_partition(state.columns.begin(),
                            state.columns.end(),
                            [](const GridColumn &c) { return c.pinned; });
      close = true;
    }
    if (item("Autosize column")) {
      col.width = AutoWidth(ctx,
                            view,
                            state,
                            state.menu_column,
                            static_cast<std::int64_t>(state.scroll_y / g.row_h),
                            static_cast<std::int64_t>(viewport / g.row_h) + 1);
      close = true;
    }
    if (visible_count > 1 && item("Hide column")) {
      col.visible = false;
      close = true;
    }
    if (visible_count < state.columns.size() && item("Show all columns")) {
      for (GridColumn &c : state.columns) {
        c.visible = true;
      }
      close = true;
    }
    ctx.PopId();
    ctx.EndLayer();
    if (close) {
      ctx.ClosePopup(menu_id);
    }
  }
}

}  // namespace

auto DefaultColumns(const data::TableSource &source)
    -> std::vector<GridColumn> {
  std::vector<GridColumn> columns;
  const auto infos = source.Columns();
  for (std::size_t i = 0; i < infos.size(); ++i) {
    GridColumn c;
    c.column = static_cast<int>(i);
    c.width = infos[i].type == ColumnType::kText ? 180.0F
              : infos[i].IsNumeric()             ? 110.0F
                                                 : 90.0F;
    if (infos[i].type == ColumnType::kEnum) {
      c.renderer = CellRenderer::kBadge;
    }
    columns.push_back(c);
  }
  return columns;
}

auto SelectedRows(const data::TableView &view, const GridState &state)
    -> std::vector<RowIndex> {
  std::vector<RowIndex> out;
  const auto rows = view.rows();
  if (state.selection.Empty()) {
    if (state.cursor_pos >= 0 &&
        state.cursor_pos < static_cast<std::int64_t>(rows.size())) {
      out.push_back(rows[static_cast<std::size_t>(state.cursor_pos)]);
    }
    return out;
  }
  for (const RowIndex r : rows) {
    if (state.selection.Contains(view.source().RowKey(r))) {
      out.push_back(r);
    }
  }
  return out;
}

auto CopyAsTsv(const data::TableView &view, const GridState &state)
    -> std::string {
  const data::TableSource &src = view.source();
  std::string out;
  std::vector<int> cols;
  for (const GridColumn &c : state.columns) {
    if (c.visible) {
      cols.push_back(c.column);
    }
  }
  auto clean = [](std::string_view s, std::string &o) {
    for (const char ch : s) {
      o.push_back(ch == '\t' || ch == '\n' || ch == '\r' ? ' ' : ch);
    }
  };
  for (std::size_t i = 0; i < cols.size(); ++i) {
    if (i > 0) {
      out.push_back('\t');
    }
    clean(src.Columns()[cols[i]].title, out);
  }
  out.push_back('\n');
  std::string cell;
  for (const RowIndex r : SelectedRows(view, state)) {
    for (std::size_t i = 0; i < cols.size(); ++i) {
      if (i > 0) {
        out.push_back('\t');
      }
      src.FormatCell(r, cols[i], cell);
      clean(cell, out);
    }
    out.push_back('\n');
  }
  return out;
}

auto DataGrid(Context &ctx, std::string_view label, data::TableView &view,
              GridState &state, LayoutSpec layout) -> GridResult {
  const Theme &t = ctx.theme();
  GridResult result;
  const Id id = ctx.GetId(label);
  const Id bar_id = HashCombine(id, 0x76626172ULL);
  const Id menu_id = HashCombine(id, 0x6D656E75ULL);
  const Id tip_id = HashCombine(id, 0x746970ULL);

  if (state.columns.empty()) {
    state.columns = DefaultColumns(view.source());
  }
  if (view.query() != state.query) {
    view.SetQuery(state.query);
  }
  view.Update(ctx.time());
  SyncDisplay(view, state);
  SyncCursor(view, state);

  const std::int64_t n = LineCount(state);
  const data::TableSource &src = view.source();
  const auto infos = src.Columns();
  const Interaction it = ctx.Interact(id);
  const Rect body = it.rect;
  const Geometry g = MakeGeometry(state, body, t);
  const double viewport = std::max(0.0F, g.rows.Height());
  const int page = std::max(1, static_cast<int>(viewport / g.row_h) - 1);
  const std::uint64_t selection_before =
      state.selection.keys.size() + (state.selection.all ? 1ULL << 62U : 0);
  const std::optional<std::uint64_t> cursor_before = state.cursor;
  const data::Query query_before = state.query;
  bool reveal_cursor = false;

  // Visible display columns, for cursor movement.
  std::vector<int> visible;
  for (std::size_t i = 0; i < state.columns.size(); ++i) {
    if (state.columns[i].visible) {
      visible.push_back(static_cast<int>(i));
    }
  }
  state.cursor_col = std::clamp(
      state.cursor_col, 0, std::max(0, static_cast<int>(visible.size()) - 1));

  if (ctx.HasFocus(id)) {
    reveal_cursor = HandleKeys(ctx, view, state, visible, page, result);
  }

  if (HandlePointer(ctx, view, state, it, g, visible, id, menu_id, result)) {
    reveal_cursor = true;
  }
  if (state.reveal && !view.busy() && view.view().query == state.query) {
    // The view is current: the cursor's position is final.
    state.cursor_generation = ~0ULL;
    SyncCursor(view, state);
    reveal_cursor = state.cursor_pos >= 0;
    state.reveal = false;
    if (state.cursor) {
      state.selection.Clear();
      if (CursorOnData(state)) {
        state.selection.Set(*state.cursor, true);
      }
      SetAnchor(state);
    }
  }
  // Expanding or collapsing groups changed the lines.
  SyncDisplay(view, state);
  SyncCursor(view, state);
  const Vec2 mouse = ctx.mouse();

  // Scrolling.
  const Vec2 wheel = ctx.Wheel(id);
  if ((ctx.mods() & kModShift) != 0 && wheel.x == 0.0F) {
    state.scroll_x -= wheel.y * 40.0F;
  } else {
    state.scroll_y -= wheel.y * g.row_h * 3.0;
    state.scroll_x -= wheel.x * 40.0F;
  }
  const double content_h = static_cast<double>(LineCount(state)) * g.row_h;
  if (reveal_cursor && state.cursor_pos >= 0) {
    const double top = static_cast<double>(state.cursor_pos) * g.row_h;
    if (top < state.scroll_y) {
      state.scroll_y = top;
    } else if (top + g.row_h > state.scroll_y + viewport) {
      state.scroll_y = top + g.row_h - viewport;
    }
    if (!visible.empty() && body.Width() > 0) {
      const GridColumn &c = state.columns[static_cast<std::size_t>(
          visible[static_cast<std::size_t>(state.cursor_col)])];
      if (!c.pinned) {
        float x = 0.0F;
        for (const ColSpan &s : g.cols) {
          if (s.index == visible[static_cast<std::size_t>(state.cursor_col)]) {
            x = s.x0 + state.scroll_x - (body.x0 + g.pinned_w);
          }
        }
        const float avail = body.Width() - g.pinned_w;
        if (x < state.scroll_x) {
          state.scroll_x = x;
        } else if (x + c.width > state.scroll_x + avail) {
          state.scroll_x = x + c.width - avail;
        }
      }
    }
  }
  state.scroll_y =
      std::clamp(state.scroll_y, 0.0, std::max(0.0, content_h - viewport));
  state.scroll_x = std::clamp(
      state.scroll_x, 0.0F, std::max(0.0F, g.content_w - body.Width()));

  // Hover, for explanation tooltips.
  std::optional<data::Explanation> tooltip;
  {
    const Hit hit = it.hovered ? HitAt(g, mouse, state.scroll_y, n) : Hit{};
    if (hit.kind == Hit::Kind::kCell) {
      if (hit.pos != state.hover_pos || hit.col != state.hover_col) {
        state.hover_pos = hit.pos;
        state.hover_col = hit.col;
        state.hover_since = ctx.time();
      }
      const GridColumn &c = state.columns[static_cast<std::size_t>(hit.col)];
      data::Explanation e;
      const auto row = LineRow(view, state, hit.pos);
      if (row && infos[c.column].explained &&
          ctx.time() - state.hover_since >= kHoverDelay &&
          src.Explain(*row, c.column, e)) {
        tooltip = std::move(e);
      }
    } else {
      state.hover_pos = -1;
      state.hover_col = -1;
    }
  }

  // Nodes: the body (painted by hand) and the scroll bar.
  if (layout.width.kind == SizeKind::kFit) {
    layout.width = SizeSpec::Grow();
  }
  if (layout.height.kind == SizeKind::kFit) {
    layout.height = SizeSpec::Grow();
  }
  NodeDesc outer;
  outer.layout = layout;
  outer.layout.mode = LayoutMode::kRow;
  outer.style.fill = t.colors.surface;
  outer.style.border = t.colors.border;
  outer.style.border_width = t.metrics.border_width;
  outer.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  outer.layout.padding = Padding::All(1);
  ctx.Open(outer);
  NodeDesc grid;
  grid.id = id;
  grid.layout.width = SizeSpec::Grow();
  grid.layout.height = SizeSpec::Grow();
  grid.flags = kNodeInteractive | kNodeFocusable | kNodeWheel | kNodeClip |
               kNodeFocusRing;
  const int node = ctx.Open(grid);
  ctx.Close();
  const bool focused = ctx.HasFocus(id);
  const bool busy = view.busy();
  ctx.AddPainter(
      node, [&view, &state, focused, busy](PaintContext &pc, const Rect &r) {
        PaintGrid(pc, r, view, state, focused, busy);
      });
  detail::ScrollBar(ctx, bar_id, state.scroll_y, content_h, viewport);
  ctx.Close();

  HeaderMenu(ctx, view, state, g, menu_id, visible.size());

  if (tooltip) {
    ctx.BeginLayer(LayerKind::kTooltip, tip_id);
    LabelOptions opt;
    opt.size = t.metrics.small_font_size + 1;
    Label(ctx, tooltip->reason, opt);
    for (const data::Explanation::Item &line : tooltip->breakdown) {
      std::string text = line.label + "  ";
      data::FormatDouble(line.value, 0, true, text);
      LabelOptions muted;
      muted.muted = true;
      muted.mono = true;
      muted.size = t.metrics.small_font_size;
      Label(ctx, text, muted);
    }
    ctx.EndLayer();
  }

  result.query_changed = state.query != query_before;
  if (result.query_changed) {
    view.SetQuery(state.query);
  }
  result.selection_changed =
      state.cursor != cursor_before ||
      selection_before !=
          state.selection.keys.size() + (state.selection.all ? 1ULL << 62U : 0);
  return result;
}

namespace {

struct CellMenuTarget {
  data::RowIndex row = 0;
  int column = -1;
};

}  // namespace

auto GridCellMenu(Context &ctx, std::string_view label,
                  const data::TableView &view, GridState &state,
                  const GridResult &result) -> bool {
  CellMenuTarget &target =
      ctx.State<CellMenuTarget>(HashCombine(ctx.GetId(label), 0x746774ULL));
  if (result.context_row && result.context_column >= 0) {
    target = {*result.context_row, result.context_column};
    OpenContextMenu(ctx, label);
  }
  if (!BeginContextMenu(ctx, label)) {
    return false;
  }
  const data::Query before = state.query;
  const data::TableSource &src = view.source();
  const ColumnInfo &info = src.Columns()[target.column];
  std::string cell;
  src.FormatCell(target.row, target.column, cell);
  std::string shown =
      cell.size() > 24 ? cell.substr(0, 24) + "\xE2\x80\xA6" : cell;
  if (MenuItem(ctx, "Copy cell")) {
    ctx.clipboard().set(cell);
  }
  if (MenuItem(ctx, "Copy rows", "Ctrl+C")) {
    ctx.clipboard().set(CopyAsTsv(view, state));
  }
  MenuSeparator(ctx);
  auto value_filter = [&](bool keep) {
    data::Filter f;
    f.column = target.column;
    if (info.type == ColumnType::kEnum || info.type == ColumnType::kBool) {
      const std::int64_t v = src.GetInt64(target.column, target.row);
      f.op = data::FilterOp::kInSet;
      if (keep) {
        f.set = {v};
      } else {
        const std::int64_t n =
            info.type == ColumnType::kBool
                ? 2
                : static_cast<std::int64_t>(info.enum_labels.size());
        for (std::int64_t i = 0; i < n; ++i) {
          if (i != v) {
            f.set.push_back(i);
          }
        }
      }
    } else if (info.IsNumeric()) {
      f.op = keep ? data::FilterOp::kEqual : data::FilterOp::kNotEqual;
      f.a = src.GetDouble(target.column, target.row);
    } else {
      f.op = keep ? data::FilterOp::kTextEquals : data::FilterOp::kNotContains;
      f.text = cell;
    }
    state.query.filters.push_back(std::move(f));
  };
  if (MenuItem(ctx, info.title + ": only \"" + shown + "\"")) {
    value_filter(true);
  }
  if (MenuItem(ctx, info.title + ": hide \"" + shown + "\"")) {
    value_filter(false);
  }
  if (MenuItem(ctx, "Clear filters", {}, !state.query.filters.empty())) {
    state.query.filters.clear();
  }
  EndContextMenu(ctx);
  return state.query != before;
}

}  // namespace gloomy::ui
