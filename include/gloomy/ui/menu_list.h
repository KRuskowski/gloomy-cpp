#ifndef GLOOMY_UI_MENU_LIST_H_
#define GLOOMY_UI_MENU_LIST_H_

#include <span>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/layout.h"

namespace gloomy::ui {

// Menu list: the X3-style vertical menu of a main menu or pause menu
// (docs/design.md section 11.1). Each item is a large label with an
// optional detail line under it (answer first: what the item leads to,
// e.g. the save Continue would load). The item under the app-owned cursor
// is filled with the selection colour and marked with a bar in
// Palette::selection_accent (else primary). Keyboard: Up/Down move the
// cursor (wrapping, skipping disabled items), Enter activates; pointer:
// hovering moves the cursor, a click activates. Disabled items stay
// visible and show why instead of their detail.

/// One item of a menu list.
struct MenuListItem {
  std::string label;
  /// Second line: what the item does or leads to; empty for none.
  std::string detail;
  bool enabled = true;
  /// Shown instead of the detail while disabled ("No saved game yet").
  std::string disabled_reason;
};

struct MenuListOptions {
  SizeSpec width = SizeSpec::Fixed(360.0F);
  /// Label size; 0 is the theme's font size plus 5.
  float label_size = 0.0F;
  /// Take Up/Down/Enter from this frame's keys (off when another widget
  /// owns them).
  bool keys = true;
};

/// The cursor after moving `direction` (+1 down, -1 up) from `cursor`,
/// wrapping and skipping disabled items; `cursor` if no other item is
/// enabled.
[[nodiscard]] auto MenuListStep(std::span<const MenuListItem> items, int cursor,
                                int direction) -> int;

/// Declares a menu list.
/// @param[in,out] cursor the highlighted item, owned by the app; clamped,
///     and moved off a disabled item
/// @return the item activated this frame; -1 if none
auto MenuList(Context &ctx, std::string_view label,
              std::span<const MenuListItem> items, int &cursor,
              const MenuListOptions &options = {}) -> int;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_MENU_LIST_H_
