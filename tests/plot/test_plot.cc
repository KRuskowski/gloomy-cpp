#include "gloomy/plot/plot.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <vector>

#include <matplot/axes_objects/error_bar.h>
#include <matplot/axes_objects/filled_area.h>
#include <matplot/axes_objects/line.h>
#include <matplot/axes_objects/stair.h>
#include <matplot/core/axes_type.h>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/plot/backend.h"
#include "gloomy/plot/series.h"
#include "gloomy/ui/widgets.h"
#include "plot/plot_scene.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::plot {
namespace {

using testing::UiHarness;
using ui::InputEvent;

TEST(AxisFormatTest, TimeTicksFallOnWholeUnits) {
  const AxisFormat time = TimeAxis();
  // Two days in seconds, four ticks: every 12 hours.
  Ticks t = MakeTicks(time, 0, 2 * 86400, 4);
  EXPECT_DOUBLE_EQ(t.step, 12 * 3600.0);
  ASSERT_EQ(t.values.size(), 5U);
  EXPECT_EQ(FormatValue(time, t.values[0], t.step), "D0 00:00");
  EXPECT_EQ(FormatValue(time, t.values[1], t.step), "D0 12:00");
  EXPECT_EQ(FormatValue(time, t.values[2], t.step), "D1 00:00");
  // Two hours: every 30 minutes, the hours with their day.
  t = MakeTicks(time, 3600, 3 * 3600, 4);
  EXPECT_DOUBLE_EQ(t.step, 1800.0);
  EXPECT_EQ(FormatValue(time, 3600 + 1800, t.step), "01:30");
  EXPECT_EQ(FormatValue(time, 7200, t.step), "02:00");
  EXPECT_EQ(FormatValue(time, 86400, t.step), "D1 00:00");
  // A week and more: days.
  EXPECT_EQ(FormatValue(time, 3 * 86400, 86400), "D3");
  // The tooltip in full: the day, seconds only when there are any.
  EXPECT_EQ(FormatDetail(time, 86400 + 3725, 60), "D1 01:02:05");
  EXPECT_EQ(FormatDetail(time, 86400 + 3720, 60), "D1 01:02");
  // Hours as the unit.
  const AxisFormat hours = TimeAxis(3600.0);
  EXPECT_EQ(FormatValue(hours, 26.5, 0.5), "02:30");
  EXPECT_EQ(FormatDetail(hours, 26.5, 0.5), "D1 02:30");
  // Numbers.
  EXPECT_EQ(FormatValue(NumberAxis(), 1250, 50), "1,250");
}

class PlotBehaviourTest : public ::testing::Test {
 protected:
  PlotBehaviourTest() {
    figure_ = NewFigure();
    auto ax = figure_->current_axes();
    for (int i = 0; i <= 100; ++i) {
      days_.push_back(i);
      ore_.push_back(100 + 10 * std::sin(i * 0.1));
      food_.push_back(90 + i * 0.2);
    }
    ore_line_ = ax->plot(days_, ore_);
    ore_line_->display_name("Ore");
    ax->hold(true);
    ax->plot(days_, food_)->display_name("Food");
  }

  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](ui::Context &ctx) {
          options_.select_range = true;
          result_ = Plot(ctx, "##prices", figure_, state_, options_);
        },
        std::move(events));
  }

  auto Area() -> Rect {
    Frame();
    Frame();
    return state_.scene->frames.front().area;
  }

  UiHarness h_{{640, 400}};
  std::shared_ptr<matplot::figure_type> figure_;
  matplot::line_handle ore_line_;
  std::vector<double> days_;
  std::vector<double> ore_;
  std::vector<double> food_;
  PlotState state_;
  PlotOptions options_;
  PlotResult result_;
};

