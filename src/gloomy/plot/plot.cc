#include "gloomy/plot/plot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <matplot/axes_objects/bars.h>
#include <matplot/axes_objects/error_bar.h>
#include <matplot/axes_objects/filled_area.h>
#include <matplot/axes_objects/histogram.h>
#include <matplot/axes_objects/line.h>
#include <matplot/axes_objects/stair.h>
#include <matplot/core/axes_type.h>
#include <matplot/core/axis_type.h>

#include "gloomy/gfx/draw_list.h"
#include "gloomy/plot/series.h"
#include "gloomy/ui/widgets.h"
#include "plot/plot_scene.h"

namespace gloomy::plot {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// line_spec's enums are hidden by its accessors of the same names.
using LineStyle = std::remove_cvref_t<
    decltype(std::declval<const matplot::line_spec &>().line_style())>;
using MarkerStyle = std::remove_cvref_t<
    decltype(std::declval<const matplot::line_spec &>().marker_style())>;
constexpr double kZoomStep = 0.85;
/// Pixels between the plot area and its labels.
constexpr float kPad = 6.0F;
/// Markers are drawn up to this many visible points per series.
constexpr std::size_t kMaxMarkers = 4000;

/// matplot++ colours are {transparency, r, g, b}.
auto FromMatplot(const std::array<float, 4> &c) -> Color {
  return {c[1], c[2], c[3], 1.0F - c[0]};
}

auto AutoColor(const ui::Theme &t, std::size_t i) -> Color {
  const Color colors[] = {t.colors.primary,
                          t.colors.accent,
                          t.colors.warning,
                          t.colors.success,
                          t.colors.error,
                          t.colors.info};
  return colors[i % 6];
}

/// Whether an area's fill is one matplot++ picked itself: area() stamps
/// fills from the axes' colour order, as if the app had chosen them.
auto AutomaticFill(const matplot::filled_area &a) -> bool {
  if (!a.fill_user_color()) {
    return true;
  }
  const std::array<float, 4> &face = a.face_color();
  for (const auto &c : a.parent()->colororder()) {
    if (std::fabs(c[1] - face[1]) < 1e-3F &&
        std::fabs(c[2] - face[2]) < 1e-3F &&
        std::fabs(c[3] - face[3]) < 1e-3F) {
      return true;
    }
  }
  return false;
}

/// The colour a child shows in the legend and the crosshair: what the app
/// set, else the theme's colour for its position among the axes' series.
auto SeriesColor(const ui::Theme &t, matplot::axes_object *o, std::size_t index)
    -> Color {
  if (dynamic_cast<Candlesticks *>(o) != nullptr ||
      dynamic_cast<DepthChart *>(o) != nullptr) {
    return t.colors.success;
  }
  if (const auto *a = dynamic_cast<matplot::filled_area *>(o)) {
    return AutomaticFill(*a) ? AutoColor(t, index)
                             : FromMatplot(a->face_color()).WithAlpha(1.0F);
  }
  if (const auto *l = dynamic_cast<matplot::line *>(o)) {
    return l->line_spec().user_color() ? FromMatplot(l->line_spec().color())
                                       : AutoColor(t, index);
  }
  if (const auto *b = dynamic_cast<matplot::bars *>(o)) {
    return b->manual_face_color() ? FromMatplot(b->face_color())
                                  : AutoColor(t, index);
  }
  if (const auto *h = dynamic_cast<matplot::histogram *>(o)) {
    return h->manual_face_color() ? FromMatplot(h->face_color())
                                  : AutoColor(t, index);
  }
  return AutoColor(t, index);
}

/// The x format of axes `index`.
auto XFormat(const PlotOptions &o, std::size_t index) -> const AxisFormat & {
  return index < o.axes_x.size() ? o.axes_x[index] : o.x;
}

/// Bars keep their data protected; a member pointer named through a
/// derived class reads it without changing matplot++.
struct BarsData : matplot::bars {
  static auto X(const matplot::bars &b) -> const std::vector<double> & {
    return b.*(&BarsData::x_);
  }

  static auto Ys(const matplot::bars &b)
      -> const std::vector<std::vector<double>> & {
    return b.*(&BarsData::ys_);
  }
};

/// A data range that ignores non-finite values.
struct Range {
  double lo = kInf;
  double hi = -kInf;

  void Add(double v) {
    if (std::isfinite(v)) {
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }

  [[nodiscard]] auto Valid() const -> bool { return lo <= hi; }

  /// Padded by `pad` of the span; a flat range opens up around its value.
  void Settle(double pad) {
    if (!Valid()) {
      lo = 0.0;
      hi = 1.0;
      return;
    }
    if (hi - lo < 1e-12) {
      const double d = std::max(1.0, std::fabs(lo) * 0.05);
      lo -= d;
      hi += d;
      return;
    }
    const double p = (hi - lo) * pad;
    lo -= p;
    hi += p;
  }
};

/// What a child of an axes is, for drawing it.
enum class Kind : std::uint8_t {
  kLine,
  kStair,
  kArea,
  kErrorBar,
  kBars,
  kHistogram,
  kCandles,
  kDepth,
  kOther,
};

auto KindOf(matplot::axes_object *o) -> Kind {
  if (dynamic_cast<Candlesticks *>(o) != nullptr) {
    return Kind::kCandles;
  }
  if (dynamic_cast<DepthChart *>(o) != nullptr) {
    return Kind::kDepth;
  }
  if (dynamic_cast<matplot::histogram *>(o) != nullptr) {
    return Kind::kHistogram;
  }
  if (dynamic_cast<matplot::bars *>(o) != nullptr) {
    return Kind::kBars;
  }
  if (dynamic_cast<matplot::error_bar *>(o) != nullptr) {
    return Kind::kErrorBar;
  }
  if (dynamic_cast<matplot::stair *>(o) != nullptr) {
    return Kind::kStair;
  }
  if (dynamic_cast<matplot::filled_area *>(o) != nullptr) {
    return Kind::kArea;
  }
  if (dynamic_cast<matplot::line *>(o) != nullptr) {
    return Kind::kLine;
  }
  return Kind::kOther;
}

/// The x values of a line: its x data, or 1..n as matplot++ plots y alone.
auto LineX(const matplot::line &l) -> std::vector<double> {
  if (!l.x_data().empty()) {
    return l.x_data();
  }
  std::vector<double> x(l.y_data().size());
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] = static_cast<double>(i + 1);
  }
  return x;
}

/// An area's top as matplot++ draws it: a stacked area carries the areas
/// added after it in the same axes (drawn behind them, so the stack shows).
auto AreaTop(const matplot::axes_type &ax, const matplot::filled_area &a)
    -> std::vector<double> {
  std::vector<double> top = a.y_data();
  if (!a.stacked()) {
    return top;
  }
  for (auto it = ax.children().rbegin(); it != ax.children().rend(); ++it) {
    const auto *other = dynamic_cast<const matplot::filled_area *>(it->get());
    if (other == &a) {
      break;
    }
    if (other != nullptr) {
      const std::size_t n = std::min(top.size(), other->y_data().size());
      for (std::size_t i = 0; i < n; ++i) {
        top[i] += other->y_data()[i];
      }
    }
  }
  return top;
}

