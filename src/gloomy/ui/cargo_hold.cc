#include "gloomy/ui/cargo_hold.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

constexpr float kBayGap = 3.0F;
constexpr float kGridPad = 6.0F;
constexpr int kMaxRows = 4;
constexpr float kDefaultWidth = 360.0F;

/// A lot as the painter keeps it.
struct PaintedLot {
  Color color;
  icon::IconId icon = icon::kNoIcon;
  CargoForm form = CargoForm::kCrate;
};

auto Darker(const Color &c, float f) -> Color {
  return {c.r * f, c.g * f, c.b * f, c.a};
}

auto BayRect(const Rect &grid, const HoldGrid &g, int i, float cell) -> Rect {
  const int col = i % g.columns;
  const int row = i / g.columns;
  const float x =
      grid.x0 + kGridPad + static_cast<float>(col) * (cell + kBayGap);
  const float y =
      grid.y0 + kGridPad + static_cast<float>(row) * (cell + kBayGap);
  return {x, y, x + cell, y + cell};
}

void PaintBay(PaintContext &pc, const Rect &r, const HoldBay &bay,
              const PaintedLot *lot, const PanelSkin &s, bool hovered) {
  gfx::DrawList &list = pc.list;
  if (lot == nullptr || bay.fill <= 0.0F) {
    // Empty: a faint floor and corner marks.
    list.FillRect(r, s.paper_edge.WithAlpha(0.5F));
    const Color m = s.ink_muted.WithAlpha(0.45F);
    const float k = r.Width() * 0.25F;
    list.FillRect({r.x0, r.y0, r.x0 + k, r.y0 + 1}, m);
    list.FillRect({r.x0, r.y0, r.x0 + 1, r.y0 + k}, m);
    list.FillRect({r.x1 - k, r.y1 - 1, r.x1, r.y1}, m);
    list.FillRect({r.x1 - 1, r.y1 - k, r.x1, r.y1}, m);
    return;
  }
  list.FillRect(r, s.paper_edge.WithAlpha(0.5F));
  const float f = std::clamp(bay.fill, 0.0F, 1.0F);
  const Rect box{r.x0, r.y1 - r.Height() * f, r.x1, r.y1};
  const Color body = lot->color;
  const Color edge = Darker(body, 0.55F);
  if (lot->form == CargoForm::kTank) {
    const float rad = std::min(box.Width(), box.Height()) * 0.38F;
    list.FillRectGradient(
        box, body, Darker(body, 0.75F), gfx::CornerRadii::All(rad));
    list.StrokeRect(box, edge, 1.5F, gfx::CornerRadii::All(rad));
    if (box.Height() > 10.0F) {
      const float y = box.y0 + box.Height() * 0.35F;
      list.FillRect({box.x0 + 1, y, box.x1 - 1, y + 2}, edge.WithAlpha(0.8F));
      list.FillRect({box.x0 + 3, box.y0 + 3, box.x0 + 5, box.y1 - 4},
                    Color{1, 1, 1, 0.25F});
    }
  } else {
    list.FillRect(box, body, gfx::CornerRadii::All(1.5F));
    list.StrokeRect(box, edge, 1.5F, gfx::CornerRadii::All(1.5F));
    if (box.Height() > 8.0F) {
      const Rect in = box.Inset(3.0F);
      list.Line({in.x0, in.y0}, {in.x1, in.y1}, 1.2F, edge.WithAlpha(0.7F));
      list.Line({in.x0, in.y1}, {in.x1, in.y0}, 1.2F, edge.WithAlpha(0.7F));
    }
  }
  if (pc.icons != nullptr && lot->icon != icon::kNoIcon && f >= 0.99F) {
    pc.icons->Draw(list,
                   lot->icon,
                   r.Center(),
                   r.Width() * 0.58F,
                   Darker(body, 0.3F).WithAlpha(0.9F));
  }
  if (hovered) {
    list.StrokeRect(r.Inset(-1.5F), s.text, 1.5F);
  }
}

auto Percent(double share) -> std::string {
  return std::to_string(static_cast<int>(std::lround(share * 100.0))) + "%";
}

auto Volume(double v) -> std::string {
  // Grouped thousands: 12,400.
  std::string digits = std::to_string(static_cast<long long>(std::llround(v)));
  std::string out;
  const std::size_t n = digits.size();
  for (std::size_t i = 0; i < n; ++i) {
    out += digits[i];
    if (digits[0] != '-' && (n - i - 1) % 3 == 0 && i + 1 < n) {
      out += ',';
    }
  }
  return out;
}

}  // namespace

