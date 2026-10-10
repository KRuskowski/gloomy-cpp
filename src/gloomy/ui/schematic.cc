#include "gloomy/ui/schematic.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gloomy/ui/hud.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

// Wall thickness inside the hull and the gap between rooms.
constexpr float kWall = 4.0F;
constexpr float kRoomGap = 3.0F;
// Gauge bar size.
constexpr float kGaugeWidth = 14.0F;
constexpr float kGaugeHeight = 44.0F;
constexpr float kCrewDot = 2.5F;

/// A room as the painter keeps it (owning its label).
struct PaintedRoom {
  std::string label;
  RoomKind kind = RoomKind::kOther;
  float level = 1.0F;
  float damage = 0.0F;
  float fill = -1.0F;
  Color color;
  int crew = 0;
  icon::IconId icon = icon::kNoIcon;
};

auto LevelColor(const PanelSkin &s, float level) -> Color {
  if (level >= 0.66F) {
    return s.good;
  }
  return level >= 0.33F ? s.warning : s.bad;
}

auto HullColor(const PanelSkin &s, float hull) -> Color {
  if (hull >= 0.6F) {
    return s.ink;
  }
  return hull >= 0.3F ? s.warning : s.bad;
}

void PaintHull(PaintContext &pc, const SchematicLayout &l, const PanelSkin &s,
               float hull, float shield, float engine_level) {
  gfx::DrawList &list = pc.list;
  // The shield bubble: rings brighter with the shield's strength.
  if (shield >= 0.0F) {
    const Color c = s.friendly;
    const float a = 0.12F + 0.55F * std::clamp(shield, 0.0F, 1.0F);
    for (int i = 0; i < 3; ++i) {
      StrokeEllipse(list,
                    l.bubble.Inset(static_cast<float>(i) * 2.0F),
                    i == 0 ? 1.5F : 1.0F,
                    c.WithAlpha(a / static_cast<float>(i + 1)),
                    72);
    }
  }
  const Color edge = HullColor(s, hull);
  const Color plate = s.paper_edge;
  // Engine glow behind the nozzles, by the engines' level.
  if (engine_level > 0.0F) {
    const Color glow = s.warning.WithAlpha(0.35F * engine_level);
    const Rect e = l.engines;
    list.Shadow({e.x0 - 4, e.y0 + 2, e.x0, e.y1 - 2}, glow, 8.0F, {-3, 0});
  }
  list.FillRect(l.engines, plate);
  list.StrokeRect(l.engines, edge, 2.0F);
  const float nh = l.engines.Height() / 3.0F;
  for (int i = 1; i < 3; ++i) {
    const float y = l.engines.y0 + nh * static_cast<float>(i);
    list.FillRect({l.engines.x0 + 2, y - 0.5F, l.engines.x1, y + 0.5F},
                  edge.WithAlpha(0.6F));
  }
  list.FillRect(l.bow_outer, plate, gfx::CornerRadii{0, 6, 6, 0});
  list.StrokeRect(l.bow_outer, edge, 2.0F, gfx::CornerRadii{0, 6, 6, 0});
  list.FillRect(l.bow_inner, plate);
  list.StrokeRect(l.bow_inner, edge, 2.0F);
  list.FillRect(l.body, plate, gfx::CornerRadii::All(3.0F));
  list.StrokeRect(l.body, edge, s.frame, gfx::CornerRadii::All(3.0F));
  // Seams where the steps meet, so the hull reads as one piece.
  list.FillRect({l.bow_inner.x0 - 2,
                 l.bow_inner.y0 + 2,
                 l.bow_inner.x0 + 2,
                 l.bow_inner.y1 - 2},
                plate);
  list.FillRect({l.bow_outer.x0 - 2,
                 l.bow_outer.y0 + 2,
                 l.bow_outer.x0 + 2,
                 l.bow_outer.y1 - 2},
                plate);
}

