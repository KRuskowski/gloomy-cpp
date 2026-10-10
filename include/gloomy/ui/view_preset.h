#ifndef GLOOMY_UI_VIEW_PRESET_H_
#define GLOOMY_UI_VIEW_PRESET_H_

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/data/query.h"
#include "gloomy/data/table_source.h"
#include "gloomy/ui/data_grid.h"

namespace gloomy::ui {

/// A saved view of a table: query and column layout. Curated presets ship
/// with the app; players save, duplicate and share their own
/// (docs/design.md section 2.4).
struct ViewPreset {
  std::string name;
  std::string description;
  data::Query query;
  /// Column order, widths, visibility and pinning; empty keeps the
  /// grid's columns.
  std::vector<GridColumn> columns;
  bool curated = false;
};

/// Applies a preset to a grid: query, then columns in the preset's order.
/// Renderer settings of columns come from the grid's current columns;
/// grid columns the preset does not mention are appended hidden.
void ApplyPreset(const ViewPreset &preset, GridState &state);

/// The grid's current view as a preset.
auto CapturePreset(std::string name, const GridState &state) -> ViewPreset;

/// Encodes a preset as one line of text to copy and paste. Columns are
/// referred to by their stable names, so presets survive schema changes.
auto EncodePreset(const ViewPreset &preset, const data::TableSource &source)
    -> std::string;

/// Decodes EncodePreset() output; unknown columns are ignored.
/// @return std::nullopt if the text is not a preset.
auto DecodePreset(std::string_view text, const data::TableSource &source)
    -> std::optional<ViewPreset>;

struct PresetBarResult {
  /// `active` changed; apply the preset.
  bool changed = false;
  /// "Save view" was chosen.
  bool save = false;
  /// "Share" was chosen.
  bool share = false;
};

/// A row of preset chips (curated first, marked) with Save and Share.
/// `active` is -1 when the view was modified away from every preset.
auto PresetBar(Context &ctx, std::string_view label,
               std::span<const ViewPreset> presets, int &active)
    -> PresetBarResult;

struct PresetSegmentsOptions {
  /// Rows each preset shows, in the presets' order; empty if unknown.
  std::span<const int> counts;
  /// Presets that show no rows go to the menu (unless chosen), so the
  /// segments are the views worth looking at now.
  bool hide_empty = true;
  /// At most this many presets as segments; the rest go to the menu.
  int max_segments = 8;
};

/// Which segments of widths `widths` fit into `available` pixels, with
/// `frame` taken once and `gap` between two shown segments: those marked
/// in `keep` always stay; the others leave last first until the rest fit.
/// @return per segment whether it is shown
[[nodiscard]] auto FitSegments(std::span<const float> widths,
                               const std::vector<bool> &keep, float frame,
                               float gap, float available) -> std::vector<bool>;

/// The presets as a segmented control with their row counts: the chosen
/// one and those with rows first, the rest, Save and Share in a "Views"
/// menu at the end. While the view is modified away from every preset
/// (`active` -1) a "Your view" segment shows selected. The control grows
/// to the width its parent gives it and never cuts a name: views that do
/// not fit at last frame's width move into the menu, last first, under
/// "More views"; the chosen one always stays.
auto PresetSegments(Context &ctx, std::string_view label,
                    std::span<const ViewPreset> presets, int &active,
                    const PresetSegmentsOptions &opt = {}) -> PresetBarResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_VIEW_PRESET_H_
