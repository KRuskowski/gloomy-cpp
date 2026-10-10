#ifndef GLOOMY_UI_FEED_H_
#define GLOOMY_UI_FEED_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gloomy/ui/context.h"
#include "gloomy/ui/summary.h"

namespace gloomy::ui {

// A notification feed, answer first: what needs the user, most urgent
// first, each line saying where it comes from, how old it is and what can
// be done about it, one click away. The host keeps the notifications (it
// knows when each appeared and when it stops holding) and formats their
// ages in its own clock (game time or real time).

/// How urgent a notification is.
enum class Priority : std::uint8_t { kLow, kNormal, kHigh, kUrgent };

/// The tone a priority is drawn in: urgent bad, high warning, the rest
/// neutral.
[[nodiscard]] auto PriorityTone(Priority priority) -> Tone;

/// One notification.
struct Notification {
  std::string_view text;
  Priority priority = Priority::kNormal;
  /// Where it comes from: "Fleet", "Courier 2", "Contracts"; empty for
  /// none.
  std::string_view source;
  /// How old it is, as the host words it: "just now", "12 min ago".
  std::string_view age;
  /// Buttons on its line ("Orders", "Show"); the first is the main one.
  std::span<const std::string_view> actions;
  /// Tooltip with more detail; empty for none.
  std::string_view why;
  /// Not seen yet: marked with a dot.
  bool fresh = false;
};

/// How a feed shows its notifications.
struct FeedOptions {
  /// Lines shown; the rest are summed up as "+N more", which acts like
  /// FeedResult::more. 0 shows all.
  std::size_t max_shown = 6;
  /// Most urgent first (newest first within a priority, as given);
  /// otherwise in the order given.
  bool by_priority = true;
  /// Said when there is nothing; empty for none.
  std::string_view empty;
  /// Width of the age and source columns at the default font size
  /// (scaled with the theme like cell rows); 0 hides the column.
  float age_width = 80.0F;
  float source_width = 110.0F;
};

/// What the user did with the feed this frame.
struct FeedResult {
  /// Index into the notifications of the line clicked or acted on; -1 for
  /// none.
  int item = -1;
  /// Index into that notification's actions of the button pressed; -1 if
  /// the line itself was clicked (or Enter on it: the main action, 0, if
  /// it has one).
  int action = -1;
  /// "+N more" was clicked.
  bool more = false;
};

/// The order the feed shows `items` in: indices into `items`, most urgent
/// first when `by_priority` (stable: the given order within a priority).
[[nodiscard]] auto FeedOrder(std::span<const Notification> items,
                             bool by_priority) -> std::vector<std::size_t>;

/// The feed: one line per notification (priority bar, fresh dot, age,
/// source, text, actions), up to `max_shown` and "+N more".
auto NotificationFeed(Context &ctx, std::string_view label,
                      std::span<const Notification> items,
                      const FeedOptions &options = {}) -> FeedResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_FEED_H_
