#include "gloomy/gfx/draw_list.h"

#include <algorithm>
#include <cmath>

namespace gloomy::gfx {

namespace {

void SetRadii(Instance &inst, const CornerRadii &radii) {
  inst.radii[0] = radii.top_left;
  inst.radii[1] = radii.top_right;
  inst.radii[2] = radii.bottom_right;
  inst.radii[3] = radii.bottom_left;
}

}  // namespace

void DrawList::Clear() {
  instances_.clear();
  clip_stack_.assign(1, kUnclipped);
  alpha_stack_.assign(1, 1.0F);
}

void DrawList::PushAlpha(float alpha) {
  alpha_stack_.push_back(alpha_stack_.back() * std::clamp(alpha, 0.0F, 1.0F));
}

void DrawList::PopAlpha() {
  if (alpha_stack_.size() > 1) {
    alpha_stack_.pop_back();
  }
}

auto DrawList::Packed(const Color &c) const -> std::uint32_t {
  const float a = alpha_stack_.back();
  return a >= 1.0F ? c.Pack() : c.WithAlpha(c.a * a).Pack();
}

auto DrawList::Push(PrimitiveKind kind, const Rect &bounds) -> Instance & {
  Instance &inst = instances_.emplace_back();
  inst = Instance{};
  inst.rect[0] = bounds.x0;
  inst.rect[1] = bounds.y0;
  inst.rect[2] = bounds.x1;
  inst.rect[3] = bounds.y1;
  const Rect &c = clip();
  inst.clip[0] = c.x0;
  inst.clip[1] = c.y0;
  inst.clip[2] = c.x1;
  inst.clip[3] = c.y1;
  inst.kind = static_cast<std::uint32_t>(kind);
  return inst;
}

void DrawList::FillRect(const Rect &r, const Color &c,
                        const CornerRadii &radii) {
  FillRectGradient(r, c, c, radii);
}

void DrawList::FillRectGradient(const Rect &r, const Color &top,
                                const Color &bottom, const CornerRadii &radii) {
  if (r.Empty() || Culled(r)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kRect, r);
  inst.color = Packed(top);
  inst.color2 = Packed(bottom);
  SetRadii(inst, radii);
}

void DrawList::FillGlass(const Rect &r, const GlassLook &look,
                         const Color &fallback, const CornerRadii &radii) {
  if (r.Empty() || Culled(r)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kGlass, r);
  inst.color = Packed(look.tint);
  inst.color2 = Packed(fallback);
  // The look rides in the otherwise unused atlas coordinates.
  inst.uv[0] = look.darken;
  inst.uv[1] = look.noise;
  inst.uv[2] = look.highlight;
  inst.uv[3] = look.highlight_width;
  SetRadii(inst, radii);
}

void DrawList::StrokeRect(const Rect &r, const Color &c, float width,
                          const CornerRadii &radii) {
  if (r.Empty() || Culled(r) || width <= 0.0F) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kRect, r);
  inst.color = Packed(c);
  inst.color2 = inst.color;
  inst.stroke = width;
  SetRadii(inst, radii);
}

void DrawList::Shadow(const Rect &r, const Color &c, float blur, Vec2 offset,
                      const CornerRadii &radii) {
  const Rect box = r.Offset(offset);
  if (box.Empty() || Culled(box.Inset(-blur))) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kRect, box);
  inst.color = Packed(c);
  inst.color2 = inst.color;
  inst.soft = blur;
  SetRadii(inst, radii);
}

void DrawList::FillCircle(Vec2 center, float radius, const Color &c) {
  StrokeCircle(center, radius, c, 0.0F);
}

void DrawList::StrokeCircle(Vec2 center, float radius, const Color &c,
                            float width) {
  const Rect r{center.x - radius,
               center.y - radius,
               center.x + radius,
               center.y + radius};
  if (radius <= 0.0F || Culled(r)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kCircle, r);
  inst.color = Packed(c);
  inst.color2 = inst.color;
  inst.stroke = width;
}

void DrawList::FillHexagon(Vec2 center, float radius, const Color &c,
                           bool pointy) {
  StrokeHexagon(center, radius, c, 0.0F, pointy);
}

void DrawList::StrokeHexagon(Vec2 center, float radius, const Color &c,
                             float width, bool pointy) {
  const Rect r{center.x - radius,
               center.y - radius,
               center.x + radius,
               center.y + radius};
  if (radius <= 0.0F || Culled(r)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kHexagon, r);
  inst.color = Packed(c);
  inst.color2 = inst.color;
  inst.stroke = width;
  inst.radii[0] = pointy ? 1.0F : 0.0F;
}

void DrawList::Line(Vec2 a, Vec2 b, float width, const Color &c) {
  const Rect bounds{std::fmin(a.x, b.x) - width,
                    std::fmin(a.y, b.y) - width,
                    std::fmax(a.x, b.x) + width,
                    std::fmax(a.y, b.y) + width};
  if (width <= 0.0F || Culled(bounds)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kLine, Rect{a.x, a.y, b.x, b.y});
  inst.color = Packed(c);
  inst.color2 = inst.color;
  inst.radii[0] = width;
}

void DrawList::Polyline(std::span<const Vec2> points, float width,
                        const Color &c) {
  for (std::size_t i = 1; i < points.size(); ++i) {
    Line(points[i - 1], points[i], width, c);
  }
  // Round joints: wide segments meeting at an angle leave a notch.
  if (width > 2.0F) {
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
      FillCircle(points[i], width * 0.5F, c);
    }
  }
}