/// An area's base per point: none is 0, one value is constant.
auto AreaBase(const matplot::filled_area &a, std::size_t n)
    -> std::vector<double> {
  const std::vector<double> &b = a.base_data();
  std::vector<double> out(n, b.empty() ? 0.0 : b.front());
  if (b.size() > 1) {
    for (std::size_t i = 0; i < n && i < b.size(); ++i) {
      out[i] = b[i];
    }
  }
  return out;
}

auto Ascending(std::span<const double> x) -> bool {
  return std::ranges::is_sorted(x);
}

/// Index range [a, b) of sorted `x` covering [x0, x1] and one point beyond
/// each side; everything if `x` is not sorted.
auto VisibleRange(std::span<const double> x, double x0, double x1, bool sorted)
    -> std::pair<std::size_t, std::size_t> {
  if (!sorted) {
    return {0, x.size()};
  }
  auto a = static_cast<std::size_t>(std::lower_bound(x.begin(), x.end(), x0) -
                                    x.begin());
  auto b = static_cast<std::size_t>(std::upper_bound(x.begin(), x.end(), x1) -
                                    x.begin());
  a = a > 0 ? a - 1 : 0;
  b = std::min(x.size(), b + 1);
  return {a, b};
}

auto NearestIndex(std::span<const double> x, double v, bool sorted)
    -> std::size_t {
  if (x.empty()) {
    return 0;
  }
  if (sorted) {
    std::size_t i =
        std::min(x.size() - 1,
                 static_cast<std::size_t>(
                     std::lower_bound(x.begin(), x.end(), v) - x.begin()));
    if (i > 0 && v - x[i - 1] < x[i] - v) {
      --i;
    }
    return i;
  }
  std::size_t best = 0;
  for (std::size_t i = 1; i < x.size(); ++i) {
    if (std::fabs(x[i] - v) < std::fabs(x[best] - v)) {
      best = i;
    }
  }
  return best;
}

}  // namespace

namespace {

/// Everything a scene build needs.
struct Build {
  const ui::Theme &theme;
  text::TextSystem &text;
  const PlotState &state;
  const PlotOptions &options;
  PlotScene &scene;