auto FillHold(double capacity, std::span<const CargoLot> lots, int bays)
    -> std::vector<HoldBay> {
  std::vector<HoldBay> out(static_cast<std::size_t>(std::max(0, bays)));
  if (bays <= 0 || capacity <= 0.0) {
    return out;
  }
  const double per = capacity / static_cast<double>(bays);
  // Each lot's interval in the hold, poured in order.
  std::vector<double> start(lots.size());
  std::vector<double> end(lots.size());
  double at = 0.0;
  for (std::size_t i = 0; i < lots.size(); ++i) {
    start[i] = at;
    at = std::min(capacity, at + std::max(0.0, lots[i].volume));
    end[i] = at;
  }
  for (int b = 0; b < bays; ++b) {
    const double b0 = per * static_cast<double>(b);
    const double b1 = b0 + per;
    double best = 0.0;
    double total = 0.0;
    for (std::size_t i = 0; i < lots.size(); ++i) {
      const double overlap =
          std::max(0.0, std::min(b1, end[i]) - std::max(b0, start[i]));
      total += overlap;
      if (overlap > best + 1e-9) {
        best = overlap;
        out[static_cast<std::size_t>(b)].lot = static_cast<int>(i);
      }
    }
    out[static_cast<std::size_t>(b)].fill =
        static_cast<float>(std::clamp(total / per, 0.0, 1.0));
  }
  return out;
}

auto HoldGridFor(int bays, float width, float cell) -> HoldGrid {
  HoldGrid g;
  const float usable = std::max(cell, width - 2.0F * kGridPad + kBayGap);
  g.columns = std::max(1, static_cast<int>(usable / (cell + kBayGap)));
  g.columns = std::min(g.columns, std::max(1, bays));
  g.rows = std::max(1, (bays + g.columns - 1) / g.columns);
  return g;
}

auto AutoBays(float width, float cell) -> int {
  const HoldGrid g = HoldGridFor(1 << 20, width, cell);
  return std::clamp(g.columns * 2, 8, g.columns * kMaxRows);
}

auto HoldUsed(double capacity, std::span<const CargoLot> lots) -> double {
  if (capacity <= 0.0) {
    return 0.0;
  }
  double used = 0.0;
  for (const CargoLot &l : lots) {
    used += std::max(0.0, l.volume);
  }
  return std::clamp(used / capacity, 0.0, 1.0);
}

