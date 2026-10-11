#include "gloomy/ui/dashboard.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

using detail::InteractiveFill;

namespace {

// Width of a tile's coloured left edge, as on a card.
constexpr float kTileEdge = 3.0F;

/// The grid's width last frame, kept per grid.
struct TileGridState {
  float width = 0.0F;
};

auto Small(const Theme &t, std::string_view text, Color color,
           bool bold = false) -> TextSpec {
  TextSpec spec;
  spec.text = text;
  spec.font = bold ? t.fonts.bold : t.fonts.sans;
  spec.size = t.metrics.small_font_size + 1;
  spec.color = color;
  return spec;
}

/// One clickable item line. @return True when clicked.
auto ItemLine(Context &ctx, int index, const TileLine &line,
              const PanelSkin *skin) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId("##line" + std::to_string(index));
  const Interaction it = ctx.Interact(id);
  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.width = SizeSpec::Grow();
  row.layout.padding = Padding::Xy(4, 2);
  row.layout.gap = 8;
  row.layout.align = Align::kCenter;
  row.style.fill = it.hovered ? InteractiveFill(t, t.colors.surface_raised, it)
                              : Color{0, 0, 0, 0};
  row.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  row.flags = kNodeInteractive;
  ctx.Open(row);
  const Color tinted =
      skin != nullptr ? skin->ToneInk(line.tone) : ToneColor(t, line.tone);
  const Color plain = skin != nullptr ? skin->text : t.colors.text;
  LabelOptions text;
  text.width = SizeSpec::Grow();
  text.color = line.tone == Tone::kNeutral ? plain : tinted;
  Label(ctx, line.text, text);
  if (!line.value.empty()) {
    // The number keeps its width: a long text yields, not the value.
    LabelOptions value;
    value.color = line.tone == Tone::kNeutral ? plain : tinted;
    value.bold = line.tone != Tone::kNeutral;
    value.width = SizeSpec::Fixed(
        std::ceil(ctx.text().Measure(line.value,
                                     value.bold ? t.fonts.bold : t.fonts.sans,
                                     t.metrics.font_size)) +
        1.0F);
    Label(ctx, line.value, value);
  }
  ctx.Close();
  if (!line.why.empty()) {
    Tooltip(ctx, id, line.why);
  }
  return it.clicked;
}

}  // namespace

