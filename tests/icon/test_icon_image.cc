#include <gtest/gtest.h>

#include <array>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/icon/icon_cache.h"
#include "gloomy/icon/icon_set.h"
#include "test_support/gpu_test.h"

namespace gloomy::icon {
namespace {

// Three shapes in the style of a map icon set: a filled square station, a
// stroked ring with ticks (a gate) and a ship silhouette with a cut-out.
constexpr const char *kStation =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"
         width="24" height="24"><rect x="3" y="3" width="18" height="18"
         rx="2" fill="none" stroke="#fff" stroke-width="2"/>
         <rect x="8" y="8" width="8" height="8" fill="#fff"/></svg>)";

constexpr const char *kGate =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"
         width="24" height="24"><circle cx="12" cy="12" r="8" fill="none"
         stroke="#fff" stroke-width="2"/><path d="M12 1v5M12 18v5M1 12h5
         M18 12h5" stroke="#fff" stroke-width="2"/></svg>)";

constexpr const char *kShip =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"
         width="24" height="24"><path fill="#fff" fill-rule="evenodd"
         d="M12 2L20 21L12 17L4 21Z M12 8L9 15L12 14L15 15Z"/></svg>)";

class IconImageTest : public testing::GpuTest {
 protected:
  auto Render(const gfx::DrawList &list, VkExtent2D extent) -> Image {
    gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, atlas_);
    gfx::OffscreenTarget target(ctx(), extent);
    const std::array<const gfx::DrawList *, 1> lists{&list};
    return gfx::RenderToImage(
        ctx(), renderer, target, lists, Color::Rgb(0x091219));
  }

  Atlas atlas_{512, 1};
  IconSet icons_;
};

TEST_F(IconImageTest, IconsMatchReference) {
  const std::array<IconId, 3> ids = {icons_.Add("station", kStation),
                                     icons_.Add("gate", kGate),
                                     icons_.Add("ship", kShip)};
  IconCache cache(icons_, atlas_);
  gfx::DrawList list;
  const std::array<Color, 3> colors = {
      Color::Rgb(0x3CDC3C), Color::Rgb(0xD0D8E0), Color::Rgb(0xE0383A)};
  // Rows: sizes; columns: icons in three relation colours.
  float y = 8.0F;
  for (const float size : {12.0F, 16.0F, 24.0F, 48.0F}) {
    float x = 8.0F;
    for (const IconId id : ids) {
      for (const Color &c : colors) {
        cache.Draw(list, id, {x + size / 2, y + size / 2}, size, c);
        x += size + 8.0F;
      }
    }
    y += size + 8.0F;
  }
  // Over a crossing line, with and without a halo, and on quarter pixels.
  list.Line({8, y + 12}, {500, y + 12}, 2.0F, Color::Rgb(0x6EDCFF));
  IconDrawOptions halo;
  halo.halo = Color::Rgb(0x000000);
  float x = 20.0F;
  for (const IconId id : ids) {
    cache.Draw(list, id, {x, y + 12}, 16.0F, colors[1]);
    cache.Draw(list, id, {x + 30, y + 12}, 16.0F, colors[1], halo);
    x += 70.0F;
  }
  IconDrawOptions subpixel;
  subpixel.subpixel = true;
  for (int k = 0; k < 4; ++k) {
    cache.Draw(list,
               ids[2],
               {230.0F + 20.25F * static_cast<float>(k), y + 34},
               12.0F,
               colors[0],
               subpixel);
  }
  const Image image = Render(list, {520, 200});
  testing::ExpectMatchesReference(image, "icons");
}

}  // namespace
}  // namespace gloomy::icon
