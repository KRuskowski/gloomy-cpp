#include "gloomy/ui/hud.h"

#include <gtest/gtest.h>

#include <cmath>

namespace gloomy::ui {
namespace {

TEST(HudTest, ArcPointsGoClockwiseFromUp) {
  const Vec2 c{100, 100};
  const Vec2 up = ArcPoint(c, 10, 0.0F);
  EXPECT_NEAR(up.x, 100.0F, 1e-4F);
  EXPECT_NEAR(up.y, 90.0F, 1e-4F);
  const Vec2 right = ArcPoint(c, 10, 1.5707963F);
  EXPECT_NEAR(right.x, 110.0F, 1e-4F);
  EXPECT_NEAR(right.y, 100.0F, 1e-4F);
}

TEST(HudTest, ArcsAreSmoothLineStrips) {
  gfx::DrawList list;
  StrokeArc(list, {0, 0}, 50, 0.0F, 3.14159F, 2.0F, Color::Rgb(0xFFFFFF));
  // About 6 degrees per segment.
  EXPECT_EQ(list.instances().size(), 30U);
  gfx::DrawList none;
  StrokeArc(none, {0, 0}, 50, 1.0F, 1.0F, 2.0F, Color::Rgb(0xFFFFFF));
  EXPECT_TRUE(none.instances().empty());
}

TEST(HudTest, EdgePointsLieOnTheInsetRectangle) {
  const Rect r{0, 0, 200, 100};
  const Vec2 centre{100, 50};
  const Vec2 right = EdgePoint(r, centre, {1, 0}, 10);
  EXPECT_NEAR(right.x, 190.0F, 1e-4F);
  EXPECT_NEAR(right.y, 50.0F, 1e-4F);
  const Vec2 up_left = EdgePoint(r, centre, {-1, -1}, 10);
  // The top edge (40 px up) comes before the left one (90 px).
  EXPECT_NEAR(up_left.y, 10.0F, 1e-4F);
  EXPECT_NEAR(up_left.x, 60.0F, 1e-4F);
  const Vec2 still = EdgePoint(r, centre, {0, 0}, 10);
  EXPECT_EQ(still.x, centre.x);
}

TEST(HudTest, ArrowsGlowAndScanlines) {
  gfx::DrawList list;
  EdgeArrow(list, {50, 50}, {1, 0}, 10, 2, Color::Rgb(0xFF0000));
  EXPECT_EQ(list.instances().size(), 2U);
  GlowLine(list, {0, 0}, {10, 0}, 1, Color::Rgb(0xFFFFFF), 3);
  EXPECT_EQ(list.instances().size(), 5U);
  gfx::DrawList lines;
  Scanlines(lines, {0, 0, 10, 30}, 3, Color::Rgba(0x00000020));
  EXPECT_EQ(lines.instances().size(), 10U);
}

}  // namespace
}  // namespace gloomy::ui
