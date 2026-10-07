#include <gtest/gtest.h>

#include <array>

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
