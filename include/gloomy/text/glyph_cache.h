#ifndef GLOOMY_TEXT_GLYPH_CACHE_H_
#define GLOOMY_TEXT_GLYPH_CACHE_H_

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "gloomy/core/atlas.h"
#include "gloomy/text/font_set.h"

namespace gloomy::text {

/// Horizontal subpixel positions a glyph is rasterised at.
inline constexpr int kSubpixelBins = 4;

/// A rasterised glyph in the atlas.
struct GlyphBitmap {
  /// False for glyphs without pixels (spaces); nothing to draw.
  bool visible = false;
  AtlasRegion region;
  /// Offset of the bitmap's top-left corner from the pen position, y down.
  int left = 0;
  int top = 0;
};

/// Rasterises glyphs with FreeType (light hinting, 8-bit coverage) at exact
/// pixel sizes and subpixel offsets, and packs them into the atlas.
/// See docs/design.md section 6.3.
class GlyphCache {
 public:
  GlyphCache(const FontSet &fonts, Atlas &atlas);

  /// The bitmap of `glyph` of `face` at `size` pixels, shifted right by
  /// `bin` / kSubpixelBins of a pixel. Rasterises on first use. When the
  /// atlas is full it is cleared and the cache starts over.
  auto Get(FaceId face, std::uint32_t glyph, float size, int bin)
      -> const GlyphBitmap &;

  auto atlas() const -> const Atlas & { return atlas_; }

  /// Glyphs currently cached.
  auto size() const -> std::size_t { return glyphs_.size(); }

 private:
  auto Rasterise(FaceId face, std::uint32_t glyph, int size64, int bin)
      -> GlyphBitmap;

  const FontSet &fonts_;
  Atlas &atlas_;
  std::unordered_map<std::uint64_t, GlyphBitmap> glyphs_;
  std::vector<std::uint8_t> scratch_;
};

}  // namespace gloomy::text

#endif  // GLOOMY_TEXT_GLYPH_CACHE_H_
