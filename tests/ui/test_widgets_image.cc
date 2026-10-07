#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/ui/widgets.h"
#include "test_support/gpu_test.h"
#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

struct GalleryState {
  bool gates = true;
  bool traders = false;
  int mode = 1;
  float zoom = 0.4F;
  std::string search = "ore";
  std::string empty;
  int good = 1;
  int tab = 0;
  ListState list;
};

void Gallery(Context &ctx, GalleryState &s) {
  static const std::vector<std::string> kGoods{
      "Energy", "Ore", "Silicon", "Food"};
  static const std::vector<std::string> kTabs{"Stations", "Ships", "Prices"};
  LayoutSpec root;
  root.padding = Padding::All(12);
  root.gap = 12;
  root.width = SizeSpec::Grow();
  root.height = SizeSpec::Grow();
  BeginRow(ctx, root);

  LayoutSpec left;
  left.width = SizeSpec::Fixed(260);
  left.height = SizeSpec::Grow();
  BeginPanel(ctx, left);
  Heading(ctx, "Controls");
  LayoutSpec buttons;
  buttons.gap = 8;
  BeginRow(ctx, buttons);
  Button(ctx, "Undock", ButtonKind::kPrimary);
  Button(ctx, "Trade");
  Button(ctx, "Cancel", ButtonKind::kFlat);
  End(ctx);
  Checkbox(ctx, "Show gates", s.gates);
  Checkbox(ctx, "Show traders", s.traders);
  LayoutSpec radios;
  radios.gap = 12;
  BeginRow(ctx, radios);
  RadioButton(ctx, "Buy", s.mode, 0);
  RadioButton(ctx, "Sell", s.mode, 1);
  End(ctx);
  Slider(ctx, "Zoom", s.zoom, 0, 1);
  TextFieldOptions field;
  field.width = SizeSpec::Grow();
  TextField(ctx, "##search", s.search, field);
  field.placeholder = "Search stations";
  TextField(ctx, "##empty", s.empty, field);
  Dropdown(ctx, "Good", s.good, kGoods);
  ProgressBar(ctx, 0.62F);
  LabelOptions muted;
  muted.muted = true;
  muted.wrap = true;
  Label(ctx,
        "Prices are what your traders last saw; older reports are dimmer.",
        muted);
  End(ctx);

  LayoutSpec right;
  right.width = SizeSpec::Grow();
  right.height = SizeSpec::Grow();
  BeginPanel(ctx, right);
  Tabs(ctx, "##tabs", s.tab, kTabs);
  LayoutSpec list_layout;
  list_layout.height = SizeSpec::Grow();
  List(
      ctx,
      "##stations",
      s.list,
      1000,
      [](int i) -> std::string_view {
        static std::string buffer;
        buffer = "Station " + std::to_string(i) + "  (Argon Prime)";
        return buffer;
      },
      list_layout);
  End(ctx);
  End(ctx);
}

class WidgetsImageTest : public testing::GpuTest {
 protected:
  auto Render(testing::UiHarness &h) -> Image {
    gfx::UiRenderer renderer(ctx(), gfx::OffscreenTarget::kFormat, h.atlas());
    gfx::OffscreenTarget target(ctx(), {640, 400});
    return gfx::RenderToImage(ctx(),
                              renderer,
                              target,
                              h.ctx().draw_lists(),
                              h.ctx().theme().colors.background);
  }
};

TEST_F(WidgetsImageTest, DarkGallery) {
  testing::UiHarness h({640, 400});
  GalleryState s;
  s.list.selected = 2;
  auto ui = [&](Context &ctx) {
    Gallery(ctx, s);
  };
  h.Frame(ui);
  h.Frame(ui);
  // Focus the list with the keyboard so its focus ring shows.
  h.ctx().SetFocus(h.ctx().GetId("##stations"));
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h), "widgets_dark");
}

TEST_F(WidgetsImageTest, LightGalleryWithOpenDropdown) {
  testing::UiHarness h({640, 400});
  h.ctx().SetTheme(Theme::Light(h.fonts()));
  GalleryState s;
  auto ui = [&](Context &ctx) {
    Gallery(ctx, s);
  };
  h.Frame(ui);
  h.Frame(ui);
  h.ctx().SetFocus(h.ctx().GetId("Good"));
  h.Frame(ui, testing::UiHarness::Key(Key::kEnter));
  h.Frame(ui, testing::UiHarness::Key(Key::kDown));
  h.Frame(ui);
  testing::ExpectMatchesReference(Render(h), "widgets_light");
}

}  // namespace
}  // namespace gloomy::ui
