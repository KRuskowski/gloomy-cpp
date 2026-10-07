#ifndef GLOOMY_TEXT_TEXT_RENDERER_H_
#define GLOOMY_TEXT_TEXT_RENDERER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gloomy/core/atlas.h"
#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/text/font_set.h"
#include "gloomy/text/glyph_cache.h"
#include "gloomy/text/shaper.h"

namespace gloomy::text {

/// A line of a wrapped paragraph: a byte range of the source text.
struct TextLine {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  float width = 0.0F;
};

/// A paragraph broken into lines at a maximum width.
struct WrappedText {
  std::vector<TextLine> lines;
  float width = 0.0F;
  float line_height = 0.0F;

  auto Height() const -> float {
    return line_height * static_cast<float>(lines.size());
  }
};

/// Shapes, caches, wraps and draws text. Single-line results are cached
/// per (text, font, size) and evicted when not used for a while, so a
/// table showing the same strings every frame shapes them once.
class TextSystem {
 public:
  /// @param[in] atlas Where glyphs are rasterised to; the renderer uploads
  ///     it. Must outlive the TextSystem.
  TextSystem(FontSet &fonts, Atlas &atlas);

  /// Call once per frame: entries unused for `kEvictFrames` frames are
  /// dropped when the cache is large.
  void BeginFrame();

  /// Shaped single line, cached.
  auto Shape(std::string_view text, FontId font, float size)
      -> const ShapedText &;

  /// Width of `text` as a single line.
  auto Measure(std::string_view text, FontId font, float size) -> float {
    return Shape(text, font, size).width;
  }

  /// Breaks `text` into lines no wider than `max_width` at spaces (and
  /// hard line breaks); words longer than a line are split at clusters.
  auto Wrap(std::string_view text, FontId font, float size, float max_width)
      -> WrappedText;

  /// Draws shaped text with its baseline starting at `origin`.
  void Draw(gfx::DrawList &list, const ShapedText &shaped, Vec2 origin,
            const Color &color);

  /// Draws a byte range of shaped text (glyphs whose cluster lies in
  /// [begin, end)), shifted so the range starts at `origin`.
  void DrawRange(gfx::DrawList &list, const ShapedText &shaped,
                 std::uint32_t begin, std::uint32_t end, Vec2 origin,
                 const Color &color);

  /// Shapes and draws `text` with its top-left corner at `top_left`.
  void DrawText(gfx::DrawList &list, std::string_view text, FontId font,
                float size, Vec2 top_left, const Color &color);

  auto fonts() -> FontSet & { return fonts_; }

  auto glyphs() -> GlyphCache & { return glyphs_; }

  /// Cached shaped strings.
  auto cache_size() const -> std::size_t { return cache_.size(); }

  /// Frames an entry may go unused before eviction.
  static constexpr std::uint64_t kEvictFrames = 120;
  /// Entries kept before eviction kicks in.
  static constexpr std::size_t kCacheSoftLimit = 16384;

 private:
  struct Entry {
    std::string text;
    ShapedText shaped;
    std::uint64_t last_used = 0;
  };

  FontSet &fonts_;
  GlyphCache glyphs_;
  Shaper shaper_;
  std::unordered_map<std::uint64_t, std::unique_ptr<Entry>> cache_;
  std::uint64_t frame_ = 0;
};

}  // namespace gloomy::text

#endif  // GLOOMY_TEXT_TEXT_RENDERER_H_
