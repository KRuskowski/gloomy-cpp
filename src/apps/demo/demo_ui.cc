#include "demo_ui.h"

#include <string>
#include <vector>

namespace gloomy::demo {

void DemoUi(ui::Context &ctx, DemoState &s) {
  using ui::LayoutSpec;
  using ui::SizeSpec;
  static const std::vector<std::string> kGoods{
      "Energy", "Ore", "Silicon", "Food"};
  static const std::vector<std::string> kTabs{"Stations", "Ships", "Prices"};
  static const std::vector<std::string> kThemes{
      "Dark", "Light", "High contrast"};

  LayoutSpec root;
  root.padding = ui::Padding::All(12);
  root.gap = 12;
  root.width = SizeSpec::Grow();
  root.height = SizeSpec::Grow();
  ui::BeginRow(ctx, root);

  LayoutSpec left;
  left.width = SizeSpec::Fixed(280);
  left.height = SizeSpec::Grow();
  ui::BeginPanel(ctx, left);
  ui::Heading(ctx, "gloomy-cpp");
  LayoutSpec buttons;
  buttons.gap = 8;
  ui::BeginRow(ctx, buttons);
  ui::Button(ctx, "Undock", ui::ButtonKind::kPrimary);
  ui::Button(ctx, "Trade");
  ui::Button(ctx, "Cancel", ui::ButtonKind::kFlat);
  ui::End(ctx);
  ui::Checkbox(ctx, "Show gates", s.gates);
  ui::Checkbox(ctx, "Show traders", s.traders);
  LayoutSpec radios;
  radios.gap = 12;
  ui::BeginRow(ctx, radios);
  ui::RadioButton(ctx, "Buy", s.mode, 0);
  ui::RadioButton(ctx, "Sell", s.mode, 1);
  ui::End(ctx);
  ui::Slider(ctx, "Zoom", s.zoom, 0, 1);
  ui::TextFieldOptions field;
  field.width = SizeSpec::Grow();
  field.placeholder = "Search stations";
  ui::TextField(ctx, "##search", s.search, field);
  ui::Dropdown(ctx, "Good", s.good, kGoods);
  ui::Dropdown(ctx, "Theme", s.theme, kThemes);
  ui::ProgressBar(ctx, s.zoom);
  ui::LabelOptions muted;
  muted.muted = true;
  muted.wrap = true;
  ui::Label(ctx,
            "Tab moves focus, arrows and Page Up/Down move in the list, "
            "typing jumps to a station, Enter activates.",
            muted);
  if (s.activated >= 0) {
    ui::Label(ctx, "Activated station " + std::to_string(s.activated));
  }
  ui::End(ctx);

  LayoutSpec right;
  right.width = SizeSpec::Grow();
  right.height = SizeSpec::Grow();
  ui::BeginPanel(ctx, right);
  ui::Tabs(ctx, "##tabs", s.tab, kTabs);
  LayoutSpec list_layout;
  list_layout.height = SizeSpec::Grow();
  std::string buffer;
  const ui::ListResult r = ui::List(
      ctx,
      "##stations",
      s.list,
      100000,
      [&](int i) -> std::string_view {
        buffer = "Station " + std::to_string(i) + "  (Argon Prime)";
        return buffer;
      },
      list_layout);
  if (r.activated) {
    s.activated = s.list.selected;
  }
  ui::End(ctx);
  ui::End(ctx);
}

}  // namespace gloomy::demo