auto DashboardTile(Context &ctx, std::string_view label, const TileSpec &spec)
    -> TileResult {
  const Theme &t = ctx.theme();
  TileResult result;
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  if (it.clicked) {
    result.action = true;
  }
  if (ctx.HasFocus(id) &&
      (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace))) {
    result.action = true;
  }
  NodeDesc tile;
  tile.id = id;
  tile.layout.mode = LayoutMode::kColumn;
  tile.layout.width = SizeSpec::Grow();
  tile.layout.height = SizeSpec::Grow();
  tile.layout.padding = {12 + kTileEdge, 10, 12, 10};
  tile.layout.gap = 5;
  tile.layout.align = Align::kStretch;
  tile.style.fill = InteractiveFill(t, t.colors.surface, it);
  tile.style.material = t.MaterialOf(Surface::kCard);
  tile.style.border = t.colors.border;
  tile.style.border_width = t.metrics.border_width;
  tile.style.radii = gfx::CornerRadii::All(t.metrics.radius + 2);
  tile.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const PanelSkin *skin = spec.skin;
  if (skin != nullptr) {
    // The skin paints the frame and paper under the content.
    tile.style.fill = Color{0, 0, 0, 0};
    tile.style.border_width = 0.0F;
    tile.style.radii = gfx::CornerRadii::All(skin->corner);
    const float f = skin->frame;
    tile.layout.padding = {12 + kTileEdge + f, 10 + f, 12 + f, 10 + f};
  }
  const int node = ctx.Open(tile);
  const Color status_ink =
      skin != nullptr ? skin->ToneInk(spec.status) : ToneColor(t, spec.status);
  const Color edge = spec.status == Tone::kNeutral
                         ? (skin != nullptr ? skin->ink_muted : t.colors.border)
                         : status_ink;
  if (skin != nullptr) {
    const PanelSkin s = *skin;
    const bool lit = it.hovered || ctx.HasFocus(id);
    ctx.AddPainter(
        node,
        [s, edge, lit](PaintContext &pc, const Rect &r) {
          const Color paper = lit ? Mix(s.paper, s.ink, 0.06F) : s.paper;
          PaintFrame(pc.list, r, s, paper, lit ? s.ink : s.ink_muted);
          const Rect in = r.Inset(s.frame);
          pc.list.FillRect(
              {in.x0 + 1, in.y0 + 4, in.x0 + 1 + kTileEdge, in.y1 - 4},
              edge,
              gfx::CornerRadii::All(kTileEdge * 0.5F));
        },
        true);
  } else {
    ctx.AddPainter(
        node,
        [edge](PaintContext &pc, const Rect &r) {
          pc.list.FillRect({r.x0 + 1, r.y0 + 4, r.x0 + 1 + kTileEdge, r.y1 - 4},
                           edge,
                           gfx::CornerRadii::All(kTileEdge * 0.5F));
        },
        true);
  }
  const Color muted = skin != nullptr ? skin->text_muted : t.colors.text_muted;
  ctx.PushId(id);
  // Title and aside.
  {
    LayoutSpec head;
    head.gap = 8;
    head.width = SizeSpec::Grow();
    head.align = Align::kCenter;
    BeginRow(ctx, head);
    ctx.Text(Small(t, spec.title, muted, true));
    Spacer(ctx);
    if (!spec.aside.empty()) {
      ctx.Text(Small(t, spec.aside, muted));
    }
    End(ctx);
  }
  // The answer, with its trend.
  {
    LayoutSpec row;
    row.gap = 10;
    row.width = SizeSpec::Grow();
    row.align = Align::kCenter;
    BeginRow(ctx, row);
    const Id answer = ctx.GetId("##answer");
    NodeDesc box;
    box.id = answer;
    box.layout.mode = LayoutMode::kColumn;
    box.layout.width = SizeSpec::Grow();
    box.flags = spec.answer.why.empty() ? kNodeNone : kNodeInteractive;
    ctx.Open(box);
    LabelOptions a;
    a.bold = true;
    a.wrap = true;
    a.width = SizeSpec::Grow();
    a.size = t.metrics.font_size + 2;
    a.color = spec.status != Tone::kNeutral ? status_ink
              : skin != nullptr             ? skin->text
                                            : t.colors.text;
    Label(ctx, spec.answer.value, a);
    ctx.Close();
    if (!spec.answer.why.empty()) {
      Tooltip(ctx, answer, spec.answer.why);
      if (ctx.Interact(answer).clicked) {
        result.action = true;
      }
    }
    if (spec.trend.size() >= 2) {
      Sparkline(ctx, spec.trend, SizeSpec::Fixed(72), 22, spec.trend_tone);
    }
    End(ctx);
  }
  // The items behind it.
  const std::size_t shown = std::min(spec.lines.size(), spec.max_lines);
  for (std::size_t i = 0; i < shown; ++i) {
    if (ItemLine(ctx, static_cast<int>(i), spec.lines[i], spec.skin)) {
      result.line = static_cast<int>(i);
    }
  }
  if (spec.lines.size() > shown) {
    const std::string more =
        "+" + std::to_string(spec.lines.size() - shown) + " more";
    TileLine line;
    line.text = more;
    if (ItemLine(ctx, static_cast<int>(spec.lines.size()), line, spec.skin)) {
      result.action = true;
    }
  }
  if (spec.lines.empty() && !spec.empty.empty()) {
    LabelOptions e;
    e.color = muted;
    e.wrap = true;
    e.width = SizeSpec::Grow();
    Label(ctx, spec.empty, e);
  }
  // The action at the bottom right, whatever the row's height.
  if (!spec.action.empty()) {
    Spacer(ctx);
    LayoutSpec foot;
    foot.width = SizeSpec::Grow();
    foot.justify = Justify::kEnd;
    BeginRow(ctx, foot);
    if (Button(ctx, spec.action, ButtonKind::kFlat)) {
      result.action = true;
    }
    End(ctx);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

auto TileColumns(float width, float min_tile_width, float gap, int max_columns)
    -> int {
  if (min_tile_width <= 0.0F || max_columns <= 1) {
    return 1;
  }
  const float fit = std::floor((width + gap) / (min_tile_width + gap));
  return std::clamp(static_cast<int>(fit), 1, max_columns);
}

void BeginTileGrid(Context &ctx, std::string_view label, float min_tile_width,
                   int max_columns, float gap) {
  const Id id = ctx.GetId(label);
  auto &state = ctx.State<TileGridState>(id);
  if (const std::optional<Rect> last = ctx.LastRect(id)) {
    state.width = last->Width();
  }
  const float width = state.width > 0.0F ? state.width : ctx.viewport().x;
  NodeDesc grid;
  grid.id = id;
  grid.layout.mode = LayoutMode::kGrid;
  grid.layout.grid_columns =
      TileColumns(width, min_tile_width, gap, max_columns);
  grid.layout.width = SizeSpec::Grow();
  grid.layout.gap = gap;
  ctx.Open(grid);
}

void EndTileGrid(Context &ctx) {
  ctx.Close();
}

}  // namespace gloomy::ui
