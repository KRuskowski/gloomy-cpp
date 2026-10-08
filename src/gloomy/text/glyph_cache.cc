#include "gloomy/text/glyph_cache.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <cmath>
#include <cstring>

namespace gloomy::text {

namespace {

auto Key(FaceId face, std::uint32_t glyph, int size64, int bin)
    -> std::uint64_t {
  return (static_cast<std::uint64_t>(face) << 48U) |
         (static_cast<std::uint64_t>(size64 & 0xFFFF) << 32U) |
         (static_cast<std::uint64_t>(bin & 0x3) << 30U) | (glyph & 0x3FFFFFFFU);
}

}  // namespace

GlyphCache::GlyphCache(const FontSet &fonts, Atlas &atlas)
    : fonts_(fonts), atlas_(atlas), generation_(atlas.generation()) {}

auto GlyphCache::Get(FaceId face, std::uint32_t glyph, float size, int bin)
    -> const GlyphBitmap & {
  const int size64 = static_cast<int>(std::lround(size * 64.0F));
  const std::uint64_t key = Key(face, glyph, size64, bin);
  if (atlas_.generation() != generation_) {
    glyphs_.clear();
    generation_ = atlas_.generation();
  }
  auto it = glyphs_.find(key);
  if (it != glyphs_.end()) {
    return it->second;
  }
  return glyphs_.emplace(key, Rasterise(face, glyph, size64, bin))
      .first->second;
}

auto GlyphCache::Rasterise(FaceId face, std::uint32_t glyph, int size64,
                           int bin) -> GlyphBitmap {
  FT_Face ft = fonts_.ft_face(face);
  GlyphBitmap result;
  if (FT_Set_Char_Size(ft, 0, size64, 72, 72) != 0 ||
      FT_Load_Glyph(ft, glyph, FT_LOAD_TARGET_LIGHT) != 0) {
    return result;
  }
  FT_GlyphSlot slot = ft->glyph;
  if (slot->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Outline_Translate(&slot->outline, bin * 64 / kSubpixelBins, 0);
  }
  if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
    return result;
  }
  const FT_Bitmap &bitmap = slot->bitmap;
  if (bitmap.width == 0 || bitmap.rows == 0 ||
      bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
    return result;
  }
  const int w = static_cast<int>(bitmap.width);
  const int h = static_cast<int>(bitmap.rows);
  std::optional<AtlasRegion> region = atlas_.Allocate(w, h);
  if (!region) {
    // Full: start over. Glyphs already handed out this frame point at
    // stale texels for one frame.
    atlas_.Clear();
    glyphs_.clear();
    generation_ = atlas_.generation();
    region = atlas_.Allocate(w, h);
    if (!region) {
      return result;
    }
  }
  atlas_.Write(*region, bitmap.buffer, bitmap.pitch);
  result.visible = true;
  result.region = *region;
  result.left = slot->bitmap_left;
  result.top = slot->bitmap_top;
  return result;
}

}  // namespace gloomy::text
