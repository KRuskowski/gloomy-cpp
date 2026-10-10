#include "gloomy/ui/list_rows.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ui/widget_util.h"

namespace gloomy::ui {

using detail::InteractiveFill;

namespace {

// The font size column widths are given for.
constexpr float kBaseFontSize = 13.0F;

// Width of a row's status bar at the left edge.
constexpr float kStatusEdge = 3.0F;

// Gap between cells and between buttons.
constexpr float kCellGap = 10.0F;
constexpr float kButtonGap = 4.0F;

// The narrowest a column gets, in font sizes, where the row is short of
// space: its text is cut (with an ellipsis; a label gets no narrower than
// three font sizes) rather than the row run out of its panel.
constexpr float kMinColumnEms = 3.0F;

auto ColumnWidth(const Theme &t, const CellColumn &column) -> SizeSpec {
  const float min = kMinColumnEms * t.metrics.font_size;
  if (column.width > 0.0F) {
    return SizeSpec::Preferred(column.width * CellScale(t), min);
  }
  SizeSpec grow = SizeSpec::Grow();
  grow.min = min;
  return grow;
}

/// A cell: a box of the column's width (id "##c<index>") holding a label
/// that cuts its text; its tooltip, if any, on the box. @return The box's
/// interaction (clicks on it count as clicks on the row).
auto DrawCell(Context &ctx, std::size_t index, const CellColumn &column,
              const Cell &cell) -> Interaction {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId("##c" + std::to_string(index));
  NodeDesc box;
  box.id = id;
  box.layout.mode = LayoutMode::kRow;
  box.layout.width = ColumnWidth(t, column);
  // The column decides the width, not the text: header and rows match.
  box.layout.shrink_x = true;
  box.layout.align = Align::kCenter;
  box.flags = cell.why.empty() ? kNodeNone : kNodeInteractive;
  ctx.Open(box);
  LabelOptions o;
  o.width = SizeSpec::Grow();
  o.align =
      column.align == CellAlign::kRight ? TextAlign::kRight : TextAlign::kLeft;
  o.bold = cell.bold;
  o.muted = cell.muted;
  if (const std::optional<Color> color = CellTextColor(t, cell)) {
    o.color = *color;
  }
  Label(ctx, cell.text, o);
  ctx.Close();
  if (cell.why.empty()) {
    return {};
  }
  Tooltip(ctx, id, cell.why);
  return ctx.Interact(id);
}

/// The row's frame: a focusable row node with the status bar and the
/// selection painted under its children. @return Its interaction.
auto OpenRow(Context &ctx, Id id, bool selected, Tone status,
             bool child_hovered = false) -> Interaction {
  const Theme &t = ctx.theme();
  Interaction it = ctx.Interact(id);
  it.hovered = it.hovered || child_hovered;
  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.width = SizeSpec::Grow();
  row.layout.height = SizeSpec::Fit(t.metrics.control_height);
  row.layout.padding = {6 + kStatusEdge, 1, 4, 1};
  row.layout.gap = kCellGap;
  row.layout.align = Align::kCenter;
  // Glass shows through: no fill unless hovered or selected.
  row.style.fill = it.hovered ? InteractiveFill(t, t.colors.surface_raised, it)
                              : Color{0, 0, 0, 0};
  row.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  row.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const int node = ctx.Open(row);
  const bool focused = ctx.HasFocus(id);
  const Color selection =
      focused ? t.colors.selection
              : t.colors.selection.WithAlpha(t.colors.selection.a * 0.6F);
  const Color accent = t.colors.selection_accent;
  const Color edge =
      status == Tone::kNeutral ? Color{0, 0, 0, 0} : ToneColor(t, status);
  const float radius = t.metrics.radius;
  ctx.AddPainter(
      node,
      [=](PaintContext &pc, const Rect &r) {
        if (selected) {
          pc.list.FillRect(r, selection, gfx::CornerRadii::All(radius));
          if (accent.a > 0.0F) {
            pc.list.FillRect({r.x0, r.y0, r.x0 + 3.0F, r.y1}, accent);
          }
        }
        if (edge.a > 0.0F) {
          pc.list.FillRect(
              {r.x0 + 1, r.y0 + 3, r.x0 + 1 + kStatusEdge, r.y1 - 3},
              edge,
              gfx::CornerRadii::All(kStatusEdge * 0.5F));
        }
      },
      true);
  return it;
}

/// The actions in a box as wide as `slots` would be, right aligned.
/// @return The index of the action pressed, or -1.
auto DrawActions(Context &ctx, std::span<const RowAction> actions,
                 std::span<const RowAction> slots) -> int {
  const float width = RowActionsWidth(ctx, slots, kButtonGap);
  LayoutSpec box;
  box.width = SizeSpec::Fixed(width);
  box.gap = kButtonGap;
  box.align = Align::kCenter;
  box.justify = Justify::kEnd;
  BeginRow(ctx, box);
  int pressed = -1;
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const RowAction &a = actions[i];
    if (!a.disabled.empty()) {
      DisabledButton(ctx, a.label, a.disabled, a.kind);
    } else if (Button(ctx, a.label, a.kind)) {
      pressed = static_cast<int>(i);
    }
  }
  End(ctx);
  return pressed;
}

}  // namespace

