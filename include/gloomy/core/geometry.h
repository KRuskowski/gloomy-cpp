#ifndef GLOOMY_CORE_GEOMETRY_H_
#define GLOOMY_CORE_GEOMETRY_H_

#include <algorithm>

namespace gloomy {

/// A 2D point or vector in pixels.
struct Vec2 {
  float x = 0.0F;
  float y = 0.0F;

  friend constexpr auto operator+(Vec2 a, Vec2 b) -> Vec2 {
    return {a.x + b.x, a.y + b.y};
  }

  friend constexpr auto operator-(Vec2 a, Vec2 b) -> Vec2 {
    return {a.x - b.x, a.y - b.y};
  }

  friend constexpr auto operator*(Vec2 a, float s) -> Vec2 {
    return {a.x * s, a.y * s};
  }

  friend constexpr auto operator==(Vec2 a, Vec2 b) -> bool = default;
};

/// An axis-aligned rectangle given by its min and max corners, in pixels.
/// Y grows downwards.
struct Rect {
  float x0 = 0.0F;
  float y0 = 0.0F;
  float x1 = 0.0F;
  float y1 = 0.0F;

  /// Builds a rectangle from position and size.
  static constexpr auto FromXywh(float x, float y, float w, float h) -> Rect {
    return {x, y, x + w, y + h};
  }

  constexpr auto Width() const -> float { return x1 - x0; }

  constexpr auto Height() const -> float { return y1 - y0; }

  constexpr auto Min() const -> Vec2 { return {x0, y0}; }

  constexpr auto Max() const -> Vec2 { return {x1, y1}; }

  constexpr auto Center() const -> Vec2 {
    return {(x0 + x1) * 0.5F, (y0 + y1) * 0.5F};
  }

  /// True if the rectangle has no area.
  constexpr auto Empty() const -> bool { return x1 <= x0 || y1 <= y0; }

  /// True if `p` lies inside; the max edges are exclusive.
  constexpr auto Contains(Vec2 p) const -> bool {
    return p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1;
  }

  /// The overlap of two rectangles; may be empty.
  constexpr auto Intersect(const Rect &o) const -> Rect {
    return {std::max(x0, o.x0),
            std::max(y0, o.y0),
            std::min(x1, o.x1),
            std::min(y1, o.y1)};
  }

  /// Shrinks the rectangle by `d` on every side (grows for negative `d`).
  constexpr auto Inset(float d) const -> Rect {
    return {x0 + d, y0 + d, x1 - d, y1 - d};
  }

  /// Moves the rectangle by `d`.
  constexpr auto Offset(Vec2 d) const -> Rect {
    return {x0 + d.x, y0 + d.y, x1 + d.x, y1 + d.y};
  }

  friend constexpr auto operator==(const Rect &a, const Rect &b)
      -> bool = default;
};

}  // namespace gloomy

#endif  // GLOOMY_CORE_GEOMETRY_H_
