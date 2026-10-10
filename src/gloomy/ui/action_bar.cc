#include "gloomy/ui/action_bar.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>

#include "gloomy/ui/overlays.h"

namespace gloomy::ui {

namespace {

/// The bar's width last frame.
struct ActionBarState {
  float width = 0.0F;
};

/// Width of the visible actions and the menu button.
auto Total(std::span<const ActionFit> fit, std::span<const float> full,
           std::span<const float> compact, float gap, float more_width)
    -> float {
  float total = 0.0F;
  int shown = 0;
  bool menu = false;
  for (std::size_t i = 0; i < fit.size(); ++i) {
    switch (fit[i]) {
      case ActionFit::kFull:
        total += full[i];
        ++shown;
        break;
      case ActionFit::kCompact:
        total += compact[i];
        ++shown;
        break;
      case ActionFit::kMenu:
        menu = true;
        break;
    }
  }
  if (menu) {
    total += more_width;
    ++shown;
  }
  return total + gap * static_cast<float>(std::max(0, shown - 1));
}

/// Width of `text` as a button's label.
auto ButtonWidth(Context &ctx, std::string_view text, float extra) -> float {
  const Theme &t = ctx.theme();
  return std::ceil(ctx.text().Measure(
             DisplayText(text), t.fonts.sans, t.metrics.font_size)) +
         t.metrics.padding_x * 3.0F + extra + 1.0F;
}

}  // namespace

auto FitActions(std::span<const float> full, std::span<const float> compact,
                std::span<const int> priority, float available, float gap,
                float more_width) -> std::vector<ActionFit> {
  const std::size_t n = full.size();
  std::vector<ActionFit> fit(n, ActionFit::kFull);
  // Who yields first: the lowest priority, then the later position.
  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::ranges::stable_sort(order, [priority](std::size_t a, std::size_t b) {
    const int pa = a < priority.size() ? priority[a] : 0;
    const int pb = b < priority.size() ? priority[b] : 0;
    return pa != pb ? pa < pb : a > b;
  });
  const auto compact_of = [&](std::size_t i) {
    return i < compact.size() ? compact[i] : full[i];
  };
  std::vector<float> small(n);
  for (std::size_t i = 0; i < n; ++i) {
    small[i] = compact_of(i);
  }
  const auto fits = [&] {
    return Total(fit, full, small, gap, more_width) <= available;
  };
  for (const std::size_t i : order) {
    if (fits()) {
      return fit;
    }
    if (small[i] < full[i]) {
      fit[i] = ActionFit::kCompact;
    }
  }
  for (const std::size_t i : order) {
    if (fits()) {
      return fit;
    }
    fit[i] = ActionFit::kMenu;
  }
  return fit;
}

auto ActionBar(Context &ctx, std::string_view label,
               std::span<const BarAction> actions,
               const ActionBarOptions &options) -> ActionBarResult {
  const Id id = ctx.GetId(label);
  auto &state = ctx.State<ActionBarState>(id);
  if (const std::optional<Rect> last = ctx.LastRect(id)) {
    state.width = last->Width();
  }
  const float available = state.width > 0.0F ? state.width : ctx.viewport().x;
  std::vector<float> full;
  std::vector<float> compact;
  std::vector<int> priority;
  for (const BarAction &a : actions) {
    full.push_back(ButtonWidth(ctx, a.label, 0.0F));
    compact.push_back(a.compact.empty() ? full.back()
                                        : ButtonWidth(ctx, a.compact, 0.0F));
    priority.push_back(a.priority);
  }
  const float more_width =
      ButtonWidth(ctx, options.more, 22.0F - ctx.theme().metrics.padding_x);
  const std::vector<ActionFit> fit =
      FitActions(full, compact, priority, available, options.gap, more_width);

  NodeDesc bar;
  bar.id = id;
  bar.layout.mode = LayoutMode::kRow;
  bar.layout.width = SizeSpec::Grow();
  bar.layout.gap = options.gap;
  bar.layout.align = Align::kCenter;
  bar.layout.justify = Justify::kEnd;
  bar.flags = kNodeClip;
  ctx.Open(bar);
  ctx.PushId(id);
  ActionBarResult result;
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const BarAction &a = actions[i];
    if (fit[i] == ActionFit::kMenu) {
      ++result.in_menu;
      continue;
    }
    result.compact += fit[i] == ActionFit::kCompact ? 1 : 0;
    const std::string_view text =
        fit[i] == ActionFit::kCompact ? a.compact : a.label;
    // The index in the id: equal labels never clash.
    const std::string shown =
        std::string(DisplayText(text)) + "##bar" + std::to_string(i);
    const Id button = ctx.GetId(shown);
    if (!a.disabled.empty()) {
      DisabledButton(ctx, shown, a.disabled, a.kind);
      continue;
    }
    if (Button(ctx, shown, a.kind)) {
      result.chosen = static_cast<int>(i);
    }
    const std::string tip =
        fit[i] == ActionFit::kCompact
            ? std::string(DisplayText(a.label)) +
                  (a.hint.empty() ? "" : ": " + std::string(a.hint))
            : std::string(a.hint);
    if (!tip.empty()) {
      Tooltip(ctx, button, tip);
    }
  }
  if (result.in_menu > 0 && BeginMenuButton(ctx, options.more)) {
    result.menu_open = true;
    for (std::size_t i = 0; i < actions.size(); ++i) {
      if (fit[i] != ActionFit::kMenu) {
        continue;
      }
      const BarAction &a = actions[i];
      MenuItemSpec item;
      item.text = DisplayText(a.label);
      item.hint = a.disabled.empty() ? a.hint : a.disabled;
      item.enabled = a.disabled.empty();
      if (MenuItem(ctx, item)) {
        result.chosen = static_cast<int>(i);
        result.from_menu = true;
      }
    }
    EndMenuButton(ctx);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

}  // namespace gloomy::ui