void DrawList::FillTriangle(Vec2 a, Vec2 b, Vec2 c, const Color &col,
                            std::uint32_t aa_edges) {
  const Rect bounds{std::fmin(a.x, std::fmin(b.x, c.x)),
                    std::fmin(a.y, std::fmin(b.y, c.y)),
                    std::fmax(a.x, std::fmax(b.x, c.x)),
                    std::fmax(a.y, std::fmax(b.y, c.y))};
  if (Culled(bounds)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kTriangle, bounds);
  inst.radii[0] = a.x;
  inst.radii[1] = a.y;
  inst.radii[2] = b.x;
  inst.radii[3] = b.y;
  inst.uv[0] = c.x;
  inst.uv[1] = c.y;
  inst.stroke = static_cast<float>(aa_edges & 7U);
  inst.color = Packed(col);
  inst.color2 = inst.color;
}

namespace {

auto Cross(Vec2 o, Vec2 a, Vec2 b) -> float {
  return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

/// Whether `p` lies inside (or on) triangle a, b, c of orientation `sign`.
auto Inside(Vec2 p, Vec2 a, Vec2 b, Vec2 c, float sign) -> bool {
  return Cross(a, b, p) * sign >= 0.0F && Cross(b, c, p) * sign >= 0.0F &&
         Cross(c, a, p) * sign >= 0.0F;
}

}  // namespace

void DrawList::FillPolygon(std::span<const Vec2> points, const Color &c) {
  const std::size_t n = points.size();
  if (n < 3) {
    return;
  }
  // Orientation from the signed area.
  float area = 0.0F;
  for (std::size_t i = 0; i < n; ++i) {
    const Vec2 p = points[i];
    const Vec2 q = points[(i + 1) % n];
    area += p.x * q.y - q.x * p.y;
  }
  if (area == 0.0F) {
    return;
  }
  const float sign = area > 0.0F ? 1.0F : -1.0F;
  // The remaining polygon as indices; an edge between neighbours in it is
  // an outline edge when they were neighbours in the original polygon.
  std::vector<std::size_t> left(n);
  for (std::size_t i = 0; i < n; ++i) {
    left[i] = i;
  }
  const auto outline = [n](std::size_t i, std::size_t j) {
    return (i + 1) % n == j || (j + 1) % n == i;
  };
  std::size_t guard = 0;
  std::size_t i = 0;
  while (left.size() > 3 && guard < 2 * n * n) {
    ++guard;
    const std::size_t m = left.size();
    const std::size_t ia = left[(i + m - 1) % m];
    const std::size_t ib = left[i % m];
    const std::size_t ic = left[(i + 1) % m];
    const Vec2 a = points[ia];
    const Vec2 b = points[ib];
    const Vec2 cc = points[ic];
    bool ear = Cross(a, b, cc) * sign > 0.0F;
    for (std::size_t k = 0; ear && k < m; ++k) {
      const std::size_t ik = left[k];
      ear = ik == ia || ik == ib || ik == ic ||
            !Inside(points[ik], a, b, cc, sign);
    }
    if (!ear) {
      i = (i + 1) % m;
      continue;
    }
    FillTriangle(a,
                 b,
                 cc,
                 c,
                 (outline(ia, ib) ? 1U : 0U) | (outline(ib, ic) ? 2U : 0U) |
                     (outline(ic, ia) ? 4U : 0U));
    left.erase(left.begin() + static_cast<std::ptrdiff_t>(i % m));
    i = i % left.size();
  }
  if (left.size() == 3) {
    const std::size_t ia = left[0];
    const std::size_t ib = left[1];
    const std::size_t ic = left[2];
    FillTriangle(points[ia],
                 points[ib],
                 points[ic],
                 c,
                 (outline(ia, ib) ? 1U : 0U) | (outline(ib, ic) ? 2U : 0U) |
                     (outline(ic, ia) ? 4U : 0U));
  }
}

void DrawList::FillBand(std::span<const Vec2> top, std::span<const Vec2> bottom,
                        const Color &c) {
  const std::size_t n = std::min(top.size(), bottom.size());
  for (std::size_t i = 1; i < n; ++i) {
    // Quad t0 t1 b1 b0 as two triangles sharing the diagonal t0-b1; the
    // ends of the band antialias too.
    const std::uint32_t first_end = i == 1 ? 2U : 0U;
    const std::uint32_t last_end = i + 1 == n ? 2U : 0U;
    FillTriangle(top[i - 1], top[i], bottom[i], c, 1U | last_end);
    FillTriangle(bottom[i], bottom[i - 1], top[i - 1], c, 1U | first_end);
  }
}

void DrawList::Glyph(const Rect &r, const Rect &uv, int page, const Color &c) {
  if (Culled(r)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kGlyph, r);
  inst.uv[0] = uv.x0;
  inst.uv[1] = uv.y0;
  inst.uv[2] = uv.x1;
  inst.uv[3] = uv.y1;
  inst.color = Packed(c);
  inst.color2 = inst.color;
  inst.kind |= static_cast<std::uint32_t>(page) << 8U;
}

void DrawList::PushClip(const Rect &r) {
  clip_stack_.push_back(clip().Intersect(r));
}

void DrawList::PopClip() {
  if (clip_stack_.size() > 1) {
    clip_stack_.pop_back();
  }
}

}  // namespace gloomy::gfx
