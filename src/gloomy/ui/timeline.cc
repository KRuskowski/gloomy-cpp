#include "gloomy/ui/timeline.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>

#include "gloomy/ui/chart.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

constexpr float kSide = 12.0F;
constexpr float kLaneHeight = 30.0F;
constexpr float kAxisZone = 18.0F;
constexpr float kLabelGap = 10.0F;
constexpr float kDotRadius = 4.5F;
constexpr float kHoverPx = 8.0F;
constexpr float kDash = 6.0F;

auto MarkColor(const Theme &t, const TimelineEvent &e) -> Color {
  if (e.color.a > 0.0F) {
    return e.color;
  }
  switch (e.mark) {
    case TimelineMark::kDone:
      return t.colors.primary;
    case TimelineMark::kExpected:
      return t.colors.text_muted;
    case TimelineMark::kDeadline:
      return t.colors.warning;
  }
  return t.colors.text;
}

auto TimeText(const TimelineOptions &o, double v, double step) -> std::string {
  return o.time_format ? o.time_format(v) : TickText(v, step);
}

// Screen geometry of a timeline in a rectangle.
struct Geometry {
  double t0 = 0.0;
  double t1 = 1.0;
  float x0 = 0.0F;
  float x1 = 1.0F;
  float axis_y = 0.0F;

  auto X(double t) const -> float {
    return x0 + static_cast<float>((t - t0) / (t1 - t0)) * (x1 - x0);
  }
};

auto MakeGeometry(const Rect &r, std::span<const TimelineEvent> events,
                  double now) -> Geometry {
  Geometry g;
  std::tie(g.t0, g.t1) = TimelineRange(events, now);
  g.x0 = r.x0 + kSide;
  g.x1 = std::max(g.x0 + 1.0F, r.x1 - kSide);
  g.axis_y = r.y1 - kAxisZone * 0.5F;
  return g;
}

void DashedLine(gfx::DrawList &list, Vec2 a, Vec2 b, float width,
                const Color &c) {
  const float length = b.x - a.x;
  for (float x = 0.0F; x < length; x += 2.0F * kDash) {
    list.Line(
        {a.x + x, a.y}, {a.x + std::min(length, x + kDash), b.y}, width, c);
  }
}

void PaintTimeline(PaintContext &pc, const Rect &r,
                   const std::vector<TimelineEvent> &events,
                   const TimelineOptions &options, int hovered) {
  const Theme &t = pc.theme;
  const Palette &c = t.colors;
  gfx::DrawList &list = pc.list;
  const Geometry g = MakeGeometry(r, events, options.now);
  const float small = t.metrics.small_font_size;
  const double step = NiceStep(g.t1 - g.t0, 4);

  // Axis: solid up to now, dashed after.
  const bool has_now = std::isfinite(options.now);
  const float split = has_now ? std::clamp(g.X(options.now), g.x0, g.x1) : g.x1;
  list.Line({g.x0, g.axis_y}, {split, g.axis_y}, 2.0F, c.text_muted);
  DashedLine(list, {split, g.axis_y}, {g.x1, g.axis_y}, 2.0F, c.divider);

  // Labels: the event's text and its time, in lanes above the axis.
  std::vector<double> times;
  std::vector<float> widths;
  std::vector<std::string> stamps;
  for (const TimelineEvent &e : events) {
    times.push_back(e.time);
    stamps.push_back(TimeText(options, e.time, step));
    widths.push_back(
        std::max(pc.text.Measure(e.label, t.fonts.sans, small + 1),
                 pc.text.Measure(stamps.back(), t.fonts.sans, small)));
  }
  const std::vector<TimelineSlot> slots = LayoutTimeline(times,
                                                         widths,
                                                         g.t0,
                                                         g.t1,
                                                         g.x0 - kSide,
                                                         g.x1 + kSide,
                                                         options.lanes,
                                                         kLabelGap);
  for (std::size_t i = 0; i < events.size(); ++i) {
    const TimelineSlot &s = slots[i];
    if (s.lane < 0) {
      continue;
    }
    const TimelineEvent &e = events[i];
    const float bottom =
        g.axis_y - kAxisZone * 0.5F - static_cast<float>(s.lane) * kLaneHeight;
    const float top = bottom - kLaneHeight + 2.0F;
    if (s.lane > 0) {
      list.Line({s.x, bottom}, {s.x, g.axis_y - kDotRadius}, 1.0F, c.divider);
    }
    const bool expected = e.mark == TimelineMark::kExpected;
    const bool hot = static_cast<int>(i) == hovered;
    pc.text.DrawFitted(list,
                       e.label,
                       t.fonts.sans,
                       small + 1,
                       {s.x0, top, s.x1, top + 14},
                       hot ? c.text : (expected ? c.text_muted : c.text),
                       text::HAlign::kCenter);
    pc.text.DrawFitted(list,
                       stamps[i],
                       t.fonts.sans,
                       small,
                       {s.x0, top + 14, s.x1, bottom},
                       c.text_muted,
                       text::HAlign::kCenter);
  }

  // Now, then the markers on top.
  if (has_now) {
    list.Line(
        {split, g.axis_y - 8.0F}, {split, g.axis_y + 8.0F}, 2.0F, c.accent);
    pc.text.DrawFitted(list,
                       options.now_label,
                       t.fonts.sans,
                       small,
                       {split - 40, g.axis_y + 2, split + 40, r.y1 + 6},
                       c.accent,
                       text::HAlign::kCenter);
  }
  for (std::size_t i = 0; i < events.size(); ++i) {
    const TimelineEvent &e = events[i];
    const Vec2 p{slots[i].x, g.axis_y};
    const Color color = MarkColor(t, e);
    const float grow = static_cast<int>(i) == hovered ? 1.5F : 0.0F;
    switch (e.mark) {
      case TimelineMark::kDone:
        list.FillCircle(p, kDotRadius + grow, color);
        break;
      case TimelineMark::kExpected:
        list.FillCircle(p, kDotRadius + grow, c.surface);
        list.StrokeCircle(p, kDotRadius + grow, color, 1.5F);
        break;
      case TimelineMark::kDeadline:
        list.FillRect({p.x - 1.5F - grow,
                       p.y - 8.0F - grow,
                       p.x + 1.5F + grow,
                       p.y + 8.0F + grow},
                      color);
        break;
    }
  }
}

}  // namespace