void PaintRoom(PaintContext &pc, const Rect &r, const PaintedRoom &room,
               const PanelSkin &s, bool hovered, float font) {
  gfx::DrawList &list = pc.list;
  list.FillRect(r, s.paper);
  if (room.fill > 0.0F) {
    const float f = std::clamp(room.fill, 0.0F, 1.0F);
    const Color c = room.color.a > 0.0F ? room.color : s.ink;
    list.FillRect({r.x0, r.y1 - r.Height() * f, r.x1, r.y1},
                  c.WithAlpha(0.45F));
  }
  if (room.damage > 0.0F) {
    const float d = std::clamp(room.damage, 0.0F, 1.0F);
    list.FillRect(r, s.bad.WithAlpha(0.12F + 0.25F * d));
    PaintHatch(list, r, s.bad.WithAlpha(0.25F + 0.4F * d), 6.0F, 1.5F);
  }
  // Floor bar: how well the room works.
  const float lv = std::clamp(room.level, 0.0F, 1.0F);
  const Rect bar{r.x0 + 3, r.y1 - 5, r.x1 - 3, r.y1 - 2};
  list.FillRect(bar, s.ink_muted.WithAlpha(0.25F));
  list.FillRect({bar.x0, bar.y0, bar.x0 + bar.Width() * lv, bar.y1},
                LevelColor(s, lv));
  list.StrokeRect(
      r, hovered ? s.text : s.ink_muted.WithAlpha(0.8F), hovered ? 2.0F : 1.5F);
  // Icon in the middle, label at the top left, as space allows.
  const bool roomy = r.Width() >= 34.0F && r.Height() >= 30.0F;
  if (pc.icons != nullptr && room.icon != icon::kNoIcon && roomy) {
    const float size = std::min({20.0F, r.Width() - 10, r.Height() - 16});
    const Color ic = lv > 0.0F ? s.ink : s.text_muted;
    pc.icons->Draw(list, room.icon, {r.Center().x, r.Center().y + 3}, size, ic);
  }
  if (r.Width() >= 30.0F && r.Height() >= 18.0F) {
    pc.text.DrawFitted(list,
                       room.label,
                       pc.theme.fonts.bold,
                       font,
                       {r.x0 + 4, r.y0 + 2, r.x1 - 3, r.y0 + 2 + font * 1.3F},
                       room.damage > 0.5F ? s.bad : s.text_muted);
  }
  // Crew as dots along the bottom right.
  const int shown = std::min(room.crew, 6);
  for (int i = 0; i < shown; ++i) {
    const Vec2 c{r.x1 - 6 - static_cast<float>(i) * (kCrewDot * 2 + 2),
                 r.y1 - 10};
    list.FillCircle(c, kCrewDot, s.friendly);
  }
}

}  // namespace

auto AutoDecks(std::size_t rooms) -> int {
  if (rooms <= 3) {
    return 1;
  }
  return rooms <= 8 ? 2 : 3;
}

auto LayoutSchematic(std::span<const SchematicRoom> rooms, const Rect &area,
                     int decks) -> SchematicLayout {
  SchematicLayout l;
  l.decks = std::max(1, decks);
  l.rooms.assign(rooms.size(), Rect{});
  const float my = std::max(8.0F, area.Height() * 0.1F);
  const float mx = 14.0F;
  const Rect hull{area.x0 + mx, area.y0 + my, area.x1 - mx, area.y1 - my};
  if (hull.Empty()) {
    return l;
  }
  const float h = hull.Height();
  const float engine_w = std::clamp(hull.Width() * 0.05F, 6.0F, 14.0F);
  const float bow_w = hull.Width() * 0.18F;
  l.body = {hull.x0 + engine_w, hull.y0, hull.x1 - bow_w, hull.y1};
  l.bow_inner = {l.body.x1,
                 hull.y0 + h * 0.14F,
                 l.body.x1 + bow_w * 0.62F,
                 hull.y1 - h * 0.14F};
  l.bow_outer = {
      l.bow_inner.x1, hull.y0 + h * 0.3F, hull.x1, hull.y1 - h * 0.3F};
  l.engines = {hull.x0, hull.y0 + h * 0.22F, l.body.x0, hull.y1 - h * 0.22F};
  l.bubble = area.Inset(2.0F);
  // The bridge in the bow.
  int bridge = -1;
  for (std::size_t i = 0; i < rooms.size(); ++i) {
    if (rooms[i].kind == RoomKind::kBridge && rooms[i].deck < 0) {
      bridge = static_cast<int>(i);
      l.rooms[i] = l.bow_inner.Inset(kWall);
      l.rooms[i].x1 += kWall;
      break;
    }
  }
  // Decks: explicit ones kept, the rest onto the emptiest.
  std::vector<std::vector<std::size_t>> on(static_cast<std::size_t>(l.decks));
  std::vector<float> cells(static_cast<std::size_t>(l.decks), 0.0F);
  for (std::size_t i = 0; i < rooms.size(); ++i) {
    if (static_cast<int>(i) == bridge) {
      continue;
    }
    int d = rooms[i].deck;
    if (d < 0) {
      d = static_cast<int>(std::ranges::min_element(cells) - cells.begin());
    }
    d = std::clamp(d, 0, l.decks - 1);
    on[static_cast<std::size_t>(d)].push_back(i);
    cells[static_cast<std::size_t>(d)] += std::max(0.1F, rooms[i].cells);
  }
  const Rect inner = l.body.Inset(kWall);
  const float deck_h =
      (inner.Height() - kRoomGap * static_cast<float>(l.decks - 1)) /
      static_cast<float>(l.decks);
  for (int d = 0; d < l.decks; ++d) {
    const auto &list = on[static_cast<std::size_t>(d)];
    if (list.empty()) {
      continue;
    }
    const float y0 = inner.y0 + static_cast<float>(d) * (deck_h + kRoomGap);
    const float usable =
        inner.Width() - kRoomGap * static_cast<float>(list.size() - 1);
    float x = inner.x0;
    for (const std::size_t i : list) {
      const float w = usable * std::max(0.1F, rooms[i].cells) /
                      cells[static_cast<std::size_t>(d)];
      l.rooms[i] = {x, y0, x + w, y0 + deck_h};
      x += w + kRoomGap;
    }
  }
  return l;
}

