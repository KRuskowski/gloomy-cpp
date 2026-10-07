#ifndef GLOOMY_UI_CHART_H_
#define GLOOMY_UI_CHART_H_

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// One line of a chart. `x` must be ascending.
struct ChartSeries {
  std::string_view name;
  std::span<const double> x;
  std::span<const double> y;
  /// 0: left axis, 1: right axis.
  int axis = 0;
  /// Alpha 0 picks a palette colour by series index.
  Color color{0, 0, 0, 0};
};

struct ChartOptions {
  std::string_view x_label;
  std::string_view y_label;
  /// Label of the right axis; it appears when a series uses it.
  std::string_view y2_label;
  SizeSpec width = SizeSpec::Grow();
  SizeSpec height = SizeSpec::Fixed(240.0F);
  bool legend = true;
};

/// Zoom, pan and visibility of a chart; owned by the app.
struct ChartState {
  /// Zoomed or panned away from the automatic range.
  bool zoomed = false;
  double x0 = 0.0;
  double x1 = 1.0;
  /// Names of series hidden through the legend.
  std::unordered_set<std::string> hidden;
  // Drag panning.
  bool panning = false;
  double pan_x0 = 0.0;
  double pan_x1 = 1.0;
};

/// A line chart for time series (prices, stock, money): automatic ranges
/// with readable ticks, a second Y axis, a legend that toggles series, a
/// crosshair showing every series' value at the pointer, wheel zoom and
/// drag pan along x, double click to reset. Long series are reduced to a
/// min/max pair per pixel column before drawing.
void LineChart(Context &ctx, std::string_view label,
               std::span<const ChartSeries> series, ChartState &state,
               const ChartOptions &options = {});

/// A "nice" tick step (1, 2 or 5 times a power of ten) for showing about
/// `target` ticks over `range`.
auto NiceStep(double range, int target) -> double;

/// Short tick text: 1,250 / 12.5k / 3.4M; `step` decides the decimals.
auto TickText(double value, double step) -> std::string;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_CHART_H_
