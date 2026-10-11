#ifndef GLOOMY_PLOT_BACKEND_H_
#define GLOOMY_PLOT_BACKEND_H_

#include <memory>
#include <string>

#include <matplot/backend/backend_interface.h>
#include <matplot/core/figure_type.h>

namespace gloomy::plot {

/// matplot++'s backend for figures gloomy draws (docs/design.md 11.4).
/// matplot++ keeps the figure: axes, limits, labels and the plot objects
/// with their data; Plot() draws that model with gloomy's primitives and
/// text, so the backend has nothing to render. It exists so that
/// figure::draw() and the plotting calls that redraw never start gnuplot.
/// It takes matplot++'s gnuplot commands and drops them: that way
/// matplot++ reports axis limits as they were set (an infinite end is
/// automatic, as bars and histograms set them), where for other backends
/// it reports -10..10, and none of its unfinished direct drawing runs.
class Backend : public matplot::backend::backend_interface {
 public:
  auto is_interactive() -> bool override { return true; }

  auto consumes_gnuplot_commands() -> bool override { return true; }

  void run_command(const std::string & /*text*/) override {}

  void include_comment(const std::string & /*text*/) override {}

  auto new_frame() -> bool override { return true; }

  auto render_data() -> bool override { return true; }

  auto should_close() -> bool override { return false; }

  auto supports_fonts() -> bool override { return true; }

  void show(matplot::figure_type * /*figure*/) override {}

  auto width() -> unsigned int override { return width_; }

  auto height() -> unsigned int override { return height_; }

  void width(unsigned int w) override { width_ = w; }

  void height(unsigned int h) override { height_ = h; }

 private:
  unsigned int width_ = 560;
  unsigned int height_ = 420;
};

/// A new quiet figure with one axes, drawn by gloomy (Backend). Build it
/// with matplot++'s object API (figure->current_axes()->plot(...)); the
/// free functions act on matplot++'s global current figure instead.
[[nodiscard]] auto NewFigure() -> std::shared_ptr<matplot::figure_type>;

}  // namespace gloomy::plot

#endif  // GLOOMY_PLOT_BACKEND_H_
