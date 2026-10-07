#ifndef GLOOMY_GFX_DRAW_LIST_H_
#define GLOOMY_GFX_DRAW_LIST_H_

#include <cstdint>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"

namespace gloomy::gfx {

/// What an Instance draws. The low byte of Instance::kind; the atlas page
/// sits in the next byte.
enum class PrimitiveKind : std::uint32_t {
  kRect = 0,
  kCircle = 1,
  kLine = 2,
  kGlyph = 3,
};

/// One instanced quad: the only thing the UI shader draws. 96 bytes, laid
/// out to match the vertex attributes in src/gloomy/gfx/shaders/ui.vert.
/// See docs/design.md section 6.2.
struct Instance {
  /// Bounds x0, y0, x1, y1 in pixels; for lines the two end points.
  float rect[4];
  /// Atlas coordinates u0, v0, u1, v1 (normalised) for glyphs.
  float uv[4];
  /// Corner radii top-left, top-right, bottom-right, bottom-left; for
  /// lines radii[0] is the width.
  float radii[4];
  /// Clip rectangle x0, y0, x1, y1 in pixels.
  float clip[4];
  /// Packed RGBA8 fill colour, or gradient top.
  std::uint32_t color;
  /// Packed RGBA8 gradient bottom; equal to `color` for flat fills.
  std::uint32_t color2;
  /// Border width; 0 fills the shape.
  float stroke;
  /// Edge softness in pixels; 0 for a crisp antialiased edge.
  float soft;
  /// PrimitiveKind | atlas page << 8.
  std::uint32_t kind;
  float pad[3];
};

static_assert(sizeof(Instance) == 96);

/// Per-corner radii in pixels.
struct CornerRadii {
  float top_left = 0.0F;
  float top_right = 0.0F;
  float bottom_right = 0.0F;
  float bottom_left = 0.0F;

  /// All four corners equal.
  static constexpr auto All(float r) -> CornerRadii { return {r, r, r, r}; }
};

/// A list of primitives in painter's order, with a clip stack. Pure CPU
/// data; UiRenderer uploads it. Coordinates are pixels, y down.
class DrawList {
 public:
  /// Removes all primitives and resets the clip stack.
  void Clear();

  /// A filled (optionally rounded) rectangle.
  void FillRect(const Rect &r, const Color &c, const CornerRadii &radii = {});

  /// A filled rectangle with a vertical gradient from `top` to `bottom`.
  void FillRectGradient(const Rect &r, const Color &top, const Color &bottom,
                        const CornerRadii &radii = {});

  /// A border of `width` pixels drawn inside `r`.
  void StrokeRect(const Rect &r, const Color &c, float width,
                  const CornerRadii &radii = {});

  /// A soft shadow of the box `r` moved by `offset` with blur radius `blur`.
  void Shadow(const Rect &r, const Color &c, float blur, Vec2 offset,
              const CornerRadii &radii = {});

  /// A filled circle.
  void FillCircle(Vec2 center, float radius, const Color &c);

  /// A circle outline `width` pixels wide, inside `radius`.
  void StrokeCircle(Vec2 center, float radius, const Color &c, float width);

  /// A straight line with flat caps, centred on the segment. For crisp
  /// axis-aligned 1 px lines put the end points on pixel centres (x.5), or
  /// use FillRect.
  void Line(Vec2 a, Vec2 b, float width, const Color &c);

  /// A glyph (or any coverage bitmap) from the atlas.
  /// @param[in] r Pixel rectangle covered by the bitmap.
  /// @param[in] uv Normalised atlas rectangle u0, v0, u1, v1.
  void Glyph(const Rect &r, const Rect &uv, int page, const Color &c);

  /// Intersects the clip rectangle with `r` until the matching PopClip().
  void PushClip(const Rect &r);

  void PopClip();

  /// The current clip rectangle.
  auto clip() const -> const Rect & { return clip_stack_.back(); }

  auto instances() const -> const std::vector<Instance> & { return instances_; }

  /// True if nothing inside `r` can be visible under the current clip.
  auto Culled(const Rect &r) const -> bool {
    return r.Intersect(clip()).Empty();
  }

 private:
  auto Push(PrimitiveKind kind, const Rect &bounds) -> Instance &;

  std::vector<Instance> instances_;
  std::vector<Rect> clip_stack_{kUnclipped};

  static constexpr Rect kUnclipped{-1.0e9F, -1.0e9F, 1.0e9F, 1.0e9F};
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_DRAW_LIST_H_
