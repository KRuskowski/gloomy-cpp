#include "gloomy/ui/radial_menu.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "gloomy/icon/icon_cache.h"

namespace gloomy::ui {

namespace {

constexpr float kTau = 2.0F * std::numbers::pi_v<float>;
/// Room for the options' labels beyond the ring.
constexpr float kLabelRoom = 120.0F;

/// Per-menu state, kept by the context under the menu's id.
struct RadialState {
  bool open = false;
  /// Where it was opened, and its centre once fitted on screen.
  Vec2 at;
  Vec2 centre;
  int highlight = -1;
  /// Opened with the right button held: releasing it over an option
  /// chooses.
  bool right_drag = false;
  Vec2 last_mouse;
  std::uint64_t opened_frame = 0;
};

/// Angle of a direction: 0 up, clockwise, in [0, tau).
auto DirectionAngle(Vec2 d) -> float {
  float a = std::atan2(d.x, -d.y);
  if (a < 0.0F) {
    a += kTau;
  }
  return a;
}

auto Distance(Vec2 a, Vec2 b) -> float {
  return std::hypot(a.x - b.x, a.y - b.y);
}

auto Closed(RadialState &st, RadialMenuResult result) -> RadialMenuResult {
  st.open = false;
  result.open = false;
  return result;
}

/// Paints the menu: the ring, the options with icons and labels, the
/// pointed-at option's name in the middle and why it is disabled below.
void PaintRadial(PaintContext &pc, Vec2 centre,
                 const std::vector<RadialItem> &items, const std::string &title,
                 int highlight, const RadialMetrics &m) {
  const Theme &t = pc.theme;
  const Palette &c = t.colors;
  const text::FontId font = t.fonts.sans;
  const float small = t.metrics.small_font_size;
  const int n = static_cast<int>(items.size());
  const float outer = m.ring_radius + m.item_radius + 8.0F;
  pc.list.FillCircle(centre, outer, c.surface_raised.WithAlpha(0.94F));
  pc.list.StrokeCircle(centre, outer, c.border, t.metrics.border_width);
  pc.list.StrokeCircle(
      centre, m.ring_radius - m.item_radius - 6.0F, c.divider, 1.0F);
  if (highlight >= 0 && highlight < n) {
    // A spoke towards the option pointed at.
    const Vec2 to = RadialItemCentre(centre, m.ring_radius, highlight, n);
    const float k = (m.ring_radius - m.item_radius) / m.ring_radius;
    pc.list.Line(
        centre,
        {centre.x + (to.x - centre.x) * k, centre.y + (to.y - centre.y) * k},
        2.0F,
        items[highlight].enabled ? c.primary : c.text_disabled);
  }
  for (int i = 0; i < n; ++i) {
    const RadialItem &item = items[i];
    const Vec2 p = RadialItemCentre(centre, m.ring_radius, i, n);
    const bool hot = i == highlight;
    Color fill = c.surface_alt;
    Color ink = item.enabled ? c.text : c.text_disabled;
    if (hot && item.enabled) {
      fill = c.primary;
      ink = c.on_primary;
    } else if (hot) {
      fill = c.surface;
    }
    pc.list.FillCircle(p, m.item_radius, fill);
    pc.list.StrokeCircle(p,
                         m.item_radius,
                         hot ? (item.enabled ? c.focus : c.warning) : c.border,
                         hot ? 2.0F : 1.0F);
    if (item.icon != icon::kNoIcon && pc.icons != nullptr) {
      pc.icons->Draw(pc.list, item.icon, p, m.item_radius * 1.1F, ink);
    } else {
      const std::string number = std::to_string(i + 1);
      const float w = pc.text.Measure(number, font, t.metrics.font_size);
      pc.text.DrawText(pc.list,
                       number,
                       font,
                       t.metrics.font_size,
                       {p.x - 0.5F * w, p.y - 0.6F * t.metrics.font_size},
                       ink);
    }
    // The label outside the disc, away from the centre; numbered for the
    // keys 1-9.
    const std::string label =
        (i < 9 ? std::to_string(i + 1) + "  " : std::string()) + item.label;
    const float w = pc.text.Measure(label, font, small);
    const float a = RadialItemAngle(i, n);
    const float dx = std::sin(a);
    const float dy = -std::cos(a);
    const Vec2 edge{p.x + dx * (m.item_radius + 4.0F),
                    p.y + dy * (m.item_radius + 4.0F)};
    float x = edge.x - 0.5F * w;
    if (dx > 0.35F) {
      x = edge.x;
    } else if (dx < -0.35F) {
      x = edge.x - w;
    }
    float y = edge.y - 0.6F * small;
    if (dy > 0.35F) {
      y = edge.y;
    } else if (dy < -0.35F) {
      y = edge.y - 1.3F * small;
    }
    const Rect back{x - 3.0F, y - 1.0F, x + w + 3.0F, y + 1.4F * small};
    pc.list.FillRect(back,
                     c.surface_raised.WithAlpha(0.92F),
                     gfx::CornerRadii::All(t.metrics.radius));
    pc.text.DrawText(pc.list,
                     label,
                     font,
                     small,
                     {x, y},
                     hot ? (item.enabled ? c.text : c.warning) : ink);
  }
  // The middle: what is pointed at, else the title.
  const std::string &middle =
      highlight >= 0 && highlight < n ? items[highlight].label : title;
  if (!middle.empty()) {
    const float r = m.ring_radius - m.item_radius - 8.0F;
    pc.text.DrawFitted(pc.list,
                       middle,
                       font,
                       small,
                       {centre.x - r,
                        centre.y - 0.7F * small,
                        centre.x + r,
                        centre.y + 0.7F * small},
                       c.text,
                       text::HAlign::kCenter);
  }
  // Why a disabled option cannot be chosen, below the ring.
  if (highlight >= 0 && highlight < n && !items[highlight].enabled &&
      !items[highlight].disabled_reason.empty()) {
    const std::string &why = items[highlight].disabled_reason;
    const float w = pc.text.Measure(why, font, t.metrics.font_size);
    const float y = centre.y + outer + 10.0F;
    const Rect box{centre.x - 0.5F * w - 8.0F,
                   y,
                   centre.x + 0.5F * w + 8.0F,
                   y + t.metrics.font_size + 10.0F};
    pc.list.FillRect(box, c.surface_raised, gfx::CornerRadii::All(4.0F));
    pc.list.StrokeRect(box, c.warning, 1.0F, gfx::CornerRadii::All(4.0F));
    pc.text.DrawText(pc.list,
                     why,
                     font,
                     t.metrics.font_size,
                     {box.x0 + 8.0F, box.y0 + 4.0F},
                     c.text);
  }
}

}  // namespace

auto RadialItemAngle(int index, int count) -> float {
  return count > 0
             ? kTau * static_cast<float>(index) / static_cast<float>(count)
             : 0.0F;
}

auto RadialItemCentre(Vec2 centre, float radius, int index, int count) -> Vec2 {
  const float a = RadialItemAngle(index, count);
  return {centre.x + radius * std::sin(a), centre.y - radius * std::cos(a)};
}

auto RadialPick(Vec2 centre, Vec2 pointer, int count, float dead_zone) -> int {
  if (count <= 0 || Distance(centre, pointer) < dead_zone) {
    return -1;
  }
  const float slice = kTau / static_cast<float>(count);
  const float a = DirectionAngle({pointer.x - centre.x, pointer.y - centre.y});
  return static_cast<int>(std::floor((a + 0.5F * slice) / slice)) % count;
}

auto RadialItemToward(Vec2 direction, int count) -> int {
  if (count <= 0 || (direction.x == 0.0F && direction.y == 0.0F)) {
    return -1;
  }
  const float want = DirectionAngle(direction);
  int best = 0;
  float best_off = kTau;
  for (int i = 0; i < count; ++i) {
    float off = std::abs(RadialItemAngle(i, count) - want);
    off = std::min(off, kTau - off);
    // Ties go to the first, clockwise from the top.
    if (off < best_off - 1e-4F) {
      best = i;
      best_off = off;
    }
  }
  return best;
}

auto RadialFit(Vec2 at, float extent, Vec2 viewport) -> Vec2 {
  auto fit = [extent](float v, float size) {
    return size <= 2.0F * extent ? 0.5F * size
                                 : std::clamp(v, extent, size - extent);
  };
  return {fit(at.x, viewport.x), fit(at.y, viewport.y)};
}

void OpenRadialMenu(Context &ctx, std::string_view label, Vec2 at) {
  RadialState &st = ctx.State<RadialState>(ctx.GetId(label));
  st = RadialState{};
  st.open = true;
  st.at = at;
  st.right_drag = ctx.right_down();
  st.last_mouse = ctx.mouse();
  st.opened_frame = ctx.frame();
}

auto IsRadialMenuOpen(Context &ctx, std::string_view label) -> bool {
  return ctx.State<RadialState>(ctx.GetId(label)).open;
}

void CloseRadialMenu(Context &ctx, std::string_view label) {
  ctx.State<RadialState>(ctx.GetId(label)).open = false;
}

auto RadialMenu(Context &ctx, std::string_view label,
                std::span<const RadialItem> items, std::string_view title,
                const RadialMetrics &metrics) -> RadialMenuResult {
  const Id id = ctx.GetId(label);
  RadialState &st = ctx.State<RadialState>(id);
  RadialMenuResult result;
  if (!st.open) {
    return result;
  }
  const int n = static_cast<int>(items.size());
  const float extent = metrics.ring_radius + metrics.item_radius + kLabelRoom;
  st.centre = RadialFit(st.at, extent, ctx.viewport());
  const Id area = HashCombine(id, 0x72616469616CULL);
  const Interaction it = ctx.Interact(area);
  const bool fresh = ctx.frame() == st.opened_frame;
  auto choose = [&](int index) {
    result.chosen = index;
    result.highlight = index;
    return Closed(st, result);
  };

  // Keys first: the menu owns them while open.
  if (ctx.ConsumeKey(Key::kEscape)) {
    result.cancelled = true;
    return Closed(st, result);
  }
  const std::pair<Key, Vec2> arrows[] = {{Key::kUp, {0.0F, -1.0F}},
                                         {Key::kDown, {0.0F, 1.0F}},
                                         {Key::kLeft, {-1.0F, 0.0F}},
                                         {Key::kRight, {1.0F, 0.0F}}};
  for (const auto &[key, direction] : arrows) {
    if (ctx.ConsumeKey(key)) {
      st.highlight = RadialItemToward(direction, n);
    }
  }
  if (n > 0 && ctx.ConsumeKey(Key::kTab)) {
    st.highlight = (st.highlight + 1) % n;
  }
  if (n > 0 && ctx.ConsumeKey(Key::kTab, kModShift)) {
    st.highlight = st.highlight <= 0 ? n - 1 : st.highlight - 1;
  }
  for (int i = 0; i < std::min(n, 9); ++i) {
    if (ctx.ConsumeKey(static_cast<Key>(static_cast<int>(Key::k1) + i))) {
      if (items[i].enabled) {
        return choose(i);
      }
      st.highlight = i;
    }
  }
  if (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace)) {
    if (st.highlight >= 0 && st.highlight < n && items[st.highlight].enabled) {
      return choose(st.highlight);
    }
  }
  // Letters and other keys do not leak to widgets below; Tab steps round
  // the options instead of moving the focus.
  ctx.TakeText();
  ctx.WantTab();

