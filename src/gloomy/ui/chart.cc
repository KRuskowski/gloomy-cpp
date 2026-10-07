#include "gloomy/ui/chart.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "gloomy/data/table_source.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

constexpr float kLeft = 54.0F;
constexpr float kRight = 54.0F;
constexpr float kTop = 8.0F;
constexpr float kBottom = 36.0F;
constexpr double kZoomStep = 0.85;

auto SeriesColor(const Theme &t, const ChartSeries &s, std::size_t i) -> Color {
  if (s.color.a > 0.0F) {
    return s.color;
  }
  const Color palette[] = {t.colors.primary,
                           t.colors.accent,
                           t.colors.warning,
                           t.colors.success,
                           t.colors.error,
                           t.colors.info};
  return palette[i % 6];
}

struct Range {
  double lo = std::numeric_limits<double>::infinity();
  double hi = -std::numeric_limits<double>::infinity();

  void Add(double v) {
    if (std::isfinite(v)) {
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }

  auto Valid() const -> bool { return lo <= hi; }

  // Pads the range and keeps flat data visible.
  void Settle() {
    if (!Valid()) {
      lo = 0.0;
      hi = 1.0;
      return;
    }
    if (hi - lo < 1e-12) {
      lo -= 1.0;
      hi += 1.0;
      return;
    }
    const double pad = (hi - lo) * 0.05;
    lo -= pad;
    hi += pad;
  }
};

// Index of the first x >= v.
auto LowerIndex(std::span<const double> x, double v) -> std::size_t {
  return static_cast<std::size_t>(std::lower_bound(x.begin(), x.end(), v) -
                                  x.begin());
}

// Index of the x nearest to v; x must not be empty.
auto NearestIndex(std::span<const double> x, double v) -> std::size_t {
  std::size_t i = std::min(x.size() - 1, LowerIndex(x, v));
  if (i > 0 && v - x[i - 1] < x[i] - v) {
    --i;
  }
  return i;
}

// Everything the painter needs, computed once per frame.
struct Plot {
  Rect area;
  double x0 = 0.0;
  double x1 = 1.0;
  Range y[2];
  bool right_axis = false;

  auto X(double v) const -> float {
    return area.x0 + static_cast<float>((v - x0) / (x1 - x0)) * area.Width();
  }

  auto Y(int axis, double v) const -> float {
    const Range &r = y[axis];
    return area.y1 -
           static_cast<float>((v - r.lo) / (r.hi - r.lo)) * area.Height();
  }

