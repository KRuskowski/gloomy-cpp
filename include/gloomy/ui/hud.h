#ifndef GLOOMY_UI_HUD_H_
#define GLOOMY_UI_HUD_H_

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/gfx/draw_list.h"

namespace gloomy::ui {

// Drawing for head-up displays over a game scene (canvases, painters):
// arcs for status rings and radars, arrows at the edge for things off
// screen, glowing lines, scan lines. Angles are radians clockwise from
// straight up (12 o'clock), as on gauges.

/// The point of `angle` on a circle (y down, clockwise from up).
auto ArcPoint(Vec2 centre, float radius, float angle) -> Vec2;

/// An arc from angle `from` to `to` (clockwise when to > from) as line
/// segments of at most about 6 degrees.
void StrokeArc(gfx::DrawList &list, Vec2 centre, float radius, float from,
               float to, float width, const Color &color);

/// Where a ray from `from` (inside `rect`) in direction `dir` leaves
/// `rect` shrunk by `inset`: for arrows pointing at things off screen.
auto EdgePoint(const Rect &rect, Vec2 from, Vec2 dir, float inset) -> Vec2;

/// A chevron arrow with its tip at `tip`, pointing along `dir`.
void EdgeArrow(gfx::DrawList &list, Vec2 tip, Vec2 dir, float size, float width,
               const Color &color);

/// A line with a soft glow around it (a wide faint line under a thin one);
/// `glow` 0 draws the plain line.
void GlowLine(gfx::DrawList &list, Vec2 a, Vec2 b, float width,
              const Color &color, float glow);

/// Horizontal scan lines over `rect`, `spacing` pixels apart.
void Scanlines(gfx::DrawList &list, const Rect &rect, float spacing,
               const Color &color);

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_HUD_H_