TEST_F(PlotBehaviourTest, TheSceneIsBuiltOnceAndRebuiltOnChanges) {
  Frame();
  Frame();
  Frame();
  ASSERT_TRUE(state_.scene);
  const std::uint64_t built = state_.builds;
  EXPECT_LE(built, 2U) << "the first frames lay out, then replay";
  Frame();
  Frame();
  EXPECT_EQ(state_.builds, built);
  // New data: the app bumps the revision.
  ore_line_->y_data(food_);
  ++options_.revision;
  Frame();
  EXPECT_EQ(state_.builds, built + 1);
  ASSERT_EQ(state_.scene->frames.size(), 1U);
  const auto &f = state_.scene->frames.front();
  EXPECT_DOUBLE_EQ(f.x0, 0.0);
  EXPECT_DOUBLE_EQ(f.x1, 100.0);
}

TEST_F(PlotBehaviourTest, WheelZoomsAroundThePointerAndDoubleClickResets) {
  const Rect area = Area();
  const Vec2 c = area.Center();
  Frame({InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, 2)});
  Frame();
  EXPECT_TRUE(state_.zoomed);
  EXPECT_GT(state_.x0, 0.0);
  EXPECT_LT(state_.x1, 100.0);
  EXPECT_NEAR((state_.x0 + state_.x1) / 2, 50.0, 2.0);
  EXPECT_TRUE(result_.hovered);
  EXPECT_NEAR(result_.x, 50.0, 2.0);
  Frame(UiHarness::Click(c));
  Frame(UiHarness::Click(c));
  EXPECT_FALSE(state_.zoomed);
}

TEST_F(PlotBehaviourTest, LegendHidesSeries) {
  Frame();
  Id item = kNoId;
  h_.Frame([&](ui::Context &ctx) {
    Plot(ctx, "##prices", figure_, state_, options_);
    ctx.PushId(ctx.GetId("##prices"));
    item = ctx.GetId("##legendFood");
    ctx.PopId();
  });
  Frame(UiHarness::Click(h_.Center(item)));
  EXPECT_TRUE(state_.hidden.contains("Food"));
  Frame(UiHarness::Click(h_.Center(item)));
  EXPECT_FALSE(state_.hidden.contains("Food"));
}

TEST_F(PlotBehaviourTest, ShiftDragSelectsARangeAndDragPans) {
  const Rect area = Area();
  const float y = area.Center().y;
  const float a = area.x0 + area.Width() * 0.25F;
  const float b = area.x0 + area.Width() * 0.5F;
  Frame({InputEvent::Move(a, y),
         InputEvent::Down(a, y, ui::PointerButton::kLeft, ui::kModShift)});
  Frame({InputEvent::Move(b, y)});
  Frame({InputEvent::Up(b, y)});
  ASSERT_TRUE(state_.selection.has_value());
  EXPECT_NEAR(state_.selection->first, 25.0, 1.5);
  EXPECT_NEAR(state_.selection->second, 50.0, 1.5);
  EXPECT_FALSE(state_.zoomed) << "selecting does not pan";
  // A plain drag pans (once a double click is no longer possible).
  for (int i = 0; i < 60; ++i) {
    Frame();
  }
  Frame({InputEvent::Move(b, y), InputEvent::Down(b, y)});
  Frame({InputEvent::Move(a, y)});
  Frame({InputEvent::Up(a, y)});
  EXPECT_TRUE(state_.zoomed);
  EXPECT_GT(state_.x0, 10.0);
}

// The app replaces its figure while the pointer is over the plot: the
// tooltip reads the values under the pointer from the scene painted last
// frame, before this frame's paint rebuilds it, so the scene keeps the old
// figure alive until then (under ASan, a raw pointer was a use after free).
TEST_F(PlotBehaviourTest, AReplacedFigureLivesUntilTheSceneIsRebuilt) {
  const Rect area = Area();
  const Vec2 c = area.Center();
  Frame({InputEvent::Move(c.x, c.y)});
  Frame();
  ASSERT_TRUE(result_.hovered);
  const std::weak_ptr<matplot::figure_type> old = figure_;
  ore_line_.reset();
  figure_ = NewFigure();
  std::vector<double> y(days_.size(), 42.0);
  figure_->current_axes()->plot(days_, y)->display_name("Water");
  EXPECT_FALSE(old.expired()) << "the scene still points into it";
  ++options_.revision;
  Frame();
  EXPECT_TRUE(result_.hovered);
  EXPECT_TRUE(old.expired()) << "released once the scene was rebuilt";
  ASSERT_TRUE(state_.scene);
  EXPECT_EQ(&*state_.scene->figure, figure_.get());
  ASSERT_EQ(state_.scene->frames.size(), 1U);
  EXPECT_EQ(&*state_.scene->frames.front().axes,
            figure_->children().front().get());
  // And the tooltip reads the new figure from now on.
  Frame();
  EXPECT_TRUE(result_.hovered);
}

