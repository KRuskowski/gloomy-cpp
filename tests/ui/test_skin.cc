#include "gloomy/ui/skin.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

// A HUD skin file as the game ships them: instrument lines among the
// colours, which a panel skin skips.
constexpr std::string_view kHudSkin = R"(# A HUD skin. "# " starts a comment.
name = military
for = escort
primary = #50E080
secondary = #90A890   # a comment after a value
alert = #FF2020
own = #40FF40
line = 2
glow = 3
scanlines = 0.06
reticle = minimal center
radar = standard bottom_left 14 14
)";

TEST(SkinTest, ReadsTheKeysItSharesWithHudSkins) {
  std::vector<std::string> errors;
  const PanelSkin s = ParsePanelSkin(kHudSkin, PanelSkin{}, &errors);
  EXPECT_TRUE(errors.empty()) << errors.front();
  EXPECT_EQ(s.name, "military");
  EXPECT_EQ(s.ink, Color::Rgb(0x50E080));
  EXPECT_EQ(s.ink_muted, Color::Rgb(0x90A890));
  EXPECT_EQ(s.bad, Color::Rgb(0xFF2020));
  EXPECT_EQ(s.good, Color::Rgb(0x40FF40));
  EXPECT_FLOAT_EQ(s.frame, 4.0F) << "frame is twice the HUD's line";
  EXPECT_FLOAT_EQ(s.glow, 3.0F);
  // Scan lines are gone; old skins that still set them parse cleanly.
  // Untouched keys keep the base.
  EXPECT_EQ(s.paper, PanelSkin{}.paper);
}

TEST(SkinTest, PanelKeysAndBadValues) {
  std::vector<std::string> errors;
  const PanelSkin s = ParsePanelSkin(
      "panel_paper = #102030\npanel_frame = 5\npanel_bolt = 0\n"
      "panel_text = #FFFFFF80\nprimary = cyan\npanel_corner = -2\n",
      PanelSkin{},
      &errors);
  EXPECT_EQ(s.paper, Color::Rgb(0x102030));
  EXPECT_FLOAT_EQ(s.frame, 5.0F);
  EXPECT_FLOAT_EQ(s.bolt, 0.0F);
  EXPECT_NEAR(s.text.a, 128.0F / 255.0F, 1e-3F);
  ASSERT_EQ(errors.size(), 2U);
  EXPECT_NE(errors[0].find("primary"), std::string::npos);
  EXPECT_NE(errors[1].find("panel_corner"), std::string::npos);
  EXPECT_EQ(s.ink, PanelSkin{}.ink) << "a bad value keeps the base";
}

TEST(SkinTest, HexColors) {
  Color c;
  EXPECT_TRUE(ParseHexColor("#6EDCFF", c));
  EXPECT_EQ(c, Color::Rgb(0x6EDCFF));
  EXPECT_TRUE(ParseHexColor("#6EDCFF40", c));
  EXPECT_NEAR(c.a, 0x40 / 255.0F, 1e-4F);
  EXPECT_FALSE(ParseHexColor("6EDCFF", c));
  EXPECT_FALSE(ParseHexColor("#6EDCF", c));
  EXPECT_FALSE(ParseHexColor("#6EDCFG", c));
}

TEST(SkinTest, FromThemeAndOnPaper) {
  testing::UiHarness h;
  const Theme &t = h.ctx().theme();
  const PanelSkin s = PanelSkin::FromTheme(t);
  EXPECT_EQ(s.ink, t.colors.accent);
  EXPECT_EQ(s.bad, t.colors.error);
  EXPECT_EQ(s.ToneInk(Tone::kGood), t.colors.success);
  EXPECT_EQ(s.ToneInk(Tone::kNeutral), s.text);
  // On paper: light paper, dark text, so text reads on it.
  const PanelSkin p = s.OnPaper();
  auto luma = [](const Color &c) {
    return 0.3F * c.r + 0.6F * c.g + 0.1F * c.b;
  };
  EXPECT_GT(luma(p.paper), 0.8F);
  EXPECT_LT(luma(p.text), 0.2F);
  EXPECT_LT(luma(p.ink), luma(s.ink));
}

TEST(SkinTest, OrnamentsAreStablePerIssuerAndAllUsed) {
  std::set<Ornament> seen;
  for (std::uint32_t seed = 0; seed < 32; ++seed) {
    EXPECT_EQ(OrnamentOf(seed), OrnamentOf(seed));
    seen.insert(OrnamentOf(seed));
  }
  EXPECT_EQ(seen.size(), static_cast<std::size_t>(Ornament::kCount));
}

TEST(SkinTest, SegmentsDrawFilledDamagedAndEmpty) {
  gfx::DrawList list;
  const Color c = Color::Rgb(0xFFFFFF);
  PaintSegments(list, {0, 0, 14, 60}, 6, 3, 1, c, c, c);
  // Filled and empty segments are one primitive each, a damaged one three
  // (tint, outline, slash).
  EXPECT_EQ(list.instances().size(), 3U + 3U + 2U);
  list.Clear();
  PaintSegments(list, {0, 0, 14, 60}, 4, 9, 9, c, c, c);
  EXPECT_EQ(list.instances().size(), 4U) << "filled is clamped";
  list.Clear();
  PaintSegments(list, {0, 0, 14, 60}, 0, 0, 0, c, c, c);
  EXPECT_TRUE(list.instances().empty());
}

TEST(SkinTest, HatchStaysInsideItsRect) {
  gfx::DrawList list;
  const Rect before = list.clip();
  PaintHatch(list, {10, 10, 50, 30}, Color::Rgb(0xFF0000));
  EXPECT_GT(list.instances().size(), 3U);
  EXPECT_EQ(list.clip(), before);
  for (const gfx::Instance &i : list.instances()) {
    EXPECT_GE(i.clip[0], 10.0F);
    EXPECT_LE(i.clip[2], 50.0F);
  }
}

TEST(SkinTest, StampGrowsWithItsText) {
  testing::UiHarness h;
  const text::FontId bold = h.fonts().bold;
  const Vec2 a = StampSize(h.text(), bold, 16, "Open");
  const Vec2 b = StampSize(h.text(), bold, 16, "Fulfilled");
  EXPECT_GT(b.x, a.x);
  EXPECT_FLOAT_EQ(a.y, b.y);
  // Lower case is stamped in capitals.
  EXPECT_FLOAT_EQ(StampSize(h.text(), bold, 16, "open").x, a.x);
}

}  // namespace
}  // namespace gloomy::ui
