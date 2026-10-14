#ifndef GLOOMY_PLOT_PLOT_H_
#define GLOOMY_PLOT_PLOT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <matplot/core/figure_type.h>

#include "gloomy/plot/axis_format.h"
#include "gloomy/ui/context.h"

namespace gloomy::plot {

/// Plot(): a matplot++ figure drawn by gloomy (docs/design.md 11.4). The
/// app builds the figure with matplot++ (lines, stairs, areas, bars,
/// histograms, error bars, a second y axis, subplots) and gloomy's market
/// series (series.h: candlesticks, depth charts); the widget draws it with
/// SDF primitives and the theme's text and colours, and adds what a game
/// needs: wheel zoom and drag pan along x, a crosshair with every series'
/// value at the pointer, a legend that hides series, x range selection.
///
/// Drawing reads the figure only when it changed: the scene (geometry and
/// labels) is kept and replayed until PlotOptions::revision, the widget's
/// size, the view, the hidden series or the theme change. A live series
/// of thousands of points costs a rebuild per update, not per frame.

struct PlotOptions {
  ui::SizeSpec width = ui::SizeSpec::Grow();
  ui::SizeSpec height = ui::SizeSpec::Fixed(240.0F);
  /// How the x, left y and right y axes place ticks and word values.
  AxisFormat x = NumberAxis();
  AxisFormat y = NumberAxis();
  AxisFormat y2 = NumberAxis();
  /// The x format of each axes by index where they differ (subplots: a
  /// price axis next to a time axis); the others use `x`.
  std::vector<AxisFormat> axes_x;
  /// A row above the plot with the named series; a click hides one.
  bool legend = true;
  /// A crosshair and a tooltip with every series' value at the pointer.
  bool crosshair = true;
  /// Shift and drag selects a range of x (PlotResult::selection).
  bool select_range = false;
  /// Fits y to what is visible in x when zoomed (else to all the data).
  bool fit_y_to_view = true;
  /// Bump when the figure's data or styles changed: the scene is rebuilt.
  std::uint64_t revision = 0;
};

/// The scene kept between frames (opaque).
struct PlotScene;

/// The view and interaction state of one plot; owned by the app.
struct PlotState {
  /// Zoomed or panned away from the automatic x range, to [x0, x1].
  bool zoomed = false;
  double x0 = 0.0;
  double x1 = 1.0;
  /// Display names of series hidden through the legend.
  std::unordered_set<std::string> hidden;
  /// The selected x range, if any (PlotOptions::select_range).
  std::optional<std::pair<double, double>> selection;
  // Dragging.
  bool panning = false;
  bool selecting = false;
  double drag_x0 = 0.0;
  double drag_x1 = 1.0;
  double anchor = 0.0;
  /// The kept scene.
  std::shared_ptr<PlotScene> scene;
  /// How often the scene was built (tests: replays must not rebuild).
  std::uint64_t builds = 0;
};

struct PlotResult {
  /// The pointer is over a plot area, at data x `x`.
  bool hovered = false;
  double x = 0.0;
  /// The selection changed this frame (state.selection holds it).
  bool selection_changed = false;
};

/// Draws `figure`. The state keeps the figure it last drew alive (its scene
/// points into it), so the app may replace or drop a figure at any time.
auto Plot(ui::Context &ctx, std::string_view label,
          const std::shared_ptr<matplot::figure_type> &figure, PlotState &state,
          const PlotOptions &options = {}) -> PlotResult;

}  // namespace gloomy::plot

#endif  // GLOOMY_PLOT_PLOT_H_