auto CargoHold(Context &ctx, std::string_view label, const CargoHoldSpec &spec)
    -> CargoHoldResult {
  const Theme &t = ctx.theme();
  const PanelSkin skin =
      spec.skin != nullptr ? *spec.skin : PanelSkin::FromTheme(t);
  CargoHoldResult result;
  const Id id = ctx.GetId(label);
  LayoutSpec col;
  col.width = SizeSpec::Grow();
  col.gap = 5;
  col.align = Align::kStretch;
  BeginColumn(ctx, col);
  ctx.PushId(id);
  const double used = HoldUsed(spec.capacity, spec.lots);
  if (!spec.title.empty()) {
    LayoutSpec head;
    head.width = SizeSpec::Grow();
    head.gap = 8;
    head.align = Align::kEnd;
    BeginRow(ctx, head);
    TextSpec title;
    title.text = spec.title;
    title.font = t.fonts.bold;
    title.size = t.metrics.font_size;
    title.color = skin.text;
    ctx.Text(title);
    Spacer(ctx);
    TextSpec full = title;
    const std::string text =
        Percent(used) + " full - " + Volume(spec.capacity * used) + " of " +
        Volume(spec.capacity) + " " + std::string(spec.unit);
    full.text = text;
    full.font = t.fonts.sans;
    full.size = t.metrics.small_font_size;
    full.color = used >= 0.999 ? skin.warning : skin.text_muted;
    ctx.Text(full);
    End(ctx);
  }
  // The grid: one interactive node; bays hit by position.
  NodeDesc grid;
  grid.id = ctx.GetId("##grid");
  const float width = ctx.LastRect(grid.id).has_value()
                          ? ctx.LastRect(grid.id)->Width()
                          : kDefaultWidth;
  const float cell = spec.cell;
  const int bays = spec.bays > 0 ? spec.bays : AutoBays(width, cell);
  const HoldGrid g = HoldGridFor(bays, width, cell);
  const std::vector<HoldBay> filled = FillHold(spec.capacity, spec.lots, bays);
  grid.layout.width = SizeSpec::Grow();
  grid.layout.height =
      SizeSpec::Fixed(2.0F * kGridPad +
                      static_cast<float>(g.rows) * (cell + kBayGap) - kBayGap);
  grid.flags = kNodeInteractive;
  const Interaction it = ctx.Interact(grid.id);
  int hovered_bay = -1;
  if (it.hovered && !it.rect.Empty()) {
    const Vec2 p = it.rect.Min() + it.local;
    for (int i = 0; i < bays; ++i) {
      if (BayRect(it.rect, g, i, cell).Contains(p)) {
        hovered_bay = i;
        result.hovered = filled[static_cast<std::size_t>(i)].lot;
        break;
      }
    }
    if (it.clicked) {
      result.clicked = result.hovered;
    }
  }
  const int grid_node = ctx.Open(grid);
  ctx.Close();
  std::vector<PaintedLot> lots;
  lots.reserve(spec.lots.size());
  for (const CargoLot &l : spec.lots) {
    lots.push_back({l.color, l.icon, l.form});
  }
  const int hovered_lot = result.hovered;
  ctx.AddPainter(
      grid_node,
      [skin, lots, filled, g, cell, hovered_lot, hovered_bay](PaintContext &pc,
                                                              const Rect &r) {
        PanelSkin frame = skin;
        frame.bolt = 0.0F;
        frame.inset = 0.0F;
        frame.frame = 2.0F;
        PaintFrame(
            pc.list, r, frame, skin.paper, skin.ink_muted.WithAlpha(0.7F));
        for (std::size_t i = 0; i < filled.size(); ++i) {
          const HoldBay &bay = filled[i];
          const PaintedLot *lot =
              bay.lot >= 0 ? &lots[static_cast<std::size_t>(bay.lot)] : nullptr;
          const bool hot = hovered_lot >= 0
                               ? bay.lot == hovered_lot
                               : static_cast<int>(i) == hovered_bay;
          PaintBay(pc,
                   BayRect(r, g, static_cast<int>(i), cell),
                   bay,
                   lot,
                   skin,
                   hot && bay.lot >= 0);
        }
      });
  if (result.hovered >= 0) {
    const CargoLot &l = spec.lots[static_cast<std::size_t>(result.hovered)];
    if (!l.why.empty()) {
      Tooltip(ctx, grid.id, l.why);
    }
  }
  // Legend: each lot's swatch, name, amount and share; then the free
  // space.
  if (spec.legend) {
    LayoutSpec row;
    row.width = SizeSpec::Grow();
    row.gap = 14;
    row.align = Align::kCenter;
    BeginRow(ctx, row);
    auto entry = [&](std::size_t i,
                     std::string_view name,
                     const std::string &detail,
                     Color color,
                     bool empty) {
      NodeDesc box;
      box.id = ctx.GetId(static_cast<std::uint64_t>(i) + 1);
      box.layout.mode = LayoutMode::kRow;
      box.layout.gap = 5;
      box.layout.align = Align::kCenter;
      box.flags = kNodeInteractive;
      ctx.Open(box);
      NodeDesc swatch;
      swatch.layout.width = SizeSpec::Fixed(10);
      swatch.layout.height = SizeSpec::Fixed(10);
      if (empty) {
        swatch.style.border = skin.ink_muted;
        swatch.style.border_width = 1.0F;
      } else {
        swatch.style.fill = color;
      }
      swatch.style.radii = gfx::CornerRadii::All(2.0F);
      ctx.Open(swatch);
      ctx.Close();
      TextSpec n;
      n.text = name;
      n.font = t.fonts.bold;
      n.size = t.metrics.small_font_size;
      n.color = empty ? skin.text_muted : skin.text;
      ctx.Text(n);
      TextSpec d = n;
      d.text = detail;
      d.font = t.fonts.sans;
      d.color = skin.text_muted;
      ctx.Text(d);
      ctx.Close();
      return box.id;
    };
    for (std::size_t i = 0; i < spec.lots.size(); ++i) {
      const CargoLot &l = spec.lots[i];
      const double share =
          spec.capacity > 0.0 ? std::max(0.0, l.volume) / spec.capacity : 0.0;
      std::string detail(l.amount);
      detail += (detail.empty() ? "" : ", ") + Percent(share);
      const Id e = entry(i, l.name, detail, l.color, false);
      const Interaction ei = ctx.Interact(e);
      if (ei.hovered) {
        result.hovered = static_cast<int>(i);
      }
      if (ei.clicked) {
        result.clicked = static_cast<int>(i);
      }
      if (!l.why.empty()) {
        Tooltip(ctx, e, l.why);
      }
    }
    const double free_share = 1.0 - used;
    entry(spec.lots.size(),
          "Free",
          Volume(spec.capacity * free_share) + " " + std::string(spec.unit) +
              ", " + Percent(free_share),
          {},
          true);
    End(ctx);
  }
  ctx.PopId();
  End(ctx);
  return result;
}

}  // namespace gloomy::ui
