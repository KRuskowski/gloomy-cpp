#include "gloomy/icon/icon_set.h"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/icon/icon_cache.h"
#include "gloomy/ui/data_grid.h"
#include "gloomy/ui/widgets.h"
#include "test_support/market_table.h"
#include "test_support/ui_harness.h"

namespace gloomy::icon {
namespace {

// A 16 x 16 square in the middle of a 24 x 24 view box.
constexpr const char *kSquare =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"
         viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16"
         fill="#fff"/></svg>)";

// A ring: a stroked circle, 2 units wide.
constexpr const char *kRing =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"
         viewBox="0 0 24 24"><circle cx="12" cy="12" r="8" fill="none"
         stroke="#fff" stroke-width="2"/></svg>)";

auto CountInstances(const gfx::DrawList &list, gfx::PrimitiveKind kind) -> int {
  int n = 0;
  for (const gfx::Instance &inst : list.instances()) {
    n += (inst.kind & 0xFFU) == static_cast<std::uint32_t>(kind) ? 1 : 0;
  }
  return n;
}

TEST(IconSetTest, AddsFindsAndReplacesByName) {
  IconSet icons;
  const IconId square = icons.Add("square", kSquare);
  const IconId ring = icons.Add("ring", kRing);
  ASSERT_NE(square, kNoIcon);
  ASSERT_NE(ring, kNoIcon);
  EXPECT_NE(square, ring);
  EXPECT_EQ(icons.size(), 2U);
  EXPECT_EQ(icons.Find("ring"), ring);
  EXPECT_EQ(icons.Find("nothing"), kNoIcon);
  EXPECT_EQ(icons.Name(square), "square");
  const std::uint64_t version = icons.version();
  EXPECT_EQ(icons.Add("square", kRing), square);
  EXPECT_GT(icons.version(), version);
  EXPECT_EQ(icons.size(), 2U);
}

TEST(IconSetTest, RejectsEmptyOrBrokenSvg) {
  IconSet icons;
  EXPECT_EQ(icons.Add("empty", ""), kNoIcon);
  EXPECT_EQ(icons.Add("text", "not an svg"), kNoIcon);
  EXPECT_EQ(icons.Add("no shapes",
                      R"(<svg width="24" height="24" viewBox="0 0 24 24">
                         </svg>)"),
            kNoIcon);
  EXPECT_EQ(icons.size(), 0U);
}

TEST(IconSetTest, RasterisesCoverageAtPixelSizes) {
  IconSet icons;
  const IconId square = icons.Add("square", kSquare);
  for (const int px : {12, 24, 48}) {
    const IconImage image = icons.Rasterise(square, px);
    ASSERT_EQ(image.width, px);
    ASSERT_EQ(image.height, px);
    const int edge = px / 6;
    // Inside full, outside empty; edges on whole pixels stay crisp.
    EXPECT_EQ(image.At(px / 2, px / 2), 255) << px;
    EXPECT_EQ(image.At(edge, edge), 255) << px;
    EXPECT_EQ(image.At(edge - 1, edge), 0) << px;
    EXPECT_EQ(image.At(px - edge - 1, px / 2), 255) << px;
    EXPECT_EQ(image.At(px - edge, px / 2), 0) << px;
  }
  EXPECT_TRUE(icons.Rasterise(kNoIcon, 12).Empty());
  EXPECT_TRUE(icons.Rasterise(square, 0).Empty());
}

TEST(IconSetTest, SubpixelShiftMovesTheEdge) {
  IconSet icons;
  const IconId square = icons.Add("square", kSquare);
  const IconImage half = icons.Rasterise(square, 24, 0.5F, 0.0F);
  // The left edge now covers half of column 4 and all of column 5.
  EXPECT_NEAR(half.At(4, 12), 128, 8);
  EXPECT_EQ(half.At(5, 12), 255);
  const IconImage down = icons.Rasterise(square, 24, 0.0F, 0.25F);
  EXPECT_NEAR(down.At(12, 4), 191, 20);
  EXPECT_NEAR(down.At(8, 4), 191, 20);
}

TEST(IconSetTest, StrokesHaveTheirWidth) {
  IconSet icons;
  const IconId ring = icons.Add("ring", kRing);
  const IconImage image = icons.Rasterise(ring, 24);
  // Radius 8, 2 wide: x 3..5 on the left, the centre empty.
  EXPECT_EQ(image.At(12, 12), 0);
  EXPECT_GT(image.At(3, 12), 200);
  EXPECT_GT(image.At(4, 12), 200);
  EXPECT_EQ(image.At(1, 12), 0);
  EXPECT_EQ(image.At(6, 12), 0);
}

