#ifndef GLOOMY_UI_CARGO_HOLD_H_
#define GLOOMY_UI_CARGO_HOLD_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/list_rows.h"
#include "gloomy/ui/skin.h"

namespace gloomy::ui {

// A hold, answer first: on top one row per cargo type (bulk, container,
// later liquid, gas, reefer) with its fill as abstract units, rounded
// squares of a fixed volume in the colours of the goods in them; below
// the manifest, one line per good with the host's columns (quantity,
// volume, cost basis, best known buyer).
//
// Scale: a unit is a fixed volume from a ladder of round sizes (10, 25,
// 50, 100, 250, 500, 1000, 2500 ... m3), the smallest that keeps a type's
// row at most kMaxUnits squares, so a courier and a bulk freighter both
// show 8 to 40 squares of the same pixel size and the legend "1 unit =
// 250 m3" says the scale. A shared unit (CargoHoldSpec::unit_volume) puts
// ships side by side on one scale: then a big hold is visibly longer.
// Goods of one type fill consecutive units in their colours; a unit split
// between two goods shows both, a partly filled last unit its share.

/// One good in the hold.
struct CargoLot {
  /// "Food Rations".
  std::string_view name;
  /// Space it takes, in the hold's unit (m3).
  double volume = 0.0;
  Color color = Color::Rgb(0x808C98);
  /// Tooltip on its units and its manifest line: cost basis, best buyer.
  std::string_view why;
  /// Its manifest line, in CargoHoldSpec::columns; the first cell is
  /// drawn in the lot's colour.
  std::span<const Cell> cells;
};

/// One cargo type's space: "Container", 600 m3, and what is in it.
struct HoldSection {
  std::string_view name;
  double capacity = 0.0;
  std::span<const CargoLot> lots;
};

/// The hold.
struct CargoHoldSpec {
  std::span<const HoldSection> sections;
  /// Volume of one unit for every section; 0 picks one per section from
  /// the ladder (UnitVolumeFor()). Pass the same value to several holds
  /// to compare them on one scale.
  double unit_volume = 0.0;
  /// Side of a unit's square in pixels (scaled with the theme).
  float cell = 14.0F;
  /// The volume's unit in the legend.
  std::string_view unit = "m3";
  /// Manifest columns; empty draws no manifest.
  std::span<const CellColumn> columns;
  /// Said in the manifest when the hold is empty; empty for none.
  std::string_view empty = "Hold empty";
  /// Null derives the skin from the theme.
  const PanelSkin *skin = nullptr;
};

/// At most this many units in a row before the next ladder step.
inline constexpr int kMaxUnits = 40;

/// The ladder of unit volumes: 10, 25, 50, 100, 250, 500, 1000, ... for
/// step 0, 1, 2, ...
[[nodiscard]] auto LadderVolume(int step) -> double;

/// The smallest ladder volume that holds `capacity` in at most
/// `max_units` units.
[[nodiscard]] auto UnitVolumeFor(double capacity, int max_units = kMaxUnits)
    -> double;

/// A good's part of one unit.
struct UnitPiece {
  /// Index of the lot.
  int lot = 0;
  /// Its share of the unit, 0 to 1.
  float share = 0.0F;
};

/// One unit: the goods in it, bottom up, and its size (the last unit of a
/// hold that is no multiple of the unit is smaller).
struct HoldUnit {
  std::vector<UnitPiece> pieces;
  /// Share of a full unit this unit's space is, 0 to 1.
  float size = 1.0F;

  /// Share of the unit filled.
  [[nodiscard]] auto Fill() const -> float;
};

/// Pours the lots in order into units of `unit_volume`: ceil(capacity /
/// unit_volume) units; volume beyond the capacity is cut off.
[[nodiscard]] auto FillUnits(double capacity, double unit_volume,
                             std::span<const CargoLot> lots)
    -> std::vector<HoldUnit>;

/// Share of a section used, 0 to 1.
[[nodiscard]] auto HoldUsed(double capacity, std::span<const CargoLot> lots)
    -> double;

/// "1 unit = 250 m3", "1 unit = 2,500 m3".
[[nodiscard]] auto UnitLegend(double unit_volume, std::string_view unit)
    -> std::string;

/// What the user did this frame.
struct CargoHoldResult {
  /// Section and lot under the pointer (a unit or a manifest line); -1
  /// for none.
  int section = -1;
  int lot = -1;
  /// The lot clicked (in `section`); -1 for none.
  int clicked = -1;
};

auto CargoHold(Context &ctx, std::string_view label, const CargoHoldSpec &spec)
    -> CargoHoldResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_CARGO_HOLD_H_
