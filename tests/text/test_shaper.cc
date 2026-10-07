#include "gloomy/text/shaper.h"

#include <gtest/gtest.h>

#include "gloomy/core/atlas.h"
#include "gloomy/text/font_set.h"
#include "gloomy/text/text_renderer.h"

namespace gloomy::text {
namespace {

class ShaperTest : public ::testing::Test {
 protected:
  void SetUp() override { fonts_ids_ = fonts_.AddDefaultFonts({}); }

  FontSet fonts_;
  DefaultFonts fonts_ids_;
};

TEST_F(ShaperTest, ShapesAsciiWithClusters) {
  Shaper shaper(fonts_);
  ShapedText out;
  shaper.Shape("Hello", fonts_ids_.sans, 16.0F, out);
  ASSERT_EQ(out.glyphs.size(), 5U);
  for (std::uint32_t i = 0; i < 5; ++i) {
    EXPECT_EQ(out.glyphs[i].cluster, i);
  }
  EXPECT_GT(out.width, 30.0F);
  EXPECT_LT(out.width, 45.0F);
  EXPECT_FLOAT_EQ(out.glyphs[1].x, out.glyphs[0].advance);
}

TEST_F(ShaperTest, WidthScalesWithSize) {
  Shaper shaper(fonts_);
  ShapedText small;
  ShapedText large;
  shaper.Shape("Market prices", fonts_ids_.sans, 12.0F, small);
  shaper.Shape("Market prices", fonts_ids_.sans, 24.0F, large);
  EXPECT_NEAR(large.width, small.width * 2.0F, 0.5F);
}

TEST_F(ShaperTest, MonoDigitsHaveEqualAdvances) {
  Shaper shaper(fonts_);
  ShapedText out;
  shaper.Shape("1234567890", fonts_ids_.mono, 13.0F, out);
  for (const ShapedGlyph &g : out.glyphs) {
    EXPECT_FLOAT_EQ(g.advance, out.glyphs[0].advance);
  }
}

TEST_F(ShaperTest, MultiByteClustersAreByteOffsets) {
  Shaper shaper(fonts_);
  ShapedText out;
  shaper.Shape("a\xC3\xA9z", fonts_ids_.sans, 16.0F, out);
  ASSERT_EQ(out.glyphs.size(), 3U);
  EXPECT_EQ(out.glyphs[1].cluster, 1U);
  EXPECT_EQ(out.glyphs[2].cluster, 3U);
  EXPECT_FLOAT_EQ(out.CaretX(3), out.glyphs[2].x);
  EXPECT_FLOAT_EQ(out.CaretX(4), out.width);
}

TEST_F(ShaperTest, HitTestFindsNearestBoundary) {
  Shaper shaper(fonts_);
  ShapedText out;
  shaper.Shape("abc", fonts_ids_.sans, 20.0F, out);
  EXPECT_EQ(out.HitTest(-5.0F), 0U);
  EXPECT_EQ(out.HitTest(out.glyphs[1].x + 0.1F), 1U);
  EXPECT_EQ(out.HitTest(out.width + 10.0F), 3U);
}

TEST_F(ShaperTest, FallsBackToLaterFaces) {
  FontSet fonts;
  const DefaultFonts ids = fonts.AddDefaultFonts();
  if (fonts.face_count() < 4) {
    GTEST_SKIP() << "no fallback font installed";
  }
  Shaper shaper(fonts);
  ShapedText out;
  // U+2713 CHECK MARK is in DejaVu Sans but not in Liberation Sans.
  shaper.Shape("a\xE2\x9C\x93", ids.sans, 16.0F, out);
  ASSERT_EQ(out.glyphs.size(), 2U);
  EXPECT_NE(out.glyphs[1].glyph, 0U);
  EXPECT_NE(out.glyphs[1].face, out.glyphs[0].face);
}

TEST_F(ShaperTest, WrapBreaksAtSpacesAndHardBreaks) {
  Atlas atlas(512, 1);
  TextSystem text(fonts_, atlas);
  const std::string para = "one two three four\nfive";
  const float word = text.Measure("three", fonts_ids_.sans, 16.0F);
  const WrappedText wrapped =
      text.Wrap(para, fonts_ids_.sans, 16.0F, word * 1.6F);
  ASSERT_GE(wrapped.lines.size(), 4U);
  auto line = [&](std::size_t i) {
    return para.substr(wrapped.lines[i].begin,
                       wrapped.lines[i].end - wrapped.lines[i].begin);
  };
  EXPECT_EQ(line(0), "one two");
  EXPECT_EQ(line(1), "three");
  EXPECT_EQ(line(2), "four");
  EXPECT_EQ(line(3), "five");
  for (const TextLine &l : wrapped.lines) {
    EXPECT_LE(l.width, word * 1.6F);
  }
}

TEST_F(ShaperTest, WrapSplitsOverlongWords) {
  Atlas atlas(512, 1);
  TextSystem text(fonts_, atlas);
  const WrappedText wrapped =
      text.Wrap("abcdefghijklmnop", fonts_ids_.sans, 16.0F, 40.0F);
  EXPECT_GT(wrapped.lines.size(), 2U);
  EXPECT_EQ(wrapped.lines.front().begin, 0U);
  EXPECT_EQ(wrapped.lines.back().end, 16U);
}

TEST_F(ShaperTest, ShapeCacheReturnsSameEntry) {
  Atlas atlas(512, 1);
  TextSystem text(fonts_, atlas);
  const ShapedText &a = text.Shape("cached", fonts_ids_.sans, 14.0F);
  const ShapedText &b = text.Shape("cached", fonts_ids_.sans, 14.0F);
  EXPECT_EQ(&a, &b);
  EXPECT_EQ(text.cache_size(), 1U);
  text.Shape("cached", fonts_ids_.sans, 15.0F);
  EXPECT_EQ(text.cache_size(), 2U);
}

}  // namespace
}  // namespace gloomy::text
