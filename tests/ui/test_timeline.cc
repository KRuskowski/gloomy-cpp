#include "gloomy/ui/timeline.h"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

TEST(TimelineLayoutTest, PlacesMarkersByTime) {
  const std::vector<double> times = {0.0, 5.0, 10.0};
  const std::vector<float> widths = {10.0F, 10.0F, 10.0F};
  const auto slots =
      LayoutTimeline(times, widths, 0.0, 10.0, 0.0F, 100.0F, 2, 4.0F);
  ASSERT_EQ(slots.size(), 3U);
  EXPECT_FLOAT_EQ(slots[0].x, 0.0F);
  EXPECT_FLOAT_EQ(slots[1].x, 50.0F);
  EXPECT_FLOAT_EQ(slots[2].x, 100.0F);
  for (const TimelineSlot &s : slots) {
    EXPECT_EQ(s.lane, 0);
  }
  // Labels stay inside the timeline.
  EXPECT_FLOAT_EQ(slots[0].x0, 0.0F);
  EXPECT_FLOAT_EQ(slots[2].x1, 100.0F);
  EXPECT_FLOAT_EQ(slots[1].x0, 45.0F);
}

TEST(TimelineLayoutTest, CrowdedLabelsMoveUpALaneOrHide) {
  // Given in reverse time order: slots come back in the order given.
  const std::vector<double> times = {2.0, 1.0, 0.0};
  const std::vector<float> widths = {40.0F, 40.0F, 40.0F};
  const auto slots =
      LayoutTimeline(times, widths, 0.0, 10.0, 0.0F, 200.0F, 2, 4.0F);
  EXPECT_EQ(slots[2].lane, 0);
  EXPECT_EQ(slots[1].lane, 1);
  EXPECT_EQ(slots[0].lane, -1) << "no room in either lane";
  EXPECT_LT(slots[2].x, slots[1].x);
}

TEST(TimelineLayoutTest, RangeCoversEventsAndNowWithPadding) {
  std::vector<TimelineEvent> events(2);
  events[0].time = 100.0;
  events[1].time = 200.0;
  const auto [lo, hi] = TimelineRange(events, 300.0);
  EXPECT_DOUBLE_EQ(lo, 90.0);
  EXPECT_DOUBLE_EQ(hi, 310.0);
  const auto [lo1, hi1] = TimelineRange(
      std::span<const TimelineEvent>(events).first(1), std::nan(""));
  EXPECT_DOUBLE_EQ(lo1, 99.0);
  EXPECT_DOUBLE_EQ(hi1, 101.0);
  const auto [lo0, hi0] = TimelineRange({}, std::nan(""));
  EXPECT_DOUBLE_EQ(lo0, 0.0);
  EXPECT_DOUBLE_EQ(hi0, 1.0);
}

auto MessageEvents() -> std::vector<TimelineEvent> {
  std::vector<TimelineEvent> e(5);
  e[0] = {0.0, "Posted", TimelineMark::kDone, {}, "At the issuer"};
  e[1] = {40.0, "Bid sent", TimelineMark::kDone, {}, "By beacon"};
  e[2] = {70.0, "Bid arrives", TimelineMark::kExpected, {}, "About"};
  e[3] = {90.0, "Bidding closes", TimelineMark::kDeadline, {}, ""};
  e[4] = {130.0, "Award reaches you", TimelineMark::kExpected, {}, ""};
  return e;
}

TEST(TimelineTest, HoveringAMarkerShowsItsTooltip) {
  testing::UiHarness h({480, 200});
  const std::vector<TimelineEvent> events = MessageEvents();
  TimelineOptions options;
  options.now = 50.0;
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(10);
    root.width = SizeSpec::Grow();
    BeginColumn(ctx, root);
    Timeline(ctx, "##timeline", events, options);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const auto rect = h.ctx().LastRect(h.ctx().GetId("##timeline"));
  ASSERT_TRUE(rect.has_value());
  EXPECT_FLOAT_EQ(rect->Height(), 2 * 30.0F + 18.0F + 6.0F);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
  // The first marker sits at the left end of the axis, past the padding.
  const auto [t0, t1] = TimelineRange(events, options.now);
  const float x0 = rect->x0 + 12.0F;
  const float x1 = rect->x1 - 12.0F;
  const float x = x0 + static_cast<float>((0.0 - t0) / (t1 - t0)) * (x1 - x0);
  const float y = rect->y1 - 9.0F;
  h.Frame(ui, {InputEvent::Move(x + 2.0F, y)});
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U) << "tooltip layer";
  // Away from every marker: no tooltip.
  h.Frame(ui, {InputEvent::Move(x + 30.0F, y)});
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
}

class TimelineImageTest : public testing::GpuTest {};

TEST_F(TimelineImageTest, PastSolidFutureDashed) {
  testing::UiHarness h({480, 120});
  const std::vector<TimelineEvent> events = MessageEvents();
  TimelineOptions options;
  options.now = 50.0;
  options.time_format = [](double v) {
    return "t+" + std::to_string(static_cast<int>(v));
  };
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(10);
    root.width = SizeSpec::Grow();
    BeginColumn(ctx, root);
    Timeline(ctx, "##timeline", events, options);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {480, 120});
  testing::ExpectMatchesReference(
      gfx::RenderToImage(ctx(),
                         renderer,
                         target,
                         h.ctx().draw_lists(),
                         h.ctx().theme().colors.background),
      "timeline");
}

}  // namespace
}  // namespace gloomy::ui