  [[nodiscard]] auto Hidden(const matplot::axes_object &o) const -> bool {
    return !o.display_name().empty() && state.hidden.contains(o.display_name());
  }
};

/// Colour of a line-like child: its own if the app chose one, else the
/// theme's next.
auto LineColor(const Build &b, const matplot::line &l, std::size_t index)
    -> Color {
  return l.line_spec().user_color() ? FromMatplot(l.line_spec().color())
                                    : AutoColor(b.theme, index);
}

auto FaceColor(const Build &b, bool manual, const std::array<float, 4> &face,
               std::size_t index) -> Color {
  return manual ? FromMatplot(face) : AutoColor(b.theme, index);
}

/// Splits a polyline into dashes (or dots) along its length.
void Dashed(gfx::DrawList &list, std::span<const Vec2> run, float width,
            const Color &c, LineStyle style) {
  std::vector<float> pattern;
  const float w = std::max(1.0F, width);
  switch (style) {
    case LineStyle::dashed_line:
      pattern = {6 * w, 4 * w};
      break;
    case LineStyle::dotted_line:
      pattern = {w, 2 * w};
      break;
    case LineStyle::dash_dot_line:
      pattern = {6 * w, 3 * w, w, 3 * w};
      break;
    default:
      list.Polyline(run, width, c);
      return;
  }
  std::size_t phase = 0;
  float left = pattern[0];
  for (std::size_t i = 1; i < run.size(); ++i) {
    Vec2 a = run[i - 1];
    const Vec2 b = run[i];
    float len = std::hypot(b.x - a.x, b.y - a.y);
    while (len > 0.0F) {
      const float take = std::min(left, len);
      const float t = take / len;
      const Vec2 m{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
      if (phase % 2 == 0) {
        list.Line(a, m, width, c);
      }
      a = m;
      len -= take;
      left -= take;
      if (left <= 0.0F) {
        phase = (phase + 1) % pattern.size();
        left = pattern[phase];
      }
    }
  }
}

void Marker(gfx::DrawList &list, Vec2 p, float size, const Color &c,
            MarkerStyle style, bool face) {
  using M = MarkerStyle;
  const float r = std::max(1.5F, size * 0.5F);
  switch (style) {
    case M::none:
      return;
    case M::point:
      list.FillCircle(p, std::max(1.5F, r * 0.5F), c);
      return;
    case M::square:
      if (face) {
        list.FillRect({p.x - r, p.y - r, p.x + r, p.y + r}, c);
      } else {
        list.StrokeRect({p.x - r, p.y - r, p.x + r, p.y + r}, c, 1.2F);
      }
      return;
    case M::diamond: {
      const std::array<Vec2, 4> d{
          {{p.x, p.y - r}, {p.x + r, p.y}, {p.x, p.y + r}, {p.x - r, p.y}}};
      list.FillPolygon(d, c);
      return;
    }
    case M::upward_pointing_triangle:
      list.FillTriangle(
          {p.x, p.y - r}, {p.x + r, p.y + r}, {p.x - r, p.y + r}, c);
      return;
    case M::downward_pointing_triangle:
      list.FillTriangle(
          {p.x - r, p.y - r}, {p.x + r, p.y - r}, {p.x, p.y + r}, c);
      return;
    case M::right_pointing_triangle:
      list.FillTriangle(
          {p.x - r, p.y - r}, {p.x + r, p.y}, {p.x - r, p.y + r}, c);
      return;
    case M::left_pointing_triangle:
      list.FillTriangle(
          {p.x + r, p.y - r}, {p.x - r, p.y}, {p.x + r, p.y + r}, c);
      return;
    case M::plus_sign:
      list.Line({p.x - r, p.y}, {p.x + r, p.y}, 1.2F, c);
      list.Line({p.x, p.y - r}, {p.x, p.y + r}, 1.2F, c);
      return;
    case M::cross:
      list.Line({p.x - r, p.y - r}, {p.x + r, p.y + r}, 1.2F, c);
      list.Line({p.x - r, p.y + r}, {p.x + r, p.y - r}, 1.2F, c);
      return;
    case M::asterisk:
      list.Line({p.x - r, p.y}, {p.x + r, p.y}, 1.2F, c);
      list.Line({p.x - r * 0.7F, p.y - r * 0.7F},
                {p.x + r * 0.7F, p.y + r * 0.7F},
                1.2F,
                c);
      list.Line({p.x - r * 0.7F, p.y + r * 0.7F},
                {p.x + r * 0.7F, p.y - r * 0.7F},
                1.2F,
                c);
      return;
    default:
      if (face) {
        list.FillCircle(p, r, c);
      } else {
        list.StrokeCircle(p, r, c, 1.2F);
      }
      return;
  }
}

/// The pixel polylines of x, y: split at non-finite values; a sorted
/// series is reduced to its first, lowest, highest and last point per
/// pixel column (the shape survives, thousands of points cost a few
/// hundred segments). Lone points between gaps come back as runs of one.
auto Runs(const AxesFrame &f, std::span<const double> x,
          std::span<const double> y, bool second)
    -> std::vector<std::vector<Vec2>> {
  std::vector<std::vector<Vec2>> runs;
  const std::size_t n = std::min(x.size(), y.size());
  const bool sorted = Ascending(x.first(n));
  const auto [a, b] = VisibleRange(x.first(n), f.x0, f.x1, sorted);
  std::vector<Vec2> run;
  // The current column's points, in index order.
  int column = std::numeric_limits<int>::min();
  Vec2 first;
  Vec2 low;
  Vec2 high;
  Vec2 last;
  std::size_t i_low = 0;
  std::size_t i_high = 0;
  std::size_t count = 0;
  const auto flush = [&] {
    if (count == 0) {
      return;
    }
    run.push_back(first);
    if (count > 2) {
      // Lowest and highest in the order they came.
      if (i_low < i_high) {
        run.push_back(low);
        run.push_back(high);
      } else {
        run.push_back(high);
        run.push_back(low);
      }
    }
    if (count > 1) {
      run.push_back(last);
    }
    count = 0;
  };
  const auto end_run = [&] {
    flush();
    if (!run.empty()) {
      runs.push_back(std::move(run));
      run.clear();
    }
    column = std::numeric_limits<int>::min();
  };
  for (std::size_t i = a; i < b; ++i) {
    if (!std::isfinite(x[i]) || !std::isfinite(y[i])) {
      end_run();
      continue;
    }
    const Vec2 p{f.X(x[i]), f.Y(y[i], second)};
    if (!sorted) {
      run.push_back(p);
      continue;
    }
    const int col = static_cast<int>(std::floor(p.x));
    if (col != column) {
      flush();
      column = col;
      first = low = high = last = p;
      i_low = i_high = i;
      count = 1;
      continue;
    }
    if (p.y > low.y) {
      low = p;
      i_low = i;
    }
    if (p.y < high.y) {
      high = p;
      i_high = i;
    }
    last = p;
    ++count;
  }
  end_run();
  return runs;
}

void DrawLine(Build &b, const AxesFrame &f, const matplot::line &l,
              std::span<const double> x, std::span<const double> y,
              const Color &color) {
  gfx::DrawList &list = b.scene.geometry;
  const matplot::line_spec &spec = l.line_spec();
  const float width = std::max(1.0F, spec.line_width() * 1.5F);
  for (const std::vector<Vec2> &run : Runs(f, x, y, l.use_y2())) {
    if (run.size() == 1) {
      list.FillCircle(run.front(), std::max(2.0F, width), color);
      continue;
    }
    if (spec.line_style() != LineStyle::none) {
      Dashed(list, run, width, color, spec.line_style());
    }
  }
  if (spec.marker_style() == MarkerStyle::none) {
    return;
  }
  const std::size_t n = std::min(x.size(), y.size());
  const bool sorted = Ascending(x.first(n));
  const auto [a, e] = VisibleRange(x.first(n), f.x0, f.x1, sorted);
  if (e - a > kMaxMarkers) {
    return;
  }
  const Color marker =
      spec.marker_user_color() ? FromMatplot(spec.marker_color()) : color;
  for (std::size_t i = a; i < e; ++i) {
    if (std::isfinite(x[i]) && std::isfinite(y[i])) {
      Marker(list,
             {f.X(x[i]), f.Y(y[i], l.use_y2())},
             spec.marker_size() * 1.2F,
             marker,
             spec.marker_style(),
             spec.marker_face() || spec.marker_style() == MarkerStyle::point);
    }
  }
}

/// x, y as stairs: each value holds until the next x.
void StairPoints(const std::vector<double> &x, const std::vector<double> &y,
                 std::vector<double> &sx, std::vector<double> &sy) {
  const std::size_t n = std::min(x.size(), y.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0) {
      sx.push_back(x[i]);
      sy.push_back(y[i - 1]);
    }
    sx.push_back(x[i]);
    sy.push_back(y[i]);
  }
}

void DrawArea(Build &b, const AxesFrame &f, std::span<const double> x,
              std::span<const double> top, std::span<const double> base,
              bool second, const Color &fill) {
  const std::size_t n = std::min(x.size(), top.size());
  std::vector<Vec2> t;
  std::vector<Vec2> u;
  const auto emit = [&] {
    if (t.size() > 1) {
      b.scene.geometry.FillBand(t, u, fill);
    }
    t.clear();
    u.clear();
  };
  const bool sorted = Ascending(x.first(n));
  const auto [a, e] = VisibleRange(x.first(n), f.x0, f.x1, sorted);
  for (std::size_t i = a; i < e; ++i) {
    const double lo = i < base.size() ? base[i] : 0.0;
    if (!std::isfinite(x[i]) || !std::isfinite(top[i]) || !std::isfinite(lo)) {
      emit();
      continue;
    }
    t.push_back({f.X(x[i]), f.Y(top[i], second)});
    u.push_back({f.X(x[i]), f.Y(lo, second)});
  }
  emit();
}

void DrawBars(Build &b, const AxesFrame &f, matplot::bars &bars,
              std::size_t index) {
  const std::vector<double> &xs = BarsData::X(bars);
  const std::vector<std::vector<double>> &ys = BarsData::Ys(bars);
  const std::size_t groups = ys.size();
  if (groups == 0) {
    return;
  }
  const std::size_t n = ys.front().size();
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    x[i] = i < xs.size() ? xs[i] : static_cast<double>(i + 1);
  }
  double spacing = 1.0;
  for (std::size_t i = 1; i < n; ++i) {
    spacing = std::min(i == 1 ? kInf : spacing, std::fabs(x[i] - x[i - 1]));
  }
  if (!std::isfinite(spacing) || spacing <= 0.0) {
    spacing = 1.0;
  }
  const double group_w = spacing * bars.bar_width();
  const double bar_w = group_w / static_cast<double>(groups);
  const bool vertical = bars.vertical_orientation();
  for (std::size_t g = 0; g < groups; ++g) {
    const std::vector<std::array<float, 4>> &faces = bars.face_colors();
    const Color face =
        FaceColor(b,
                  bars.manual_face_color(),
                  g < faces.size() ? faces[g] : bars.face_color(),
                  index + g);
    for (std::size_t i = 0; i < n && i < ys[g].size(); ++i) {
      const double v = ys[g][i];
      if (!std::isfinite(v)) {
        continue;
      }
      const double c0 = x[i] - group_w / 2 + bar_w * static_cast<double>(g);
      const double c1 = c0 + bar_w;
      Rect r;
      if (vertical) {
        r = {f.X(c0), f.Y(std::max(v, 0.0)), f.X(c1), f.Y(std::min(v, 0.0))};
      } else {
        r = {f.X(std::min(v, 0.0)), f.Y(c1), f.X(std::max(v, 0.0)), f.Y(c0)};
      }
      // At least a pixel wide, a hair apart.
      if (r.Width() > 3.0F) {
        r.x1 -= 1.0F;
      }
      b.scene.geometry.FillRect(r, face);
    }
  }
}

