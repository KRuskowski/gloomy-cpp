#ifndef GLOOMY_UI_RADIAL_MENU_H_
#define GLOOMY_UI_RADIAL_MENU_H_

#include <span>
#include <string>
#include <string_view>

#include "gloomy/core/geometry.h"
#include "gloomy/icon/icon_set.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

// Radial context menu: options in a circle around the pointer, first at
// the top, clockwise (docs/design.md section 11.1). Mouse: move towards an
// option and click, or press the right button, drag towards an option and
// release; a plain right click leaves the menu open. Keyboard: arrows pick
// the option in that direction, Tab and Shift+Tab step round, 1-9 choose
// directly, Enter or Space chooses, Escape cancels. A click in the middle
// or a right press cancels too. Disabled options stay visible; when
// pointed at they say why (answer first) instead of acting.

/// One option of a radial menu.
struct RadialItem {
  std::string label;
  /// Icon of Context::icons(); kNoIcon draws the option's number.
  icon::IconId icon = icon::kNoIcon;
  bool enabled = true;
  /// Why the option cannot be chosen now, answer first ("Not docked: fly
  /// there first"); shown while a disabled option is pointed at.
  std::string disabled_reason;
};

/// Sizes of a radial menu in pixels.
struct RadialMetrics {
  /// Distance from the centre to the options' centres.
  float ring_radius = 78.0F;
  /// Radius of an option's disc.
  float item_radius = 22.0F;
  /// Pointer movement from the centre below which nothing is pointed at.
  float dead_zone = 18.0F;
};

/// What a radial menu did this frame.
struct RadialMenuResult {
  /// Whether the menu is (still) open.
  bool open = false;
  /// The option chosen this frame; -1 if none. The menu closes.
  int chosen = -1;
  /// Closed without a choice this frame (Escape, click in the middle).
  bool cancelled = false;
  /// The option pointed at or selected with the keyboard; -1 if none.
  int highlight = -1;
};

/// Direction of option `index` of `count` in radians: 0 at the top, growing
/// clockwise (screen y down).
[[nodiscard]] auto RadialItemAngle(int index, int count) -> float;

/// Centre of option `index` of `count` on a ring of `radius` around
/// `centre`.
[[nodiscard]] auto RadialItemCentre(Vec2 centre, float radius, int index,
                                    int count) -> Vec2;

/// The option in the pointer's direction from the centre: the one whose
/// sector (an equal slice of the circle around its direction) contains
/// it; -1 within `dead_zone` of the centre or without options.
[[nodiscard]] auto RadialPick(Vec2 centre, Vec2 pointer, int count,
                              float dead_zone) -> int;

/// The option whose direction is closest to `direction` (arrow keys); -1
/// without options or for no direction.
[[nodiscard]] auto RadialItemToward(Vec2 direction, int count) -> int;

/// Where a menu opened at `at` is centred so that a ring of `extent`
/// pixels around it (options and labels) stays inside `viewport`.
[[nodiscard]] auto RadialFit(Vec2 at, float extent, Vec2 viewport) -> Vec2;

/// Opens radial menu `label` around `at` (usually the pointer). If the
/// right button is held, releasing it over an option chooses it.
void OpenRadialMenu(Context &ctx, std::string_view label, Vec2 at);

/// Whether radial menu `label` is open.
[[nodiscard]] auto IsRadialMenuOpen(Context &ctx, std::string_view label)
    -> bool;

/// Closes radial menu `label` without a choice.
void CloseRadialMenu(Context &ctx, std::string_view label);

/// Declares radial menu `label` if it is open, on an overlay layer that
/// blocks input below; handles its keys and pointer before anything
/// declared later. Call every frame, before widgets that take the same
/// keys, with the same options while it is open.
/// @param[in] title Shown in the middle while nothing is pointed at.
auto RadialMenu(Context &ctx, std::string_view label,
                std::span<const RadialItem> items, std::string_view title = {},
                const RadialMetrics &metrics = {}) -> RadialMenuResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_RADIAL_MENU_H_
