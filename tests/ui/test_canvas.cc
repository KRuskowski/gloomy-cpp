#include "gloomy/ui/canvas.h"

#include <gtest/gtest.h>

#include <algorithm>

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

class CanvasTest : public ::testing::Test {
 protected:
  CanvasTest() { options_.fit = {-100, -50, 100, 50}; }

  auto Frame(std::vector<InputEvent> events = {}) -> CanvasResult {
    CanvasResult result;
    h_.Frame(
        [&](Context &ctx) {
          LayoutSpec root;
          root.width = SizeSpec::Grow();
          root.height = SizeSpec::Grow();
          BeginColumn(ctx, root);
          result = Canvas(ctx, "##map", state_, options_, nullptr);
          End(ctx);
        },
        std::move(events));
    return result;
  }

  UiHarness h_{{400, 200}};
  CanvasState state_;
  CanvasOptions options_;
};

TEST_F(CanvasTest, FitsTheBoundsOnTheFirstFrames) {
  Frame();
  Frame();
  EXPECT_DOUBLE_EQ(state_.x, 0.0);
  EXPECT_DOUBLE_EQ(state_.y, 0.0);
  // 200 x 100 world units into 400 x 200 pixels less 16 px margins.
  EXPECT_NEAR(state_.zoom, (200.0 - 32.0) / 100.0, 1e-9);
}

TEST_F(CanvasTest, WheelZoomsAroundThePointer) {
  Frame();
  const CanvasResult before = Frame({InputEvent::Move(300, 60)});
  const WorldPoint under = before.pointer;
  const double zoom = state_.zoom;
  const CanvasResult after =
      Frame({InputEvent::Move(300, 60), InputEvent::Wheel(0, 2)});
  EXPECT_NEAR(state_.zoom, zoom * 1.15 * 1.15, 1e-9);
  const WorldPoint still = after.view.ToWorld({300, 60});
  EXPECT_NEAR(still.x, under.x, 1e-6);
  EXPECT_NEAR(still.y, under.y, 1e-6);
}

TEST_F(CanvasTest, DragPansAndIsNotAClick) {
  Frame();
  Frame();
  const double x = state_.x;
  const double zoom = state_.zoom;
  Frame({InputEvent::Move(200, 100), InputEvent::Down(200, 100)});
  Frame({InputEvent::Move(240, 100)});
  EXPECT_NEAR(state_.x, x - 40.0 / zoom, 1e-9);
  const CanvasResult up = Frame({InputEvent::Up(240, 100)});
  EXPECT_FALSE(up.clicked);
  const CanvasResult click = Frame(UiHarness::Click({100, 50}));
  EXPECT_TRUE(click.clicked);
  EXPECT_NEAR(click.pointer.x, click.view.ToWorld({100, 50}).x, 1e-9);
}

TEST_F(CanvasTest, KeysZoomAndFitWhileFocused) {
  Frame();
  const CanvasResult r = Frame(UiHarness::Click({200, 100}));
  ASSERT_TRUE(h_.ctx().HasFocus(r.id));
  const double zoom = state_.zoom;
  Frame(UiHarness::Key(Key::kEquals));
  EXPECT_NEAR(state_.zoom, zoom * 1.15, 1e-9);
  Frame(UiHarness::Key(Key::kMinus));
  Frame(UiHarness::Key(Key::kMinus));
  EXPECT_NEAR(state_.zoom, zoom / 1.15, 1e-9);
  state_.x = 77;
  Frame(UiHarness::Key(Key::kHome));
  EXPECT_DOUBLE_EQ(state_.x, 0.0);
  EXPECT_NEAR(state_.zoom, zoom, 1e-9);
}

TEST(CanvasMarksTest, TargetBracketsBreatheAroundTheCentre) {
  auto extent = [](double time) {
    gfx::DrawList list;
    DrawTargetBrackets(list, {100, 100}, 10, Color::Rgb(0xFF5050), time);
    // Four corners of two strokes each, over a halo: 16 lines.
    EXPECT_EQ(list.instances().size(), 16U);
    float right = 0.0F;
    for (const gfx::Instance &i : list.instances()) {
      right = std::max({right, i.rect[0], i.rect[2]});
    }
    return right;
  };
  EXPECT_FLOAT_EQ(extent(0.0), 110.0F);
  EXPECT_FLOAT_EQ(extent(0.5), 113.0F);
  EXPECT_FLOAT_EQ(extent(1.0), extent(0.0));
}

TEST(CanvasMarksTest, HoverRingIsFainterThanItsColour) {
  gfx::DrawList list;
  DrawHoverRing(list, {50, 50}, 12, Color::Rgb(0xFFFFFF));
  ASSERT_EQ(list.instances().size(), 1U);
  EXPECT_LT(list.instances()[0].color >> 24U, 0xFFU);
}

TEST(CanvasPickTest, PrefersHigherLayersThenTheNearest) {
  CanvasView view;
  view.rect = {0, 0, 100, 100};
  view.zoom = 1.0;
  const PickItem items[] = {
      {.key = 1, .pos = {0, 0}, .radius = 20, .layer = 0},
      {.key = 2, .pos = {3, 0}, .radius = 6, .layer = 1},
      {.key = 3, .pos = {1, 0}, .radius = 20, .layer = 0},
      {.key = 4, .pos = {30, 30}, .radius = 6, .layer = 2},
  };
  // Screen centre (50, 50) is world (0, 0).
  EXPECT_EQ(PickNearest(view, items, {50, 50}), 2U);
  EXPECT_EQ(PickNearest(view, items, {49, 50}), 2U);
  EXPECT_EQ(PickNearest(view, items, {40, 50}), 1U);
  EXPECT_EQ(PickNearest(view, items, {80, 80}), 4U);
  EXPECT_FALSE(PickNearest(view, items, {5, 95}).has_value());
}