void DrawHistogram(Build &b, const AxesFrame &f, matplot::histogram &h,
                   std::size_t index) {
  const std::vector<double> &edges = h.bin_edges();
  const std::vector<double> &values = h.values();
  const Color face = FaceColor(b, h.manual_face_color(), h.face_color(), index);
  for (std::size_t i = 0; i + 1 < edges.size() && i < values.size(); ++i) {
    Rect r{f.X(edges[i]), f.Y(values[i]), f.X(edges[i + 1]), f.Y(0.0)};
    if (r.Width() > 3.0F) {
      r.x1 -= 1.0F;
    }
    b.scene.geometry.FillRect(r, face);
  }
}

void DrawCandles(Build &b, const AxesFrame &f, const Candlesticks &c) {
  const std::vector<double> &x = c.x();
  const std::size_t n = x.size();
  double spacing = kInf;
  for (std::size_t i = 1; i < n; ++i) {
    spacing = std::min(spacing, std::fabs(x[i] - x[i - 1]));
  }
  if (!std::isfinite(spacing) || spacing <= 0.0) {
    spacing = 1.0;
  }
  const float half =
      std::max(0.5F,
               static_cast<float>(spacing * c.body_share() / (f.x1 - f.x0)) *
                   f.area.Width() * 0.5F);
  const Color up = b.theme.colors.success;
  const Color down = b.theme.colors.error;
  const auto [a, e] = VisibleRange(x, f.x0, f.x1, Ascending(x));
  for (std::size_t i = a; i < e; ++i) {
    const double o = c.open()[i];
    const double cl = c.close()[i];
    if (!std::isfinite(o) || !std::isfinite(cl)) {
      continue;
    }
    const Color color = cl >= o ? up : down;
    const float px = std::round(f.X(x[i])) + 0.5F;
    b.scene.geometry.Line(
        {px, f.Y(c.high()[i])}, {px, f.Y(c.low()[i])}, 1.0F, color);
    float top = f.Y(std::max(o, cl));
    float bottom = f.Y(std::min(o, cl));
    if (bottom - top < 1.0F) {
      bottom = top + 1.0F;
    }
    b.scene.geometry.FillRect({px - half, top, px + half, bottom}, color);
  }
}

void DrawDepthSide(Build &b, const AxesFrame &f,
                   const std::vector<DepthChart::Step> &steps,
                   const Color &color) {
  if (steps.empty()) {
    return;
  }
  // From the spread outwards: up to each price's depth, then along it to
  // the next price.
  std::vector<Vec2> top;
  std::vector<Vec2> bottom;
  const float base = f.Y(0.0);
  top.push_back({f.X(steps.front().price), base});
  for (std::size_t i = 0; i < steps.size(); ++i) {
    const float y = f.Y(steps[i].depth);
    top.push_back({f.X(steps[i].price), y});
    const double next =
        i + 1 < steps.size() ? steps[i + 1].price : steps[i].price;
    top.push_back({f.X(next), y});
  }
  for (const Vec2 &p : top) {
    bottom.push_back({p.x, base});
  }
  b.scene.geometry.FillBand(top, bottom, color.WithAlpha(0.25F));
  b.scene.geometry.Polyline(top, 1.5F, color);
}

/// The ranges of an axes: x automatic (or the axes' manual limits, or the
/// state's zoom), y over what is visible.
void Ranges(Build &b, AxesFrame &f) {
  matplot::axes_type &ax = *f.axes;
  Range xr;
  for (const auto &child : ax.children()) {
    if (b.Hidden(*child) || KindOf(child.get()) == Kind::kOther) {
      continue;
    }
    xr.Add(child->xmin());
    xr.Add(child->xmax());
  }
  if (ax.x_axis().limits_mode_manual()) {
    // An infinite end stays automatic.
    const auto lim = ax.x_axis().limits();
    xr.lo = std::isfinite(lim[0]) ? lim[0] : xr.lo;
    xr.hi = std::isfinite(lim[1]) ? lim[1] : xr.hi;
  }
  if (!xr.Valid() || xr.hi - xr.lo < 1e-12) {
    xr.Settle(0.0);
  }
  f.x0 = b.state.zoomed ? b.state.x0 : xr.lo;
  f.x1 = b.state.zoomed ? b.state.x1 : xr.hi;
  if (!(f.x1 > f.x0)) {
    f.x1 = f.x0 + 1.0;
  }
  const bool view = b.options.fit_y_to_view;
  const double vx0 = view ? f.x0 : -kInf;
  const double vx1 = view ? f.x1 : kInf;
  Range y1;
  Range y2;
  const auto add_xy =
      [&](std::span<const double> x, std::span<const double> y, Range &r) {
        const std::size_t n = std::min(x.size(), y.size());
        const bool sorted = Ascending(x.first(n));
        const auto [a, e] = VisibleRange(x.first(n), vx0, vx1, sorted);
        for (std::size_t i = a; i < e; ++i) {
          if (x[i] >= vx0 && x[i] <= vx1) {
            r.Add(y[i]);
          }
        }
      };
  for (const auto &child : ax.children()) {
    if (b.Hidden(*child)) {
      continue;
    }
    matplot::axes_object *o = child.get();
    switch (KindOf(o)) {
      case Kind::kLine:
      case Kind::kStair:
      case Kind::kErrorBar:
      case Kind::kArea: {
        const auto &l = static_cast<const matplot::line &>(*o);
        const std::vector<double> x = LineX(l);
        Range &r = l.use_y2() ? y2 : y1;
        add_xy(x, l.y_data(), r);
        if (const auto *e = dynamic_cast<const matplot::error_bar *>(o)) {
          std::vector<double> lo(l.y_data());
          std::vector<double> hi(l.y_data());
          for (std::size_t i = 0; i < lo.size(); ++i) {
            if (i < e->y_negative_delta().size()) {
              lo[i] -= e->y_negative_delta()[i];
            }
            if (i < e->y_positive_delta().size()) {
              hi[i] += e->y_positive_delta()[i];
            }
          }
          add_xy(x, lo, r);
          add_xy(x, hi, r);
        }
        if (const auto *a = dynamic_cast<const matplot::filled_area *>(o)) {
          add_xy(x, AreaTop(ax, *a), r);
          add_xy(x, AreaBase(*a, x.size()), r);
        }
        break;
      }
      case Kind::kBars: {
        // From the data: matplot++'s bars do not report their limits.
        auto &bars = static_cast<matplot::bars &>(*o);
        y1.Add(0.0);
        for (const std::vector<double> &g : BarsData::Ys(bars)) {
          for (const double v : g) {
            y1.Add(v);
          }
        }
        break;
      }
      case Kind::kHistogram:
        y1.Add(0.0);
        for (const double v : static_cast<matplot::histogram &>(*o).values()) {
          y1.Add(v);
        }
        break;
      case Kind::kDepth:
        y1.Add(0.0);
        y1.Add(o->ymax());
        break;
      case Kind::kCandles: {
        const auto &c = static_cast<const Candlesticks &>(*o);
        add_xy(c.x(), c.low(), y1);
        add_xy(c.x(), c.high(), y1);
        break;
      }
      case Kind::kOther:
        break;
    }
  }
  y1.Settle(0.05);
  if (ax.y_axis().limits_mode_manual()) {
    // An infinite end stays automatic (bars and histograms set 0..inf).
    const auto lim = ax.y_axis().limits();
    y1.lo = std::isfinite(lim[0]) ? lim[0] : y1.lo;
    y1.hi = std::isfinite(lim[1]) ? lim[1] : y1.hi;
  }
  f.y0 = y1.lo;
  f.y1 = y1.hi;
  f.has_y2 = y2.Valid();
  if (f.has_y2) {
    y2.Settle(0.05);
    if (ax.y2_axis().limits_mode_manual()) {
      const auto lim = ax.y2_axis().limits();
      y2.lo = std::isfinite(lim[0]) ? lim[0] : y2.lo;
      y2.hi = std::isfinite(lim[1]) ? lim[1] : y2.hi;
    }
    f.y20 = y2.lo;
    f.y21 = y2.hi;
  }
}

