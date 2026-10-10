#ifndef GLOOMY_UI_SKIN_H_
#define GLOOMY_UI_SKIN_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/summary.h"
#include "gloomy/ui/theme.h"

namespace gloomy::ui {

// A panel skin: the colours, frame weights and effects that stylised
// widgets (documents, certificates, schematics, cargo holds) draw with,
// in the chunky-framed, clearly iconed look of FTL's panels
// (docs/design.md section 13.3). A skin is plain data so that one file can
// flavour a game's HUD and its panels alike: ParsePanelSkin() reads the
// same "key = value" lines as a HUD skin and skips keys it does not know.
// The painters below take a draw list, so a HUD can use them outside a
// Context too.

/// Colours, weights and effects of stylised panels.
struct PanelSkin {
  std::string name = "default";
  /// Frames, rules and headings (a HUD's "primary").
  Color ink = Color::Rgb(0x6EDCFF);
  /// Secondary lines, labels, empty gauge segments (a HUD's
  /// "secondary").
  Color ink_muted = Color::Rgb(0xA0B4C8);
  /// A document's body.
  Color paper = Color::Rgb(0x0E1A24);
  /// Bands on the paper: the letterhead strip, room floors, empty bays.
  Color paper_edge = Color::Rgb(0x16283A);
  /// Text on the paper.
  Color text = Color::Rgb(0xD0D8E0);
  Color text_muted = Color::Rgb(0x808C98);
  /// Tones: good (a HUD's "own"), warning, bad (a HUD's "alert").
  Color good = Color::Rgb(0x3CDC3C);
  Color warning = Color::Rgb(0xE0B030);
  Color bad = Color::Rgb(0xFF3030);
  /// Relations, for parties and signatures.
  Color friendly = Color::Rgb(0x78B4FF);
  Color neutral = Color::Rgb(0xD0D8E0);
  Color hostile = Color::Rgb(0xFF5050);
  /// Width of the chunky outer frame in pixels.
  float frame = 3.0F;
  /// Width of the thin bevel line inside the frame; 0 for none.
  float inset = 1.0F;
  /// Corner radius of frames.
  float corner = 3.0F;
  /// Radius of the bolts in a frame's corners; 0 for none.
  float bolt = 1.5F;
  /// Glow around lines in pixels (GlowLine()); 0 for none.
  float glow = 0.0F;
  /// Strength (alpha) of scan lines over terminal paper; 0 for none.
  float scanlines = 0.0F;

  /// The colour of a tone: neutral is the text colour.
  [[nodiscard]] auto ToneInk(Tone tone) const -> Color;

  /// The skin a theme implies: its accent as ink, its surfaces as paper,
  /// its success, warning and error colours as tones.
  [[nodiscard]] static auto FromTheme(const Theme &theme) -> PanelSkin;

  /// The same skin printed on light paper (certificates, policies): cream
  /// paper, dark ink and text, the tones darkened to read on it.
  [[nodiscard]] auto OnPaper() const -> PanelSkin;
};

/// Reads a skin from "key = value" lines over `base` ('#' starts a
/// comment). Keys shared with HUD skins: name, primary (ink), secondary
/// (ink_muted), own (good), friendly, neutral, hostile, warning, alert
/// (bad), line (frame = 2 x line), glow, scanlines. Panel keys:
/// panel_paper, panel_paper_edge, panel_text, panel_text_muted,
/// panel_frame, panel_inset, panel_corner, panel_bolt. Colours are #RRGGBB
/// or #RRGGBBAA. Unknown keys are skipped silently (a HUD skin's
/// instruments); bad values of known keys are reported in `errors` and
/// leave `base`'s value.
[[nodiscard]] auto ParsePanelSkin(std::string_view text, const PanelSkin &base,
                                  std::vector<std::string> *errors = nullptr)
    -> PanelSkin;

/// Parses "#RRGGBB" or "#RRGGBBAA".
/// @return false if `text` is neither.
[[nodiscard]] auto ParseHexColor(std::string_view text, Color &out) -> bool;

/// A chunky frame: `fill`, an outer frame of skin.frame pixels in
/// `edge`, a bevel line inside it and bolts in the corners.
void PaintFrame(gfx::DrawList &list, const Rect &r, const PanelSkin &skin,
                const Color &fill, const Color &edge);

/// A rubber stamp: `text` in bold capitals inside a double border, in
/// `color` at stamp opacity, centred on `centre`.
/// @return The stamp's rectangle.
auto PaintStamp(gfx::DrawList &list, text::TextSystem &text, text::FontId bold,
                float size, Vec2 centre, std::string_view label,
                const Color &color) -> Rect;

/// The width and height PaintStamp() takes for `label` at `size`.
[[nodiscard]] auto StampSize(text::TextSystem &text, text::FontId bold,
                             float size, std::string_view label) -> Vec2;

/// A seal: a ring of `points` ticks round two circles, as on
/// certificates and notarised documents.
void PaintSeal(gfx::DrawList &list, Vec2 centre, float radius,
               const Color &color, int points = 24);

/// Kinds of ornate border, picked by an issuer's seed (OrnamentOf()).
enum class Ornament : std::uint8_t {
  /// A chain of small rings.
  kRings,
  /// Chevrons along the edge.
  kChevrons,
  /// Diamonds between two rules.
  kDiamonds,
  /// A wave between two rules.
  kWave,
  kCount,
};

/// The ornament an issuer's seed picks.
[[nodiscard]] auto OrnamentOf(std::uint32_t seed) -> Ornament;

/// An ornate certificate border `band` pixels wide inside `r`, with
/// rosettes in the corners.
void PaintOrnateBorder(gfx::DrawList &list, const Rect &r, Ornament ornament,
                       float band, const Color &color);

/// Diagonal hatching over `r` (clipped to it), `spacing` pixels apart.
void PaintHatch(gfx::DrawList &list, const Rect &r, const Color &color,
                float spacing = 6.0F, float width = 1.5F);

/// A segmented gauge as in FTL's power bars: `segments` blocks in `r`,
/// bottom to top when `vertical` (else left to right); the first
/// `filled` in `on`, the next `damaged` in `damaged_color`, the rest as
/// empty outlines in `off`.
void PaintSegments(gfx::DrawList &list, const Rect &r, int segments, int filled,
                   int damaged, const Color &on, const Color &damaged_color,
                   const Color &off, bool vertical = true);

/// An ellipse outline through `segments` straight pieces.
void StrokeEllipse(gfx::DrawList &list, const Rect &box, float width,
                   const Color &color, int segments = 48);

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_SKIN_H_
