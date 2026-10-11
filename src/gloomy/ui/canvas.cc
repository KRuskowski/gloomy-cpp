#include "gloomy/ui/canvas.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace gloomy::ui {

namespace {

/// Pointer movement (pixels) that turns a press into a drag.
constexpr float kDragSlop = 3.0F;

/// Zooms by `factor` keeping the world point under `anchor` in place.
void ZoomAround(CanvasState &state, const CanvasView &view, Vec2 anchor,
                double factor, const CanvasOptions &options) {
  const WorldPoint before = view.ToWorld(anchor);
  state.zoom =
      std::clamp(state.zoom * factor, options.min_zoom, options.max_zoom);
  CanvasView after = view;
  after.zoom = state.zoom;
  const WorldPoint moved = after.ToWorld(anchor);
  state.x += before.x - moved.x;
  state.y += before.y - moved.y;
}

}  // namespace

void FitCanvas(CanvasState &state, const Rect &rect, const WorldRect &fit,
               float margin) {
  state.x = (fit.x0 + fit.x1) / 2.0;
  state.y = (fit.y0 + fit.y1) / 2.0;
  const double w = std::max(1e-9, fit.x1 - fit.x0);
  const double h = std::max(1e-9, fit.y1 - fit.y0);
  const double avail_w = std::max(1.0, rect.Width() - 2.0 * margin);
  const double avail_h = std::max(1.0, rect.Height() - 2.0 * margin);
  state.zoom = std::min(avail_w / w, avail_h / h);
}

auto MakeCanvasView(const CanvasState &state, const Rect &rect) -> CanvasView {
  CanvasView view;
  view.rect = rect;
  view.center_x = state.x;
  view.center_y = state.y;
  view.zoom = state.zoom > 0.0 ? state.zoom : 1.0;
  return view;
}

auto Canvas(Context &ctx, std::string_view label, CanvasState &state,
            const CanvasOptions &options, CanvasPainter paint) -> CanvasResult {
  CanvasResult result;
  result.id = ctx.GetId(label);
  const Interaction it = ctx.Interact(result.id);
  const bool laid_out = it.rect.Width() > 0.0F && it.rect.Height() > 0.0F;
  if (state.zoom <= 0.0 && laid_out) {
    FitCanvas(state, it.rect, options.fit, options.fit_margin);
  }
  result.view = MakeCanvasView(state, it.rect);
  result.hovered = it.hovered;
  result.pointer_screen = ctx.mouse();
  result.pointer = result.view.ToWorld(ctx.mouse());

  if (laid_out && state.zoom > 0.0) {
    // Wheel: zoom around the pointer.
    const Vec2 wheel = ctx.Wheel(result.id);
    if (wheel.y != 0.0F) {
      ZoomAround(state,
                 result.view,
                 ctx.mouse(),
                 std::pow(options.zoom_step, wheel.y),
                 options);
    }
    // Drag: pan.
    if (it.pressed) {
      if (!state.panning) {
        state.panning = true;
        state.dragged = false;
        state.pan_x = state.x;
        state.pan_y = state.y;
      }
      if (std::fabs(it.drag.x) > kDragSlop ||
          std::fabs(it.drag.y) > kDragSlop) {
        state.dragged = true;
      }
      if (state.dragged) {
        state.x = state.pan_x - it.drag.x / state.zoom;
        state.y = state.pan_y - it.drag.y / state.zoom;
      }
    } else {
      state.panning = false;
    }
    // Keys while focused: zoom around the centre, fit.
    if (ctx.HasFocus(result.id)) {
      const Vec2 centre = it.rect.Center();
      if (ctx.ConsumeKey(Key::kEquals, kModNone, true)) {
        ZoomAround(state, result.view, centre, options.zoom_step, options);
      }
      if (ctx.ConsumeKey(Key::kMinus, kModNone, true)) {
        ZoomAround(
            state, result.view, centre, 1.0 / options.zoom_step, options);
      }
      if (ctx.ConsumeKey(Key::kHome)) {
        FitCanvas(state, it.rect, options.fit, options.fit_margin);
      }
    }
  }
  result.clicked = it.clicked && !state.dragged;
  result.double_clicked = it.double_clicked;
  result.right_clicked = it.right_clicked;
  result.right_pressed = it.right_pressed;
  if (it.clicked) {
    state.dragged = false;
  }
  // The view after this frame's camera changes, for the app's picking.
  result.view = MakeCanvasView(state, it.rect);

  NodeDesc desc;
  desc.id = result.id;
  desc.layout = options.layout;
  desc.flags = kNodeInteractive | kNodeFocusable | kNodeWheel | kNodeWheelZoom |
               kNodeClip | kNodeFocusRing;
  const int node = ctx.Open(desc);
  ctx.Close();
  const Color background = options.background.a > 0.0F
                               ? options.background
                               : ctx.theme().colors.background;
  ctx.AddPainter(
      node,
      [&state, options, background, paint = std::move(paint)](PaintContext &pc,
                                                              const Rect &r) {
        if (state.zoom <= 0.0) {
          FitCanvas(state, r, options.fit, options.fit_margin);
        }
        pc.list.PushClip(r);
        pc.list.FillRect(r, background);
        if (paint) {
          paint(pc, MakeCanvasView(state, r));
        }
        pc.list.PopClip();
      },
      true);
  return result;
}

