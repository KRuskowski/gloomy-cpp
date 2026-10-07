#include "gloomy/ui/chart.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(ChartTest, NiceStepsAndTickText) {
  EXPECT_DOUBLE_EQ(NiceStep(100, 5), 20.0);
  EXPECT_DOUBLE_EQ(NiceStep(1, 4), 0.2);
  EXPECT_DOUBLE_EQ(NiceStep(37, 4), 10.0);
  EXPECT_DOUBLE_EQ(NiceStep(7000, 6), 1000.0);
  EXPECT_EQ(TickText(1250, 50), "1,250");
  EXPECT_EQ(TickText(25000, 5000), "25k");
  EXPECT_EQ(TickText(3400000, 200000), "3.4M");
  EXPECT_EQ(TickText(0.4, 0.2), "0.4");
  EXPECT_EQ(TickText(1e-17, 0.5), "0.0");
}

class ChartBehaviourTest : public ::testing::Test {
 protected:
  ChartBehaviourTest() {
    for (int i = 0; i <= 100; ++i) {
      days_.push_back(i);
      ore_.push_back(100 + 10 * std::sin(i * 0.1));
      food_.push_back(90 + i * 0.2);
    }
  }

  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](Context &ctx) {
          const ChartSeries series[] = {{"Ore", days_, ore_},
                                        {"Food", days_, food_}};
          LineChart(ctx, "##prices", series, state_);
        },
        std::move(events));
  }

  auto Plot() -> Rect {
    Id plot = kNoId;
    h_.Frame([&](Context &ctx) {
      const ChartSeries series[] = {{"Ore", days_, ore_},
                                    {"Food", days_, food_}};
      LineChart(ctx, "##prices", series, state_);
      ctx.PushId(ctx.GetId("##prices"));
      plot = ctx.GetId("##plot");
      ctx.PopId();
    });
    return h_.ctx().LastRect(plot).value();
  }

  UiHarness h_{{800, 400}};
  std::vector<double> days_;
  std::vector<double> ore_;
  std::vector<double> food_;
  ChartState state_;
};

TEST_F(ChartBehaviourTest, WheelZoomsAroundThePointerAndDoubleClickResets) {
  Frame();
  const Rect r = Plot();
  const Vec2 c = r.Center();
  Frame({InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, 3)});
  EXPECT_TRUE(state_.zoomed);
  EXPECT_LT(state_.x1 - state_.x0, 100.0);
  EXPECT_GT(state_.x0, 0.0);
  Frame({InputEvent::Down(c.x, c.y), InputEvent::Up(c.x, c.y)});
  Frame({InputEvent::Down(c.x, c.y), InputEvent::Up(c.x, c.y)});
  EXPECT_FALSE(state_.zoomed);
}

TEST_F(ChartBehaviourTest, DragPans) {
  Frame();
  const Rect r = Plot();
  const Vec2 c = r.Center();
  Frame({InputEvent::Move(c.x, c.y), InputEvent::Down(c.x, c.y)});
  Frame({InputEvent::Move(c.x - 100, c.y)});
  Frame({InputEvent::Up(c.x - 100, c.y)});
  EXPECT_TRUE(state_.zoomed);
  EXPECT_GT(state_.x0, 1.0) << "dragging left shows later days";
}

TEST_F(ChartBehaviourTest, LegendHidesSeries) {
  Frame();
  Id item = kNoId;
  h_.Frame([&](Context &ctx) {
    const ChartSeries series[] = {{"Ore", days_, ore_}, {"Food", days_, food_}};
    LineChart(ctx, "##prices", series, state_);
    ctx.PushId(ctx.GetId("##prices"));
    item = ctx.GetId("##legendFood");
    ctx.PopId();
  });
  Frame(UiHarness::Click(h_.Center(item)));
  EXPECT_TRUE(state_.hidden.contains("Food"));
  Frame(UiHarness::Click(h_.Center(item)));
  EXPECT_FALSE(state_.hidden.contains("Food"));
}

class ChartImageTest : public testing::GpuTest {};

TEST_F(ChartImageTest, PricesWithSecondAxisAndCrosshair) {
  UiHarness h({720, 340});
  std::vector<double> days;
  std::vector<double> ore;
  std::vector<double> energy;
  std::vector<double> traders;
  for (int i = 0; i <= 2000; ++i) {
    const double d = i * 0.05;
    days.push_back(d);
    ore.push_back(100 + 12 * std::sin(d * 0.3) + 3 * std::sin(d * 5.0));
    energy.push_back(95 + 6 * std::cos(d * 0.2));
    traders.push_back(2.0e6 + 1.5e5 * d + 2e5 * std::sin(d));
  }
  ChartState state;
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.padding = Padding::All(12);
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    BeginColumn(ctx, root);
    const ChartSeries series[] = {{"Ore", days, ore},
                                  {"Energy cells", days, energy},
                                  {"Traders", days, traders, 1}};
    ChartOptions options;
    options.x_label = "day";
    options.y_label = "% of base price";
    options.y2_label = "traders (Cr)";
    options.height = SizeSpec::Fixed(280);
    LineChart(ctx, "##chart", series, state, options);
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui, {InputEvent::Move(420, 180)});
  h.Frame(ui);
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {720, 340});
  testing::ExpectMatchesReference(
      gfx::RenderToImage(ctx(),
                         renderer,
                         target,
                         h.ctx().draw_lists(),
                         h.ctx().theme().colors.background),
      // Thousands of thin antialiased segments: drivers may differ by a
      // few levels in a few pixels (seen on NVIDIA vs lavapipe).
      "chart", 2, 8);
}

}  // namespace
}  // namespace gloomy::ui