// A live series: twenty thousand points, updated, rebuilt once per update
// and replayed (a copy of the kept geometry) on other frames.
TEST(PlotPerformanceTest, ALiveSeriesOfThousandsOfPointsReplays) {
  UiHarness h({1000, 400});
  auto figure = NewFigure();
  std::vector<double> x(20000);
  std::vector<double> y(20000);
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] = static_cast<double>(i);
    y[i] = std::sin(static_cast<double>(i) * 0.01) * 50 +
           std::sin(static_cast<double>(i) * 0.7) * 5;
  }
  auto line = figure->current_axes()->plot(x, y);
  PlotState state;
  PlotOptions options;
  options.height = ui::SizeSpec::Fixed(300);
  const auto frame = [&] {
    h.Frame(
        [&](ui::Context &ctx) { Plot(ctx, "##live", figure, state, options); });
  };
  frame();
  frame();
  const std::uint64_t built = state.builds;
  const std::size_t instances = state.scene->geometry.instances().size();
  // Reduced per pixel column: a few segments per column, not 20,000.
  EXPECT_LT(instances, 6000U);
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 30; ++i) {
    frame();
  }
  const auto t1 = std::chrono::steady_clock::now();
  EXPECT_EQ(state.builds, built);
  for (int i = 0; i < 10; ++i) {
    y[static_cast<std::size_t>(i)] += 1.0;
    line->y_data(y);
    ++options.revision;
    frame();
  }
  const auto t2 = std::chrono::steady_clock::now();
  EXPECT_EQ(state.builds, built + 10);
  const auto ms = [](auto d) {
    return std::chrono::duration<double, std::milli>(d).count();
  };
  std::printf("replay frame %.2f ms, rebuild frame %.2f ms (%zu instances)\n",
              ms(t1 - t0) / 30,
              ms(t2 - t1) / 10,
              instances);
}

class PlotImageTest : public testing::GpuTest {
 protected:
  void Expect(UiHarness &h, const std::string &name) {
    gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
    gfx::OffscreenTarget target(
        ctx(),
        {static_cast<std::uint32_t>(h.ctx().viewport().x),
         static_cast<std::uint32_t>(h.ctx().viewport().y)});
    testing::ExpectMatchesReference(
        gfx::RenderToImage(ctx(),
                           renderer,
                           target,
                           h.ctx().draw_lists(),
                           h.ctx().theme().colors.background),
        name,
        2,
        16);
  }
};

