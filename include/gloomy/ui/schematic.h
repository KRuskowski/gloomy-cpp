#ifndef GLOOMY_UI_SCHEMATIC_H_
#define GLOOMY_UI_SCHEMATIC_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gloomy/core/geometry.h"
#include "gloomy/icon/icon_set.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/skin.h"
#include "gloomy/ui/summary.h"

namespace gloomy::ui {

// A ship (or station, or any machine) as a cross-section, in the manner
// of FTL's ship view: a hull with rooms, each room a system or a space
// with its state drawn in it (damage hatched, contents filled, crew as
// dots), a shield bubble round the hull, and segmented gauges for the
// systems under it. Entirely data-driven: the host lists the rooms (from
// a class's stats now, from modular slots later) and the layout follows.

/// What a room is, for its default look (the bridge goes in the bow).
enum class RoomKind : std::uint8_t {
  kOther,
  kBridge,
  kHold,
  kEngine,
  kShield,
  kWeapon,
  kSensor,
  kCrew,
};

/// One room of the cross-section.
struct SchematicRoom {
  std::string_view label;
  RoomKind kind = RoomKind::kOther;
  /// Relative size: rooms of a deck share its length by their cells.
  float cells = 1.0F;
  /// Deck from the top, 0-based; -1 puts it on the emptiest deck.
  int deck = -1;
  /// How well it works, 0 to 1: a bar along the room's floor.
  float level = 1.0F;
  /// Damage, 0 to 1: the room tinted and hatched in the bad colour.
  float damage = 0.0F;
  /// How full it is, 0 to 1, in `color` from the floor up (a hold's
  /// cargo); below 0 draws nothing.
  float fill = -1.0F;
  Color color{0, 0, 0, 0};
  /// Crew in the room, as dots.
  int crew = 0;
  icon::IconId icon = icon::kNoIcon;
  /// Tooltip.
  std::string_view why;
};

/// A system's gauge under the hull: segments as in FTL's power bars.
struct SystemGauge {
  std::string_view label;
  int segments = 4;
  int filled = 4;
  /// Segments shown broken after the filled ones.
  int damaged = 0;
  Tone tone = Tone::kGood;
  /// The number beside it: "320/400".
  std::string_view value;
  std::string_view why;
  icon::IconId icon = icon::kNoIcon;
};

/// The whole cross-section.
struct SchematicSpec {
  /// Header: the ship's name and class line; empty hides the header.
  std::string_view name;
  std::string_view subtitle;
  std::span<const SchematicRoom> rooms;
  std::span<const SystemGauge> gauges;
  /// Shield strength 0 to 1 as the bubble's brightness; below 0 the ship
  /// has no shields and no bubble is drawn.
  float shield = -1.0F;
  /// Hull 0 to 1: the hull outline turns warning below 60%, bad below
  /// 30%.
  float hull = 1.0F;
  /// Crew line in the header: "Crew 6 - 140 Cr/h".
  std::string_view crew;
  /// Decks; 0 picks by the number of rooms (LayoutSchematic()).
  int decks = 0;
  /// Height of the hull area in pixels.
  float height = 120.0F;
  /// No frame, header or gauge labels: for dialogs and lists.
  bool compact = false;
  /// Null derives the skin from the theme.
  const PanelSkin *skin = nullptr;
};

/// Where the hull and the rooms go in an area.
struct SchematicLayout {
  /// The hull's main body (rooms other than the bridge go here).
  Rect body;
  /// The bow's two steps, narrowing to the right.
  Rect bow_inner;
  Rect bow_outer;
  /// Engine nozzles at the stern (left).
  Rect engines;
  /// The shield bubble's box.
  Rect bubble;
  /// One rectangle per room, in the order given.
  std::vector<Rect> rooms;
  int decks = 1;
};

/// The decks `rooms` take when the spec says 0: one up to 3 rooms, two up
/// to 8, else three.
[[nodiscard]] auto AutoDecks(std::size_t rooms) -> int;

/// Lays the hull and rooms out in `area`: the stern left, the bow right.
/// The first bridge room without a deck goes in the bow; rooms with a
/// deck keep it (clamped), the rest go in order onto the deck with the
/// fewest cells; each deck's rooms share its length by their cells.
[[nodiscard]] auto LayoutSchematic(std::span<const SchematicRoom> rooms,
                                   const Rect &area, int decks)
    -> SchematicLayout;

/// The room at `p`, or -1.
[[nodiscard]] auto RoomAt(const SchematicLayout &layout, Vec2 p) -> int;

/// What the user did this frame.
struct SchematicResult {
  int hovered = -1;
  int clicked = -1;
};

/// Draws the cross-section with its header and gauges.
auto ShipSchematic(Context &ctx, std::string_view label,
                   const SchematicSpec &spec) -> SchematicResult;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_SCHEMATIC_H_