TEST(IconSetTest, DilateGrowsByTheRadius) {
  IconImage dot;
  dot.width = 5;
  dot.height = 5;
  dot.pixels.assign(25, 0);
  dot.pixels[2 * 5 + 2] = 200;
  const IconImage one = Dilate(dot, 1);
  ASSERT_EQ(one.width, 7);
  ASSERT_EQ(one.height, 7);
  int lit = 0;
  for (const std::uint8_t v : one.pixels) {
    lit += v == 200 ? 1 : 0;
  }
  EXPECT_EQ(lit, 9);
  EXPECT_EQ(one.At(3, 3), 200);
  EXPECT_EQ(one.At(2, 2), 200);
  EXPECT_EQ(one.At(1, 3), 0);
  // From a radius of 2 the halo is round: no square corners.
  const IconImage two = Dilate(dot, 2);
  ASSERT_EQ(two.width, 9);
  EXPECT_EQ(two.At(4, 2), 200);
  EXPECT_EQ(two.At(2, 2), 0);
  EXPECT_EQ(Dilate(dot, 0).width, 5);
}

TEST(IconCacheTest, CachesPerSizeBinAndHalo) {
  IconSet icons;
  const IconId square = icons.Add("square", kSquare);
  Atlas atlas(256, 1);
  IconCache cache(icons, atlas);
  const IconCache::Bitmap a = cache.Get(square, 12);
  ASSERT_TRUE(a.visible);
  EXPECT_EQ(a.region.w, 12);
  EXPECT_EQ(a.margin, 0);
  EXPECT_EQ(cache.Get(square, 12).region.x, a.region.x);
  EXPECT_EQ(cache.size(), 1U);
  const IconCache::Bitmap halo = cache.Get(square, 12, 1);
  EXPECT_EQ(halo.region.w, 14);
  EXPECT_EQ(halo.margin, 1);
  cache.Get(square, 12, 0, 3);
  cache.Get(square, 18);
  EXPECT_EQ(cache.size(), 4U);
  // Someone else (the glyph cache) clears the atlas: start over.
  atlas.Clear();
  cache.Get(square, 12);
  EXPECT_EQ(cache.size(), 1U);
  // A replaced icon is rasterised again.
  icons.Add("square", kRing);
  EXPECT_EQ(cache.Get(square, 12).visible, true);
  EXPECT_EQ(cache.size(), 1U);
}

TEST(IconCacheTest, ClearsTheAtlasWhenFull) {
  IconSet icons;
  const IconId square = icons.Add("square", kSquare);
  Atlas atlas(64, 1);
  IconCache cache(icons, atlas);
  const std::uint64_t generation = atlas.generation();
  for (int px = 8; px < 40; ++px) {
    EXPECT_TRUE(cache.Get(square, px).visible) << px;
  }
  EXPECT_GT(atlas.generation(), generation);
}

TEST(IconCacheTest, DrawsSnappedOrExactlyPlaced) {
  IconSet icons;
  const IconId square = icons.Add("square", kSquare);
  Atlas atlas(256, 1);
  IconCache cache(icons, atlas);
  gfx::DrawList list;
  const Color red = Color::Rgb(0xFF0000);
  cache.Draw(list, square, {20.3F, 30.6F}, 12.0F, red);
  ASSERT_EQ(list.instances().size(), 1U);
  const gfx::Instance &snapped = list.instances()[0];
  EXPECT_EQ(snapped.rect[0], std::round(20.3F - 6.0F));
  EXPECT_EQ(snapped.rect[1], std::round(30.6F - 6.0F));
  EXPECT_EQ(snapped.rect[2] - snapped.rect[0], 12.0F);
  EXPECT_EQ(snapped.color, red.Pack());
  list.Clear();
  IconDrawOptions options;
  options.subpixel = true;
  options.halo = Color::Rgb(0x000000);
  cache.Draw(list, square, {20.3F, 30.6F}, 12.0F, red, options);
  ASSERT_EQ(list.instances().size(), 2U);
  // Halo first, one pixel larger on each side, then the icon, both where
  // they are, not on whole pixels.
  EXPECT_FLOAT_EQ(list.instances()[0].rect[0], 20.3F - 7.0F);
  EXPECT_FLOAT_EQ(list.instances()[1].rect[0], 20.3F - 6.0F);
  EXPECT_FLOAT_EQ(list.instances()[1].rect[1], 30.6F - 6.0F);
  EXPECT_EQ(list.instances()[1].color, red.Pack());
  // The same body bitmap, plus the halo.
  EXPECT_EQ(cache.size(), 2U);
  list.Clear();
  cache.Draw(list, kNoIcon, {0, 0}, 12.0F, red);
  list.PushClip(Rect{0, 0, 10, 10});
  cache.Draw(list, square, {200, 200}, 12.0F, red);
  EXPECT_EQ(CountInstances(list, gfx::PrimitiveKind::kGlyph), 0);
}