// Lines (thousands of points, a dashed one with markers), an area, stairs,
// error bars and a second y axis, with title, labels, legend and the
// crosshair's tooltip.
TEST_F(PlotImageTest, LinesAreasStairsAndASecondAxis) {
  UiHarness h({760, 380});
  auto figure = NewFigure();
  auto ax = figure->current_axes();
  std::vector<double> days;
  std::vector<double> ore;
  std::vector<double> traders;
  for (int i = 0; i <= 2000; ++i) {
    const double d = i * 0.01;
    days.push_back(d);
    ore.push_back(100 + 12 * std::sin(d * 0.3) + 3 * std::sin(d * 5.0));
    traders.push_back(2.0e6 + 1.5e5 * d + 2e5 * std::sin(d));
  }
  ax->plot(days, ore)->display_name("Ore");
  ax->hold(true);
  const std::vector<double> week{0, 3, 6, 9, 12, 15, 18, 20};
  const std::vector<double> food{80, 84, 83, 90, 88, 92, 95, 94};
  ax->area(week, std::vector<double>{60, 62, 65, 61, 66, 70, 68, 67})
      ->display_name("Stock");
  auto dashed = ax->plot(week, food, "--o");
  dashed->display_name("Food");
  ax->stairs(week, std::vector<double>{75, 72, 78, 74, 76, 71, 73, 72})
      ->display_name("Water");
  ax->errorbar(std::vector<double>{4, 10, 16},
               std::vector<double>{110, 108, 112},
               std::vector<double>{3, 5, 2})
      ->display_name("Quotes");
  auto money = ax->plot(days, traders);
  money->use_y2(true);
  money->display_name("Traders (Cr)");
  ax->title("Prices and money");
  ax->xlabel("day");
  ax->ylabel("% of base price");
  ax->y2label("Cr");
  PlotState state;
  const auto ui = [&](ui::Context &ctx) {
    ui::LayoutSpec root;
    root.padding = ui::Padding::All(12);
    root.width = ui::SizeSpec::Grow();
    root.height = ui::SizeSpec::Grow();
    ui::BeginColumn(ctx, root);
    PlotOptions options;
    options.height = ui::SizeSpec::Fixed(320);
    Plot(ctx, "##plot", figure, state, options);
    ui::End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui, {InputEvent::Move(430, 200)});
  h.Frame(ui);
  Expect(h, "plot_lines");
}

// Markets: candlesticks over game time with volume bars below (two axes),
// and an order book's depth.
TEST_F(PlotImageTest, CandlesticksVolumeAndDepth) {
  UiHarness h({760, 420});
  auto figure = NewFigure();
  auto prices = figure->current_axes();
  auto volume = figure->add_axes({0.0F, 0.0F, 0.62F, 0.3F});
  prices->position({0.0F, 0.3F, 0.62F, 0.7F});
  auto depth = figure->add_axes({0.62F, 0.0F, 0.38F, 1.0F});
  std::vector<double> t;
  std::vector<double> o;
  std::vector<double> hi;
  std::vector<double> lo;
  std::vector<double> c;
  std::vector<double> v;
  double last = 100;
  for (int i = 0; i < 40; ++i) {
    // Hours of a game day, in seconds.
    t.push_back(i * 3600.0);
    const double open = last;
    const double close =
        open + 4 * std::sin(i * 0.9) + 2 * std::cos(i * 0.37) - 0.3;
    o.push_back(open);
    c.push_back(close);
    hi.push_back(std::max(open, close) + 1.5 + std::fabs(std::sin(i * 1.3)));
    lo.push_back(std::min(open, close) - 1.2 - std::fabs(std::cos(i * 0.7)));
    v.push_back(200 + 120 * std::fabs(std::sin(i * 0.5)));
    last = close;
  }
  AddCandlesticks(*prices, t, o, hi, lo, c)->display_name("Ore");
  prices->title("Ore, hourly");
  volume->bar(t, v);
  AddDepthChart(*depth,
                {99.5, 99, 98.5, 98, 97},
                {40, 60, 30, 80, 120},
                {100.5, 101, 101.5, 102, 104},
                {30, 50, 90, 40, 150});
  depth->title("Order book");
  PlotState state;
  const auto ui = [&](ui::Context &ctx) {
    ui::LayoutSpec root;
    root.padding = ui::Padding::All(12);
    root.width = ui::SizeSpec::Grow();
    root.height = ui::SizeSpec::Grow();
    ui::BeginColumn(ctx, root);
    PlotOptions options;
    options.height = ui::SizeSpec::Fixed(390);
    // Time for the prices and the volume, price for the order book.
    options.x = TimeAxis();
    options.axes_x = {TimeAxis(), TimeAxis(), NumberAxis()};
    Plot(ctx, "##markets", figure, state, options);
    ui::End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui, {InputEvent::Move(250, 150)});
  h.Frame(ui);
  Expect(h, "plot_markets");
}

