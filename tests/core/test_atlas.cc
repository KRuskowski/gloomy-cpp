#include "gloomy/core/atlas.h"

#include <gtest/gtest.h>

#include <vector>

namespace gloomy {
namespace {

auto Overlap(const AtlasRegion &a, const AtlasRegion &b) -> bool {
  return a.page == b.page && a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

TEST(AtlasTest, RegionsDoNotOverlapAndStayInside) {
  Atlas atlas(128, 1);
  std::vector<AtlasRegion> regions;
  for (int i = 0; i < 60; ++i) {
    auto r = atlas.Allocate(5 + i % 7, 6 + i % 5);
    ASSERT_TRUE(r.has_value()) << i;
    EXPECT_GE(r->x, 1);
    EXPECT_GE(r->y, 1);
    EXPECT_LE(r->x + r->w, 127);
    EXPECT_LE(r->y + r->h, 127);
    for (const AtlasRegion &o : regions) {
      EXPECT_FALSE(Overlap(*r, o));
    }
    regions.push_back(*r);
  }
}

TEST(AtlasTest, AddsPagesUpToTheLimit) {
  Atlas atlas(32, 2);
  int allocated = 0;
  while (atlas.Allocate(14, 14)) {
    ++allocated;
  }
  EXPECT_EQ(atlas.page_count(), 2);
  EXPECT_EQ(allocated, 8);
}

TEST(AtlasTest, TooLargeFails) {
  Atlas atlas(32, 4);
  EXPECT_FALSE(atlas.Allocate(31, 4).has_value());
}

TEST(AtlasTest, WriteMarksDirtyAndCopiesPixels) {
  Atlas atlas(16, 1);
  auto r = atlas.Allocate(2, 2);
  ASSERT_TRUE(r);
  const std::uint8_t pixels[] = {1, 2, 0, 3, 4};
  atlas.Write(*r, pixels, 3);
  const auto dirty = atlas.TakeDirty();
  ASSERT_EQ(dirty.size(), 1U);
  EXPECT_EQ(dirty[0].x0, r->x);
  EXPECT_EQ(dirty[0].x1, r->x + 2);
  EXPECT_TRUE(atlas.TakeDirty().empty());
  const std::uint8_t *page = atlas.PagePixels(0);
  EXPECT_EQ(page[r->y * 16 + r->x], 1);
  EXPECT_EQ(page[(r->y + 1) * 16 + r->x + 1], 4);
}

TEST(AtlasTest, ClearResetsAndBumpsGeneration) {
  Atlas atlas(16, 1);
  while (atlas.Allocate(4, 4)) {}
  atlas.Clear();
  EXPECT_EQ(atlas.generation(), 1U);
  EXPECT_TRUE(atlas.Allocate(4, 4).has_value());
  EXPECT_EQ(atlas.TakeDirty().size(), 1U);
}

}  // namespace
}  // namespace gloomy
