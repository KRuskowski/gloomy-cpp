#include "gloomy/ui/layout.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace gloomy::ui {
namespace {

// Text made of `words` words of 10 px each, separated by 5 px spaces,
// 20 px per line.
class FakeMeasurer : public Measurer {
 public:
  std::vector<int> words;

  auto MeasureWidth(int m) -> WidthRange override {
    const int k = words[m];
    return {10.0F, static_cast<float>(k * 10 + (k - 1) * 5)};
  }

  auto MeasureHeight(int m, float width) -> float override {
    const int per_line = std::max(1, static_cast<int>((width + 5) / 15));
    const int lines = (words[m] + per_line - 1) / per_line;
    return static_cast<float>(lines) * 20.0F;
  }
};

auto Spec(LayoutMode mode, SizeSpec w = {}, SizeSpec h = {}) -> LayoutSpec {
  LayoutSpec s;
  s.mode = mode;
  s.width = w;
  s.height = h;
  return s;
}

TEST(LayoutTest, RowWithFixedAndGrowChildren) {
  LayoutTree tree;
  FakeMeasurer m;
  LayoutSpec root = Spec(LayoutMode::kRow, SizeSpec::Grow(), SizeSpec::Grow());
  root.padding = Padding::All(10);
  root.gap = 5;
  const int r = tree.Add(-1, root);
  const int a = tree.Add(
      r, Spec(LayoutMode::kColumn, SizeSpec::Fixed(50), SizeSpec::Fixed(30)));
  const int b = tree.Add(
      r, Spec(LayoutMode::kColumn, SizeSpec::Grow(1), SizeSpec::Fixed(30)));
  const int c = tree.Add(
      r, Spec(LayoutMode::kColumn, SizeSpec::Grow(3), SizeSpec::Fixed(30)));
  tree.Compute(r, Rect{0, 0, 300, 100}, m);
  // Inner 280, minus 50 fixed, minus 10 gaps = 220 split 1:3.
  EXPECT_EQ(tree.node(a).rect, Rect::FromXywh(10, 10, 50, 30));
  EXPECT_EQ(tree.node(b).rect, Rect::FromXywh(65, 10, 55, 30));
  EXPECT_EQ(tree.node(c).rect, Rect::FromXywh(125, 10, 165, 30));
}

TEST(LayoutTest, FitRootTakesContentSize) {
  LayoutTree tree;
  FakeMeasurer m;
  m.words = {3};
  LayoutSpec root = Spec(LayoutMode::kColumn);
  root.padding = Padding::Xy(4, 2);
  const int r = tree.Add(-1, root);
  tree.Add(r, Spec(LayoutMode::kColumn), 0);
  tree.Compute(r, Rect{100, 50, 500, 500}, m);
  EXPECT_EQ(tree.node(r).rect, Rect::FromXywh(100, 50, 48, 24));
}

TEST(LayoutTest, TextWrapsToAssignedWidthAndSetsHeight) {
  LayoutTree tree;
  FakeMeasurer m;
  m.words = {6};
  const int r = tree.Add(
      -1, Spec(LayoutMode::kColumn, SizeSpec::Fixed(35), SizeSpec::Fit()));
  const int t = tree.Add(r, Spec(LayoutMode::kColumn, SizeSpec::Grow()), 0);
  tree.Compute(r, Rect{0, 0, 1000, 1000}, m);
  // 35 px fits two words per line: three lines.
  EXPECT_EQ(tree.node(t).rect.Width(), 35.0F);
  EXPECT_EQ(tree.node(t).rect.Height(), 60.0F);
  EXPECT_EQ(tree.node(r).rect.Height(), 60.0F);
}

TEST(LayoutTest, RowShrinksChildrenToTheirMinimum) {
  LayoutTree tree;
  FakeMeasurer m;
  m.words = {4, 4};
  const int r = tree.Add(
      -1, Spec(LayoutMode::kRow, SizeSpec::Fixed(80), SizeSpec::Fit()));
  const int a = tree.Add(r, Spec(LayoutMode::kColumn), 0);
  const int b = tree.Add(r, Spec(LayoutMode::kColumn), 1);
  tree.Compute(r, Rect{0, 0, 1000, 1000}, m);
  // Prefs 55 + 55 = 110 > 80: both shrink equally to 40.
  EXPECT_FLOAT_EQ(tree.node(a).rect.Width(), 40.0F);
  EXPECT_FLOAT_EQ(tree.node(b).rect.x0, 40.0F);
  EXPECT_FLOAT_EQ(tree.node(a).rect.Height(), 40.0F);
}

TEST(LayoutTest, ColumnAlignAndJustify) {
  LayoutTree tree;
  FakeMeasurer m;
  LayoutSpec root =
      Spec(LayoutMode::kColumn, SizeSpec::Fixed(100), SizeSpec::Fixed(100));
  root.align = Align::kCenter;
  root.justify = Justify::kEnd;
  const int r = tree.Add(-1, root);
  const int a = tree.Add(
      r, Spec(LayoutMode::kColumn, SizeSpec::Fixed(20), SizeSpec::Fixed(10)));
  tree.Compute(r, Rect{0, 0, 100, 100}, m);
  EXPECT_EQ(tree.node(a).rect, Rect::FromXywh(40, 90, 20, 10));
}

TEST(LayoutTest, StretchFillsCrossAxis) {
  LayoutTree tree;
  FakeMeasurer m;
  LayoutSpec root =
      Spec(LayoutMode::kColumn, SizeSpec::Fixed(100), SizeSpec::Fixed(100));
  root.align = Align::kStretch;
  const int r = tree.Add(-1, root);
  const int a = tree.Add(
      r, Spec(LayoutMode::kColumn, SizeSpec::Fit(), SizeSpec::Fixed(10)));
  tree.Compute(r, Rect{0, 0, 100, 100}, m);
  EXPECT_EQ(tree.node(a).rect.Width(), 100.0F);
}

TEST(LayoutTest, SpaceBetween) {
  LayoutTree tree;
  FakeMeasurer m;
  LayoutSpec root =
      Spec(LayoutMode::kRow, SizeSpec::Fixed(100), SizeSpec::Fixed(10));
  root.justify = Justify::kSpaceBetween;
  const int r = tree.Add(-1, root);
  tree.Add(r, Spec(LayoutMode::kRow, SizeSpec::Fixed(10), SizeSpec::Fixed(10)));
  tree.Add(r, Spec(LayoutMode::kRow, SizeSpec::Fixed(10), SizeSpec::Fixed(10)));
  const int c = tree.Add(
      r, Spec(LayoutMode::kRow, SizeSpec::Fixed(10), SizeSpec::Fixed(10)));
  tree.Compute(r, Rect{0, 0, 100, 10}, m);
  EXPECT_FLOAT_EQ(tree.node(c).rect.x0, 90.0F);
}

TEST(LayoutTest, ScrollContainerKeepsContentSizeAndOffsets) {
  LayoutTree tree;
  FakeMeasurer m;
  LayoutSpec root =
      Spec(LayoutMode::kColumn, SizeSpec::Fixed(100), SizeSpec::Fixed(50));
  root.scroll_y = true;
  root.scroll = {0, 30};
  const int r = tree.Add(-1, root);
  int last = -1;
  for (int i = 0; i < 10; ++i) {
    last = tree.Add(
        r, Spec(LayoutMode::kColumn, SizeSpec::Grow(), SizeSpec::Fixed(20)));
  }
  tree.Compute(r, Rect{0, 0, 100, 50}, m);
  EXPECT_EQ(tree.node(r).content.y, 200.0F);
  EXPECT_EQ(tree.node(last).rect.y0, 180.0F - 30.0F);
  EXPECT_EQ(tree.node(last).rect.Height(), 20.0F);
}

TEST(LayoutTest, GridPlacesChildrenInRows) {
  LayoutTree tree;
  FakeMeasurer m;
  LayoutSpec root = Spec(LayoutMode::kGrid, SizeSpec::Fixed(210));
  root.grid_columns = 3;
  root.gap = 5;
  const int r = tree.Add(-1, root);
  std::vector<int> cells;
  for (int i = 0; i < 5; ++i) {
    cells.push_back(
        tree.Add(r,
                 Spec(LayoutMode::kColumn,
                      SizeSpec::Fit(),
                      SizeSpec::Fixed(static_cast<float>(10 + i)))));
  }
  tree.Compute(r, Rect{0, 0, 1000, 1000}, m);
  // Columns are (210 - 2 * 5) / 3 wide.
  EXPECT_FLOAT_EQ(tree.node(cells[1]).rect.x0, 200.0F / 3.0F + 5.0F);
  EXPECT_FLOAT_EQ(tree.node(cells[1]).rect.Width(), 200.0F / 3.0F);
  EXPECT_FLOAT_EQ(tree.node(cells[1]).rect.Height(), 11.0F);
  EXPECT_FLOAT_EQ(tree.node(cells[3]).rect.y0, 12.0F + 5.0F);
  EXPECT_FLOAT_EQ(tree.node(r).rect.Height(), 12.0F + 5.0F + 14.0F);
}

TEST(LayoutTest, GrowRespectsMax) {
  LayoutTree tree;
  FakeMeasurer m;
  const int r = tree.Add(
      -1, Spec(LayoutMode::kRow, SizeSpec::Fixed(100), SizeSpec::Fixed(10)));
  SizeSpec capped = SizeSpec::Grow();
  capped.max = 20;
  const int a = tree.Add(r, Spec(LayoutMode::kRow, capped));
  const int b = tree.Add(r, Spec(LayoutMode::kRow, SizeSpec::Grow()));
  tree.Compute(r, Rect{0, 0, 100, 10}, m);
  EXPECT_FLOAT_EQ(tree.node(a).rect.Width(), 20.0F);
  EXPECT_FLOAT_EQ(tree.node(b).rect.Width(), 80.0F);
}

TEST(LayoutTest, PercentOfParent) {
  LayoutTree tree;
  FakeMeasurer m;
  const int r = tree.Add(
      -1, Spec(LayoutMode::kRow, SizeSpec::Fixed(200), SizeSpec::Fixed(10)));
  const int a = tree.Add(r, Spec(LayoutMode::kRow, SizeSpec::Percent(0.25F)));
  tree.Compute(r, Rect{0, 0, 200, 10}, m);
  EXPECT_FLOAT_EQ(tree.node(a).rect.Width(), 50.0F);
}

}  // namespace
}  // namespace gloomy::ui
