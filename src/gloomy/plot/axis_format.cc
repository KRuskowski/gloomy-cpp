#include "gloomy/plot/axis_format.h"

#include <array>
#include <cmath>
#include <cstdio>

#include "gloomy/ui/chart.h"

namespace gloomy::plot {

namespace {

constexpr double kMinute = 60.0;
constexpr double kHour = 3600.0;
constexpr double kDay = 86400.0;

/// Time steps in seconds, ascending: whole minutes, hours and days.
constexpr std::array<double, 15> kTimeSteps = {1.0,
                                               5.0,
                                               15.0,
                                               30.0,
                                               kMinute,
                                               5 * kMinute,
                                               15 * kMinute,
                                               30 * kMinute,
                                               kHour,
                                               2 * kHour,
                                               3 * kHour,
                                               6 * kHour,
                                               12 * kHour,
                                               kDay,
                                               7 * kDay};

/// "D3 14:05:30" with the parts `step` (seconds) calls for.
auto TimeText(double seconds, double step) -> std::string {
  const double whole = std::floor(seconds + 1e-6);
  const auto day = static_cast<long long>(std::floor(whole / kDay));
  const double in_day = whole - static_cast<double>(day) * kDay;
  const int h = static_cast<int>(in_day / kHour);
  const int m = static_cast<int>((in_day - h * kHour) / kMinute);
  const int s = static_cast<int>(in_day - h * kHour - m * kMinute);
  char buf[48];
  if (step >= kDay) {
    std::snprintf(buf, sizeof(buf), "D%lld", day);
  } else if (step >= kHour || (h == 0 && m == 0 && s == 0)) {
    // On the hour: the day too, so that the axis reads across midnight.
    std::snprintf(buf, sizeof(buf), "D%lld %02d:%02d", day, h, m);
  } else if (step >= kMinute) {
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  } else {
    std::snprintf(buf, sizeof(buf), "D%lld %02d:%02d:%02d", day, h, m, s);
  }
  return buf;
}

auto Multiples(double lo, double hi, double step) -> std::vector<double> {
  std::vector<double> out;
  if (!(step > 0.0) || !(hi >= lo)) {
    return out;
  }
  for (double v = std::ceil(lo / step) * step; v <= hi + step * 1e-9;
       v += step) {
    out.push_back(std::fabs(v) < step * 1e-9 ? 0.0 : v);
    if (out.size() > 1000) {
      break;
    }
  }
  return out;
}

}  // namespace

auto NiceTicks(double lo, double hi, int target) -> Ticks {
  const double step = ui::NiceStep(hi - lo, std::max(1, target));
  return {Multiples(lo, hi, step), step};
}

auto NumberAxis() -> AxisFormat {
  AxisFormat f;
  f.ticks = NiceTicks;
  f.text = [](double value, double step) {
    return ui::TickText(value, step);
  };
  return f;
}

auto TimeAxis(double seconds_per_unit) -> AxisFormat {
  AxisFormat f;
  const double k = seconds_per_unit > 0.0 ? seconds_per_unit : 1.0;
  f.ticks = [k](double lo, double hi, int target) {
    const double range = (hi - lo) * k;
    const double want = range / std::max(1, target);
    double step = kTimeSteps.back();
    for (const double s : kTimeSteps) {
      if (s >= want) {
        step = s;
        break;
      }
    }
    if (want > kTimeSteps.back()) {
      // Whole weeks beyond that.
      step = std::ceil(want / (7 * kDay)) * 7 * kDay;
    }
    Ticks t = {Multiples(lo * k, hi * k, step), step / k};
    for (double &v : t.values) {
      v /= k;
    }
    return t;
  };
  f.text = [k](double value, double step) {
    return TimeText(value * k, step * k);
  };
  f.detail = [k](double value) {
    // Seconds only when there are any.
    const double s = value * k;
    const bool whole_minute = std::fabs(s - std::round(s / 60) * 60) < 0.5;
    return whole_minute ? TimeText(s, kHour) : TimeText(s, 1.0);
  };
  return f;
}

auto MakeTicks(const AxisFormat &format, double lo, double hi, int target)
    -> Ticks {
  return format.ticks ? format.ticks(lo, hi, target)
                      : NiceTicks(lo, hi, target);
}

auto FormatDetail(const AxisFormat &format, double value, double step)
    -> std::string {
  return format.detail ? format.detail(value)
                       : FormatValue(format, value, step / 10.0);
}

auto FormatValue(const AxisFormat &format, double value, double step)
    -> std::string {
  return format.text ? format.text(value, step) : ui::TickText(value, step);
}

}  // namespace gloomy::plot
