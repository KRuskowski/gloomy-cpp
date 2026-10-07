#ifndef GLOOMY_UI_TIMELINE_H_
#define GLOOMY_UI_TIMELINE_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// How an event on a timeline is drawn.
enum class TimelineMark : std::uint8_t {
  /// It happened: a filled dot.
  kDone,
  /// Expected but not confirmed (a message still on its way): a ring.
  kExpected,
  /// A limit such as a deadline or a close: a tall bar.
  kDeadline,
};

/// One event on a timeline.
struct TimelineEvent {
  double time = 0.0;
  std::string label;
  TimelineMark mark = TimelineMark::kDone;
  /// Alpha 0: the theme's colour for the mark.
  Color color{0, 0, 0, 0};
  /// Shown in the tooltip under the label and time.
  std::string detail;
};

struct TimelineOptions {
  SizeSpec width = SizeSpec::Grow();
  /// Rows of labels above the axis. A label that fits in none is hidden;
  /// hovering its marker still shows it.
  int lanes = 2;
  /// The present: the axis is solid before it and dashed after it, and a
  /// marker shows it. NaN for none.
  double now = std::numeric_limits<double>::quiet_NaN();
  std::string_view now_label = "now";
  /// The time range shown; NaN for TimelineRange(). Events outside it sit
  /// at the nearer end as arrows pointing out, so that a far deadline does
  /// not squeeze what happens soon.
  double t0 = std::numeric_limits<double>::quiet_NaN();
  double t1 = std::numeric_limits<double>::quiet_NaN();
  /// Formats times under the labels and in tooltips; empty: numbers.
  std::function<std::string(double)> time_format;
};

/// Where an event's label goes.
struct TimelineSlot {
  /// The marker's x.
  float x = 0.0F;
  /// Label row, 0 nearest the axis; -1 if the label is hidden.
  int lane = -1;
  /// The label's horizontal extent.
  float x0 = 0.0F;
  float x1 = 0.0F;
};

/// The time range a timeline shows: every event and `now` (unless NaN),
/// padded by a twentieth on each side; one unit around a single time.
auto TimelineRange(std::span<const TimelineEvent> events, double now)
    -> std::pair<double, double>;

/// Lays out the labels of a timeline: each event's marker at its time
/// between x0 and x1 (times outside [t0, t1] at the nearer end), its label
/// centred over it (kept inside [x0, x1]) in the lowest lane where it keeps
/// `gap` from the label before it. Events are taken in time order.
/// @param[in] times event times, any order
/// @param[in] widths label widths, one per event
/// @return one slot per event, in the order given
auto LayoutTimeline(std::span<const double> times,
                    std::span<const float> widths, double t0, double t1,
                    float x0, float x1, int lanes, float gap)
    -> std::vector<TimelineSlot>;

/// A horizontal timeline: events on a time axis with labels and times
/// above, the past solid and the future dashed, so that what has happened
/// and what is only expected read apart at a glance (for messages that
/// take time to travel, plans and deadlines). Hovering a marker shows its
/// label, time and detail.
void Timeline(Context &ctx, std::string_view label,
              std::span<const TimelineEvent> events,
              const TimelineOptions &options = {});

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_TIMELINE_H_
