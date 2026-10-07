#include "gloomy/text/shaper.h"

#include <hb.h>

#include <cmath>

#include "gloomy/core/utf8.h"

namespace gloomy::text {

namespace {

// Marks and joiners stay with the face of the preceding character so a
// cluster is never split across fonts.
auto ExtendsCluster(char32_t cp) -> bool {
  if (cp == 0x200D || cp == 0x200C || (cp >= 0xFE00 && cp <= 0xFE0F)) {
    return true;
  }
  const hb_unicode_general_category_t category =
      hb_unicode_general_category(hb_unicode_funcs_get_default(), cp);
  return category == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ||
         category == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK ||
         category == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK;
}

}  // namespace

auto ShapedText::CaretX(std::uint32_t byte) const -> float {
  // Glyphs are in visual order; for left-to-right text the caret before a
  // cluster sits at the pen position of its first glyph.
  for (const ShapedGlyph &g : glyphs) {
    if (g.cluster >= byte) {
      return g.x;
    }
  }
  return width;
}

auto ShapedText::HitTest(float x) const -> std::uint32_t {
  for (const ShapedGlyph &g : glyphs) {
    if (x < g.x + g.advance * 0.5F) {
      return g.cluster;
    }
  }
  return text_length;
}

Shaper::Shaper(const FontSet &fonts)
    : fonts_(fonts), buffer_(hb_buffer_create()) {}

Shaper::~Shaper() {
  hb_buffer_destroy(buffer_);
}

void Shaper::Shape(std::string_view text, FontId font, float size,
                   ShapedText &out) {
  out.glyphs.clear();
  out.font = font;
  out.size = size;
  out.metrics = fonts_.Metrics(font, size);
  out.text_length = static_cast<std::uint32_t>(text.size());
  out.width = 0.0F;
  const std::span<const FaceId> chain = fonts_.Chain(font);
  const int scale = static_cast<int>(std::lround(size * 64.0F));

  float pen = 0.0F;
  std::size_t run_start = 0;
  std::size_t pos = 0;
  FaceId run_face = chain.front();
  bool have_run = false;

  auto shape_run = [&](std::size_t begin, std::size_t end, FaceId face) {
    if (end <= begin) {
      return;
    }
    hb_font_t *hb_font = fonts_.hb_font(face);
    hb_font_set_scale(hb_font, scale, scale);
    hb_buffer_clear_contents(buffer_);
    hb_buffer_add_utf8(buffer_,
                       text.data(),
                       static_cast<int>(text.size()),
                       static_cast<unsigned>(begin),
                       static_cast<int>(end - begin));
    hb_buffer_guess_segment_properties(buffer_);
    hb_shape(hb_font, buffer_, nullptr, 0);
    unsigned count = 0;
    const hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer_, &count);
    const hb_glyph_position_t *positions =
        hb_buffer_get_glyph_positions(buffer_, &count);
    for (unsigned i = 0; i < count; ++i) {
      ShapedGlyph g;
      g.glyph = info[i].codepoint;
      g.face = face;
      g.cluster = info[i].cluster;
      g.x = pen + static_cast<float>(positions[i].x_offset) / 64.0F;
      g.y = -static_cast<float>(positions[i].y_offset) / 64.0F;
      g.advance = static_cast<float>(positions[i].x_advance) / 64.0F;
      pen += g.advance;
      out.glyphs.push_back(g);
    }
  };

  while (pos < text.size()) {
    const std::size_t start = pos;
    const char32_t cp = DecodeUtf8(text, pos);
    FaceId face = run_face;
    if (!have_run || !ExtendsCluster(cp)) {
      face = chain.front();
      for (const FaceId candidate : chain) {
        if (fonts_.GlyphIndex(candidate, cp) != 0) {
          face = candidate;
          break;
        }
      }
    }
    if (!have_run) {
      run_face = face;
      have_run = true;
    } else if (face != run_face) {
      shape_run(run_start, start, run_face);
      run_start = start;
      run_face = face;
    }
  }
  shape_run(run_start, text.size(), run_face);
  out.width = pen;
}

}  // namespace gloomy::text
