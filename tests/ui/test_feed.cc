#include "gloomy/ui/feed.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

const std::vector<std::string_view> kShipActions = {"Orders", "Show"};

/// Newest first, as a host keeps them.
auto Items() -> std::vector<Notification> {
  return {
      {"Courier 2 is idle",
       Priority::kHigh,
       "Fleet",
       "just now",
       kShipActions,
       {},
       true},
      {"Report sold", Priority::kLow, "Guild", "5 min ago", {}, {}, false},
      {"Freighter 1 under attack",
       Priority::kUrgent,
       "Fleet",
       "12 min ago",
       kShipActions,
       "Pirates of the Veil",
       false},
      {"Contract due in 2 h",
       Priority::kHigh,
       "Contracts",
       "1 h ago",
       {},
       {},
       false},
  };
}

TEST(FeedTest, OrderPutsTheMostUrgentFirstAndKeepsAge) {
  const std::vector<Notification> items = Items();
  EXPECT_EQ(FeedOrder(items, true), (std::vector<std::size_t>{2, 0, 3, 1}));
  EXPECT_EQ(FeedOrder(items, false), (std::vector<std::size_t>{0, 1, 2, 3}));
  EXPECT_TRUE(FeedOrder({}, true).empty());
}

TEST(FeedTest, PriorityTones) {
  EXPECT_EQ(PriorityTone(Priority::kUrgent), Tone::kBad);
  EXPECT_EQ(PriorityTone(Priority::kHigh), Tone::kWarning);
  EXPECT_EQ(PriorityTone(Priority::kNormal), Tone::kNeutral);
  EXPECT_EQ(PriorityTone(Priority::kLow), Tone::kNeutral);
}

/// The id of item `index`'s row, or of a button on it.
auto RowId(Context &ctx, std::size_t index, std::string_view button = {})
    -> Id {
  ctx.PushId(ctx.GetId("##feed"));
  ctx.PushId(static_cast<std::uint64_t>(index) + 1);
  Id id = ctx.GetId("##item");
  if (!button.empty()) {
    ctx.PushId(id);
    id = ctx.GetId(button);
    ctx.PopId();
  }
  ctx.PopId();
  ctx.PopId();
  return id;
}

TEST(FeedTest, LinesActionsAndMore) {
  UiHarness h({900, 400});
  const std::vector<Notification> items = Items();
  FeedResult result;
  FeedOptions options;
  options.max_shown = 3;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx);
    result = NotificationFeed(ctx, "##feed", items, options);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  // Shown in priority order: the attack on top.
  const Rect attack = c.LastRect(RowId(c, 2)).value();
  const Rect idle = c.LastRect(RowId(c, 0)).value();
  EXPECT_LT(attack.y0, idle.y0);
  EXPECT_FALSE(c.LastRect(RowId(c, 1)).has_value()) << "fourth: in +1 more";
  // A click on a line names it; a button names it and the action.
  h.Frame(ui, UiHarness::Click({idle.x0 + 300, idle.Center().y}));
  EXPECT_EQ(result.item, 0);
  EXPECT_EQ(result.action, -1);
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(RowId(c, 2, "Show"))));
  EXPECT_EQ(result.item, 2);
  EXPECT_EQ(result.action, 1);
  // Enter on a focused line is its main action.
  c.SetFocus(RowId(c, 0));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(result.item, 0);
  EXPECT_EQ(result.action, 0);
  // "+1 more".
  c.PushId(c.GetId("##feed"));
  const Id more = c.GetId("+1 more##more");
  c.PopId();
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(more)));
  EXPECT_TRUE(result.more);
  EXPECT_EQ(result.item, -1);
}

TEST(FeedTest, AgeAndSourceColumnsLineUp) {
  UiHarness h({900, 400});
  const std::vector<Notification> items = Items();
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx);
    NotificationFeed(ctx, "##feed", items);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  Context &c = h.ctx();
  const auto text_x = [&c](std::size_t i) {
    return c.LastRect(RowId(c, i, "##c2")).value().x0;
  };
  EXPECT_NEAR(text_x(0), text_x(1), 0.5F);
  EXPECT_NEAR(text_x(0), text_x(2), 0.5F);
}

TEST(FeedTest, EmptySaysSo) {
  UiHarness h({600, 300});
  FeedOptions options;
  options.empty = "Nothing needs you";
  FeedResult result;
  auto ui = [&](Context &ctx) {
    BeginColumn(ctx);
    result = NotificationFeed(ctx, "##feed", {}, options);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_EQ(result.item, -1);
  EXPECT_FALSE(result.more);
  const Rect feed = h.ctx().LastRect(h.ctx().GetId("##feed")).value();
  EXPECT_GT(feed.Height(), 5.0F) << "the empty line is drawn";
}

}  // namespace
}  // namespace gloomy::ui