/// Widths of the tick labels left and right of a plot area.
struct Gutter {
  float left = 0.0F;
  float right = 0.0F;
};

/// Lays one axes out in `outer`: labels around, ticks, grid, series. The
/// tick label columns are at least as wide as `min` (so that stacked axes
/// line up). Returns how wide they needed to be.
auto BuildAxes(Build &b, const std::shared_ptr<matplot::axes_type> &handle,
               std::size_t axes_index, const Rect &outer, Gutter min = {})
    -> Gutter {
  matplot::axes_type &ax = *handle;
  const ui::Theme &t = b.theme;
  const ui::Palette &c = t.colors;
  const float small = t.metrics.small_font_size;
  const auto font = t.fonts.sans;
  AxesFrame f;
  f.axes = handle;
  f.index = axes_index;
  Ranges(b, f);
  // Vertical layout first: the ticks depend on the area's height.
  const bool title = !ax.title().empty();
  const bool y_label =
      !ax.ylabel().empty() || (f.has_y2 && !ax.y2label().empty());
  float top = outer.y0 + kPad;
  if (title) {
    b.scene.texts.push_back({ax.title(),
                             {outer.x0, top, outer.x1, top + small + 4},
                             small + 1,
                             c.text,
                             text::HAlign::kCenter});
    top += small + 6;
  }
  const float y_label_top = top;
  if (y_label) {
    top += small + 4;
  }
  float bottom = outer.y1 - small - kPad;
  const bool x_label = !ax.xlabel().empty();
  if (x_label) {
    bottom -= small + 4;
  }
  const float height = std::max(10.0F, bottom - top);
  const int y_target = std::max(2, static_cast<int>(height / 36.0F));
  const Ticks yt = MakeTicks(b.options.y, f.y0, f.y1, y_target);
  const Ticks y2t =
      f.has_y2 ? MakeTicks(b.options.y2, f.y20, f.y21, y_target) : Ticks{};
  std::vector<std::string> y_texts;
  std::vector<std::string> y2_texts;
  float left_w = 0.0F;
  float right_w = 0.0F;
  for (const double v : yt.values) {
    y_texts.push_back(FormatValue(b.options.y, v, yt.step));
    left_w = std::max(left_w, b.text.Measure(y_texts.back(), font, small));
  }
  for (const double v : y2t.values) {
    y2_texts.push_back(FormatValue(b.options.y2, v, y2t.step));
    right_w = std::max(right_w, b.text.Measure(y2_texts.back(), font, small));
  }
  const Gutter needed{left_w, f.has_y2 ? right_w : 0.0F};
  left_w = std::max(left_w, min.left);
  right_w = std::max(right_w, min.right);
  f.area = {
      outer.x0 + left_w + 2 * kPad,
      top,
      outer.x1 - (f.has_y2 || min.right > 0.0F ? right_w + 2 * kPad : 2 * kPad),
      bottom};
  if (f.area.Width() < 10.0F || f.area.Height() < 10.0F) {
    b.scene.frames.push_back(f);
    return needed;
  }
  f.y_step = yt.step;
  f.y2_step = y2t.step;
  gfx::DrawList &list = b.scene.geometry;
  list.FillRect(f.area, c.surface_alt, gfx::CornerRadii::All(2));
  // Y grid and labels.
  for (std::size_t i = 0; i < yt.values.size(); ++i) {
    const float y = std::round(f.Y(yt.values[i])) + 0.5F;
    list.Line({f.area.x0, y}, {f.area.x1, y}, 1.0F, c.divider);
    b.scene.texts.push_back({y_texts[i],
                             {outer.x0, y - 8, f.area.x0 - kPad, y + 8},
                             small,
                             c.text_muted,
                             text::HAlign::kRight});
  }
  for (std::size_t i = 0; i < y2t.values.size(); ++i) {
    const float y = f.Y(y2t.values[i], true);
    b.scene.texts.push_back({y2_texts[i],
                             {f.area.x1 + kPad, y - 8, outer.x1, y + 8},
                             small,
                             c.text_muted,
                             text::HAlign::kLeft});
  }
  // X grid and labels, about one per 90 pixels.
  const AxisFormat &x_format = XFormat(b.options, f.index);
  const Ticks xt = MakeTicks(
      x_format, f.x0, f.x1, std::max(2, static_cast<int>(f.area.Width() / 70)));
  f.x_step = xt.step;
  for (const double v : xt.values) {
    const float x = std::round(f.X(v)) + 0.5F;
    if (x < f.area.x0 || x > f.area.x1) {
      continue;
    }
    list.Line({x, f.area.y0}, {x, f.area.y1}, 1.0F, c.divider);
    b.scene.texts.push_back(
        {FormatValue(x_format, v, xt.step),
         {x - 60, f.area.y1 + 2, x + 60, f.area.y1 + small + 4},
         small,
         c.text_muted,
         text::HAlign::kCenter});
  }
  if (x_label) {
    b.scene.texts.push_back(
        {ax.xlabel(),
         {f.area.x0, outer.y1 - small - kPad, f.area.x1, outer.y1},
         small,
         c.text_muted,
         text::HAlign::kCenter});
  }
  if (!ax.ylabel().empty()) {
    b.scene.texts.push_back(
        {ax.ylabel(),
         {outer.x0, y_label_top, f.area.x1, y_label_top + small + 4},
         small,
         c.text_muted,
         text::HAlign::kLeft});
  }
  if (f.has_y2 && !ax.y2label().empty()) {
    b.scene.texts.push_back(
        {ax.y2label(),
         {f.area.x0, y_label_top, outer.x1, y_label_top + small + 4},
         small,
         c.text_muted,
         text::HAlign::kRight});
  }
  // The series, clipped to the area.
  list.PushClip(f.area);
  // Automatic colours by position among the drawable children, so that
  // hiding one keeps the others' colours (and the legend's).
  std::size_t series = 0;
  for (const auto &child : ax.children()) {
    matplot::axes_object *o = child.get();
    const Kind kind = KindOf(o);
    if (kind == Kind::kOther) {
      continue;
    }
    const std::size_t index = series++;
    if (b.Hidden(*o)) {
      continue;
    }
    switch (kind) {
      case Kind::kLine: {
        const auto &l = static_cast<const matplot::line &>(*o);
        const Color color = LineColor(b, l, index);
        DrawLine(b, f, l, LineX(l), l.y_data(), color);
        break;
      }
      case Kind::kStair: {
        const auto &l = static_cast<const matplot::stair &>(*o);
        const Color color = LineColor(b, l, index);
        std::vector<double> sx;
        std::vector<double> sy;
        StairPoints(LineX(l), l.y_data(), sx, sy);
        if (l.fill()) {
          DrawArea(b, f, sx, sy, {}, l.use_y2(), color.WithAlpha(0.3F));
        }
        DrawLine(b, f, l, sx, sy, color);
        break;
      }
      case Kind::kArea: {
        // matplot++ gives areas a black edge and picks their fill late (in
        // its gnuplot path): the theme's colour, unless the app set one.
        const auto &a = static_cast<const matplot::filled_area &>(*o);
        const Color face = AutomaticFill(a) ? AutoColor(b.theme, index)
                                            : FromMatplot(a.face_color());
        const Color line = face.WithAlpha(1.0F);
        const Color fill = face.WithAlpha(std::min(face.a, 0.35F));
        const std::vector<double> x = LineX(a);
        const std::vector<double> top = AreaTop(ax, a);
        DrawArea(b, f, x, top, AreaBase(a, x.size()), a.use_y2(), fill);
        DrawLine(b, f, a, x, top, line);
        break;
      }
      case Kind::kErrorBar: {
        const auto &e = static_cast<const matplot::error_bar &>(*o);
        const Color color = LineColor(b, e, index);
        const std::vector<double> x = LineX(e);
        const std::vector<double> &y = e.y_data();
        for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
          const double lo =
              y[i] -
              (i < e.y_negative_delta().size() ? e.y_negative_delta()[i] : 0.0);
          const double hi =
              y[i] +
              (i < e.y_positive_delta().size() ? e.y_positive_delta()[i] : 0.0);
          const float px = f.X(x[i]);
          const float cap = std::max(2.0F, e.cap_size());
          list.Line({px, f.Y(lo)}, {px, f.Y(hi)}, 1.0F, color);
          list.Line({px - cap, f.Y(lo)}, {px + cap, f.Y(lo)}, 1.0F, color);
          list.Line({px - cap, f.Y(hi)}, {px + cap, f.Y(hi)}, 1.0F, color);
        }
        DrawLine(b, f, e, x, y, color);
        break;
      }
      case Kind::kBars:
        DrawBars(b, f, static_cast<matplot::bars &>(*o), index);
        break;
      case Kind::kHistogram:
        DrawHistogram(b, f, static_cast<matplot::histogram &>(*o), index);
        break;
      case Kind::kCandles:
        DrawCandles(b, f, static_cast<const Candlesticks &>(*o));
        break;
      case Kind::kDepth: {
        const auto &d = static_cast<const DepthChart &>(*o);
        DrawDepthSide(b, f, d.bids(), t.colors.success);
        DrawDepthSide(b, f, d.asks(), t.colors.error);
        break;
      }
      case Kind::kOther:
        break;
    }
  }
  list.PopClip();
  b.scene.frames.push_back(f);
  return needed;
}