auto RoomAt(const SchematicLayout &layout, Vec2 p) -> int {
  for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
    if (!layout.rooms[i].Empty() && layout.rooms[i].Contains(p)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

auto ShipSchematic(Context &ctx, std::string_view label,
                   const SchematicSpec &spec) -> SchematicResult {
  const Theme &t = ctx.theme();
  const PanelSkin skin =
      spec.skin != nullptr ? *spec.skin : PanelSkin::FromTheme(t);
  SchematicResult result;
  const Id id = ctx.GetId(label);
  NodeDesc frame;
  frame.id = id;
  frame.layout.mode = LayoutMode::kColumn;
  frame.layout.width = SizeSpec::Grow();
  frame.layout.padding =
      spec.compact ? Padding::All(0) : Padding::All(skin.frame + 8);
  frame.layout.gap = 6;
  frame.layout.align = Align::kStretch;
  const int frame_node = ctx.Open(frame);
  if (!spec.compact) {
    const PanelSkin s = skin;
    ctx.AddPainter(
        frame_node,
        [s](PaintContext &pc, const Rect &r) {
          PaintFrame(pc.list, r, s, s.paper, s.ink_muted.WithAlpha(0.8F));
          if (s.scanlines > 0.0F) {
            Scanlines(
                pc.list, r.Inset(s.frame), 3.0F, s.ink.WithAlpha(s.scanlines));
          }
        },
        true);
  }
  ctx.PushId(id);
  if (!spec.compact && !spec.name.empty()) {
    LayoutSpec head;
    head.width = SizeSpec::Grow();
    head.gap = 8;
    head.align = Align::kEnd;
    BeginRow(ctx, head);
    TextSpec name;
    name.text = spec.name;
    name.font = t.fonts.bold;
    name.size = t.metrics.font_size + 1;
    name.color = skin.text;
    ctx.Text(name);
    TextSpec sub = name;
    sub.text = spec.subtitle;
    sub.font = t.fonts.sans;
    sub.size = t.metrics.small_font_size;
    sub.color = skin.text_muted;
    ctx.Text(sub);
    Spacer(ctx);
    if (!spec.crew.empty()) {
      TextSpec crew = sub;
      crew.text = spec.crew;
      crew.color = skin.friendly;
      ctx.Text(crew);
    }
    End(ctx);
  }
  // The hull area: one interactive node, rooms hit by the layout.
  const int decks = spec.decks > 0 ? spec.decks : AutoDecks(spec.rooms.size());
  NodeDesc area;
  area.id = ctx.GetId("##hull");
  area.layout.width = SizeSpec::Grow();
  area.layout.height = SizeSpec::Fixed(spec.height);
  area.flags = kNodeInteractive;
  const Interaction it = ctx.Interact(area.id);
  if (it.hovered && !it.rect.Empty()) {
    const SchematicLayout l = LayoutSchematic(spec.rooms, it.rect, decks);
    result.hovered = RoomAt(l, it.rect.Min() + it.local);
    if (it.clicked) {
      result.clicked = result.hovered;
    }
  }
  const int area_node = ctx.Open(area);
  ctx.Close();
  std::vector<SchematicRoom> rooms(spec.rooms.begin(), spec.rooms.end());
  std::vector<PaintedRoom> painted;
  painted.reserve(rooms.size());
  float engine_level = 0.0F;
  for (const SchematicRoom &r : spec.rooms) {
    painted.push_back({std::string(r.label),
                       r.kind,
                       r.level,
                       r.damage,
                       r.fill,
                       r.color,
                       r.crew,
                       r.icon});
    if (r.kind == RoomKind::kEngine) {
      engine_level = std::max(engine_level, r.level);
    }
  }
  for (SchematicRoom &r : rooms) {
    // The painter owns the labels; the layout needs none.
    r.label = {};
    r.why = {};
  }
  const float hull = spec.hull;
  const float shield = spec.shield;
  const int hovered = result.hovered;
  const float font = t.metrics.small_font_size - 1;
  ctx.AddPainter(
      area_node,
      [skin, rooms, painted, decks, hull, shield, engine_level, hovered, font](
          PaintContext &pc, const Rect &r) {
        const SchematicLayout l = LayoutSchematic(rooms, r, decks);
        PaintHull(pc, l, skin, hull, shield, engine_level);
        for (std::size_t i = 0; i < painted.size(); ++i) {
          if (!l.rooms[i].Empty()) {
            PaintRoom(pc,
                      l.rooms[i],
                      painted[i],
                      skin,
                      static_cast<int>(i) == hovered,
                      font);
          }
        }
      });
  if (result.hovered >= 0 &&
      !spec.rooms[static_cast<std::size_t>(result.hovered)].why.empty()) {
    Tooltip(
        ctx, area.id, spec.rooms[static_cast<std::size_t>(result.hovered)].why);
  }
  // Gauges: segmented bars, label and value beside each.
  if (!spec.gauges.empty()) {
    LayoutSpec row;
    row.width = SizeSpec::Grow();
    row.gap = spec.compact ? 10 : 14;
    row.align = Align::kEnd;
    BeginRow(ctx, row);
    for (std::size_t i = 0; i < spec.gauges.size(); ++i) {
      const SystemGauge &g = spec.gauges[i];
      NodeDesc cell;
      cell.id = ctx.GetId(static_cast<std::uint64_t>(i) + 1);
      cell.layout.mode = LayoutMode::kRow;
      cell.layout.gap = 5;
      cell.layout.align = Align::kEnd;
      cell.flags = kNodeInteractive;
      ctx.Open(cell);
      NodeDesc bar;
      bar.layout.width = SizeSpec::Fixed(kGaugeWidth);
      bar.layout.height =
          SizeSpec::Fixed(spec.compact ? kGaugeHeight * 0.6F : kGaugeHeight);
      const int bar_node = ctx.Open(bar);
      ctx.Close();
      const Color on = skin.ToneInk(g.tone);
      const int segments = g.segments;
      const int filled = g.filled;
      const int damaged = g.damaged;
      ctx.AddPainter(bar_node,
                     [skin, on, segments, filled, damaged](PaintContext &pc,
                                                           const Rect &r) {
                       PaintSegments(pc.list,
                                     r,
                                     segments,
                                     filled,
                                     damaged,
                                     on,
                                     skin.bad,
                                     skin.ink_muted.WithAlpha(0.5F));
                     });
      LayoutSpec col;
      col.gap = 1;
      BeginColumn(ctx, col);
      if (g.icon != icon::kNoIcon) {
        Icon(ctx, g.icon, 14.0F, on);
      }
      if (!spec.compact) {
        TextSpec name;
        name.text = g.label;
        name.font = t.fonts.bold;
        name.size = t.metrics.small_font_size - 1;
        name.color = skin.text_muted;
        ctx.Text(name);
      }
      TextSpec value;
      value.text = g.value;
      value.font = t.fonts.bold;
      value.size = t.metrics.small_font_size;
      value.color = g.tone == Tone::kNeutral ? skin.text : on;
      ctx.Text(value);
      End(ctx);
      ctx.Close();
      if (!g.why.empty()) {
        Tooltip(ctx, cell.id, g.why);
      }
    }
    End(ctx);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

}  // namespace gloomy::ui
