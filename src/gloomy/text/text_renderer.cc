#include "gloomy/text/text_renderer.h"

#include <cmath>
#include <optional>

#include "gloomy/core/id.h"

namespace gloomy::text {

TextSystem::TextSystem(FontSet &fonts, Atlas &atlas)
    : fonts_(fonts), glyphs_(fonts, atlas), shaper_(fonts) {}

void TextSystem::BeginFrame() {
  ++frame_;
  if (cache_.size() < kCacheSoftLimit) {
    return;
  }
  std::erase_if(cache_, [&](const auto &item) {
    return item.second->last_used + kEvictFrames < frame_;
  });
}

auto TextSystem::Shape(std::string_view text, FontId font, float size)
    -> const ShapedText & {
  std::uint64_t key = HashString(text, font);
  key = HashCombine(key, static_cast<std::uint64_t>(std::lround(size * 64)));
  std::unique_ptr<Entry> &entry = cache_[key];
  if (!entry) {
    entry = std::make_unique<Entry>();
  } else if (entry->text == text && entry->shaped.font == font &&
             entry->shaped.size == size) {
    entry->last_used = frame_;
    return entry->shaped;
  }
  entry->text.assign(text);
  shaper_.Shape(text, font, size, entry->shaped);
  entry->last_used = frame_;
  return entry->shaped;
}

auto TextSystem::Wrap(std::string_view text, FontId font, float size,
                      float max_width) -> WrappedText {
  WrappedText result;
  result.line_height = fonts_.Metrics(font, size).LineHeight();
  std::uint32_t para_begin = 0;
  while (true) {
    const std::size_t nl = text.find('\n', para_begin);
    const std::uint32_t para_end = nl == std::string_view::npos
                                       ? static_cast<std::uint32_t>(text.size())
                                       : static_cast<std::uint32_t>(nl);
    const std::string_view para =
        text.substr(para_begin, para_end - para_begin);
    const ShapedText &shaped = Shape(para, font, size);

    // A run of spaces: the line may end before it and continue after it.
    struct Break {
      std::uint32_t space_byte;
      std::uint32_t next_byte;
      float x_trim;
      float x_next;
    };

    std::optional<Break> brk;
    std::uint32_t line_begin = 0;
    float line_x = 0.0F;
    auto push = [&](std::uint32_t begin, std::uint32_t end, float width) {
      result.lines.push_back({para_begin + begin, para_begin + end, width});
      result.width = std::fmax(result.width, width);
    };
    for (const ShapedGlyph &g : shaped.glyphs) {
      const bool space = para[g.cluster] == ' ';
      if (!space && g.x + g.advance - line_x > max_width &&
          g.cluster > line_begin) {
        if (brk && brk->space_byte > line_begin) {
          push(line_begin, brk->space_byte, brk->x_trim - line_x);
          line_begin = brk->next_byte;
          line_x = brk->x_next;
        } else {
          push(line_begin, g.cluster, g.x - line_x);
          line_begin = g.cluster;
          line_x = g.x;
        }
        brk.reset();
      }
      if (space) {
        if (brk && brk->next_byte == g.cluster) {
          brk->next_byte = g.cluster + 1;
          brk->x_next = g.x + g.advance;
        } else {
          brk = Break{g.cluster, g.cluster + 1, g.x, g.x + g.advance};
        }
      }
    }
    float end_x = shaped.width;
    std::uint32_t end_byte = static_cast<std::uint32_t>(para.size());
    if (brk && brk->next_byte == end_byte && brk->space_byte >= line_begin) {
      end_x = brk->x_trim;
      end_byte = brk->space_byte;
    }
    push(line_begin, end_byte, end_x - line_x);
    if (nl == std::string_view::npos) {
      break;
    }
    para_begin = para_end + 1;
  }
  return result;
}

void TextSystem::DrawRange(gfx::DrawList &list, const ShapedText &shaped,
                           std::uint32_t begin, std::uint32_t end, Vec2 origin,
                           const Color &color) {
  const float page_size = static_cast<float>(glyphs_.atlas().page_size());
  const float baseline = std::round(origin.y);
  float shift = 0.0F;
  bool first = true;
  for (const ShapedGlyph &g : shaped.glyphs) {
    if (g.cluster < begin || g.cluster >= end) {
      continue;
    }
    if (first) {
      shift = g.x;
      first = false;
    }
    const float pen_x = origin.x + g.x - shift;
    float ix = std::floor(pen_x);
    int bin = static_cast<int>((pen_x - ix) * kSubpixelBins + 0.5F);
    if (bin == kSubpixelBins) {
      ix += 1.0F;
      bin = 0;
    }
    const GlyphBitmap &bitmap = glyphs_.Get(g.face, g.glyph, shaped.size, bin);
    if (!bitmap.visible) {
      continue;
    }
    const AtlasRegion &r = bitmap.region;
    const float x0 = ix + static_cast<float>(bitmap.left);
    const float y0 =
        baseline + std::round(g.y) - static_cast<float>(bitmap.top);
    list.Glyph(Rect::FromXywh(
                   x0, y0, static_cast<float>(r.w), static_cast<float>(r.h)),
               Rect{static_cast<float>(r.x) / page_size,
                    static_cast<float>(r.y) / page_size,
                    static_cast<float>(r.x + r.w) / page_size,
                    static_cast<float>(r.y + r.h) / page_size},
               r.page,
               color);
  }
}

void TextSystem::Draw(gfx::DrawList &list, const ShapedText &shaped,
                      Vec2 origin, const Color &color) {
  // Keep the first glyph's offset (DrawRange subtracts it).
  const float first_x = shaped.glyphs.empty() ? 0.0F : shaped.glyphs[0].x;
  DrawRange(list,
            shaped,
            0,
            shaped.text_length,
            {origin.x + first_x, origin.y},
            color);
}

void TextSystem::DrawText(gfx::DrawList &list, std::string_view text,
                          FontId font, float size, Vec2 top_left,
                          const Color &color) {
  const ShapedText &shaped = Shape(text, font, size);
  Draw(list,
       shaped,
       {top_left.x, top_left.y + std::round(shaped.metrics.ascent)},
       color);
}

}  // namespace gloomy::text
