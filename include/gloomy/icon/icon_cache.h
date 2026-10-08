#ifndef GLOOMY_ICON_ICON_CACHE_H_
#define GLOOMY_ICON_ICON_CACHE_H_

#include <cstdint>
#include <unordered_map>

#include "gloomy/core/atlas.h"
#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/icon/icon_set.h"

namespace gloomy::icon {

/// Directions a rotated icon is rasterised at (a full turn).
inline constexpr int kIconRotationSteps = 32;

/// How an icon is drawn.
struct IconDrawOptions {
  /// Outline drawn under the icon, for contrast over lines and other
  /// icons; alpha 0 draws none.
  Color halo{0, 0, 0, 0};
  /// Width of the halo in pixels.
  int halo_px = 1;
  /// Places the icon at its exact position (moving objects; sampled
  /// bilinearly, slightly soft) instead of on whole pixels (crisp, but a
  /// moving icon would step a pixel at a time).
  bool subpixel = false;
  /// Clockwise rotation in radians (y points down), e.g. a ship's heading
  /// for an icon authored pointing up; quantised to kIconRotationSteps.
  float rotation = 0.0F;
};

/// Rasterises icons of an IconSet at exact pixel sizes (and rotations)
/// into the shared 8-bit atlas, next to the text glyphs, and draws
/// them as tinted coverage quads (gfx::DrawList::Glyph). Icons rasterised
/// once stay until the atlas is cleared (by this cache or the glyph cache
/// when full), which both notice through Atlas::generation().
class IconCache {
 public:
  /// A rasterised icon in the atlas.
  struct Bitmap {
    /// False for icons without pixels; nothing to draw.
    bool visible = false;
    AtlasRegion region;
    /// Pixels the bitmap reaches beyond the icon's square on each side
    /// (the halo).
    int margin = 0;
  };

  /// @param[in] icons Must outlive the cache.
  /// @param[in] atlas Usually the TextSystem's, so the renderer uploads
  ///     icons and glyphs together. Must outlive the cache.
  IconCache(const IconSet &icons, Atlas &atlas);

  /// The bitmap of icon `id` at `px` pixels, turned clockwise by
  /// `rotation_step` / kIconRotationSteps of a full turn, grown by `halo`
  /// pixels. Rasterises on first use.
  auto Get(IconId id, int px, int halo = 0, int rotation_step = 0)
      -> const Bitmap &;

  /// Draws icon `id` as a `size` x `size` square centred on `center`,
  /// tinted with `color` (the size is rounded to whole pixels).
  void Draw(gfx::DrawList &list, IconId id, Vec2 center, float size,
            const Color &color, const IconDrawOptions &options = {});

  /// Draws icon `id` as large as fits in `box`, centred.
  void DrawInRect(gfx::DrawList &list, IconId id, const Rect &box,
                  const Color &color, const IconDrawOptions &options = {});

  auto icons() const -> const IconSet & { return icons_; }

  /// Bitmaps currently cached.
  auto size() const -> std::size_t { return bitmaps_.size(); }

 private:
  /// Drops everything when the atlas was cleared or an icon replaced.
  void Validate();

  void DrawBitmap(gfx::DrawList &list, const Bitmap &bitmap, float x, float y,
                  const Color &color);

  const IconSet &icons_;
  Atlas &atlas_;
  std::unordered_map<std::uint64_t, Bitmap> bitmaps_;
  std::uint64_t atlas_generation_ = 0;
  std::uint64_t icons_version_ = 0;
};

}  // namespace gloomy::icon

#endif  // GLOOMY_ICON_ICON_CACHE_H_
