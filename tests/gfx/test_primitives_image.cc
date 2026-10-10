#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "test_support/gpu_test.h"

namespace gloomy::gfx {
namespace {

class PrimitivesImageTest : public testing::GpuTest {
 protected:
  auto Render(const DrawList &list, VkExtent2D extent) -> Image {
    Atlas atlas(256, 1);
    UiRenderer renderer(ctx(), OffscreenTarget::kFormat, atlas);
    OffscreenTarget target(ctx(), extent);
    const std::array<const DrawList *, 1> lists{&list};
    return RenderToImage(ctx(), renderer, target, lists, Color::Rgb(0x1E1E2E));
  }
};

TEST_F(PrimitivesImageTest, ShapesMatchReference) {
  DrawList list;
  // Card with shadow, fill and border.
  const Rect card = Rect::FromXywh(16, 16, 120, 72);
  list.Shadow(card, Color::Rgba(0x00000099), 8.0F, {0, 4}, CornerRadii::All(8));
  list.FillRect(card, Color::Rgb(0x313244), CornerRadii::All(8));
  list.StrokeRect(card, Color::Rgb(0x89B4FA), 1.0F, CornerRadii::All(8));
  // Mixed corner radii and a gradient.
  list.FillRectGradient(Rect::FromXywh(152, 16, 72, 72),
                        Color::Rgb(0xF38BA8),
                        Color::Rgb(0xFAB387),
                        {0, 16, 0, 16});
  // Pixel-aligned 1 px rectangle outline and a 3 px one.
  list.StrokeRect(Rect::FromXywh(240, 16, 64, 32), Color::Rgb(0xA6E3A1), 1);
  list.StrokeRect(Rect::FromXywh(240, 56, 64, 32), Color::Rgb(0xA6E3A1), 3);
  // Circles.
  list.FillCircle({40, 130}, 20, Color::Rgb(0xF9E2AF));
  list.StrokeCircle({96, 130}, 20, Color::Rgb(0x94E2D5), 2);
  // Lines: horizontal, vertical, diagonal.
  list.Line({136, 112.5F}, {216, 112.5F}, 1, Color::Rgb(0xCDD6F4));
  list.Line({136, 120}, {136, 180}, 2, Color::Rgb(0xCDD6F4));
  list.Line({146, 180}, {216, 124}, 2, Color::Rgb(0xEBA0AC));
  // Clipping: a large rect clipped to a box, with a semi-transparent
  // overlay on top.
  list.PushClip(Rect::FromXywh(232, 112, 72, 72));
  list.FillRect(Rect::FromXywh(200, 100, 200, 200), Color::Rgb(0x585B70));
  list.FillCircle({304, 184}, 40, Color::Rgba(0xCBA6F780));
  list.PopClip();
  // Outside the clip again: must not be clipped.
  list.FillRect(Rect::FromXywh(300, 188, 16, 8), Color::Rgb(0xF5C2E7));

  const Image image = Render(list, {320, 200});
  testing::ExpectMatchesReference(image, "primitives");
}

// Hexagons: a flat-top grid that tiles without gaps (columns 1.5 R apart,
// odd ones half a cell down), a pointy-top one and outlines.
TEST_F(PrimitivesImageTest, HexagonsMatchReference) {
  DrawList list;
  const float r = 12.0F;
  const float h = r * std::sqrt(3.0F);
  for (int col = 0; col < 6; ++col) {
    for (int row = 0; row < 4; ++row) {
      const float x = 20.0F + 1.5F * r * static_cast<float>(col);
      const float y = 20.0F + h * (static_cast<float>(row) +
                                   0.5F * static_cast<float>(col % 2));
      list.FillHexagon(
          {x, y},
          r,
          (col + row) % 2 == 0 ? Color::Rgb(0x313244) : Color::Rgb(0x45475A));
    }
  }
  list.FillHexagon({190, 50}, 30, Color::Rgb(0x89B4FA), true);
  list.StrokeHexagon({260, 50}, 30, Color::Rgb(0xA6E3A1), 2);
  list.StrokeHexagon({260, 50}, 18, Color::Rgba(0xF9E2AF80), 1, true);
  const Image image = Render(list, {320, 110});
  testing::ExpectMatchesReference(image, "hexagons");
  // The pointy-top hexagon (circumradius 30, inradius 26) reaches 27 px
  // up to its vertex but not 27 px to the side, where its edge is.
  const auto red = [&](int x, int y) {
    return image.At(x, y)[0];
  };
  EXPECT_EQ(red(190, 50), 0x89);
  EXPECT_EQ(red(190 + 27, 50), 0x1E) << "pointy-top: no vertex on x";
  EXPECT_EQ(red(190, 50 - 27), 0x89) << "pointy-top: vertex up";
}

TEST_F(PrimitivesImageTest, EmptyListClearsOnly) {
  DrawList list;
  const Image image = Render(list, {8, 8});
  const std::uint8_t *p = image.At(3, 3);
  EXPECT_EQ(p[0], 0x1E);
  EXPECT_EQ(p[1], 0x1E);
  EXPECT_EQ(p[2], 0x2E);
  EXPECT_EQ(p[3], 0xFF);
}

TEST_F(PrimitivesImageTest, OpaqueRectCoversExactPixels) {
  DrawList list;
  list.FillRect(Rect::FromXywh(2, 2, 4, 4), Color::Rgb(0xFF0000));
  const Image image = Render(list, {8, 8});
  EXPECT_EQ(image.At(2, 2)[0], 255);
  EXPECT_EQ(image.At(5, 5)[0], 255);
  EXPECT_EQ(image.At(1, 2)[0], 0x1E);
  EXPECT_EQ(image.At(6, 5)[0], 0x1E);
}

}  // namespace
}  // namespace gloomy::gfx
