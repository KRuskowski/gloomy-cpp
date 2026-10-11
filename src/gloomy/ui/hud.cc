#include "gloomy/ui/hud.h"

#include <algorithm>
#include <cmath>

namespace gloomy::ui {

namespace {

constexpr float kMaxStep = 0.105F;

}  // namespace

auto ArcPoint(Vec2 centre, float radius, float angle) -> Vec2 {
  return {centre.x + radius * std::sin(angle),
          centre.y - radius * std::cos(angle)};
}

void StrokeArc(gfx::DrawList &list, Vec2 centre, float radius, float from,
               float to, float width, const Color &color) {
  const float span = to - from;
  if (std::fabs(span) < 1e-6F || radius <= 0.0F) {
    return;
  }
  const int steps =
      std::max(1, static_cast<int>(std::ceil(std::fabs(span) / kMaxStep)));
  Vec2 prev = ArcPoint(centre, radius, from);
  for (int i = 1; i <= steps; ++i) {
    const float a =
        from + span * static_cast<float>(i) / static_cast<float>(steps);
    const Vec2 p = ArcPoint(centre, radius, a);
    list.Line(prev, p, width, color);
    prev = p;
  }
}

auto EdgePoint(const Rect &rect, Vec2 from, Vec2 dir, float inset) -> Vec2 {
  const Rect r = rect.Inset(inset);
  const float len = std::hypot(dir.x, dir.y);
  if (len < 1e-6F) {
    return from;
  }
  const Vec2 d{dir.x / len, dir.y / len};
  float t = 1e30F;
  if (d.x > 0.0F) {
    t = std::min(t, (r.x1 - from.x) / d.x);
  } else if (d.x < 0.0F) {
    t = std::min(t, (r.x0 - from.x) / d.x);
  }
  if (d.y > 0.0F) {
    t = std::min(t, (r.y1 - from.y) / d.y);
  } else if (d.y < 0.0F) {
    t = std::min(t, (r.y0 - from.y) / d.y);
  }
  t = std::max(t, 0.0F);
  return {from.x + d.x * t, from.y + d.y * t};
}

void EdgeArrow(gfx::DrawList &list, Vec2 tip, Vec2 dir, float size, float width,
               const Color &color) {
  const float len = std::hypot(dir.x, dir.y);
  if (len < 1e-6F) {
    return;
  }
  const Vec2 d{dir.x / len, dir.y / len};
  const Vec2 n{-d.y, d.x};
  const Vec2 back{tip.x - d.x * size, tip.y - d.y * size};
  const float half = size * 0.6F;
  list.Line({back.x + n.x * half, back.y + n.y * half}, tip, width, color);
  list.Line({back.x - n.x * half, back.y - n.y * half}, tip, width, color);
}

void GlowLine(gfx::DrawList &list, Vec2 a, Vec2 b, float width,
              const Color &color, float glow) {
  if (glow > 0.0F) {
    list.Line(a, b, width + 2.0F * glow, color.WithAlpha(color.a * 0.18F));
    list.Line(a, b, width + glow, color.WithAlpha(color.a * 0.3F));
  }
  list.Line(a, b, width, color);
}

}  // namespace gloomy::ui