// Grouped bars and a histogram.
TEST_F(PlotImageTest, BarsAndHistogram) {
  UiHarness h({760, 300});
  auto figure = NewFigure();
  auto bars = figure->current_axes();
  bars->position({0.0F, 0.0F, 0.5F, 1.0F});
  auto hist = figure->add_axes({0.5F, 0.0F, 0.5F, 1.0F});
  bars->bar(std::vector<std::vector<double>>{{3, 5, 2, 6}, {4, 3, 5, 4}});
  bars->title("Sales by week");
  std::vector<double> data;
  for (int i = 0; i < 2000; ++i) {
    data.push_back(50 + 15 * std::sin(i * 12.9898) * std::cos(i * 4.1414));
  }
  hist->hist(data, 20);
  hist->title("Trip times");
  PlotState state;
  const auto ui = [&](ui::Context &ctx) {
    ui::LayoutSpec root;
    root.padding = ui::Padding::All(12);
    root.width = ui::SizeSpec::Grow();
    root.height = ui::SizeSpec::Grow();
    ui::BeginColumn(ctx, root);
    PlotOptions options;
    options.height = ui::SizeSpec::Fixed(270);
    Plot(ctx, "##bars", figure, state, options);
    ui::End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  // matplot++ sets the bars' and histogram's y limits to 0..inf: the top
  // stays automatic.
  ASSERT_EQ(state.scene->frames.size(), 2U);
  EXPECT_DOUBLE_EQ(state.scene->frames[0].y0, 0.0);
  EXPECT_NEAR(state.scene->frames[0].y1, 6.3, 0.01);
  EXPECT_DOUBLE_EQ(state.scene->frames[1].y0, 0.0);
  Expect(h, "plot_bars");
}

// Stacked areas (money by kind of company) and a scatter (trips: distance
// against profit).
TEST_F(PlotImageTest, StackedAreasAndScatter) {
  UiHarness h({760, 300});
  auto figure = NewFigure();
  auto money = figure->current_axes();
  money->position({0.0F, 0.0F, 0.55F, 1.0F});
  auto trips = figure->add_axes({0.55F, 0.0F, 0.45F, 1.0F});
  std::vector<double> days;
  std::vector<std::vector<double>> kinds(3);
  for (int d = 0; d <= 30; ++d) {
    days.push_back(d);
    kinds[0].push_back(40 + 5 * std::sin(d * 0.3));
    kinds[1].push_back(25 + d * 0.5);
    kinds[2].push_back(15 + 4 * std::cos(d * 0.2));
  }
  // area() returns the areas last first.
  const auto areas = money->area(days, kinds);
  areas[2]->display_name("Traders");
  areas[1]->display_name("Producers");
  areas[0]->display_name("Factions");
  money->title("Money by kind (stacked)");
  std::vector<double> km;
  std::vector<double> profit;
  for (int i = 0; i < 300; ++i) {
    km.push_back(std::fmod(i * 37.7, 400.0));
    profit.push_back(km.back() * 2.5 + 200 * std::sin(i * 1.7));
  }
  trips->scatter(km, profit)->display_name("Trips");
  trips->title("Profit by distance");
  PlotState state;
  const auto ui = [&](ui::Context &ctx) {
    ui::LayoutSpec root;
    root.padding = ui::Padding::All(12);
    root.width = ui::SizeSpec::Grow();
    root.height = ui::SizeSpec::Grow();
    ui::BeginColumn(ctx, root);
    PlotOptions options;
    options.height = ui::SizeSpec::Fixed(250);
    Plot(ctx, "##stacked", figure, state, options);
    ui::End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  // Stacked: the top layer's area reaches the sum of all three.
  ASSERT_EQ(state.scene->frames.size(), 2U);
  EXPECT_GT(state.scene->frames[0].y1, 95.0);
  Expect(h, "plot_stacked");
}

}  // namespace
}  // namespace gloomy::plot
