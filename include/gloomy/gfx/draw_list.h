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
  /// Frosted glass: the host scene's blurred backdrop under the shape,
  /// tinted (docs/design.md section 6.5); a flat fill without a backdrop.
  kGlass = 4,
  /// A regular hexagon in its circumcircle's bounds; radii[0] 1 for
  /// pointy-top (vertices up and down), 0 for flat-top.
  kHexagon = 5,
};

/// How a glass shape looks over the blurred backdrop.
struct GlassLook {
  /// Mixed over the darkened backdrop by its alpha (the glass's opacity).
  Color tint{0.0F, 0.0F, 0.0F, 0.0F};
  /// Share of the backdrop's brightness taken away, 0 to 1 (text contrast
  /// over bright scenes).
  float darken = 0.0F;
  /// Dither amplitude in 1/255 steps against banding in the blur.
  float noise = 1.0F;
  /// Strength of the light rim along the top edge, 0 for none.
  float highlight = 0.0F;
  /// Width of that rim in pixels.
  float highlight_width = 1.0F;
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

  /// A regular hexagon of circumradius `radius` (centre to vertex), flat
  /// top and bottom unless `pointy` (vertices up and down); hex grids
  /// tile with flat-top cells `1.5 * radius` apart in x.
  void FillHexagon(Vec2 center, float radius, const Color &c,
                   bool pointy = false);

  /// The outline of FillHexagon()'s hexagon, `width` pixels inside it.
  void StrokeHexagon(Vec2 center, float radius, const Color &c, float width,
                     bool pointy = false);

  /// A frosted glass (optionally rounded) rectangle: the blurred backdrop
  /// under it, darkened and tinted as `look` says, when the renderer has a
  /// backdrop (UiRenderer::SetBackdrop()); else filled with `fallback`, so
  /// that glass looks as solid as before without a host scene.
  void FillGlass(const Rect &r, const GlassLook &look, const Color &fallback,
                 const CornerRadii &radii = {});

  /// A glyph (or any coverage bitmap) from the atlas.
  /// @param[in] r Pixel rectangle covered by the bitmap.
  /// @param[in] uv Normalised atlas rectangle u0, v0, u1, v1.
  void Glyph(const Rect &r, const Rect &uv, int page, const Color &c);

  /// Intersects the clip rectangle with `r` until the matching PopClip().
  void PushClip(const Rect &r);

  void PopClip();

  /// The current clip rectangle.
  auto clip() const -> const Rect & { return clip_stack_.back(); }

  /// Multiplies the alpha of everything drawn until PopAlpha() by `alpha`
  /// (nested pushes multiply): fades a group of shapes in or out, e.g. one
  /// map's detail into another's. Each shape fades on its own, so where
  /// shapes overlap the result is not quite fading the group as one image.
  void PushAlpha(float alpha);

  void PopAlpha();

  /// The alpha multiplier in effect (1 outside PushAlpha()).
  auto alpha() const -> float { return alpha_stack_.back(); }

  auto instances() const -> const std::vector<Instance> & { return instances_; }

  /// True if nothing inside `r` can be visible under the current clip.
  auto Culled(const Rect &r) const -> bool {
    return r.Intersect(clip()).Empty();
  }

 private:
  auto Push(PrimitiveKind kind, const Rect &bounds) -> Instance &;

  std::vector<Instance> instances_;
  std::vector<Rect> clip_stack_{kUnclipped};
  std::vector<float> alpha_stack_{1.0F};

  /// `c` packed with the alpha multiplier applied.
  auto Packed(const Color &c) const -> std::uint32_t;

  static constexpr Rect kUnclipped{-1.0e9F, -1.0e9F, 1.0e9F, 1.0e9F};
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_DRAW_LIST_H_