  // Pointer: the direction it moved to points at an option.
  const Vec2 mouse = ctx.mouse();
  if (Distance(mouse, st.last_mouse) > 0.5F) {
    st.highlight = RadialPick(st.centre, mouse, n, metrics.dead_zone);
    st.last_mouse = mouse;
  }
  if (!fresh && it.clicked) {
    const int hit = RadialPick(st.centre, mouse, n, metrics.dead_zone);
    if (hit < 0) {
      result.cancelled = true;
      return Closed(st, result);
    }
    if (items[hit].enabled) {
      return choose(hit);
    }
    st.highlight = hit;
  }
  if (st.right_drag && ctx.right_released()) {
    st.right_drag = false;
    const int hit = RadialPick(st.centre, mouse, n, metrics.dead_zone);
    // Released in the middle: a plain right click, the menu stays.
    if (hit >= 0 && items[hit].enabled) {
      return choose(hit);
    }
  } else if (!fresh && it.right_pressed) {
    result.cancelled = true;
    return Closed(st, result);
  }

  // Declare the overlay.
  ctx.BeginLayer(LayerKind::kOverlay, id);
  NodeDesc desc;
  desc.id = area;
  desc.layout.width = SizeSpec::Grow();
  desc.layout.height = SizeSpec::Grow();
  desc.flags = kNodeInteractive;
  const int node = ctx.Open(desc);
  ctx.AddPainter(node,
                 [centre = st.centre,
                  copy = std::vector<RadialItem>(items.begin(), items.end()),
                  heading = std::string(title),
                  highlight = st.highlight,
                  metrics](PaintContext &pc, const Rect &) {
                   PaintRadial(pc, centre, copy, heading, highlight, metrics);
                 });
  ctx.Close();
  ctx.EndLayer();
  result.open = true;
  result.highlight = st.highlight;
  return result;
}

}  // namespace gloomy::ui
