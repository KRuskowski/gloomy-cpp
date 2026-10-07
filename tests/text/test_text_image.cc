#include <gtest/gtest.h>

#include <array>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/text/font_set.h"
#include "gloomy/text/text_renderer.h"
#include "test_support/gpu_test.h"

namespace gloomy::text {
namespace {

class TextImageTest : public testing::GpuTest {
 protected:
  // No system fallbacks, so the result does not depend on installed fonts.
  TextImageTest() : ids_(fonts_.AddDefaultFonts({})), text_(fonts_, atlas_) {}

  auto Render(const gfx::DrawList &list, VkExtent2D extent) -> Image {
    gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, atlas_);
    gfx::OffscreenTarget target(ctx(), extent);
    const std::array<const gfx::DrawList *, 1> lists{&list};
    return gfx::RenderToImage(
        ctx(), renderer, target, lists, Color::Rgb(0x1E1E2E));
  }

  FontSet fonts_;
  Atlas atlas_{1024, 2};
  DefaultFonts ids_;
  TextSystem text_;
};

TEST_F(TextImageTest, TextMatchesReference) {
  gfx::DrawList list;
  const Color fg = Color::Rgb(0xCDD6F4);
  float y = 8.0F;
  for (float size : {11.0F, 13.0F, 16.0F, 24.0F}) {
    text_.DrawText(
        list, "The quick brown fox jumps", ids_.sans, size, {8, y}, fg);
    y += fonts_.Metrics(ids_.sans, size).LineHeight();
  }
  text_.DrawText(list,
                 "Bold: Ore 1,234,567 Cr",
                 ids_.bold,
                 14.0F,
                 {8, y},
                 Color::Rgb(0xF9E2AF));
  y += 20.0F;
  text_.DrawText(list,
                 "Mono: 0123456789 -42.50",
                 ids_.mono,
                 13.0F,
                 {8, y},
                 Color::Rgb(0xA6E3A1));
  y += 20.0F;
  text_.DrawText(list,
                 "Unicode: \xC3\xA4\xC3\xB6\xC3\xBC \xC3\x9F \xE2\x82\xAC "
                 "\xCE\xA9 \xD0\x96",
                 ids_.sans,
                 14.0F,
                 {8, y},
                 fg);
  y += 22.0F;

  // Wrapped paragraph in a box.
  const Rect box = Rect::FromXywh(8, y, 220, 70);
  list.StrokeRect(box, Color::Rgb(0x585B70), 1.0F);
  const std::string para =
      "Contract 42: deliver 500 units of ore to Smelter 2 within three "
      "days. Penalty on late delivery.";
  const WrappedText wrapped =
      text_.Wrap(para, ids_.sans, 13.0F, box.Width() - 8.0F);
  const ShapedText &shaped = text_.Shape(para, ids_.sans, 13.0F);
  float ly = box.y0 + 4.0F + shaped.metrics.ascent;
  for (const TextLine &line : wrapped.lines) {
    text_.DrawRange(list, shaped, line.begin, line.end, {box.x0 + 4, ly}, fg);
    ly += wrapped.line_height;
  }

  // Clipped text.
  list.PushClip(Rect::FromXywh(240, y, 60, 20));
  text_.DrawText(list, "Clipped text here", ids_.sans, 14.0F, {240, y}, fg);
  list.PopClip();

  const Image image = Render(list, {320, 280});
  testing::ExpectMatchesReference(image, "text");
}

}  // namespace
}  // namespace gloomy::text