TEST(CanvasPickTest, PickAllOrdersByLayerThenDistance) {
  CanvasView view;
  view.rect = {0, 0, 100, 100};
  view.zoom = 1.0;
  const PickItem items[] = {
      {.key = 1, .pos = {0, 0}, .radius = 20, .layer = 0},
      {.key = 2, .pos = {3, 0}, .radius = 6, .layer = 1},
      {.key = 3, .pos = {1, 0}, .radius = 20, .layer = 0},
      {.key = 4, .pos = {30, 30}, .radius = 6, .layer = 2},
      {.key = 5, .pos = {5, 0}, .radius = 20, .layer = 1},
  };
  EXPECT_EQ(PickAll(view, items, {50, 50}),
            (std::vector<std::uint64_t>{2, 5, 1, 3}));
  EXPECT_EQ(PickAll(view, items, {50, 50}).front(),
            PickNearest(view, items, {50, 50}));
  EXPECT_EQ(PickAll(view, items, {80, 80}), (std::vector<std::uint64_t>{4}));
  EXPECT_TRUE(PickAll(view, items, {5, 95}).empty());
}

TEST(CanvasPickTest, ClicksAtOneSpotCycleThroughTheStack) {
  PickCycle cycle;
  EXPECT_FALSE(cycle.current().has_value());
  EXPECT_EQ(cycle.Click({10, 10}, {7, 8, 9}), 7U);
  EXPECT_EQ(cycle.Hint(), "1/3");
  EXPECT_EQ(cycle.Click({11, 12}, {7, 8, 9}), 8U);
  EXPECT_EQ(cycle.Hint(), "2/3");
  // Moving items reorder by distance; the cycle keeps its order.
  EXPECT_EQ(cycle.Click({10, 10}, {9, 7, 8}), 9U);
  EXPECT_EQ(cycle.Click({10, 10}, {7, 8, 9}), 7U);
  EXPECT_EQ(cycle.current(), 7U);
  EXPECT_TRUE(cycle.Near({13, 10}));
  EXPECT_FALSE(cycle.Near({20, 10}));
  // An item came or went: on after the current one in the new order.
  EXPECT_EQ(cycle.Click({10, 10}, {7, 6, 8, 9}), 6U);
  EXPECT_EQ(cycle.Click({10, 10}, {6, 9}), 9U);
  EXPECT_EQ(cycle.Click({10, 10}, {6, 9}), 6U);
  // Elsewhere, or over items without the current one, starts over.
  EXPECT_EQ(cycle.Click({40, 10}, {7, 8, 9}), 7U);
  EXPECT_EQ(cycle.Click({40, 10}, {8, 9}), 8U);
  EXPECT_EQ(cycle.Click({40, 10}, {8, 9}), 9U);
  cycle.Reset();
  EXPECT_EQ(cycle.Click({40, 10}, {7, 8}), 7U);
  // One item: no hint; nothing: no selection.
  EXPECT_EQ(cycle.Click({60, 10}, {5}), 5U);
  EXPECT_EQ(cycle.Hint(), "");
  EXPECT_FALSE(cycle.Click({60, 10}, {}).has_value());
  EXPECT_FALSE(cycle.current().has_value());
  EXPECT_EQ(StackHint(3), "1/3");
  EXPECT_EQ(StackHint(1), "");
}

class CanvasImageTest : public testing::GpuTest {};

// A small map: discs joined by lines, labels, a selection ring.
TEST_F(CanvasImageTest, MapWithLabels) {
  UiHarness h({360, 240});
  CanvasState state;
  CanvasOptions options;
  options.fit = {0, 0, 300, 160};
  options.background = Color::Rgb(0x091219);
  const WorldPoint points[] = {{0, 0}, {120, 40}, {300, 0}, {200, 160}};
  auto ui = [&](Context &ctx) {
    LayoutSpec root;
    root.width = SizeSpec::Grow();
    root.height = SizeSpec::Grow();
    BeginColumn(ctx, root);
    Canvas(ctx,
           "##map",
           state,
           options,
           [&](PaintContext &pc, const CanvasView &view) {
             const Color line = Color::Rgb(0x465070);
             for (int i = 0; i + 1 < 4; ++i) {
               pc.list.Line(view.ToScreen(points[i]),
                            view.ToScreen(points[i + 1]),
                            2.0F,
                            line);
             }
             for (int i = 0; i < 4; ++i) {
               const Vec2 p = view.ToScreen(points[i]);
               pc.list.FillCircle(p, view.Pixels(14), Color::Rgb(0x1C2234));
               pc.list.StrokeCircle(
                   p, view.Pixels(14), Color::Rgb(0x5A6996), 1.5F);
               pc.text.DrawText(pc.list,
                                "Sector " + std::to_string(i),
                                pc.theme.fonts.sans,
                                12.0F,
                                {p.x - 20, p.y + view.Pixels(14) + 2},
                                pc.theme.colors.text_muted);
             }
             pc.list.StrokeCircle(view.ToScreen(points[1]),
                                  view.Pixels(14) + 4,
                                  Color::Rgb(0xFF5050),
                                  2.0F);
           });
    End(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
  gfx::OffscreenTarget target(ctx(), {360, 240});
  testing::ExpectMatchesReference(
      gfx::RenderToImage(ctx(),
                         renderer,
                         target,
                         h.ctx().draw_lists(),
                         h.ctx().theme().colors.background),
      "canvas",
      2,
      8);
}

}  // namespace
}  // namespace gloomy::ui
