#include "gloomy/data/fuzzy.h"

#include <gtest/gtest.h>

namespace gloomy::data {
namespace {

TEST(FuzzyTest, SubsequenceRequired) {
  EXPECT_TRUE(FuzzyMatchText("shp", "Ship Parts").has_value());
  EXPECT_FALSE(FuzzyMatchText("psh", "Ship Parts").has_value());
  EXPECT_TRUE(FuzzyMatchText("", "anything").has_value());
}

TEST(FuzzyTest, PositionsPointAtMatchedChars) {
  const auto m = FuzzyMatchText("sp", "Ship Parts");
  ASSERT_TRUE(m.has_value());
  ASSERT_EQ(m->positions.size(), 2U);
  EXPECT_EQ(m->positions[0], 0U);
  EXPECT_EQ(m->positions[1], 5U) << "prefers the word start of Parts";
}

TEST(FuzzyTest, WordStartsAndRunsScoreHigher) {
  const int word = FuzzyMatchText("tm", "Toggle Map")->score;
  const int inner = FuzzyMatchText("tm", "Settlement")->score;
  EXPECT_GT(word, inner);
  const int run = FuzzyMatchText("map", "Open map")->score;
  const int spread = FuzzyMatchText("map", "Make a plan")->score;
  EXPECT_GT(run, spread);
  const int early = FuzzyMatchText("dock", "Dock at station")->score;
  const int late = FuzzyMatchText("dock", "Request docking")->score;
  EXPECT_GT(early, late);
}

TEST(FuzzyTest, SpacesInPatternAreIgnored) {
  EXPECT_TRUE(FuzzyMatchText("sh pa", "Ship Parts").has_value());
}

}  // namespace
}  // namespace gloomy::data
