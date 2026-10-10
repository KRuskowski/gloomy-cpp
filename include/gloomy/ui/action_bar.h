#ifndef GLOOMY_UI_ACTION_BAR_H_
#define GLOOMY_UI_ACTION_BAR_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gloomy/ui/context.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

// A row of buttons that never cuts a label: when the width it gets is too
// small, the least important actions first switch to their compact label
// and then move into a "More" menu, the most important stay longest. For
// status bars and toolbars over a window of any width.

/// One action of a bar.
struct BarAction {
  /// The full label: "What should I do? (A)". May carry "##id".
  std::string_view label;
  /// A shorter label for tight widths: "Advice (A)"; empty: none.
  std::string_view compact;
  /// Higher stays full and in the bar longest.
  int priority = 0;
  ButtonKind kind = ButtonKind::kNormal;
  /// Not empty: disabled, and why (tooltip, menu hint).
  std::string_view disabled;
  /// What it does, as the tooltip and the menu item's hint; empty for none.
  std::string_view hint;
};

/// How one action is shown.
enum class ActionFit : std::uint8_t { kFull, kCompact, kMenu };

/// Fits actions of widths `full` and `compact` (equal to full when there
/// is no compact label) with `gap` between them into `available`: first
/// the lowest priority actions take their compact labels, one by one, then
/// they move into the menu, whose button is `more_width` wide. Ties go by
/// position: the later action yields first.
[[nodiscard]] auto FitActions(std::span<const float> full,
                              std::span<const float> compact,
                              std::span<const int> priority, float available,
                              float gap, float more_width)
    -> std::vector<ActionFit>;

/// Options of an action bar.
struct ActionBarOptions {
  /// The menu's button.
  std::string_view more = "More";
  float gap = 6.0F;
};

/// What happened in an action bar this frame.
struct ActionBarResult {
  /// Index into the actions of the one chosen; -1 for none.
  int chosen = -1;
  /// The chosen action came from the menu.
  bool from_menu = false;
  /// The menu is open.
  bool menu_open = false;
  /// Actions in the menu, and shown with their compact labels.
  int in_menu = 0;
  int compact = 0;
};

/// The bar: grows to the width its parent gives it, shows the actions as
/// FitActions() allows at last frame's width (the viewport's on the
/// first) and the rest in a menu.
auto ActionBar(Context &ctx, std::string_view label,
               std::span<const BarAction> actions,
               const ActionBarOptions &options = {}) -> ActionBarResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_ACTION_BAR_H_
