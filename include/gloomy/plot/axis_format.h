#ifndef GLOOMY_PLOT_AXIS_FORMAT_H_
#define GLOOMY_PLOT_AXIS_FORMAT_H_

#include <functional>
#include <string>
#include <vector>

namespace gloomy::plot {

/// Tick positions along an axis, and their spacing (which decides how
/// many decimals or which time unit a label shows).
struct Ticks {
  std::vector<double> values;
  double step = 1.0;
};

/// How an axis places its ticks and words its values (tick labels and the
/// tooltip). Empty functions mean NumberAxis()'s.
struct AxisFormat {
  /// About `target` ticks over [lo, hi].
  std::function<Ticks(double lo, double hi, int target)> ticks;
  /// A value as text; `step` is the tick spacing, or a finer one for the
  /// tooltip.
  std::function<std::string(double value, double step)> text;
  /// A value in full for the tooltip; empty: text() with a tenth of the
  /// tick step.
  std::function<std::string(double value)> detail;
};

/// Numbers: ticks 1, 2 or 5 times a power of ten apart; "1,250", "12.5k",
/// "3.4M".
[[nodiscard]] auto NumberAxis() -> AxisFormat;

/// Time counted in days from day 0, as games count it: values are seconds
/// times `seconds_per_unit` (1 for seconds, 3600 for hours); ticks fall on
/// whole minutes, hours or days ("14:05", "D3 14:00" on the hour or at
/// midnight, "D3"), the tooltip shows "D3 14:05:30".
[[nodiscard]] auto TimeAxis(double seconds_per_unit = 1.0) -> AxisFormat;

/// Ticks NumberAxis() places: multiples of NiceStep() within [lo, hi].
[[nodiscard]] auto NiceTicks(double lo, double hi, int target) -> Ticks;

/// Ticks of `format` (NumberAxis()'s if it has none).
[[nodiscard]] auto MakeTicks(const AxisFormat &format, double lo, double hi,
                             int target) -> Ticks;

/// A value in full, as the tooltip shows it (FormatValue() with a tenth of
/// `step` if `format` has no detail).
[[nodiscard]] auto FormatDetail(const AxisFormat &format, double value,
                                double step) -> std::string;

/// A value as `format` words it (NumberAxis()'s if it has no text).
[[nodiscard]] auto FormatValue(const AxisFormat &format, double value,
                               double step) -> std::string;

}  // namespace gloomy::plot

#endif  // GLOOMY_PLOT_AXIS_FORMAT_H_
