#include "gloomy/gfx/draw_list.h"

#include <gtest/gtest.h>

namespace gloomy::gfx {
namespace {

TEST(DrawListTest, FillRectRecordsOneInstance) {
  DrawList list;
  list.FillRect(Rect{1, 2, 3, 4}, Color::Rgb(0xFF0000));
  ASSERT_EQ(list.instances().size(), 1U);
  const Instance &inst = list.instances()[0];
  EXPECT_EQ(inst.rect[0], 1.0F);
  EXPECT_EQ(inst.rect[3], 4.0F);
  EXPECT_EQ(inst.color, 0xFF0000FFU);
  EXPECT_EQ(inst.kind, static_cast<std::uint32_t>(PrimitiveKind::kRect));
}

TEST(DrawListTest, HexagonsRecordTheirCircumcircleAndOrientation) {
  DrawList list;
  list.FillHexagon({50, 40}, 10, Color::Rgb(0xFFFFFF));
  list.StrokeHexagon({50, 40}, 10, Color::Rgb(0xFFFFFF), 2, true);
  ASSERT_EQ(list.instances().size(), 2U);
  const Instance &fill = list.instances()[0];
  EXPECT_EQ(fill.kind, static_cast<std::uint32_t>(PrimitiveKind::kHexagon));
  EXPECT_FLOAT_EQ(fill.rect[0], 40.0F);
  EXPECT_FLOAT_EQ(fill.rect[3], 50.0F);
  EXPECT_FLOAT_EQ(fill.radii[0], 0.0F);
  EXPECT_FLOAT_EQ(fill.stroke, 0.0F);
  EXPECT_FLOAT_EQ(list.instances()[1].radii[0], 1.0F);
  EXPECT_FLOAT_EQ(list.instances()[1].stroke, 2.0F);
  list.FillHexagon({50, 40}, 0, Color::Rgb(0xFFFFFF));
  EXPECT_EQ(list.instances().size(), 2U) << "no size, nothing drawn";
}

TEST(DrawListTest, AlphaStackFadesWhatIsDrawnInside) {
  DrawList list;
  const auto alpha_of = [&](std::size_t i) {
    return list.instances()[i].color >> 24U;
  };
  list.FillRect(Rect::FromXywh(0, 0, 4, 4), Color::Rgb(0xFFFFFF));
  list.PushAlpha(0.5F);
  list.FillCircle({8, 8}, 4, Color::Rgb(0xFFFFFF));
  list.PushAlpha(0.5F);
  list.Line({0, 0}, {8, 8}, 1, Color::Rgba(0xFFFFFF80));
  list.PopAlpha();
  list.FillHexagon({8, 8}, 4, Color::Rgb(0xFFFFFF));
  list.PopAlpha();
  list.PopAlpha();
  list.FillRect(Rect::FromXywh(0, 0, 4, 4), Color::Rgb(0xFFFFFF));
  ASSERT_EQ(list.instances().size(), 5U);
  EXPECT_EQ(alpha_of(0), 255U);
  EXPECT_NEAR(static_cast<double>(alpha_of(1)), 128.0, 1.0);
  EXPECT_NEAR(static_cast<double>(alpha_of(2)), 32.0, 1.0) << "nested";
  EXPECT_NEAR(static_cast<double>(alpha_of(3)), 128.0, 1.0);
  EXPECT_EQ(alpha_of(4), 255U) << "popped past the bottom: still 1";
  list.PushAlpha(0.25F);
  list.Clear();
  EXPECT_FLOAT_EQ(list.alpha(), 1.0F);
}

TEST(DrawListTest, ClipStackIntersects) {
  DrawList list;
  list.PushClip(Rect{0, 0, 100, 100});
  list.PushClip(Rect{50, 50, 200, 200});
  EXPECT_EQ(list.clip(), (Rect{50, 50, 100, 100}));
  list.FillRect(Rect{60, 60, 70, 70}, Color{});
  EXPECT_EQ(list.instances().back().clip[0], 50.0F);
  list.PopClip();
  EXPECT_EQ(list.clip(), (Rect{0, 0, 100, 100}));
}

TEST(DrawListTest, CulledShapesAreDropped) {
  DrawList list;
  list.PushClip(Rect{0, 0, 10, 10});
  list.FillRect(Rect{20, 20, 30, 30}, Color{});
  list.FillCircle({50, 50}, 5, Color{});
  list.Line({20, 20}, {40, 40}, 1, Color{});
  EXPECT_TRUE(list.instances().empty());
}

TEST(DrawListTest, GlyphStoresPage) {
  DrawList list;
  list.Glyph(Rect{0, 0, 4, 4}, Rect{0, 0, 1, 1}, 2, Color{});
  EXPECT_EQ(list.instances()[0].kind,
            static_cast<std::uint32_t>(PrimitiveKind::kGlyph) | (2U << 8U));
}

TEST(DrawListTest, EmptyRectsAreDropped) {
  DrawList list;
  list.FillRect(Rect{5, 5, 5, 10}, Color{});
  list.StrokeRect(Rect{0, 0, 10, 10}, Color{}, 0.0F);
  EXPECT_TRUE(list.instances().empty());
}

}  // namespace
}  // namespace gloomy::gfx
