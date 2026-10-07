#ifndef GLOOMY_TEXT_SHAPER_H_
#define GLOOMY_TEXT_SHAPER_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "gloomy/text/font_set.h"

struct hb_buffer_t;

namespace gloomy::text {

/// One positioned glyph of shaped text.
struct ShapedGlyph {
  std::uint32_t glyph = 0;
  FaceId face = 0;
  /// Pen position of the glyph origin, relative to the start of the line.
  float x = 0.0F;
  float y = 0.0F;
  float advance = 0.0F;
  /// Byte offset in the source string of the cluster this glyph belongs to.
  std::uint32_t cluster = 0;
};

/// A string shaped as one line.
struct ShapedText {
  std::vector<ShapedGlyph> glyphs;
  FontId font = 0;
  float size = 0.0F;
  float width = 0.0F;
  FontMetrics metrics;

  /// Pen x of the caret before byte offset `byte` (cluster granularity).
  auto CaretX(std::uint32_t byte) const -> float;

  /// Byte offset of the cluster boundary closest to pen x `x`.
  auto HitTest(float x) const -> std::uint32_t;

  /// Length in bytes of the source string.
  std::uint32_t text_length = 0;
};

/// Shapes UTF-8 text with HarfBuzz, splitting it into runs by the first
/// face in the font's chain that has each code point (font fallback).
class Shaper {
 public:
  explicit Shaper(const FontSet &fonts);
  Shaper(const Shaper &) = delete;
  auto operator=(const Shaper &) -> Shaper & = delete;
  ~Shaper();

  /// Shapes `text` as a single line into `out`.
  void Shape(std::string_view text, FontId font, float size, ShapedText &out);

 private:
  const FontSet &fonts_;
  hb_buffer_t *buffer_;
};

}  // namespace gloomy::text

#endif  // GLOOMY_TEXT_SHAPER_H_
