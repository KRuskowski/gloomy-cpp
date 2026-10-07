#include "gloomy/text/rich_layout.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace gloomy::text {

auto RichLayout::RangeRects(std::uint32_t begin, std::uint32_t end) const
    -> std::vector<Rect> {
  std::vector<Rect> rects;
  for (const RichLine &line : lines) {
    if (line.byte_end <= begin || line.byte_begin >= end) {
      continue;
    }
    float x0 = line.width;
    float x1 = line.width;
    bool found_start = false;
    for (std::uint32_t i = line.glyph_begin; i < line.glyph_end; ++i) {
      const LaidGlyph &g = glyphs[i];
      const float gx = g.x - line.x_offset;
      if (!found_start && g.cluster >= begin) {
        x0 = gx;
        found_start = true;
      }
      if (g.cluster >= end) {
        x1 = gx;
        break;
      }
    }
    if (!found_start) {
      continue;
    }
    rects.push_back({x0, line.top, x1, line.top + line.height});
  }
  return rects;
}

auto RichLayout::ByteAt(Vec2 p) const -> std::uint32_t {
  if (lines.empty()) {
    return 0;
  }
  const RichLine *line = &lines.back();
  for (const RichLine &l : lines) {
    if (p.y < l.top + l.height) {
      line = &l;
      break;
    }
  }
  for (std::uint32_t i = line->glyph_begin; i < line->glyph_end; ++i) {
    const LaidGlyph &g = glyphs[i];
    if (p.x < g.x - line->x_offset + g.advance) {
      return g.cluster;
    }
  }
  return line->byte_end;
}

auto LayoutRich(TextSystem &ts, std::string_view text,
                std::span<const TextRun> runs, float max_width, float line_gap)
    -> RichLayout {
  RichLayout layout;
  // Shape every run and lay the glyphs out on one long line.
  float pen = 0.0F;
  for (std::size_t r = 0; r < runs.size(); ++r) {
    const TextRun &run = runs[r];
    const ShapedText &shaped = ts.Shape(
        text.substr(run.begin, run.end - run.begin), run.font, run.size);
    for (const ShapedGlyph &g : shaped.glyphs) {
      LaidGlyph lg;
      lg.glyph = g.glyph;
      lg.face = g.face;
      lg.run = static_cast<std::uint16_t>(r);
      lg.x = pen + g.x;
      lg.y = g.y;
      lg.advance = g.advance;
      lg.cluster = run.begin + g.cluster;
      layout.glyphs.push_back(lg);
    }
    pen += shaped.width;
  }
  const auto n = static_cast<std::uint32_t>(layout.glyphs.size());

  // Greedy line breaking at spaces; '\n' forces a break.
  struct Break {
    std::uint32_t space_glyph;
    std::uint32_t next_glyph;
  };

  std::optional<Break> brk;
  std::uint32_t line_begin = 0;
  auto x_of = [&](std::uint32_t i) {
    return i < n ? layout.glyphs[i].x : pen;
  };
  auto byte_of = [&](std::uint32_t i) {
    return i < n ? layout.glyphs[i].cluster
                 : static_cast<std::uint32_t>(text.size());
  };
  auto push = [&](std::uint32_t begin,
                  std::uint32_t end,
                  std::uint32_t next_byte_begin) {
    RichLine line;
    line.glyph_begin = begin;
    line.glyph_end = end;
    line.byte_begin = byte_of(begin);
    line.byte_end = end < n ? byte_of(end) : next_byte_begin;
    line.x_offset = x_of(begin);
    line.width =
        (end > begin ? layout.glyphs[end - 1].x + layout.glyphs[end - 1].advance
                     : x_of(begin)) -
        line.x_offset;
    layout.lines.push_back(line);
  };
  for (std::uint32_t i = 0; i < n; ++i) {
    const LaidGlyph &g = layout.glyphs[i];
    const char c = text[g.cluster];
    if (c == '\n') {
      push(line_begin, i, g.cluster);
      line_begin = i + 1;
      brk.reset();
      continue;
    }
    const bool space = c == ' ';
    if (!space && g.x + g.advance - x_of(line_begin) > max_width &&
        i > line_begin) {
      if (brk && brk->space_glyph > line_begin) {
        push(line_begin, brk->space_glyph, 0);
        line_begin = brk->next_glyph;
      } else {
        push(line_begin, i, 0);
        line_begin = i;
      }
      brk.reset();
    }
    if (space) {
      if (brk && brk->next_glyph == i) {
        brk->next_glyph = i + 1;
      } else {
        brk = Break{i, i + 1};
      }
    }
  }
  // The last line; trailing spaces do not count towards its width.
  std::uint32_t end = n;
  while (end > line_begin && text[layout.glyphs[end - 1].cluster] == ' ') {
    --end;
  }
  push(line_begin, end, static_cast<std::uint32_t>(text.size()));
  layout.lines.back().byte_end = static_cast<std::uint32_t>(text.size());

  // Vertical metrics per line from the runs it uses.
  float top = 0.0F;
  for (RichLine &line : layout.lines) {
    float ascent = 0.0F;
    float descent = 0.0F;
    auto add = [&](const TextRun &run) {
      const FontMetrics m = ts.fonts().Metrics(run.font, run.size);
      ascent = std::max(ascent, m.ascent + m.line_gap * 0.5F);
      descent = std::max(descent, m.descent + m.line_gap * 0.5F);
    };
    if (line.glyph_end > line.glyph_begin) {
      std::uint16_t last_run = 0xFFFF;
      for (std::uint32_t i = line.glyph_begin; i < line.glyph_end; ++i) {
        if (layout.glyphs[i].run != last_run) {
          last_run = layout.glyphs[i].run;
          add(runs[last_run]);
        }
      }
    } else if (!runs.empty()) {
      add(runs.front());
    }
    line.top = top;
    line.baseline = top + std::round(ascent);
    line.height = std::ceil(ascent + descent);
    top += line.height + line_gap;
    layout.width = std::max(layout.width, line.width);
  }
  layout.height = layout.lines.empty() ? 0.0F : top - line_gap;
  return layout;
}

