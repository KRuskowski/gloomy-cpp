#ifndef GLOOMY_UI_LABEL_PLACER_H_
#define GLOOMY_UI_LABEL_PLACER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// Where a label sits around its marker.
enum class LabelSide : std::uint8_t {
  kRight,
  kLeft,
  kAbove,
  kBelow,
};

/// One label a map would like to show next to a marker.
struct LabelRequest {
  /// Screen position of the marker the label belongs to.
  Vec2 anchor;
  /// Half size of the marker: the label keeps this far from the anchor.
  float anchor_radius = 4.0F;
  /// The full text, and an abbreviation tried when the full text does not
  /// fit (empty: none). A label that fits neither way is hidden.
  std::string text;
  std::string short_text;
  /// Higher priorities are placed first and so win the space.
  int priority = 0;
  /// Shown even where it overlaps (e.g. the selection's label); still
  /// placed at the least crowded position.
  bool force = false;
  /// Tried first; the other sides follow in the order right, left, above,
  /// below.
  LabelSide side = LabelSide::kRight;
  Color color = Color::Rgb(0xFFFFFF);
  float size = 12.0F;
};

/// Where a label went.
struct PlacedLabel {
  /// Index of the request.
  std::size_t request = 0;
  /// Screen rectangle of the text.
  Rect rect;
  /// Whether the abbreviation was used.
  bool abbreviated = false;
};

/// Width of a text at a font size, in pixels.
using TextMeasure = std::function<float(std::string_view, float)>;

/// Declutters labels on a canvas: collects label requests and obstacles
/// (markers that labels must not cover), then places the labels by
/// priority without overlapping each other, trying the preferred side of
/// each marker, then the others, then its abbreviation, else hiding it.
/// Pure geometry, so maps can test their priorities without a GPU; Draw()
/// paints the result.
class LabelPlacer {
 public:
  /// Labels must lie inside `bounds` (the canvas rectangle).
  explicit LabelPlacer(Rect bounds) : bounds_(bounds) {}

  void Add(LabelRequest request) { requests_.push_back(std::move(request)); }

  /// A rectangle labels avoid, e.g. another marker.
  void AddObstacle(const Rect &rect) { obstacles_.push_back(rect); }

  /// A marker as an obstacle: a square of `radius` around `center`.
  void AddObstacle(Vec2 center, float radius) {
    AddObstacle({center.x - radius,
                 center.y - radius,
                 center.x + radius,
                 center.y + radius});
  }

  /// Places the labels; stable for equal priorities (request order).
  /// @return the labels shown, in placement order
  auto Place(const TextMeasure &measure) -> std::vector<PlacedLabel>;

  /// Places the labels with the theme's sans font and draws them.
  /// @return the labels shown
  auto Draw(PaintContext &pc) -> std::vector<PlacedLabel>;

  [[nodiscard]] auto requests() const -> const std::vector<LabelRequest> & {
    return requests_;
  }

  /// Height of a label's box at a font size.
  static auto LineHeight(float size) -> float { return size * 1.3F; }

 private:
  Rect bounds_;
  std::vector<LabelRequest> requests_;
  std::vector<Rect> obstacles_;
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_LABEL_PLACER_H_