namespace {

/// An item within reach of the pointer, and how it ranks.
struct PickHit {
  std::uint64_t key = 0;
  bool direct = false;
  int layer = 0;
  float d2 = 0.0F;
};

auto HitOf(const CanvasView &view, const PickItem &item, Vec2 screen)
    -> std::optional<PickHit> {
  const Vec2 p = view.ToScreen(item.pos);
  const float dx = p.x - screen.x;
  const float dy = p.y - screen.y;
  const float d2 = dx * dx + dy * dy;
  if (d2 > item.radius * item.radius) {
    return std::nullopt;
  }
  const bool direct = item.core > 0.0F && d2 <= item.core * item.core;
  return PickHit{item.key, direct, item.layer, d2};
}

auto Better(const PickHit &a, const PickHit &b) -> bool {
  if (a.direct != b.direct) {
    return a.direct;
  }
  return a.layer != b.layer ? a.layer > b.layer : a.d2 < b.d2;
}

}  // namespace

auto PickNearest(const CanvasView &view, std::span<const PickItem> items,
                 Vec2 screen) -> std::optional<std::uint64_t> {
  std::optional<PickHit> best;
  for (const PickItem &item : items) {
    const std::optional<PickHit> hit = HitOf(view, item, screen);
    if (hit && (!best || Better(*hit, *best))) {
      best = hit;
    }
  }
  if (!best) {
    return std::nullopt;
  }
  return best->key;
}

auto PickAll(const CanvasView &view, std::span<const PickItem> items,
             Vec2 screen) -> std::vector<std::uint64_t> {
  std::vector<PickHit> hits;
  for (const PickItem &item : items) {
    if (const std::optional<PickHit> hit = HitOf(view, item, screen)) {
      hits.push_back(*hit);
    }
  }
  std::ranges::stable_sort(hits, Better);
  std::vector<std::uint64_t> keys;
  keys.reserve(hits.size());
  for (const PickHit &hit : hits) {
    keys.push_back(hit.key);
  }
  return keys;
}

auto PickCycle::Click(Vec2 screen, std::vector<std::uint64_t> hits)
    -> std::optional<std::uint64_t> {
  if (hits.empty()) {
    Reset();
    return std::nullopt;
  }
  // The same items in any order (moving objects reorder by distance):
  // keep the first click's order and take the next.
  auto sorted = [](std::vector<std::uint64_t> keys) {
    std::ranges::sort(keys);
    return keys;
  };
  const bool near = Near(screen);
  const auto current =
      near ? std::ranges::find(hits, keys_[index_]) : hits.end();
  if (near && sorted(keys_) == sorted(hits)) {
    index_ = (index_ + 1) % keys_.size();
  } else if (current != hits.end()) {
    // Items came or went at the edge: go on after the current one.
    index_ = static_cast<std::size_t>(current - hits.begin() + 1) % hits.size();
    keys_ = std::move(hits);
  } else {
    keys_ = std::move(hits);
    index_ = 0;
  }
  at_ = screen;
  return keys_[index_];
}

void PickCycle::Reset() {
  keys_.clear();
  index_ = 0;
}

auto PickCycle::current() const -> std::optional<std::uint64_t> {
  if (keys_.empty()) {
    return std::nullopt;
  }
  return keys_[index_];
}

auto PickCycle::Near(Vec2 screen) const -> bool {
  const float dx = screen.x - at_.x;
  const float dy = screen.y - at_.y;
  return !keys_.empty() && dx * dx + dy * dy <= kSlop * kSlop;
}

auto PickCycle::Hint() const -> std::string {
  if (keys_.size() < 2) {
    return {};
  }
  return std::to_string(index_ + 1) + "/" + std::to_string(keys_.size());
}

void DrawTargetBrackets(gfx::DrawList &list, Vec2 centre, float radius,
                        const Color &color, double time) {
  // Breathe by up to 3 px, once a second.
  const auto breath = static_cast<float>(
      0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * std::fmod(time, 1.0))));
  const float r = radius + 3.0F * breath;
  const float arm = std::max(4.0F, 0.45F * r);
  const Color halo = Color::Rgba(0x000000B0);
  for (const auto &[w, c] : {std::pair{4.5F, halo}, std::pair{2.0F, color}}) {
    for (const float sx : {-1.0F, 1.0F}) {
      for (const float sy : {-1.0F, 1.0F}) {
        const Vec2 corner{centre.x + sx * r, centre.y + sy * r};
        list.Line(corner, {corner.x - sx * arm, corner.y}, w, c);
        list.Line(corner, {corner.x, corner.y - sy * arm}, w, c);
      }
    }
  }
}

void DrawHoverRing(gfx::DrawList &list, Vec2 centre, float radius,
                   const Color &color) {
  list.StrokeCircle(centre, radius, color.WithAlpha(0.45F * color.a), 1.0F);
}

auto StackHint(std::size_t hits) -> std::string {
  return hits < 2 ? std::string() : "1/" + std::to_string(hits);
}

}  // namespace gloomy::ui
