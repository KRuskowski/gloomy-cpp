#ifndef GLOOMY_TEXT_RICH_LAYOUT_H_
#define GLOOMY_TEXT_RICH_LAYOUT_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/text/text_renderer.h"

namespace gloomy::text {

/// A styled byte range of a rich text.
struct TextRun {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  FontId font = 0;
  float size = 13.0F;
  Color color;
  bool underline = false;
  /// Drawn behind the run when its alpha is above zero.
  Color background{0, 0, 0, 0};
};

/// A glyph placed in a rich layout.
struct LaidGlyph {
  std::uint32_t glyph = 0;
  FaceId face = 0;
  std::uint16_t run = 0;
  /// Pen x in the unbroken text; lines subtract their x_offset.
  float x = 0.0F;
  float y = 0.0F;
  float advance = 0.0F;
  std::uint32_t cluster = 0;
};

/// One line of a rich layout.
struct RichLine {
  std::uint32_t glyph_begin = 0;
  std::uint32_t glyph_end = 0;
  std::uint32_t byte_begin = 0;
  std::uint32_t byte_end = 0;
  /// Pen x of the line's first glyph in the unbroken text.
  float x_offset = 0.0F;
  float width = 0.0F;
  /// Top of the line and its baseline, from the top of the layout.
  float top = 0.0F;
  float baseline = 0.0F;
  float height = 0.0F;
};

/// Styled text broken into lines.
struct RichLayout {
  std::vector<LaidGlyph> glyphs;
  std::vector<RichLine> lines;
  float width = 0.0F;
  float height = 0.0F;

  /// Rectangles (one per line) covering bytes [begin, end), relative to
  /// the layout's top-left corner.
  auto RangeRects(std::uint32_t begin, std::uint32_t end) const
      -> std::vector<Rect>;

  /// Byte offset of the cluster at point `p` (layout coordinates), or the
  /// nearest one.
  auto ByteAt(Vec2 p) const -> std::uint32_t;
};

/// Lays out `text` styled by `runs` (sorted, covering the text without
/// gaps) at `max_width`, breaking at spaces and at '\n'. Runs are shaped
/// separately through the shape cache.
auto LayoutRich(TextSystem &text_system, std::string_view text,
                std::span<const TextRun> runs, float max_width,
                float line_gap = 0.0F) -> RichLayout;

/// Draws a layout with its top-left corner at `origin`.
void DrawRich(TextSystem &text_system, gfx::DrawList &list,
              const RichLayout &layout, std::span<const TextRun> runs,
              Vec2 origin);

}  // namespace gloomy::text

#endif  // GLOOMY_TEXT_RICH_LAYOUT_H_
