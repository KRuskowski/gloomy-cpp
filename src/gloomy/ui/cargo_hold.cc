#include "gloomy/ui/cargo_hold.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

constexpr float kUnitGap = 3.0F;
constexpr float kAreaPad = 5.0F;
constexpr float kDefaultWidth = 320.0F;

/// Units per line of the area at `width`.
auto UnitsPerLine(float width, float cell) -> int {
  const float usable = width - 2.0F * kAreaPad + kUnitGap;
  return std::max(1, static_cast<int>(usable / (cell + kUnitGap)));
}

auto UnitRect(const Rect &area, int per_line, int i, float cell) -> Rect {
  const int col = i % per_line;
  const int row = i / per_line;
  const float x =
      area.x0 + kAreaPad + static_cast<float>(col) * (cell + kUnitGap);
  const float y =
      area.y0 + kAreaPad + static_cast<float>(row) * (cell + kUnitGap);
  return {x, y, x + cell, y + cell};
}

/// The lot taking most of a unit, or -1.
auto MainLot(const HoldUnit &unit) -> int {
  int lot = -1;
  float best = 0.0F;
  for (const UnitPiece &p : unit.pieces) {
    if (p.share > best) {
      best = p.share;
      lot = p.lot;
    }
  }
  return lot;
}

auto Grouped(double v) -> std::string {
  const std::string digits =
      std::to_string(static_cast<long long>(std::llround(v)));
  std::string out;
  const std::size_t n = digits.size();
  for (std::size_t i = 0; i < n; ++i) {
    out += digits[i];
    if ((n - i - 1) % 3 == 0 && i + 1 < n) {
      out += ',';
    }
  }
  return out;
}

void PaintUnit(gfx::DrawList &list, Rect r, const HoldUnit &unit,
               const std::vector<Color> &colors, const PanelSkin &s, bool hot) {
  // A smaller last unit is narrower.
  r.x1 = r.x0 + r.Width() * std::clamp(unit.size, 0.2F, 1.0F);
  const float radius = std::min(r.Width(), r.Height()) * 0.25F;
  const auto radii = gfx::CornerRadii::All(radius);
  list.FillRect(r, s.paper_edge, radii);
  float y = r.y1;
  const float full = std::max(unit.size, 1e-3F);
  for (const UnitPiece &p : unit.pieces) {
    const float h = r.Height() * std::clamp(p.share / full, 0.0F, 1.0F);
    if (h <= 0.0F) {
      continue;
    }
    list.PushClip({r.x0, y - h, r.x1, y});
    list.FillRect(r, colors[static_cast<std::size_t>(p.lot)], radii);
    list.PopClip();
    y -= h;
  }
  // A seam between goods sharing a unit.
  if (unit.pieces.size() > 1) {
    float seam = r.y1;
    for (std::size_t i = 0; i + 1 < unit.pieces.size(); ++i) {
      seam -= r.Height() * unit.pieces[i].share / full;
      list.FillRect({r.x0 + 1, seam - 0.5F, r.x1 - 1, seam + 0.5F},
                    s.paper.WithAlpha(0.9F));
    }
  }
  list.StrokeRect(
      r,
      hot ? s.text : s.ink_muted.WithAlpha(unit.pieces.empty() ? 0.45F : 0.7F),
      hot ? 1.5F : 1.0F,
      radii);
}

}  // namespace

auto LadderVolume(int step) -> double {
  static constexpr double kMantissa[] = {1.0, 2.5, 5.0};
  // 10, 25, 50, 100, 250, 500, ...
  const int s = std::max(0, step);
  return kMantissa[s % 3] * std::pow(10.0, 1 + s / 3);
}

auto UnitVolumeFor(double capacity, int max_units) -> double {
  max_units = std::max(1, max_units);
  for (int step = 0; step < 30; ++step) {
    const double v = LadderVolume(step);
    if (capacity / v <= static_cast<double>(max_units) + 1e-9) {
      return v;
    }
  }
  return LadderVolume(29);
}

auto HoldUnit::Fill() const -> float {
  float f = 0.0F;
  for (const UnitPiece &p : pieces) {
    f += p.share;
  }
  return f;
}

