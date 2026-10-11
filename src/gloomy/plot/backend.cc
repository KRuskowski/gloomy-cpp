#include "gloomy/plot/backend.h"

#include <matplot/core/axes_type.h>

namespace gloomy::plot {

auto NewFigure() -> std::shared_ptr<matplot::figure_type> {
  auto figure = std::make_shared<matplot::figure_type>(true);
  figure->backend(std::make_shared<Backend>());
  figure->current_axes();
  return figure;
}

}  // namespace gloomy::plot