auto HiddenHash(const std::unordered_set<std::string> &hidden) -> std::size_t {
  std::size_t h = hidden.size();
  for (const std::string &name : hidden) {
    h ^= std::hash<std::string>{}(name) + 0x9E3779B97F4A7C15ULL;
  }
  return h;
}

/// The rectangle of each axes: the whole plot for one, else where the
/// axes' position (fractions of the figure, y up) puts it.
auto AxesRect(const matplot::figure_type &fig, std::size_t i, const Rect &r)
    -> Rect {
  if (fig.children().size() <= 1) {
    return r;
  }
  const std::array<float, 4> &p = fig.children()[i]->position();
  return {r.x0 + p[0] * r.Width(),
          r.y1 - (p[1] + p[3]) * r.Height(),
          r.x0 + (p[0] + p[2]) * r.Width(),
          r.y1 - p[1] * r.Height()};
}

/// Rebuilds the scene if what it shows changed.
void UpdateScene(ui::PaintContext &pc, const Rect &r,
                 const std::shared_ptr<matplot::figure_type> &fig,
                 PlotState &state, const PlotOptions &options) {
  if (!state.scene) {
    state.scene = std::make_shared<PlotScene>();
  }
  PlotScene &s = *state.scene;
  const std::size_t hidden = HiddenHash(state.hidden);
  if (s.valid && s.revision == options.revision && s.rect == r &&
      s.zoomed == state.zoomed &&
      (!state.zoomed || (s.x0 == state.x0 && s.x1 == state.x1)) &&
      s.hidden == hidden && s.theme == &pc.theme && s.figure == fig) {
    return;
  }
  s.geometry.Clear();
  s.texts.clear();
  s.frames.clear();
  s.revision = options.revision;
  s.rect = r;
  s.zoomed = state.zoomed;
  s.x0 = state.x0;
  s.x1 = state.x1;
  s.hidden = hidden;
  s.theme = &pc.theme;
  s.figure = fig;
  s.valid = true;
  ++state.builds;
  const std::size_t n = fig->children().size();
  std::vector<Rect> outer(n);
  std::vector<Gutter> min(n);
  if (n > 1) {
    // A first pass measures the tick labels; axes sharing a left (right)
    // edge then share the widest left (right) labels, so that their plot
    // areas line up.
    PlotScene scratch;
    Build measure{pc.theme, pc.text, state, options, scratch};
    std::vector<Gutter> needed(n);
    for (std::size_t i = 0; i < n; ++i) {
      outer[i] = AxesRect(*fig, i, r);
      needed[i] = BuildAxes(measure, fig->children()[i], i, outer[i]);
    }
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        if (std::fabs(outer[i].x0 - outer[j].x0) < 0.5F) {
          min[i].left = std::max(min[i].left, needed[j].left);
        }
        if (std::fabs(outer[i].x1 - outer[j].x1) < 0.5F) {
          min[i].right = std::max(min[i].right, needed[j].right);
        }
      }
    }
  } else if (n == 1) {
    outer[0] = r;
  }
  Build b{pc.theme, pc.text, state, options, s};
  for (std::size_t i = 0; i < n; ++i) {
    BuildAxes(b, fig->children()[i], i, outer[i], min[i]);
  }
}