auto CellTextColor(const Theme &theme, const Cell &cell)
    -> std::optional<Color> {
  if (cell.color) {
    return cell.color;
  }
  if (cell.tone != Tone::kNeutral) {
    return ToneColor(theme, cell.tone);
  }
  return std::nullopt;
}

auto CellScale(const Theme &theme) -> float {
  return theme.metrics.font_size / kBaseFontSize;
}

auto RowActionsWidth(Context &ctx, std::span<const RowAction> actions,
                     float gap) -> float {
  if (actions.empty()) {
    return 0.0F;
  }
  const Theme &t = ctx.theme();
  float width = 0.0F;
  for (const RowAction &a : actions) {
    width += std::ceil(ctx.text().Measure(
                 DisplayText(a.label), t.fonts.sans, t.metrics.font_size)) +
             t.metrics.padding_x * 3.0F + 1.0F;
  }
  return width + gap * static_cast<float>(actions.size() - 1);
}

void CellHeader(Context &ctx, std::string_view label,
                std::span<const CellColumn> columns,
                std::span<const RowAction> action_slots) {
  const Theme &t = ctx.theme();
  NodeDesc row;
  row.id = ctx.GetId(label);
  row.layout.mode = LayoutMode::kRow;
  row.layout.width = SizeSpec::Grow();
  row.layout.padding = {6 + kStatusEdge, 0, 4, 0};
  row.layout.gap = kCellGap;
  row.layout.align = Align::kCenter;
  ctx.Open(row);
  ctx.PushId(row.id);
  for (std::size_t i = 0; i < columns.size(); ++i) {
    const CellColumn &column = columns[i];
    const Id cell = ctx.GetId("##c" + std::to_string(i));
    NodeDesc box;
    box.id = cell;
    box.layout.mode = LayoutMode::kRow;
    box.layout.width = ColumnWidth(t, column);
    // The column decides the width, not the text: header and rows match.
    box.layout.shrink_x = true;
    box.layout.justify =
        column.align == CellAlign::kRight ? Justify::kEnd : Justify::kStart;
    box.flags = column.why.empty() ? kNodeNone : kNodeInteractive;
    ctx.Open(box);
    LabelOptions o;
    o.muted = true;
    o.bold = true;
    o.size = t.metrics.small_font_size + 1;
    o.width = SizeSpec::Grow();
    o.align = column.align == CellAlign::kRight ? TextAlign::kRight
                                                : TextAlign::kLeft;
    Label(ctx, column.title, o);
    ctx.Close();
    if (!column.why.empty()) {
      ctx.Interact(cell);
      Tooltip(ctx, cell, column.why);
    }
  }
  if (!action_slots.empty()) {
    // Room for the rows' buttons, no taller than the titles.
    NodeDesc room;
    room.layout.width =
        SizeSpec::Fixed(RowActionsWidth(ctx, action_slots, kButtonGap));
    ctx.Open(room);
    ctx.Close();
  }
  ctx.PopId();
  ctx.Close();
}