auto FillUnits(double capacity, double unit_volume,
               std::span<const CargoLot> lots) -> std::vector<HoldUnit> {
  std::vector<HoldUnit> units;
  if (capacity <= 0.0 || unit_volume <= 0.0) {
    return units;
  }
  const auto count =
      static_cast<std::size_t>(std::ceil(capacity / unit_volume - 1e-9));
  units.resize(count);
  for (std::size_t u = 0; u < count; ++u) {
    const double u0 = unit_volume * static_cast<double>(u);
    units[u].size =
        static_cast<float>(std::min(1.0, (capacity - u0) / unit_volume));
  }
  double at = 0.0;
  for (std::size_t i = 0; i < lots.size(); ++i) {
    const double start = at;
    const double end = std::min(capacity, at + std::max(0.0, lots[i].volume));
    at = end;
    for (auto u = static_cast<std::size_t>(start / unit_volume);
         u < count && unit_volume * static_cast<double>(u) < end;
         ++u) {
      const double u0 = unit_volume * static_cast<double>(u);
      const double overlap =
          std::min(end, u0 + unit_volume) - std::max(start, u0);
      if (overlap > 1e-9) {
        units[u].pieces.push_back(
            {static_cast<int>(i), static_cast<float>(overlap / unit_volume)});
      }
    }
  }
  return units;
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

auto UnitLegend(double unit_volume, std::string_view unit) -> std::string {
  return "1 unit = " + Grouped(unit_volume) + " " + std::string(unit);
}

auto CargoHold(Context &ctx, std::string_view label, const CargoHoldSpec &spec)
    -> CargoHoldResult {
  const Theme &t = ctx.theme();
  const PanelSkin skin =
      spec.skin != nullptr ? *spec.skin : PanelSkin::FromTheme(t);
  const float scale = CellScale(t);
  const float cell = spec.cell * scale;
  CargoHoldResult result;
  const Id id = ctx.GetId(label);
  LayoutSpec col;
  col.width = SizeSpec::Grow();
  col.gap = 6;
  col.align = Align::kStretch;
  BeginColumn(ctx, col);
  ctx.PushId(id);
  // Top: one row per cargo type.
  for (std::size_t si = 0; si < spec.sections.size(); ++si) {
    const HoldSection &sec = spec.sections[si];
    const double unit_volume =
        spec.unit_volume > 0.0 ? spec.unit_volume : UnitVolumeFor(sec.capacity);
    const std::vector<HoldUnit> units =
        FillUnits(sec.capacity, unit_volume, sec.lots);
    const double used = HoldUsed(sec.capacity, sec.lots);
    ctx.PushId(static_cast<std::uint64_t>(si) + 1);
    // The type's line: name, fill and the unit's size; its units below.
    LayoutSpec block;
    block.width = SizeSpec::Grow();
    block.gap = 3;
    block.align = Align::kStretch;
    BeginColumn(ctx, block);
    LayoutSpec head;
    head.width = SizeSpec::Grow();
    head.gap = 8;
    head.align = Align::kEnd;
    BeginRow(ctx, head);
    TextSpec name;
    name.text = sec.name;
    name.font = t.fonts.bold;
    name.size = t.metrics.font_size;
    name.color = skin.text;
    ctx.Text(name);
    const std::string fill =
        std::to_string(static_cast<int>(std::lround(used * 100.0))) + "% - " +
        Grouped(sec.capacity * used) + "/" + Grouped(sec.capacity) + " " +
        std::string(spec.unit);
    TextSpec line = name;
    line.text = fill;
    line.font = t.fonts.sans;
    line.size = t.metrics.small_font_size;
    line.color = used >= 0.999 ? skin.warning : skin.text_muted;
    ctx.Text(line);
    Spacer(ctx);
    const std::string legend = UnitLegend(unit_volume, spec.unit);
    line.text = legend;
    line.color = skin.text_muted;
    ctx.Text(line);
    End(ctx);
    NodeDesc area;
    area.id = ctx.GetId("##units");
    const float width = ctx.LastRect(area.id).has_value()
                            ? ctx.LastRect(area.id)->Width()
                            : kDefaultWidth;
    const int per_line = UnitsPerLine(width, cell);
    const int lines =
        std::max(1, (static_cast<int>(units.size()) + per_line - 1) / per_line);
    area.layout.width = SizeSpec::Grow();
    area.layout.height = SizeSpec::Fixed(
        2.0F * kAreaPad + static_cast<float>(lines) * (cell + kUnitGap) -
        kUnitGap);
    area.flags = kNodeInteractive;
    const Interaction it = ctx.Interact(area.id);
    if (it.hovered && !it.rect.Empty()) {
      const Vec2 p = it.rect.Min() + it.local;
      const int pl = UnitsPerLine(it.rect.Width(), cell);
      for (std::size_t u = 0; u < units.size(); ++u) {
        if (UnitRect(it.rect, pl, static_cast<int>(u), cell).Contains(p)) {
          const int lot = MainLot(units[u]);
          if (lot >= 0) {
            result.section = static_cast<int>(si);
            result.lot = lot;
            if (it.clicked) {
              result.clicked = lot;
            }
          }
          break;
        }
      }
    }
    const int node = ctx.Open(area);
    ctx.Close();
    std::vector<Color> colors;
    colors.reserve(sec.lots.size());
    for (const CargoLot &l : sec.lots) {
      colors.push_back(l.color);
    }
    const int hot = result.section == static_cast<int>(si) ? result.lot : -1;
    ctx.AddPainter(
        node,
        [skin, units, colors, cell, hot](PaintContext &pc, const Rect &r) {
          PanelSkin frame = skin;
          frame.bolt = 0.0F;
          frame.inset = 0.0F;
          frame.frame = 1.5F;
          PaintFrame(
              pc.list, r, frame, skin.paper, skin.ink_muted.WithAlpha(0.5F));
          const int pl = UnitsPerLine(r.Width(), cell);
          for (std::size_t u = 0; u < units.size(); ++u) {
            bool lit = false;
            for (const UnitPiece &p : units[u].pieces) {
              lit = lit || p.lot == hot;
            }
            PaintUnit(pc.list,
                      UnitRect(r, pl, static_cast<int>(u), cell),
                      units[u],
                      colors,
                      skin,
                      lit);
          }
        });
    if (result.section == static_cast<int>(si) && result.lot >= 0) {
      const std::string_view why =
          sec.lots[static_cast<std::size_t>(result.lot)].why;
      if (!why.empty()) {
        Tooltip(ctx, area.id, why);
      }
    }
    End(ctx);
    ctx.PopId();
  }
  // Below: the manifest.
  if (!spec.columns.empty()) {
    CellHeader(ctx, "##manifest_head", spec.columns);
    bool any = false;
    for (std::size_t si = 0; si < spec.sections.size(); ++si) {
      const HoldSection &sec = spec.sections[si];
      for (std::size_t li = 0; li < sec.lots.size(); ++li) {
        const CargoLot &l = sec.lots[li];
        any = true;
        std::vector<Cell> cells(l.cells.begin(), l.cells.end());
        if (!cells.empty()) {
          cells[0].color = l.color;
          cells[0].bold = true;
          if (cells[0].why.empty()) {
            cells[0].why = l.why;
          }
        }
        CellRowOptions o;
        o.selected = result.section == static_cast<int>(si) &&
                     result.lot == static_cast<int>(li);
        ctx.PushId((static_cast<std::uint64_t>(si) << 32U) + li + 1);
        const CellRowResult r = CellRow(ctx, "##lot", spec.columns, cells, o);
        const Interaction ri = ctx.Interact(ctx.GetId("##lot"));
        ctx.PopId();
        if (r.clicked) {
          result.section = static_cast<int>(si);
          result.lot = static_cast<int>(li);
          result.clicked = static_cast<int>(li);
        } else if (ri.hovered && result.lot < 0) {
          result.section = static_cast<int>(si);
          result.lot = static_cast<int>(li);
        }
      }
    }
    if (!any && !spec.empty.empty()) {
      LabelOptions e;
      e.muted = true;
      Label(ctx, spec.empty, e);
    }
  }
  ctx.PopId();
  End(ctx);
  return result;
}

}  // namespace gloomy::ui