/// Values of the visible series at data x `xv` of a frame, for the
/// crosshair and the tooltip.
struct Readout {
  std::string name;
  Color color;
  std::string value;
  /// Where the crosshair marks it; invalid without a point.
  bool point = false;
  Vec2 at;
};

auto ReadValues(const ui::Theme &t, const AxesFrame &f, const PlotState &state,
                const PlotOptions &options, double xv) -> std::vector<Readout> {
  std::vector<Readout> out;
  std::size_t series = 0;
  const double y_fine = f.y_step / 10.0;
  for (const auto &child : f.axes->children()) {
    matplot::axes_object *o = child.get();
    const Kind kind = KindOf(o);
    if (kind == Kind::kOther) {
      continue;
    }
    const std::size_t index = series++;
    if (!child->display_name().empty() &&
        state.hidden.contains(child->display_name())) {
      continue;
    }
    Readout r;
    r.name = child->display_name().empty()
                 ? "series " + std::to_string(index + 1)
                 : child->display_name();
    r.color = SeriesColor(t, o, index);
    switch (kind) {
      case Kind::kLine:
      case Kind::kStair:
      case Kind::kArea:
      case Kind::kErrorBar: {
        const auto &l = static_cast<const matplot::line &>(*o);
        const std::vector<double> x = LineX(l);
        const std::vector<double> &y = l.y_data();
        const std::size_t n = std::min(x.size(), y.size());
        if (n == 0) {
          continue;
        }
        std::size_t i = NearestIndex(
            std::span(x).first(n), xv, Ascending(std::span(x).first(n)));
        if (kind == Kind::kStair && i > 0 && x[i] > xv) {
          // A stair holds the value before.
          --i;
        }
        if (!std::isfinite(y[i])) {
          r.value = "no data";
        } else {
          r.value = FormatValue(l.use_y2() ? options.y2 : options.y,
                                y[i],
                                l.use_y2() ? f.y2_step / 10.0 : y_fine);
          r.point = true;
          r.at = {f.X(x[i]), f.Y(y[i], l.use_y2())};
        }
        break;
      }
      case Kind::kCandles: {
        const auto &c = static_cast<const Candlesticks &>(*o);
        if (c.x().empty()) {
          continue;
        }
        const std::size_t i = NearestIndex(c.x(), xv, Ascending(c.x()));
        const auto v = [&](double d) {
          return FormatValue(options.y, d, y_fine);
        };
        r.value = "O " + v(c.open()[i]) + "  H " + v(c.high()[i]) + "  L " +
                  v(c.low()[i]) + "  C " + v(c.close()[i]);
        r.color =
            c.close()[i] >= c.open()[i] ? t.colors.success : t.colors.error;
        r.point = true;
        r.at = {f.X(c.x()[i]), f.Y(c.close()[i])};
        break;
      }
      case Kind::kDepth: {
        const auto &d = static_cast<const DepthChart &>(*o);
        const bool bid = !d.bids().empty() && xv <= d.bids().front().price;
        const std::vector<DepthChart::Step> &side = bid ? d.bids() : d.asks();
        double depth = 0.0;
        for (const DepthChart::Step &s : side) {
          if (bid ? s.price >= xv : s.price <= xv) {
            depth = s.depth;
          }
        }
        r.name = bid ? "bids" : "asks";
        r.color = bid ? t.colors.success : t.colors.error;
        r.value = FormatValue(options.y, depth, y_fine) + " at " +
                  FormatValue(XFormat(options, f.index), xv, f.x_step / 10.0);
        break;
      }
      case Kind::kBars: {
        auto &bars = static_cast<matplot::bars &>(*o);
        const std::vector<double> &xs = BarsData::X(bars);
        const std::vector<std::vector<double>> &ys = BarsData::Ys(bars);
        if (ys.empty() || ys.front().empty()) {
          continue;
        }
        std::vector<double> x(ys.front().size());
        for (std::size_t i = 0; i < x.size(); ++i) {
          x[i] = i < xs.size() ? xs[i] : static_cast<double>(i + 1);
        }
        const std::size_t i = NearestIndex(x, xv, Ascending(x));
        std::string values;
        for (const std::vector<double> &g : ys) {
          if (i < g.size()) {
            values += (values.empty() ? "" : " / ") +
                      FormatValue(options.y, g[i], y_fine);
          }
        }
        r.value = values;
        break;
      }
      case Kind::kHistogram: {
        auto &h = static_cast<matplot::histogram &>(*o);
        const std::vector<double> &edges = h.bin_edges();
        const std::vector<double> &values = h.values();
        r.value = "-";
        for (std::size_t i = 0; i + 1 < edges.size() && i < values.size();
             ++i) {
          if (xv >= edges[i] && xv < edges[i + 1]) {
            r.value = FormatValue(options.y, values[i], y_fine);
          }
        }
        break;
      }
      case Kind::kOther:
        continue;
    }
    out.push_back(std::move(r));
  }
  return out;
}

auto FrameAt(const PlotState &state, Vec2 p) -> const AxesFrame * {
  if (!state.scene) {
    return nullptr;
  }
  for (const AxesFrame &f : state.scene->frames) {
    if (f.area.Contains(p)) {
      return &f;
    }
  }
  return nullptr;
}

/// Legend entries: the named children of every axes, in colour order.
struct LegendEntry {
  std::string name;
  Color color;
};

auto LegendEntries(const ui::Theme &t, const matplot::figure_type &fig)
    -> std::vector<LegendEntry> {
  std::vector<LegendEntry> out;
  for (const auto &ax : fig.children()) {
    std::size_t series = 0;
    for (const auto &child : ax->children()) {
      const Kind kind = KindOf(child.get());
      if (kind == Kind::kOther) {
        continue;
      }
      const std::size_t index = series++;
      if (child->display_name().empty()) {
        continue;
      }
      out.push_back(
          {child->display_name(), SeriesColor(t, child.get(), index)});
    }
  }
  return out;
}

}  // namespace