  auto DataX(float px) const -> double {
    return x0 + (px - area.x0) / std::max(1.0F, area.Width()) * (x1 - x0);
  }
};

auto MakePlot(const Rect &r, std::span<const ChartSeries> series,
              const ChartState &state) -> Plot {
  Plot p;
  for (const ChartSeries &s : series) {
    if (s.axis == 1 && !state.hidden.contains(std::string(s.name))) {
      p.right_axis = true;
    }
  }
  p.area = {r.x0 + kLeft,
            r.y0 + kTop,
            r.x1 - (p.right_axis ? kRight : 12.0F),
            r.y1 - kBottom};
  Range xr;
  for (const ChartSeries &s : series) {
    if (!s.x.empty() && !state.hidden.contains(std::string(s.name))) {
      xr.Add(s.x.front());
      xr.Add(s.x.back());
    }
  }
  if (!xr.Valid() || xr.hi - xr.lo < 1e-12) {
    xr.lo = xr.Valid() ? xr.lo - 1 : 0.0;
    xr.hi = xr.Valid() ? xr.hi + 1 : 1.0;
  }
  p.x0 = state.zoomed ? state.x0 : xr.lo;
  p.x1 = state.zoomed ? state.x1 : xr.hi;
  // Y ranges over what is visible in x.
  for (const ChartSeries &s : series) {
    if (state.hidden.contains(std::string(s.name))) {
      continue;
    }
    const std::size_t n = std::min(s.x.size(), s.y.size());
    const std::size_t a = LowerIndex(s.x.first(n), p.x0);
    const std::size_t b = std::min(n, LowerIndex(s.x.first(n), p.x1) + 1);
    for (std::size_t i = a > 0 ? a - 1 : 0; i < b; ++i) {
      p.y[s.axis == 1 ? 1 : 0].Add(s.y[i]);
    }
  }
  p.y[0].Settle();
  p.y[1].Settle();
  return p;
}

void PaintChart(PaintContext &pc, const Rect &r,
                const std::vector<ChartSeries> &series, const ChartState &state,
                const ChartOptions &options,
                const std::vector<std::string> &labels, float hover_x) {
  const Theme &t = pc.theme;
  const Palette &c = t.colors;
  gfx::DrawList &list = pc.list;
  const Plot p = MakePlot(r, series, state);
  const float small = t.metrics.small_font_size;
  list.FillRect(p.area, c.surface_alt, gfx::CornerRadii::All(2));

  // Grid and ticks.
  const double xs =
      NiceStep(p.x1 - p.x0, std::max(2, static_cast<int>(p.area.Width() / 90)));
  for (double v = std::ceil(p.x0 / xs) * xs; v <= p.x1 + xs * 1e-9; v += xs) {
    const float x = std::round(p.X(v)) + 0.5F;
    list.Line({x, p.area.y0}, {x, p.area.y1}, 1.0F, c.divider);
    pc.text.DrawFitted(list,
                       options.x_format ? options.x_format(v) : TickText(v, xs),
                       t.fonts.sans,
                       small,
                       {x - 40, p.area.y1 + 2, x + 40, p.area.y1 + 16},
                       c.text_muted,
                       text::HAlign::kCenter);
  }
  for (int axis = 0; axis < (p.right_axis ? 2 : 1); ++axis) {
    const Range &yr = p.y[axis];
    const double ys = NiceStep(
        yr.hi - yr.lo, std::max(2, static_cast<int>(p.area.Height() / 40)));
    for (double v = std::ceil(yr.lo / ys) * ys; v <= yr.hi + ys * 1e-9;
         v += ys) {
      const float y = std::round(p.Y(axis, v)) + 0.5F;
      if (axis == 0) {
        list.Line({p.area.x0, y}, {p.area.x1, y}, 1.0F, c.divider);
        pc.text.DrawFitted(list,
                           TickText(v, ys),
                           t.fonts.sans,
                           small,
                           {r.x0, y - 8, p.area.x0 - 6, y + 8},
                           c.text_muted,
                           text::HAlign::kRight);
      } else {
        pc.text.DrawFitted(list,
                           TickText(v, ys),
                           t.fonts.sans,
                           small,
                           {p.area.x1 + 6, y - 8, r.x1, y + 8},
                           c.text_muted,
                           text::HAlign::kLeft);
      }
    }
  }
  if (!options.x_label.empty()) {
    pc.text.DrawFitted(list,
                       labels[0],
                       t.fonts.sans,
                       small,
                       {p.area.x0, r.y1 - 16, p.area.x1, r.y1},
                       c.text_muted,
                       text::HAlign::kCenter);
  }
  if (!options.y_label.empty()) {
    pc.text.DrawFitted(list,
                       labels[1],
                       t.fonts.sans,
                       small,
                       {r.x0, r.y1 - 16, p.area.x0 + 60, r.y1},
                       c.text_muted,
                       text::HAlign::kLeft);
  }
  if (p.right_axis && !options.y2_label.empty()) {
    pc.text.DrawFitted(list,
                       labels[2],
                       t.fonts.sans,
                       small,
                       {p.area.x1 - 60, r.y1 - 16, r.x1, r.y1},
                       c.text_muted,
                       text::HAlign::kRight);
  }

  // Series, reduced to min/max per pixel column.
  list.PushClip(p.area);
  for (std::size_t si = 0; si < series.size(); ++si) {
    const ChartSeries &s = series[si];
    if (state.hidden.contains(std::string(s.name))) {
      continue;
    }
    const int axis = s.axis == 1 ? 1 : 0;
    const Color color = SeriesColor(t, s, si);
    const std::size_t n = std::min(s.x.size(), s.y.size());
    std::size_t a = LowerIndex(s.x.first(n), p.x0);
    a = a > 0 ? a - 1 : 0;
    const std::size_t b = std::min(n, LowerIndex(s.x.first(n), p.x1) + 1);
    bool have_prev = false;
    Vec2 prev;
    int column = std::numeric_limits<int>::min();
    float lo = 0.0F;
    float hi = 0.0F;
    float last = 0.0F;
    auto flush = [&]() {
      if (column == std::numeric_limits<int>::min()) {
        return;
      }
      const auto x = static_cast<float>(column) + 0.5F;
      if (hi - lo > 1.0F) {
        list.Line({x, lo}, {x, hi}, 1.5F, color);
      }
      prev = {x, last};
      have_prev = true;
    };
    for (std::size_t i = a; i < b; ++i) {
      if (!std::isfinite(s.y[i])) {
        flush();
        column = std::numeric_limits<int>::min();
        have_prev = false;
        continue;
      }
      const float px = p.X(s.x[i]);
      const float py = p.Y(axis, s.y[i]);
      // A point between gaps has no line to show it.
      const bool gap_before = i == 0 || !std::isfinite(s.y[i - 1]);
      const bool gap_after = i + 1 >= n || !std::isfinite(s.y[i + 1]);
      if (gap_before && gap_after) {
        list.FillCircle({px, py}, 2.0F, color);
      }
      const int col = static_cast<int>(std::floor(px));
      if (col != column) {
        flush();
        if (have_prev) {
          list.Line(prev, {px, py}, 1.5F, color);
        }
        column = col;
        lo = py;
        hi = py;
      }
      lo = std::min(lo, py);
      hi = std::max(hi, py);
      last = py;
      prev = {px, py};
      have_prev = true;
    }
    flush();
  }
  // Crosshair with the series' points at the nearest x.
  if (hover_x >= p.area.x0 && hover_x < p.area.x1) {
    list.Line({std::round(hover_x) + 0.5F, p.area.y0},
              {std::round(hover_x) + 0.5F, p.area.y1},
              1.0F,
              c.text_muted);
    const double xv = p.DataX(hover_x);
    for (std::size_t si = 0; si < series.size(); ++si) {
      const ChartSeries &s = series[si];
      const std::size_t n = std::min(s.x.size(), s.y.size());
      if (n == 0 || state.hidden.contains(std::string(s.name))) {
        continue;
      }
      const std::size_t i = NearestIndex(s.x.first(n), xv);
      if (!std::isfinite(s.y[i])) {
        continue;
      }
      const int axis = s.axis == 1 ? 1 : 0;
      list.FillCircle(
          {p.X(s.x[i]), p.Y(axis, s.y[i])}, 3.5F, SeriesColor(t, s, si));
    }
  }
  list.PopClip();
}

}  // namespace

