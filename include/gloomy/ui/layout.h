#ifndef GLOOMY_UI_LAYOUT_H_
#define GLOOMY_UI_LAYOUT_H_

#include <cstdint>
#include <limits>
#include <vector>

#include "gloomy/core/geometry.h"

namespace gloomy::ui {

/// How a node's size along one axis is determined.
enum class SizeKind : std::uint8_t {
  /// As large as the content needs, within min and max.
  kFit,
  /// `value` pixels; gives up to `value - min` when the parent is short
  /// of space (min equals value for Fixed(), so those never shrink).
  kFixed,
  /// Content size plus a share of the parent's leftover space, by weight
  /// `value`; shrinks down to the content minimum when space is short.
  kGrow,
  /// `value` (0..1) of the parent's inner size.
  kPercent,
};

/// Size of a node along one axis.
struct SizeSpec {
  SizeKind kind = SizeKind::kFit;
  float value = 0.0F;
  float min = 0.0F;
  float max = std::numeric_limits<float>::infinity();

  static constexpr auto Fit(float min = 0.0F,
                            float max = std::numeric_limits<float>::infinity())
      -> SizeSpec {
    return {SizeKind::kFit, 0.0F, min, max};
  }

  static constexpr auto Fixed(float px) -> SizeSpec {
    return {SizeKind::kFixed, px, px, px};
  }

  /// `px` pixels, shrinking towards `min` when the parent is short of
  /// space (table columns that must not push a row out of its panel).
  static constexpr auto Preferred(float px, float min) -> SizeSpec {
    return {SizeKind::kFixed, px, min < px ? min : px, px};
  }

  static constexpr auto Grow(float weight = 1.0F) -> SizeSpec {
    return {
        SizeKind::kGrow, weight, 0.0F, std::numeric_limits<float>::infinity()};
  }

  static constexpr auto Percent(float fraction) -> SizeSpec {
    return {SizeKind::kPercent,
            fraction,
            0.0F,
            std::numeric_limits<float>::infinity()};
  }
};

/// How children are arranged.
enum class LayoutMode : std::uint8_t {
  /// Left to right.
  kRow,
  /// Top to bottom.
  kColumn,
  /// On top of each other, each aligned inside the node.
  kStack,
  /// In `grid_columns` equal columns, row by row.
  kGrid,
};

/// Alignment of children on the cross axis (and both axes in a stack).
enum class Align : std::uint8_t { kStart, kCenter, kEnd, kStretch };

/// Distribution of children on the main axis.
enum class Justify : std::uint8_t { kStart, kCenter, kEnd, kSpaceBetween };

/// Space inside a node's border, in pixels.
struct Padding {
  float left = 0.0F;
  float top = 0.0F;
  float right = 0.0F;
  float bottom = 0.0F;

  static constexpr auto All(float p) -> Padding { return {p, p, p, p}; }

  static constexpr auto Xy(float x, float y) -> Padding { return {x, y, x, y}; }
};

/// The layout description of a node. See docs/design.md section 8.
struct LayoutSpec {
  LayoutMode mode = LayoutMode::kColumn;
  SizeSpec width;
  SizeSpec height;
  Padding padding;
  /// Space between children on the main axis (both axes in a grid).
  float gap = 0.0F;
  Align align = Align::kStart;
  Justify justify = Justify::kStart;
  int grid_columns = 1;
  /// Children may extend beyond the node along this axis; their offset is
  /// `scroll`. Implies clipping.
  bool scroll_x = false;
  bool scroll_y = false;
  /// Content offset for scrolling (positive moves content up/left).
  Vec2 scroll;
  /// Asks for no minimum width from its parent: it takes the width it is
  /// given and children wider than that are cut (and reported by
  /// Context::Overflows()). Vertical scroll areas set it, so a wide child
  /// never pushes the area and its scroll bar past the panel.
  bool shrink_x = false;
};

/// Content sizes reported by a Measurer for a leaf.
struct WidthRange {
  /// Narrowest possible (longest word for wrapping text).
  float min = 0.0F;
  /// Preferred (whole text on one line).
  float pref = 0.0F;
};

/// Measures leaves with intrinsic content (text). The layout engine knows
/// nothing about fonts.
class Measurer {
 public:
  virtual ~Measurer() = default;

  /// Width range of content `measure`.
  virtual auto MeasureWidth(int measure) -> WidthRange = 0;

  /// Height of content `measure` laid out at `width` (wrapping).
  virtual auto MeasureHeight(int measure, float width) -> float = 0;
};

/// A node in the layout tree, with its spec and computed results.
struct LayoutNode {
  LayoutSpec spec;
  int parent = -1;
  int first_child = -1;
  int last_child = -1;
  int next_sibling = -1;
  int child_count = 0;
  /// >= 0: a leaf whose content size the Measurer reports.
  int measure = -1;

  // Results.
  float min_w = 0.0F;
  float pref_w = 0.0F;
  float min_h = 0.0F;
  float pref_h = 0.0F;
  /// Final size.
  float w = 0.0F;
  float h = 0.0F;
  /// Final border box in pixels.
  Rect rect;
  /// Size of the children's extent, for scrolling.
  Vec2 content;
};

/// Flat layout tree; nodes are added in pre-order and laid out in passes
/// over the arrays (Clay's pass order: widths, wrap, heights, positions).
class LayoutTree {
 public:
  /// Removes all nodes, keeping capacity.
  void Clear() { nodes_.clear(); }

  /// Adds a node as last child of `parent` (-1 for a root).
  /// @return The node's index.
  auto Add(int parent, const LayoutSpec &spec, int measure = -1) -> int;

  auto node(int i) -> LayoutNode & { return nodes_[i]; }

  auto node(int i) const -> const LayoutNode & { return nodes_[i]; }

  auto size() const -> int { return static_cast<int>(nodes_.size()); }

  /// Lays out the subtree at `root` inside `available`. A root sized Fit
  /// takes its content size (clamped to `available`) at the top-left of
  /// `available`.
  void Compute(int root, const Rect &available, Measurer &measurer);

 private:
  void FitWidths(int root, Measurer &measurer);

  void AssignWidths(int node);

  void FitHeights(int root, Measurer &measurer);

  void AssignHeights(int node);

  void Position(int node);

  void DistributeMain(int node, bool horizontal, float available);

  std::vector<LayoutNode> nodes_;
  // Scratch: subtree nodes in pre-order.
  std::vector<int> order_;
  // Scratch: grow flags for DistributeMain, indexed from the parent.
  std::vector<std::uint8_t> frozen_;
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_LAYOUT_H_