auto CellRow(Context &ctx, std::string_view label,
             std::span<const CellColumn> columns, std::span<const Cell> cells,
             const CellRowOptions &options) -> CellRowResult {
  CellRowResult result;
  const Id id = ctx.GetId(label);
  // Cells with tooltips take the pointer; they act as part of the row.
  bool child_hovered = false;
  ctx.PushId(id);
  for (std::size_t i = 0; i < columns.size() && i < cells.size(); ++i) {
    if (!cells[i].why.empty()) {
      child_hovered =
          child_hovered ||
          ctx.Interact(ctx.GetId("##c" + std::to_string(i))).hovered;
    }
  }
  ctx.PopId();
  const Interaction it =
      OpenRow(ctx, id, options.selected, options.status, child_hovered);
  result.clicked = it.clicked;
  result.activated =
      it.double_clicked || (ctx.HasFocus(id) && (ctx.ConsumeKey(Key::kEnter) ||
                                                 ctx.ConsumeKey(Key::kSpace)));
  ctx.PushId(id);
  for (std::size_t i = 0; i < columns.size(); ++i) {
    const Interaction c =
        DrawCell(ctx, i, columns[i], i < cells.size() ? cells[i] : Cell{});
    if (c.clicked) {
      result.clicked = true;
      ctx.SetFocus(id, false);
    }
    result.activated = result.activated || c.double_clicked;
  }
  const std::span<const RowAction> slots =
      options.action_slots.empty() ? options.actions : options.action_slots;
  if (!slots.empty()) {
    result.action = DrawActions(ctx, options.actions, slots);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

auto QueueRow(Context &ctx, std::string_view label, const QueueRowSpec &spec)
    -> QueueEdit {
  const Theme &t = ctx.theme();
  QueueEdit edit = QueueEdit::kNone;
  const Id id = ctx.GetId(label);
  OpenRow(ctx, id, false, spec.current ? Tone::kGood : Tone::kNeutral);
  const bool first = spec.index <= 0;
  const bool last = spec.index + 1 >= spec.count;
  if (ctx.HasFocus(id)) {
    if (!first && ctx.ConsumeKey(Key::kUp, kModAlt)) {
      edit = QueueEdit::kUp;
    } else if (!last && ctx.ConsumeKey(Key::kDown, kModAlt)) {
      edit = QueueEdit::kDown;
    } else if (!spec.current && ctx.ConsumeKey(Key::kEnter)) {
      edit = QueueEdit::kGo;
    } else if (ctx.ConsumeKey(Key::kDelete)) {
      edit = QueueEdit::kRemove;
    }
  }
  ctx.PushId(id);
  // The number, marked while running.
  LabelOptions number;
  number.width = SizeSpec::Fixed(36.0F * CellScale(t));
  number.bold = spec.current;
  number.align = TextAlign::kRight;
  if (spec.current) {
    number.color = ToneColor(t, Tone::kGood);
  } else {
    number.muted = true;
  }
  Label(ctx,
        (spec.current ? "> " : "") + std::to_string(spec.index + 1) + ".",
        number);
  LabelOptions text;
  text.width = SizeSpec::Grow();
  text.bold = spec.current;
  if (spec.tone != Tone::kNeutral) {
    text.color = ToneColor(t, spec.tone);
  }
  Label(ctx, spec.text, text);
  // The buttons keep their places: those that cannot act say why.
  LayoutSpec box;
  box.gap = kButtonGap;
  box.align = Align::kCenter;
  BeginRow(ctx, box);
  const auto button = [&ctx](std::string_view name,
                             std::string_view why_not,
                             const char *tag) -> bool {
    const std::string full = std::string(name) + "##" + tag;
    if (!why_not.empty()) {
      DisabledButton(ctx, full, why_not, ButtonKind::kFlat);
      return false;
    }
    return Button(ctx, full, ButtonKind::kFlat);
  };
  if (button(spec.up, first ? "Already first" : "", "up")) {
    edit = QueueEdit::kUp;
  }
  if (button(spec.down, last ? "Already last" : "", "down")) {
    edit = QueueEdit::kDown;
  }
  if (button(spec.go, spec.current ? "Running now" : "", "go")) {
    edit = QueueEdit::kGo;
  }
  if (button(spec.remove, "", "remove")) {
    edit = QueueEdit::kRemove;
  }
  End(ctx);
  ctx.PopId();
  ctx.Close();
  if (!spec.why.empty()) {
    Tooltip(ctx, id, spec.why);
  }
  return edit;
}

}  // namespace gloomy::ui