auto Plot(ui::Context &ctx, std::string_view label,
          const std::shared_ptr<matplot::figure_type> &figure, PlotState &state,
          const PlotOptions &options) -> PlotResult {
  PlotResult result;
  const ui::Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  ctx.PushId(id);
  const Id plot_id = ctx.GetId("##plot");
  const ui::Interaction it = ctx.Interact(plot_id);
  const Vec2 mouse = ctx.mouse();
  const AxesFrame *frame = FrameAt(state, mouse);
  // Any frame for gestures that leave the area while dragging.
  const AxesFrame *any = frame;
  if (any == nullptr && state.scene && !state.scene->frames.empty()) {
    any = &state.scene->frames.front();
  }

  // Wheel zooms x around the pointer.
  const Vec2 wheel = ctx.Wheel(plot_id);
  if (wheel.y != 0.0F && frame != nullptr) {
    const double anchor = frame->DataX(mouse.x);
    const double f = std::pow(kZoomStep, wheel.y);
    state.x0 = anchor - (anchor - frame->x0) * f;
    state.x1 = anchor + (frame->x1 - anchor) * f;
    state.zoomed = true;
  }
  // Drag pans; with Shift it selects a range of x.
  const bool shift = (ctx.press_mods() & ui::kModShift) != 0;
  if (it.pressed && any != nullptr) {
    if (!state.panning && !state.selecting) {
      if (options.select_range && shift) {
        state.selecting = true;
        state.anchor = any->DataX(mouse.x - it.drag.x);
      } else {
        state.panning = true;
        state.drag_x0 = any->x0;
        state.drag_x1 = any->x1;
      }
    }
    if (state.selecting) {
      const double here = any->DataX(mouse.x);
      state.selection =
          std::pair{std::min(state.anchor, here), std::max(state.anchor, here)};
      result.selection_changed = true;
    } else if (std::fabs(it.drag.x) > 2.0F) {
      const double span = state.drag_x1 - state.drag_x0;
      state.x0 =
          state.drag_x0 - it.drag.x * span / std::max(1.0F, any->area.Width());
      state.x1 = state.x0 + span;
      state.zoomed = true;
    }
  } else {
    state.panning = false;
    state.selecting = false;
  }
  if (it.double_clicked ||
      (ctx.HasFocus(plot_id) && ctx.ConsumeKey(ui::Key::kHome))) {
    state.zoomed = false;
    if (state.selection) {
      state.selection.reset();
      result.selection_changed = true;
    }
  }

  ui::LayoutSpec column;
  column.width = options.width;
  column.gap = 4;
  column.align = ui::Align::kStretch;
  ui::BeginColumn(ctx, column);
  const std::vector<LegendEntry> legend = LegendEntries(t, *figure);
  if (options.legend && legend.size() > 1) {
    ui::LayoutSpec row;
    row.gap = 14;
    ui::BeginRow(ctx, row);
    for (const LegendEntry &e : legend) {
      const Id item_id = ctx.GetId("##legend" + e.name);
      const bool hidden = state.hidden.contains(e.name);
      if (ctx.Interact(item_id).clicked) {
        if (hidden) {
          state.hidden.erase(e.name);
        } else {
          state.hidden.insert(e.name);
        }
      }
      ui::NodeDesc item;
      item.id = item_id;
      item.layout.mode = ui::LayoutMode::kRow;
      item.layout.gap = 5;
      item.layout.align = ui::Align::kCenter;
      item.flags = ui::kNodeInteractive;
      ctx.Open(item);
      ui::NodeDesc swatch;
      swatch.layout.width = ui::SizeSpec::Fixed(12);
      swatch.layout.height = ui::SizeSpec::Fixed(3);
      swatch.style.fill = hidden ? t.colors.text_disabled : e.color;
      ctx.Open(swatch);
      ctx.Close();
      ui::LabelOptions text;
      text.size = t.metrics.small_font_size + 1;
      text.color = hidden ? t.colors.text_disabled : t.colors.text_muted;
      ui::Label(ctx, e.name, text);
      ctx.Close();
    }
    ui::End(ctx);
  }
  ui::NodeDesc area;
  area.id = plot_id;
  area.layout.width = ui::SizeSpec::Grow();
  area.layout.height = options.height;
  area.flags = ui::kNodeInteractive | ui::kNodeFocusable | ui::kNodeWheel |
               ui::kNodeWheelZoom | ui::kNodeFocusRing;
  const int node = ctx.Open(area);
  ctx.Close();
  const bool hover = it.hovered && options.crosshair && !it.pressed;
  ctx.AddPainter(
      node,
      [fig = figure, &state, options, hover, mouse](ui::PaintContext &pc,
                                                    const Rect &r) {
        UpdateScene(pc, r, fig, state, options);
        PlotScene &s = *state.scene;
        pc.list.Append(s.geometry);
        for (const SceneText &text : s.texts) {
          pc.text.DrawFitted(pc.list,
                             text.text,
                             pc.theme.fonts.sans,
                             text.size,
                             text.box,
                             text.color,
                             text.align);
        }
        for (const AxesFrame &f : s.frames) {
          pc.list.PushClip(f.area);
          if (state.selection) {
            const float a = f.X(state.selection->first);
            const float b = f.X(state.selection->second);
            pc.list.FillRect({a, f.area.y0, std::max(a + 1.0F, b), f.area.y1},
                             pc.theme.colors.primary.WithAlpha(0.18F));
          }
          if (hover && f.area.Contains(mouse)) {
            const float x = std::round(mouse.x) + 0.5F;
            pc.list.Line({x, f.area.y0},
                         {x, f.area.y1},
                         1.0F,
                         pc.theme.colors.text_muted);
            for (const Readout &v :
                 ReadValues(pc.theme, f, state, options, f.DataX(mouse.x))) {
              if (v.point) {
                pc.list.FillCircle(v.at, 3.5F, v.color);
              }
            }
          }
          pc.list.PopClip();
        }
      });
  ui::End(ctx);

  // Values at the pointer.
  if (frame != nullptr) {
    result.hovered = it.hovered;
    result.x = frame->DataX(mouse.x);
  }
  if (frame != nullptr && it.hovered && options.crosshair && !it.pressed) {
    const double xv = frame->DataX(mouse.x);
    ctx.BeginLayer(ui::LayerKind::kTooltip, HashCombine(id, 0x706C74ULL));
    ui::LabelOptions head;
    head.bold = true;
    head.size = t.metrics.small_font_size + 1;
    ui::Label(ctx,
              FormatDetail(XFormat(options, frame->index), xv, frame->x_step),
              head);
    for (const Readout &v : ReadValues(t, *frame, state, options, xv)) {
      ui::LabelOptions line;
      line.size = t.metrics.small_font_size + 1;
      line.color = v.color;
      ui::Label(ctx, v.name + "  " + v.value, line);
    }
    ctx.EndLayer();
  }
  ctx.PopId();
  return result;
}

}  // namespace gloomy::plot