auto NiceStep(double range, int target) -> double {
  if (!(range > 0.0) || target < 1) {
    return 1.0;
  }
  const double raw = range / target;
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double f = raw / mag;
  const double nice = f < 1.5 ? 1.0 : (f < 3.5 ? 2.0 : (f < 7.5 ? 5.0 : 10.0));
  return nice * mag;
}

auto TickText(double value, double step) -> std::string {
  if (std::fabs(value) < step * 1e-6) {
    value = 0.0;
  }
  const double a = std::fabs(value);
  // Decimals so that one step still shows: a step of 0.25 needs two.
  auto decimals = [](double unit_step) {
    return unit_step >= 1.0 ? 0
                            : std::min(3,
                                       static_cast<int>(std::ceil(
                                           -std::log10(unit_step) - 1e-9)));
  };

  const struct {
    double scale;
    char suffix;
  } kUnits[] = {{1e9, 'G'}, {1e6, 'M'}, {1e3, 'k'}};

  for (const auto &u : kUnits) {
    // Thousands only from 10,000 on: "1,250" reads better than "1.25k".
    const double from = u.suffix == 'k' ? 1e4 : u.scale;
    if (a >= from && step >= u.scale / 10) {
      char buf[32];
      std::snprintf(buf,
                    sizeof(buf),
                    "%.*f%c",
                    decimals(step / u.scale),
                    value / u.scale,
                    u.suffix);
      return buf;
    }
  }
  std::string out;
  data::FormatDouble(value, decimals(step), true, out);
  return out;
}

