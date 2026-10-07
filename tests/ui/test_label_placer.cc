#include "gloomy/ui/label_placer.h"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace gloomy::ui {
namespace {

/// Every character 6 pixels wide.
auto Measure(std::string_view text, float /*size*/) -> float {
  return 6.0F * static_cast<float>(text.size());
}

auto Label(Vec2 anchor, std::string text, int priority,
           std::string short_text = "") -> LabelRequest {
  LabelRequest r;
  r.anchor = anchor;
  r.text = std::move(text);
  r.short_text = std::move(short_text);
  r.priority = priority;
  return r;
}

auto Overlap(const Rect &a, const Rect &b) -> bool {
  return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

TEST(LabelPlacerTest, PlacesALoneLabelRightOfItsMarker) {
  LabelPlacer placer({0, 0, 400, 300});
  placer.Add(Label({100, 100}, "Solar Plant", 0));
  const std::vector<PlacedLabel> placed = placer.Place(Measure);
  ASSERT_EQ(placed.size(), 1U);
  EXPECT_FALSE(placed[0].abbreviated);
  EXPECT_GT(placed[0].rect.x0, 100.0F);
  EXPECT_FLOAT_EQ(placed[0].rect.Width(), 66.0F);
  EXPECT_LT(placed[0].rect.y0, 100.0F);
  EXPECT_GT(placed[0].rect.y1, 100.0F);
}

TEST(LabelPlacerTest, TriesThePreferredSideFirst) {
  LabelPlacer placer({0, 0, 400, 300});
  LabelRequest r = Label({100, 100}, "Toria Deep", 0);
  r.side = LabelSide::kBelow;
  placer.Add(r);
  const std::vector<PlacedLabel> placed = placer.Place(Measure);
  ASSERT_EQ(placed.size(), 1U);
  EXPECT_GT(placed[0].rect.y0, 100.0F);
  EXPECT_FLOAT_EQ(placed[0].rect.Center().x, 100.0F);
}

TEST(LabelPlacerTest, CrowdedLabelsNeverOverlapAndHigherPriorityWins) {
  LabelPlacer placer({0, 0, 400, 300});
  // Ten markers on top of each other: only a few labels fit around them.
  for (int i = 0; i < 10; ++i) {
    placer.Add(Label({200, 150}, "Station " + std::to_string(i), i));
  }
  const std::vector<PlacedLabel> placed = placer.Place(Measure);
  ASSERT_EQ(placed.size(), 4U);
  // The four highest priorities, highest first.
  EXPECT_EQ(placed[0].request, 9U);
  EXPECT_EQ(placed[3].request, 6U);
  for (std::size_t a = 0; a < placed.size(); ++a) {
    for (std::size_t b = a + 1; b < placed.size(); ++b) {
      EXPECT_FALSE(Overlap(placed[a].rect, placed[b].rect)) << a << " " << b;
    }
  }
}

TEST(LabelPlacerTest, AbbreviatesWhenTheFullTextDoesNotFit) {
  // Room for 10 characters right of the marker, none elsewhere.
  LabelPlacer placer({100, 90, 177, 110});
  placer.Add(Label({100, 100}, "Ice Mine (3 docked) (live)", 0, "Ice Mine"));
  const std::vector<PlacedLabel> placed = placer.Place(Measure);
  ASSERT_EQ(placed.size(), 1U);
  EXPECT_TRUE(placed[0].abbreviated);
}

TEST(LabelPlacerTest, HidesLabelsThatFitNowhere) {
  LabelPlacer placer({100, 90, 110, 110});
  placer.Add(Label({100, 100}, "Ice Mine", 0, "IM"));
  placer.Add(Label({100, 100}, "Farm", 0));
  EXPECT_TRUE(placer.Place(Measure).empty());
}

TEST(LabelPlacerTest, AvoidsObstacles) {
  LabelPlacer placer({0, 0, 400, 300});
  // A marker right of the anchor: the label moves to the left.
  placer.AddObstacle({120, 100}, 6.0F);
  placer.Add(Label({100, 100}, "Farm", 0));
  const std::vector<PlacedLabel> placed = placer.Place(Measure);
  ASSERT_EQ(placed.size(), 1U);
  EXPECT_LT(placed[0].rect.x1, 100.0F);
}

TEST(LabelPlacerTest, ForcedLabelsShowEvenWhenCrowded) {
  LabelPlacer placer({0, 0, 400, 300});
  for (int i = 0; i < 6; ++i) {
    placer.Add(Label({200, 150}, "Station " + std::to_string(i), 100));
  }
  LabelRequest selected = Label({200, 150}, "Selected", 0);
  selected.force = true;
  placer.Add(selected);
  const std::vector<PlacedLabel> placed = placer.Place(Measure);
  ASSERT_FALSE(placed.empty());
  // Placed first, so it takes the best spot.
  EXPECT_EQ(placed[0].request, 6U);
}

}  // namespace
}  // namespace gloomy::ui