TEST(IconCacheTest, RotatesInSteps) {
  IconSet icons;
  // A bar along the top edge.
  const IconId bar = icons.Add(
      "bar",
      R"(<svg width="24" height="24" viewBox="0 0 24 24"><rect x="0" y="0"
           width="24" height="4" fill="#fff"/></svg>)");
  const IconImage up = icons.Rasterise(bar, 24);
  EXPECT_EQ(up.At(12, 1), 255);
  EXPECT_EQ(up.At(22, 12), 0);
  // A quarter turn clockwise puts it on the right edge.
  const IconImage right = icons.Rasterise(bar, 24, 0, 0, 1.5707963F);
  EXPECT_EQ(right.At(12, 1), 0);
  EXPECT_GT(right.At(22, 12), 250);
  // Rotating does not change the icon for later calls.
  EXPECT_EQ(icons.Rasterise(bar, 24).pixels, up.pixels);
  Atlas atlas(256, 1);
  IconCache cache(icons, atlas);
  gfx::DrawList list;
  IconDrawOptions options;
  for (const float angle : {0.0F, 0.05F, 6.25F, 1.5707963F, -4.712389F}) {
    options.rotation = angle;
    cache.Draw(list, bar, {12, 12}, 24.0F, Color::Rgb(0xFFFFFF), options);
  }
  // 0, ~0 and ~2 pi share a bitmap; a quarter turn either way another.
  EXPECT_EQ(cache.size(), 2U);
}

/// A market table whose names carry an icon.
class IconMarketTable : public testing::MarketTable {
 public:
  IconMarketTable(int rows, IconId icon) : MarketTable(rows), icon_(icon) {}

  auto GetIcon(data::RowIndex row, int col, data::CellIcon &out) const
      -> bool override {
    if (col != 0) {
      return false;
    }
    out.icon = icon_;
    out.color = row % 2 == 0 ? Color::Rgb(0x3CDC3C) : Color{0, 0, 0, 0};
    return true;
  }

 private:
  IconId icon_;
};

TEST(IconWidgetTest, IconAndGridCellsDrawFromTheContextsIcons) {
  testing::UiHarness h({900, 400});
  const IconId square = h.ctx().icons().Add("square", kSquare);
  auto table = std::make_shared<IconMarketTable>(20, square);
  data::TableView view(table);
  ui::GridState state;
  auto frame = [&] {
    h.Frame([&](ui::Context &ctx) {
      ui::LayoutSpec grow;
      grow.width = ui::SizeSpec::Grow();
      grow.height = ui::SizeSpec::Grow();
      ui::BeginColumn(ctx, grow);
      ui::Icon(ctx, square, 24.0F, Color::Rgb(0xE0383A));
      ui::DataGrid(ctx, "##grid", view, state);
      ui::End(ctx);
    });
  };
  frame();
  frame();
  int red = 0;
  int green = 0;
  for (const gfx::DrawList *list : h.ctx().draw_lists()) {
    for (const gfx::Instance &inst : list->instances()) {
      if ((inst.kind & 0xFFU) !=
          static_cast<std::uint32_t>(gfx::PrimitiveKind::kGlyph)) {
        continue;
      }
      red += inst.color == Color::Rgb(0xE0383A).Pack() &&
                     inst.rect[2] - inst.rect[0] == 24.0F
                 ? 1
                 : 0;
      green += inst.color == Color::Rgb(0x3CDC3C).Pack() ? 1 : 0;
    }
  }
  EXPECT_EQ(red, 1);
  // Every other visible row has a green icon in its first column.
  EXPECT_GE(green, 5);
}

}  // namespace
}  // namespace gloomy::icon