void LineChart(Context &ctx, std::string_view label,
               std::span<const ChartSeries> series, ChartState &state,
               const ChartOptions &options) {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  ctx.PushId(id);
  const Id plot_id = ctx.GetId("##plot");
  const Interaction it = ctx.Interact(plot_id);
  const Plot plot = MakePlot(it.rect, series, state);

  // Zoom with the wheel around the pointer, pan by dragging, reset with a
  // double click or Home.
  const Vec2 wheel = ctx.Wheel(plot_id);
  if (wheel.y != 0.0F && plot.area.Width() > 0) {
    const double anchor = plot.DataX(ctx.mouse().x);
    const double f = std::pow(kZoomStep, wheel.y);
    state.x0 = anchor - (anchor - plot.x0) * f;
    state.x1 = anchor + (plot.x1 - anchor) * f;
    state.zoomed = true;
  }
  if (it.pressed) {
    if (!state.panning) {
      state.panning = true;
      state.pan_x0 = plot.x0;
      state.pan_x1 = plot.x1;
    }
    const double per_px =
        (state.pan_x1 - state.pan_x0) / std::max(1.0F, plot.area.Width());
    if (std::fabs(it.drag.x) > 2.0F) {
      state.x0 = state.pan_x0 - it.drag.x * per_px;
      state.x1 = state.pan_x1 - it.drag.x * per_px;
      state.zoomed = true;
    }
  } else {
    state.panning = false;
  }
  if (it.double_clicked ||
      (ctx.HasFocus(plot_id) && ctx.ConsumeKey(Key::kHome))) {
    state.zoomed = false;
  }

  LayoutSpec column;
  column.width = options.width;
  column.gap = 4;
  column.align = Align::kStretch;
  BeginColumn(ctx, column);
  if (options.legend && series.size() > 1) {
    LayoutSpec legend;
    legend.gap = 14;
    BeginRow(ctx, legend);
    for (std::size_t i = 0; i < series.size(); ++i) {
      const std::string name(series[i].name);
      const Id item_id = ctx.GetId("##legend" + name);
      const bool hidden = state.hidden.contains(name);
      if (ctx.Interact(item_id).clicked) {
        if (hidden) {
          state.hidden.erase(name);
        } else {
          state.hidden.insert(name);
        }
      }
      NodeDesc item;
      item.id = item_id;
      item.layout.mode = LayoutMode::kRow;
      item.layout.gap = 5;
      item.layout.align = Align::kCenter;
      item.flags = kNodeInteractive;
      ctx.Open(item);
      NodeDesc swatch;
      swatch.layout.width = SizeSpec::Fixed(12);
      swatch.layout.height = SizeSpec::Fixed(3);
      swatch.style.fill =
          hidden ? t.colors.text_disabled : SeriesColor(t, series[i], i);
      ctx.Open(swatch);
      ctx.Close();
      LabelOptions text;
      text.size = t.metrics.small_font_size + 1;
      text.color = hidden ? t.colors.text_disabled : t.colors.text_muted;
      Label(ctx, series[i].name, text);
      ctx.Close();
    }
    End(ctx);
  }
  NodeDesc area;
  area.id = plot_id;
  area.layout.width = SizeSpec::Grow();
  area.layout.height = options.height;
  area.flags = kNodeInteractive | kNodeFocusable | kNodeWheel | kNodeFocusRing;
  const int node = ctx.Open(area);
  ctx.Close();
  const float hover = it.hovered ? ctx.mouse().x : -1.0F;
  ctx.AddPainter(
      node,
      [series = std::vector<ChartSeries>(series.begin(), series.end()),
       &state,
       options,
       labels = std::vector<std::string>{std::string(options.x_label),
                                         std::string(options.y_label),
                                         std::string(options.y2_label)},
       hover](PaintContext &pc, const Rect &r) {
        PaintChart(pc, r, series, state, options, labels, hover);
      });
  End(ctx);

  // Values at the pointer.
  if (it.hovered && plot.area.Contains(ctx.mouse()) && !it.pressed) {
    const double xv = plot.DataX(ctx.mouse().x);
    ctx.BeginLayer(LayerKind::kTooltip, HashCombine(id, 0x7469ULL));
    LabelOptions head;
    head.bold = true;
    head.size = t.metrics.small_font_size + 1;
    const double xs = NiceStep(plot.x1 - plot.x0, 50);
    Label(ctx,
          std::string(options.x_label.empty() ? "x" : options.x_label) + " " +
              (options.x_format ? options.x_format(xv) : TickText(xv, xs)),
          head);
    for (std::size_t si = 0; si < series.size(); ++si) {
      const ChartSeries &s = series[si];
      const std::size_t n = std::min(s.x.size(), s.y.size());
      if (n == 0 || state.hidden.contains(std::string(s.name))) {
        continue;
      }
      const std::size_t i = NearestIndex(s.x.first(n), xv);
      const Range &yr = plot.y[s.axis == 1 ? 1 : 0];
      LabelOptions line;
      line.size = t.metrics.small_font_size + 1;
      line.color = SeriesColor(t, s, si);
      Label(ctx,
            std::string(s.name) + "  " +
                (std::isfinite(s.y[i])
                     ? TickText(s.y[i], NiceStep(yr.hi - yr.lo, 100))
                     : std::string("no data")),
            line);
    }
    ctx.EndLayer();
  }
  ctx.PopId();
}

}  // namespace gloomy::ui
