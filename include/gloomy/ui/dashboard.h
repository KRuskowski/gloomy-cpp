#ifndef GLOOMY_UI_DASHBOARD_H_
#define GLOOMY_UI_DASHBOARD_H_

#include <cstddef>
#include <span>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/skin.h"
#include "gloomy/ui/summary.h"

namespace gloomy::ui {

// A hub of tiles (docs/design.md section 2): each tile answers one
// question in a line, lists the few items behind the answer, and opens
// the full screen with one action. Tiles sit in a grid whose column count
// follows the width it gets.

/// One item under a tile's answer: what (left) and its number (right).
struct TileLine {
  std::string_view text;
  /// Right aligned; empty for none.
  std::string_view value;
  /// Colours the value (and the text unless neutral).
  Tone tone = Tone::kNeutral;
  /// Shown as a tooltip on hover; empty for none.
  std::string_view why;
};

/// What a DashboardTile shows.
struct TileSpec {
  /// Small, muted, top left: "Ships".
  std::string_view title;
  /// Small, muted, top right: "3 of 4 busy".
  std::string_view aside;
  /// The answer in one line, explained on hover.
  ExplainedValue answer;
  /// Colours the left edge and the answer (neutral: the theme's text).
  Tone status = Tone::kNeutral;
  /// A sparkline right of the answer (oldest first); empty for none.
  std::span<const float> trend;
  Tone trend_tone = Tone::kNeutral;
  /// The items behind the answer; each can be clicked.
  std::span<const TileLine> lines;
  /// Lines shown; the rest are summed up as "+N more", which acts like
  /// the action.
  std::size_t max_lines = 4;
  /// Muted line shown when there are no lines; empty for none.
  std::string_view empty;
  /// The button that opens the full screen behind the tile: "Fleet (O)".
  /// Empty: no button, but a click on the tile or Enter still acts.
  std::string_view action;
  /// A stylised panel skin (a ship's or a company's HUD flavour): the
  /// tile is framed and papered by it and its text uses the skin's inks.
  /// Null: the theme's card. Must outlive the call.
  const PanelSkin *skin = nullptr;
};

/// What the player did with a tile this frame.
struct TileResult {
  /// The action (button, click on the tile, Enter while it has focus, or
  /// "+N more").
  bool action = false;
  /// Index into TileSpec::lines of the line clicked; -1 for none.
  int line = -1;
};

/// A tile: title and aside, the answer (with trend), up to max_lines items
/// and the action. Stretches to the width and row height its parent gives
/// it (a tile grid's cell).
auto DashboardTile(Context &ctx, std::string_view label, const TileSpec &spec)
    -> TileResult;

/// How many tiles at least `min_tile_width` wide fit side by side in
/// `width` with `gap` between them: from 1 to `max_columns`.
[[nodiscard]] auto TileColumns(float width, float min_tile_width, float gap,
                               int max_columns) -> int;

/// Opens a grid for tiles that fills its parent's width, with as many
/// columns as TileColumns() allows at the width it had last frame (the
/// viewport's on the first). Each row is as tall as its tallest tile.
/// EndTileGrid() closes it.
void BeginTileGrid(Context &ctx, std::string_view label, float min_tile_width,
                   int max_columns = 4, float gap = 12.0F);

void EndTileGrid(Context &ctx);

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_DASHBOARD_H_
