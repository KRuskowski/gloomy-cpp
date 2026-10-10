#include "gloomy/gfx/draw_list.h"

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
  inst.color = top.Pack();
  inst.color2 = bottom.Pack();
  SetRadii(inst, radii);
}

void DrawList::FillGlass(const Rect &r, const GlassLook &look,
                         const Color &fallback, const CornerRadii &radii) {
  if (r.Empty() || Culled(r)) {
    return;
  }
  Instance &inst = Push(PrimitiveKind::kGlass, r);
  inst.color = look.tint.Pack();
  inst.color2 = fallback.Pack();
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
  inst.color = c.Pack();
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
  inst.color = c.Pack();
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
  inst.color = c.Pack();
  inst.color2 = inst.color;
  inst.stroke = width;
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
  inst.color = c.Pack();
  inst.color2 = inst.color;
  inst.radii[0] = width;
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
  inst.color = c.Pack();
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