auto TimelineRange(std::span<const TimelineEvent> events, double now)
    -> std::pair<double, double> {
  double lo = std::numeric_limits<double>::infinity();
  double hi = -std::numeric_limits<double>::infinity();
  for (const TimelineEvent &e : events) {
    lo = std::min(lo, e.time);
    hi = std::max(hi, e.time);
  }
  if (std::isfinite(now)) {
    lo = std::min(lo, now);
    hi = std::max(hi, now);
  }
  if (!(lo <= hi)) {
    return {0.0, 1.0};
  }
  if (hi - lo < 1e-12) {
    return {lo - 1.0, hi + 1.0};
  }
  const double pad = (hi - lo) / 20.0;
  return {lo - pad, hi + pad};
}

auto LayoutTimeline(std::span<const double> times,
                    std::span<const float> widths, double t0, double t1,
                    float x0, float x1, int lanes, float gap)
    -> std::vector<TimelineSlot> {
  std::vector<TimelineSlot> slots(times.size());
  std::vector<std::size_t> order(times.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(
      order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return times[a] < times[b];
      });
  const double span = t1 - t0 > 0.0 ? t1 - t0 : 1.0;
  // Right edge of the last label in each lane.
  std::vector<float> ends(static_cast<std::size_t>(std::max(0, lanes)),
                          -std::numeric_limits<float>::infinity());
  for (const std::size_t i : order) {
    TimelineSlot &s = slots[i];
    s.x = x0 + static_cast<float>((times[i] - t0) / span) * (x1 - x0);
    const float w = i < widths.size() ? widths[i] : 0.0F;
    s.x0 = std::clamp(s.x - w * 0.5F, x0, std::max(x0, x1 - w));
    s.x1 = s.x0 + w;
    for (std::size_t lane = 0; lane < ends.size(); ++lane) {
      if (s.x0 >= ends[lane] + gap) {
        s.lane = static_cast<int>(lane);
        ends[lane] = s.x1;
        break;
      }
    }
  }
  return slots;
}

void Timeline(Context &ctx, std::string_view label,
              std::span<const TimelineEvent> events,
              const TimelineOptions &options) {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  const Geometry g = MakeGeometry(it.rect, events, options.now);
  // The marker nearest the pointer, if close enough.
  int hovered = -1;
  if (it.hovered) {
    float best = kHoverPx;
    for (std::size_t i = 0; i < events.size(); ++i) {
      const float d = std::fabs(g.X(events[i].time) - ctx.mouse().x);
      if (d <= best) {
        best = d;
        hovered = static_cast<int>(i);
      }
    }
  }
  NodeDesc node;
  node.id = id;
  node.layout.width = options.width;
  node.layout.height = SizeSpec::Fixed(
      static_cast<float>(std::max(1, options.lanes)) * kLaneHeight + kAxisZone +
      6.0F);
  node.flags = kNodeInteractive;
  const int index = ctx.Open(node);
  ctx.Close();
  ctx.AddPainter(
      index,
      [events = std::vector<TimelineEvent>(events.begin(), events.end()),
       options,
       hovered](PaintContext &pc, const Rect &r) {
        PaintTimeline(pc, r, events, options, hovered);
      });
  if (hovered >= 0) {
    const TimelineEvent &e = events[static_cast<std::size_t>(hovered)];
    ctx.BeginLayer(LayerKind::kTooltip, HashCombine(id, 0x746cULL));
    LabelOptions head;
    head.bold = true;
    head.size = t.metrics.small_font_size + 1;
    Label(ctx, e.label, head);
    LabelOptions body;
    body.size = t.metrics.small_font_size + 1;
    body.muted = true;
    const double step = NiceStep(g.t1 - g.t0, 4);
    Label(ctx,
          TimeText(options, e.time, step) +
              (e.mark == TimelineMark::kExpected ? " (expected)" : ""),
          body);
    if (!e.detail.empty()) {
      Label(ctx, e.detail, body);
    }
    ctx.EndLayer();
  }
}

}  // namespace gloomy::ui
