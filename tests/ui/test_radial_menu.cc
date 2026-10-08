#include "gloomy/ui/radial_menu.h"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <vector>

#include "gloomy/ui/canvas.h"
#include "gloomy/ui/widgets.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {

namespace {

using testing::UiHarness;

constexpr float kPi = 3.14159265F;

TEST(RadialGeometry, OptionsStartAtTheTopAndGoClockwise) {
  EXPECT_FLOAT_EQ(RadialItemAngle(0, 4), 0.0F);
  EXPECT_FLOAT_EQ(RadialItemAngle(1, 4), 0.5F * kPi);
  const Vec2 top = RadialItemCentre({100, 100}, 50, 0, 4);
  EXPECT_NEAR(top.x, 100.0F, 1e-3F);
  EXPECT_NEAR(top.y, 50.0F, 1e-3F);
  const Vec2 right = RadialItemCentre({100, 100}, 50, 1, 4);
  EXPECT_NEAR(right.x, 150.0F, 1e-3F);
  EXPECT_NEAR(right.y, 100.0F, 1e-3F);
}

TEST(RadialGeometry, PickByDirectionOutsideTheDeadZone) {
  const Vec2 c{100, 100};
  EXPECT_EQ(RadialPick(c, {100, 40}, 4, 10), 0);
  EXPECT_EQ(RadialPick(c, {160, 100}, 4, 10), 1);
  EXPECT_EQ(RadialPick(c, {100, 160}, 4, 10), 2);
  EXPECT_EQ(RadialPick(c, {40, 100}, 4, 10), 3);
  // Just left of the top still points at the top option.
  EXPECT_EQ(RadialPick(c, {90, 40}, 4, 10), 0);
  EXPECT_EQ(RadialPick(c, {104, 104}, 4, 10), -1);
  EXPECT_EQ(RadialPick(c, {100, 40}, 0, 10), -1);
}

TEST(RadialGeometry, ArrowsPickTheNearestDirection) {
  EXPECT_EQ(RadialItemToward({0, -1}, 5), 0);
  EXPECT_EQ(RadialItemToward({1, 0}, 4), 1);
  EXPECT_EQ(RadialItemToward({0, 1}, 4), 2);
  EXPECT_EQ(RadialItemToward({-1, 0}, 4), 3);
  // Five options: down lies between options 2 and 3, the first wins.
  EXPECT_EQ(RadialItemToward({0, 1}, 5), 2);
  EXPECT_EQ(RadialItemToward({0, 0}, 4), -1);
}

TEST(RadialGeometry, FitKeepsTheMenuOnScreen) {
  const Vec2 fitted = RadialFit({5, 470}, 100, {640, 480});
  EXPECT_FLOAT_EQ(fitted.x, 100.0F);
  EXPECT_FLOAT_EQ(fitted.y, 380.0F);
  const Vec2 inside = RadialFit({320, 240}, 100, {640, 480});
  EXPECT_FLOAT_EQ(inside.x, 320.0F);
  // Too small a screen: centred.
  EXPECT_FLOAT_EQ(RadialFit({10, 10}, 300, {640, 480}).y, 240.0F);
}

/// A menu of four options (the third disabled) over a button.
class RadialMenuTest : public ::testing::Test {
 protected:
  RadialMenuTest() {
    for (const char *label : {"North", "East", "South", "West"}) {
      RadialItem item;
      item.label = label;
      items_.push_back(item);
    }
    items_[2].enabled = false;
    items_[2].disabled_reason = "Closed: try another way";
  }

  /// One frame: the menu first, then a button covering the screen, as an
  /// app declares a menu before what lies below.
  void Frame(std::vector<InputEvent> events = {}) {
    h_.Frame(
        [&](Context &ctx) {
          if (open_at_) {
            OpenRadialMenu(ctx, "menu", *open_at_);
            open_at_.reset();
          }
          result_ = RadialMenu(ctx, "menu", items_, "Title");
          if (open_on_right_press_ && ctx.right_down() &&
              !IsRadialMenuOpen(ctx, "menu")) {
            OpenRadialMenu(ctx, "menu", ctx.mouse());
          }
          LayoutSpec l;
          l.width = SizeSpec::Grow();
          l.height = SizeSpec::Grow();
          BeginColumn(ctx, l);
          button_clicked_ = button_clicked_ || Button(ctx, "Below");
          End(ctx);
        },
        std::move(events));
  }

  void Open(Vec2 at = kCentre) {
    open_at_ = at;
    Frame();
    Frame();
  }

  static auto Option(int i) -> Vec2 {
    return RadialItemCentre(kCentre, RadialMetrics{}.ring_radius, i, 4);
  }