void DrawRich(TextSystem &ts, gfx::DrawList &list, const RichLayout &layout,
              std::span<const TextRun> runs, Vec2 origin) {
  for (const RichLine &line : layout.lines) {
    // Backgrounds and underlines per run segment of the line.
    std::uint32_t seg = line.glyph_begin;
    while (seg < line.glyph_end) {
      const std::uint16_t r = layout.glyphs[seg].run;
      std::uint32_t seg_end = seg;
      while (seg_end < line.glyph_end && layout.glyphs[seg_end].run == r) {
        ++seg_end;
      }
      const TextRun &run = runs[r];
      const float x0 = origin.x + layout.glyphs[seg].x - line.x_offset;
      const LaidGlyph &last = layout.glyphs[seg_end - 1];
      const float x1 = origin.x + last.x + last.advance - line.x_offset;
      if (run.background.a > 0.0F) {
        list.FillRect(
            {x0, origin.y + line.top, x1, origin.y + line.top + line.height},
            run.background,
            gfx::CornerRadii::All(2));
      }
      if (run.underline) {
        const float y = std::round(origin.y + line.baseline + 2) + 0.5F;
        list.Line({x0, y}, {x1, y}, 1.0F, run.color);
      }
      seg = seg_end;
    }
    for (std::uint32_t i = line.glyph_begin; i < line.glyph_end; ++i) {
      const LaidGlyph &g = layout.glyphs[i];
      const TextRun &run = runs[g.run];
      ts.DrawGlyph(
          list,
          g.face,
          g.glyph,
          run.size,
          {origin.x + g.x - line.x_offset, origin.y + line.baseline + g.y},
          run.color);
    }
  }
}

}  // namespace gloomy::text
