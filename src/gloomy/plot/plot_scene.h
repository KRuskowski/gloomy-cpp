#ifndef GLOOMY_PLOT_PLOT_SCENE_H_
#define GLOOMY_PLOT_PLOT_SCENE_H_

// Private: the scene Plot() keeps between frames (src/gloomy/plot/plot.cc);
// tests look into it.

#include <cstdint>
#include <string>
#include <vector>

#include <matplot/core/axes_type.h>
#include <matplot/core/figure_type.h>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/plot/plot.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/theme.h"

namespace gloomy::plot {

/// One axes laid out: its plot area and the ranges it shows.
struct AxesFrame {
  matplot::axes_type *axes = nullptr;
  /// Its index among the figure's axes.
  std::size_t index = 0;
  Rect area;
  double x0 = 0.0;
  double x1 = 1.0;
  double y0 = 0.0;
  double y1 = 1.0;
  bool has_y2 = false;
  double y20 = 0.0;
  double y21 = 1.0;
  double x_step = 1.0;
  double y_step = 1.0;
  double y2_step = 1.0;

  [[nodiscard]] auto X(double v) const -> float {
    return area.x0 + static_cast<float>((v - x0) / (x1 - x0)) * area.Width();
  }

  [[nodiscard]] auto Y(double v, bool second = false) const -> float {
    const double lo = second ? y20 : y0;
    const double hi = second ? y21 : y1;
    return area.y1 - static_cast<float>((v - lo) / (hi - lo)) * area.Height();
  }

  [[nodiscard]] auto DataX(float px) const -> double {
    return x0 + (px - area.x0) / std::max(1.0F, area.Width()) * (x1 - x0);
  }
};

/// A label of the scene, drawn every frame through the text system.
struct SceneText {
  std::string text;
  Rect box;
  float size = 11.0F;
  Color color;
  text::HAlign align = text::HAlign::kLeft;
};

struct PlotScene {
  // What it was built for.
  std::uint64_t revision = 0;
  Rect rect;
  bool zoomed = false;
  double x0 = 0.0;
  double x1 = 1.0;
  std::size_t hidden = 0;
  const ui::Theme *theme = nullptr;
  const matplot::figure_type *figure = nullptr;
  bool valid = false;
  // What it holds.
  gfx::DrawList geometry;
  std::vector<SceneText> texts;
  std::vector<AxesFrame> frames;
};

}  // namespace gloomy::plot

#endif  // GLOOMY_PLOT_PLOT_SCENE_H_
