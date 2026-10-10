#include "gloomy/ui/feed.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "gloomy/ui/list_rows.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

namespace {

// Room for the fresh dot before the age.
constexpr float kDotWidth = 10.0F;

/// The fresh dot: a small filled circle in the accent colour.
void FreshDot(Context &ctx, bool fresh) {
  const Theme &t = ctx.theme();
  NodeDesc box;
  box.layout.width = SizeSpec::Fixed(kDotWidth * CellScale(t));
  box.layout.height = SizeSpec::Fixed(t.metrics.font_size);
  const int node = ctx.Open(box);
  if (fresh) {
    const Color color = t.colors.accent;
    ctx.AddPainter(node, [color](PaintContext &pc, const Rect &r) {
      const float d = std::min(r.Width(), r.Height()) * 0.5F;
      const float cx = r.x0 + r.Width() * 0.5F;
      const float cy = r.y0 + r.Height() * 0.5F;
      pc.list.FillRect(
          {cx - d * 0.5F, cy - d * 0.5F, cx + d * 0.5F, cy + d * 0.5F},
          color,
          gfx::CornerRadii::All(d * 0.5F));
    });
  }
  ctx.Close();
}

}  // namespace

auto PriorityTone(Priority priority) -> Tone {
  switch (priority) {
    case Priority::kUrgent:
      return Tone::kBad;
    case Priority::kHigh:
      return Tone::kWarning;
    case Priority::kNormal:
    case Priority::kLow:
      break;
  }
  return Tone::kNeutral;
}

auto FeedOrder(std::span<const Notification> items, bool by_priority)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> order(items.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  if (by_priority) {
    std::ranges::stable_sort(order, [items](std::size_t a, std::size_t b) {
      return items[a].priority > items[b].priority;
    });
  }
  return order;
}

auto NotificationFeed(Context &ctx, std::string_view label,
                      std::span<const Notification> items,
                      const FeedOptions &options) -> FeedResult {
  const Theme &t = ctx.theme();
  FeedResult result;
  const Id id = ctx.GetId(label);
  NodeDesc column;
  column.id = id;
  column.layout.mode = LayoutMode::kColumn;
  column.layout.width = SizeSpec::Grow();
  column.layout.gap = 1;
  column.layout.align = Align::kStretch;
  ctx.Open(column);
  ctx.PushId(id);
  if (items.empty() && !options.empty.empty()) {
    LabelOptions e;
    e.muted = true;
    e.wrap = true;
    e.width = SizeSpec::Grow();
    Label(ctx, options.empty, e);
  }
  const std::vector<std::size_t> order = FeedOrder(items, options.by_priority);
  const std::size_t shown = options.max_shown == 0
                                ? order.size()
                                : std::min(order.size(), options.max_shown);
  for (std::size_t k = 0; k < shown; ++k) {
    const std::size_t i = order[k];
    const Notification &n = items[i];
    const Tone tone = PriorityTone(n.priority);
    // Columns: dot, age, source, text; the actions after them.
    std::vector<CellColumn> columns;
    std::vector<Cell> cells;
    if (options.age_width > 0.0F) {
      columns.push_back({{}, options.age_width, CellAlign::kLeft, {}});
      cells.push_back({n.age, Tone::kNeutral, {}, false, true, {}});
    }
    if (options.source_width > 0.0F) {
      columns.push_back({{}, options.source_width, CellAlign::kLeft, {}});
      cells.push_back({n.source, Tone::kNeutral, {}, true, true, {}});
    }
    columns.push_back({});
    cells.push_back(
        {n.text, tone, {}, n.priority >= Priority::kUrgent, false, n.why});
    std::vector<RowAction> actions;
    actions.reserve(n.actions.size());
    for (std::size_t a = 0; a < n.actions.size(); ++a) {
      actions.push_back({n.actions[a], {}, ButtonKind::kFlat});
    }
    ctx.PushId(static_cast<std::uint64_t>(i) + 1);
    LayoutSpec line;
    line.width = SizeSpec::Grow();
    line.align = Align::kCenter;
    BeginRow(ctx, line);
    FreshDot(ctx, n.fresh);
    CellRowOptions o;
    o.status = tone == Tone::kNeutral && n.priority == Priority::kLow
                   ? Tone::kNeutral
                   : tone;
    o.actions = actions;
    const CellRowResult r = CellRow(ctx, "##item", columns, cells, o);
    End(ctx);
    ctx.PopId();
    if (r.action >= 0) {
      result.item = static_cast<int>(i);
      result.action = r.action;
    } else if (r.activated) {
      result.item = static_cast<int>(i);
      result.action = n.actions.empty() ? -1 : 0;
    } else if (r.clicked) {
      result.item = static_cast<int>(i);
    }
  }
  if (order.size() > shown) {
    const std::string more =
        "+" + std::to_string(order.size() - shown) + " more##more";
    LayoutSpec foot;
    foot.width = SizeSpec::Grow();
    BeginRow(ctx, foot);
    Spacer(ctx, kDotWidth * CellScale(t));
    if (Button(ctx, more, ButtonKind::kFlat)) {
      result.more = true;
    }
    End(ctx);
  }
  ctx.PopId();
  ctx.Close();
  return result;
}

}  // namespace gloomy::ui
