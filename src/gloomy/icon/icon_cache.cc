#include "gloomy/icon/icon_cache.h"

#include <algorithm>
#include <cmath>

namespace gloomy::icon {

namespace {

auto Key(IconId id, int px, int bin_x, int bin_y, int halo) -> std::uint64_t {
  return (static_cast<std::uint64_t>(id) << 32U) |
         (static_cast<std::uint64_t>(px & 0xFFFF) << 16U) |
         (static_cast<std::uint64_t>(halo & 0xFF) << 8U) |
         (static_cast<std::uint64_t>(bin_y & 0xF) << 4U) |
         static_cast<std::uint64_t>(bin_x & 0xF);
}

/// Splits a coordinate into a whole pixel and a subpixel bin.
void Snap(float v, bool subpixel, float &whole, int &bin) {
  if (!subpixel) {
    whole = std::round(v);
    bin = 0;
    return;
  }
  whole = std::floor(v);
  bin = static_cast<int>(std::lround((v - whole) * kIconSubpixelBins));
  if (bin == kIconSubpixelBins) {
    whole += 1.0F;
    bin = 0;
  }
}

}  // namespace

IconCache::IconCache(const IconSet &icons, Atlas &atlas)
    : icons_(icons),
      atlas_(atlas),
      atlas_generation_(atlas.generation()),
      icons_version_(icons.version()) {}

void IconCache::Validate() {
  if (atlas_.generation() != atlas_generation_ ||
      icons_.version() != icons_version_) {
    bitmaps_.clear();
    atlas_generation_ = atlas_.generation();
    icons_version_ = icons_.version();
  }
}

auto IconCache::Get(IconId id, int px, int bin_x, int bin_y, int halo)
    -> const Bitmap & {
  Validate();
  const std::uint64_t key = Key(id, px, bin_x, bin_y, halo);
  auto it = bitmaps_.find(key);
  if (it != bitmaps_.end()) {
    return it->second;
  }
  Bitmap bitmap;
  IconImage image =
      icons_.Rasterise(id,
                       px,
                       static_cast<float>(bin_x) / kIconSubpixelBins,
                       static_cast<float>(bin_y) / kIconSubpixelBins);
  if (halo > 0) {
    image = Dilate(image, halo);
  }
  const bool any =
      std::ranges::any_of(image.pixels, [](std::uint8_t v) { return v != 0; });
  if (any) {
    std::optional<AtlasRegion> region =
        atlas_.Allocate(image.width, image.height);
    if (!region) {
      // Full: start over, as the glyph cache does; it notices the new
      // generation too.
      atlas_.Clear();
      bitmaps_.clear();
      atlas_generation_ = atlas_.generation();
      region = atlas_.Allocate(image.width, image.height);
    }
    if (region) {
      atlas_.Write(*region, image.pixels.data(), image.width);
      bitmap.visible = true;
      bitmap.region = *region;
      bitmap.margin = halo;
    }
  }
  return bitmaps_.emplace(key, bitmap).first->second;
}

void IconCache::DrawBitmap(gfx::DrawList &list, const Bitmap &bitmap, float x,
                           float y, const Color &color) {
  if (!bitmap.visible) {
    return;
  }
  const float page = static_cast<float>(atlas_.page_size());
  const AtlasRegion &r = bitmap.region;
  const auto margin = static_cast<float>(bitmap.margin);
  list.Glyph(Rect::FromXywh(x - margin,
                            y - margin,
                            static_cast<float>(r.w),
                            static_cast<float>(r.h)),
             Rect{static_cast<float>(r.x) / page,
                  static_cast<float>(r.y) / page,
                  static_cast<float>(r.x + r.w) / page,
                  static_cast<float>(r.y + r.h) / page},
             r.page,
             color);
}

void IconCache::Draw(gfx::DrawList &list, IconId id, Vec2 center, float size,
                     const Color &color, const IconDrawOptions &options) {
  if (id == kNoIcon || id >= icons_.size()) {
    return;
  }
  const int px = std::max(1, static_cast<int>(std::lround(size)));
  const float half = 0.5F * static_cast<float>(px);
  float x = 0.0F;
  float y = 0.0F;
  int bin_x = 0;
  int bin_y = 0;
  Snap(center.x - half, options.subpixel, x, bin_x);
  Snap(center.y - half, options.subpixel, y, bin_y);
  if (list.Culled(
          Rect::FromXywh(x - static_cast<float>(options.halo_px),
                         y - static_cast<float>(options.halo_px),
                         static_cast<float>(px + 2 * options.halo_px),
                         static_cast<float>(px + 2 * options.halo_px)))) {
    return;
  }
  if (options.halo.a > 0.0F && options.halo_px > 0) {
    // Copied: the second Get() may rehash the map.
    const Bitmap halo = Get(id, px, bin_x, bin_y, options.halo_px);
    DrawBitmap(list, halo, x, y, options.halo);
  }
  const Bitmap body = Get(id, px, bin_x, bin_y, 0);
  DrawBitmap(list, body, x, y, color);
}

void IconCache::DrawInRect(gfx::DrawList &list, IconId id, const Rect &box,
                           const Color &color, const IconDrawOptions &options) {
  const float size = std::floor(std::min(box.Width(), box.Height()));
  Draw(list, id, box.Center(), size, color, options);
}

}  // namespace gloomy::icon
