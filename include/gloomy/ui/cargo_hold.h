#ifndef GLOOMY_UI_CARGO_HOLD_H_
#define GLOOMY_UI_CARGO_HOLD_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/icon/icon_set.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/skin.h"

namespace gloomy::ui {

// A hold as a grid of bays, filled in order by what it carries: one look
// says how full it is, with what, and how much room is left. Each lot is
// drawn as crates (packaged goods) or tanks (bulk), in its colour and with
// its icon; empty bays stay visible. Hovering a lot shows the host's
// detail (cost, best buyer). The host passes a plain list of lots, so one
// good or a whole manifest look the same.

/// How a lot is packed.
enum class CargoForm : std::uint8_t {
  /// Crates with cross braces.
  kCrate,
  /// Rounded tanks with a band.
  kTank,
};

/// One lot in the hold.
struct CargoLot {
  /// "Food Rations".
  std::string_view name;
  /// Space it takes, in the hold's unit (m3).
  double volume = 0.0;
  /// Its amount as the host words it: "120 units".
  std::string_view amount;
  Color color = Color::Rgb(0x808C98);
  icon::IconId icon = icon::kNoIcon;
  CargoForm form = CargoForm::kCrate;
  /// Tooltip: cost basis, best known buyer.
  std::string_view why;
};

/// The hold.
struct CargoHoldSpec {
  /// Total space, in the lots' unit.
  double capacity = 0.0;
  std::span<const CargoLot> lots;
  /// Bays drawn; 0 picks from the width (as many as fit in two to four
  /// rows).
  int bays = 0;
  /// Bay size in pixels.
  float cell = 22.0F;
  /// The unit after volumes in the legend: "m3".
  std::string_view unit = "m3";
  /// A legend under the grid: each lot's share, and the space free.
  bool legend = true;
  /// A label over the grid ("Hold"); empty for none.
  std::string_view title;
  /// Null derives the skin from the theme.
  const PanelSkin *skin = nullptr;
};

/// What one bay holds.
struct HoldBay {
  /// Index of the lot that takes most of it; -1 for an empty bay.
  int lot = -1;
  /// Share of the bay filled, 0 to 1 (a lot's last bay may be partial).
  float fill = 0.0F;
};

/// Fills `bays` bays of `capacity` / `bays` each with the lots in order,
/// as if poured in: a bay belongs to the lot taking most of it. Volumes
/// beyond the capacity are cut off.
[[nodiscard]] auto FillHold(double capacity, std::span<const CargoLot> lots,
                            int bays) -> std::vector<HoldBay>;

/// Columns and rows for `bays` in `width` pixels with bays of `cell`
/// pixels.
struct HoldGrid {
  int columns = 1;
  int rows = 1;
};

[[nodiscard]] auto HoldGridFor(int bays, float width, float cell) -> HoldGrid;

/// Bays to draw when the spec says 0: two rows' worth at `width`, at
/// least 8, at most 4 rows.
[[nodiscard]] auto AutoBays(float width, float cell) -> int;

/// Share of the hold used, 0 to 1.
[[nodiscard]] auto HoldUsed(double capacity, std::span<const CargoLot> lots)
    -> double;

/// What the user did this frame.
struct CargoHoldResult {
  /// Lot under the pointer (or its legend entry); -1 for none.
  int hovered = -1;
  int clicked = -1;
};

auto CargoHold(Context &ctx, std::string_view label, const CargoHoldSpec &spec)
    -> CargoHoldResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_CARGO_HOLD_H_