  static constexpr Vec2 kCentre{320, 240};
  UiHarness h_;
  std::vector<RadialItem> items_;
  std::optional<Vec2> open_at_;
  bool open_on_right_press_ = false;
  bool button_clicked_ = false;
  RadialMenuResult result_;
};

TEST_F(RadialMenuTest, ClickingAnOptionChoosesItAndCloses) {
  Open();
  EXPECT_TRUE(result_.open);
  Frame({InputEvent::Move(Option(1).x, Option(1).y)});
  EXPECT_EQ(result_.highlight, 1);
  Frame(UiHarness::Click(Option(1)));
  EXPECT_EQ(result_.chosen, 1);
  EXPECT_FALSE(result_.open);
  EXPECT_FALSE(button_clicked_) << "the menu takes the click";
  Frame();
  EXPECT_FALSE(IsRadialMenuOpen(h_.ctx(), "menu"));
}

TEST_F(RadialMenuTest, EscapeAndAClickInTheMiddleCancel) {
  Open();
  Frame(UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(result_.cancelled);
  EXPECT_FALSE(result_.open);
  Open();
  Frame(UiHarness::Click(kCentre));
  EXPECT_TRUE(result_.cancelled);
  EXPECT_FALSE(button_clicked_);
}

TEST_F(RadialMenuTest, KeysPointAndChoose) {
  Open();
  Frame(UiHarness::Key(Key::kLeft));
  EXPECT_EQ(result_.highlight, 3);
  Frame(UiHarness::Key(Key::kTab));
  EXPECT_EQ(result_.highlight, 0);
  Frame(UiHarness::Key(Key::kEnter));
  EXPECT_EQ(result_.chosen, 0);
  Open();
  Frame(UiHarness::Key(Key::k2));
  EXPECT_EQ(result_.chosen, 1);
}

TEST_F(RadialMenuTest, DisabledOptionsSayWhyInsteadOfActing) {
  Open();
  Frame(UiHarness::Key(Key::k3));
  EXPECT_EQ(result_.chosen, -1);
  EXPECT_TRUE(result_.open);
  EXPECT_EQ(result_.highlight, 2);
  Frame(UiHarness::Key(Key::kEnter));
  EXPECT_EQ(result_.chosen, -1);
  Frame(UiHarness::Click(Option(2)));
  EXPECT_EQ(result_.chosen, -1);
  EXPECT_TRUE(result_.open);
  // The reason is drawn: its glyphs are in the overlay's list.
  bool drawn = false;
  for (const gfx::DrawList *list : h_.ctx().draw_lists()) {
    drawn = drawn || list->instances().size() > 40;
  }
  EXPECT_TRUE(drawn);
}

TEST_F(RadialMenuTest, RightDragAndReleaseChooses) {
  open_on_right_press_ = true;
  Frame({InputEvent::Move(kCentre.x, kCentre.y),
         InputEvent::Down(kCentre.x, kCentre.y, PointerButton::kRight)});
  Frame();
  EXPECT_TRUE(result_.open);
  open_on_right_press_ = false;
  Frame({InputEvent::Move(Option(3).x, Option(3).y)});
  EXPECT_EQ(result_.highlight, 3);
  Frame({InputEvent::Up(Option(3).x, Option(3).y, PointerButton::kRight)});
  EXPECT_EQ(result_.chosen, 3);
}

TEST_F(RadialMenuTest, APlainRightClickLeavesTheMenuOpen) {
  open_on_right_press_ = true;
  Frame({InputEvent::Move(kCentre.x, kCentre.y),
         InputEvent::Down(kCentre.x, kCentre.y, PointerButton::kRight)});
  open_on_right_press_ = false;
  Frame({InputEvent::Up(kCentre.x, kCentre.y, PointerButton::kRight)});
  Frame();
  EXPECT_TRUE(result_.open);
  EXPECT_EQ(result_.chosen, -1);
  // Another right press closes it.
  Frame({InputEvent::Down(10, 10, PointerButton::kRight)});
  EXPECT_TRUE(result_.cancelled);
}

TEST(RadialMenuInput, RightPressesReachNodesAndCanvases) {
  UiHarness h;
  CanvasState camera;
  CanvasResult canvas;
  auto frame = [&](std::vector<InputEvent> events) {
    h.Frame(
        [&](Context &ctx) {
          LayoutSpec l;
          l.width = SizeSpec::Grow();
          l.height = SizeSpec::Grow();
          BeginColumn(ctx, l);
          canvas = Canvas(
              ctx, "##c", camera, {}, [](PaintContext &, const CanvasView &) {
              });
          End(ctx);
        },
        std::move(events));
  };
  frame({});
  frame({});
  frame({InputEvent::Move(100, 100),
         InputEvent::Down(100, 100, PointerButton::kRight)});
  EXPECT_TRUE(canvas.right_pressed);
  EXPECT_FALSE(canvas.right_clicked);
  EXPECT_TRUE(h.ctx().right_down());
  frame({InputEvent::Up(100, 100, PointerButton::kRight)});
  EXPECT_FALSE(canvas.right_pressed);
  EXPECT_TRUE(canvas.right_clicked);
  EXPECT_TRUE(h.ctx().right_released());
  EXPECT_FALSE(h.ctx().right_down());
}

}  // namespace

}  // namespace gloomy::ui
