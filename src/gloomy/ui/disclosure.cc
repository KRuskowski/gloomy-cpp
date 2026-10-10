#include "gloomy/ui/disclosure.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

using detail::InteractiveFill;

namespace {

// Width of a card's coloured left edge.
constexpr float kCardEdge = 3.0F;

auto EdgeColor(const Theme &t, Tone tone, Color accent) -> Color {
  if (accent.a > 0.0F) {
    return accent;
  }
  return tone == Tone::kNeutral ? t.colors.border : ToneColor(t, tone);
}

auto SmallText(const Theme &t, std::string_view text, Color color,
               bool bold = false) -> TextSpec {
  TextSpec spec;
  spec.text = text;
  spec.font = bold ? t.fonts.bold : t.fonts.sans;
  spec.size = t.metrics.small_font_size;
  spec.color = color;
  return spec;
}

}  // namespace

auto BeginCard(Context &ctx, std::string_view label, const CardOptions &opt)
    -> int {
  const Theme &t = ctx.theme();
  NodeDesc card;
  card.id = ctx.GetId(label);
  card.layout.mode = LayoutMode::kColumn;
  card.layout.width = SizeSpec::Grow();
  card.layout.padding = {
      opt.padding + kCardEdge, opt.padding, opt.padding, opt.padding};
  card.layout.gap = opt.gap;
  card.layout.align = Align::kStretch;
  card.style.fill = t.colors.surface_raised;
  card.style.border = t.colors.border;
  card.style.border_width = t.metrics.border_width;
  card.style.radii = gfx::CornerRadii::All(t.metrics.radius + 2);
  const int node = ctx.Open(card);
  const Color edge = EdgeColor(t, opt.status, opt.accent);
  ctx.AddPainter(node, [edge](PaintContext &pc, const Rect &r) {
    pc.list.FillRect({r.x0 + 1, r.y0 + 3, r.x0 + 1 + kCardEdge, r.y1 - 3},
                     edge,
                     gfx::CornerRadii::All(kCardEdge * 0.5F));
  });
  return node;
}

void EndCard(Context &ctx) {
  ctx.Close();
}

void KeyFacts(Context &ctx, std::string_view label,
              std::span<const KeyNumber> facts) {
  const Theme &t = ctx.theme();
  LayoutSpec row;
  row.mode = LayoutMode::kRow;
  row.width = SizeSpec::Grow();
  row.gap = 22;
  row.align = Align::kStart;
  BeginRow(ctx, row);
  ctx.PushId(label);
  for (std::size_t i = 0; i < facts.size(); ++i) {
    const KeyNumber &fact = facts[i];
    LayoutSpec cell;
    cell.gap = 1;
    cell.width = SizeSpec::Fit();
    BeginColumn(ctx, cell);
    ctx.Text(SmallText(t, fact.label, t.colors.text_muted));
    ExplainedLabel(
        ctx, "##fact" + std::to_string(i), fact.value, fact.tone, false);
    End(ctx);
  }
  ctx.PopId();
  End(ctx);
}

void Chip(Context &ctx, std::string_view text, Tone tone, Color color) {
  const Theme &t = ctx.theme();
  const Color base = color.a > 0.0F           ? color
                     : tone == Tone::kNeutral ? t.colors.text_muted
                                              : ToneColor(t, tone);
  const float h = t.metrics.small_font_size + 8;
  NodeDesc pill;
  pill.layout.mode = LayoutMode::kRow;
  pill.layout.height = SizeSpec::Fixed(h);
  pill.layout.padding = Padding::Xy(8, 0);
  pill.layout.align = Align::kCenter;
  pill.style.fill = base.WithAlpha(0.18F);
  pill.style.border = base.WithAlpha(0.55F);
  pill.style.border_width = t.metrics.border_width;
  pill.style.radii = gfx::CornerRadii::All(h * 0.5F);
  ctx.Open(pill);
  ctx.Text(SmallText(t, text, base, true));
  ctx.Close();
}

void SectionHeader(Context &ctx, std::string_view title,
                   std::string_view aside) {
  const Theme &t = ctx.theme();
  NodeDesc box;
  box.layout.mode = LayoutMode::kRow;
  box.layout.width = SizeSpec::Grow();
  box.layout.padding = {0, 6, 0, 4};
  box.layout.gap = t.metrics.spacing;
  box.layout.align = Align::kEnd;
  const int node = ctx.Open(box);
  TextSpec head = SmallText(t, title, t.colors.accent, true);
  head.size = t.metrics.small_font_size + 1;
  ctx.Text(head);
  Spacer(ctx);
  if (!aside.empty()) {
    ctx.Text(SmallText(t, aside, t.colors.text_muted));
  }
  ctx.Close();
  const Color line = t.colors.divider;
  ctx.AddPainter(node, [line](PaintContext &pc, const Rect &r) {
    pc.list.FillRect({r.x0, r.y1 - 1, r.x1, r.y1}, line);
  });
}

void BeginFormRow(Context &ctx, std::string_view label, float label_width) {
  const Theme &t = ctx.theme();
  LayoutSpec row;
  row.mode = LayoutMode::kRow;
  row.width = SizeSpec::Grow();
  row.gap = t.metrics.spacing;
  row.align = Align::kCenter;
  BeginRow(ctx, row);
  TextSpec text;
  text.text = DisplayText(label);
  text.font = t.fonts.sans;
  text.size = t.metrics.font_size;
  text.color = t.colors.text_muted;
  LayoutSpec cell;
  cell.width = SizeSpec::Fixed(label_width);
  ctx.Text(text, cell);
}

void EndFormRow(Context &ctx) {
  End(ctx);
}

auto BigButton(Context &ctx, std::string_view label,
               const BigButtonOptions &opt) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  bool activated = false;
  if (!opt.disabled) {
    activated = it.clicked;
    if (ctx.HasFocus(id) &&
        (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace))) {
      activated = true;
    }
  }
  const std::string_view caption =
      opt.disabled && !opt.reason.empty() ? opt.reason : opt.caption;
  NodeDesc desc;
  desc.id = id;
  desc.layout.mode = LayoutMode::kColumn;
  desc.layout.width = opt.width;
  desc.layout.height = SizeSpec::Fit(t.metrics.control_height + 12);
  desc.layout.padding = Padding::Xy(t.metrics.padding_x * 2.0F, 6);
  desc.layout.gap = 1;
  desc.layout.align = Align::kStart;
  desc.layout.justify = Justify::kCenter;
  desc.style.radii = gfx::CornerRadii::All(t.metrics.radius + 1);
  Color fg = t.colors.on_primary;
  Color sub = t.colors.on_primary.WithAlpha(0.8F);
  if (opt.disabled) {
    desc.style.fill = t.colors.surface;
    desc.style.border = t.colors.border;
    desc.style.border_width = t.metrics.border_width;
    fg = t.colors.text_disabled;
    sub = t.colors.text_muted;
    desc.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  } else if (!opt.emphasis) {
    desc.style.fill = InteractiveFill(t, t.colors.surface_alt, it);
    desc.style.border = t.colors.border;
    desc.style.border_width = t.metrics.border_width;
    fg = t.colors.text;
    sub = t.colors.text_muted;
    desc.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  } else {
    desc.style.fill = InteractiveFill(t, t.colors.primary, it);
    desc.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  }
  ctx.Open(desc);
  TextSpec text;
  text.text = DisplayText(label);
  text.font = t.fonts.bold;
  text.size = t.metrics.font_size + 2;
  text.color = fg;
  ctx.Text(text);
  if (!caption.empty()) {
    ctx.Text(SmallText(t, caption, sub));
  }
  ctx.Close();
  if (opt.disabled && !opt.reason.empty()) {
    Tooltip(ctx, id, opt.reason);
  }
  return activated;
}

auto EmptyState(Context &ctx, std::string_view label,
                const EmptyStateSpec &spec) -> bool {
  const Theme &t = ctx.theme();
  NodeDesc box;
  box.id = ctx.GetId(label);
  box.layout.mode = LayoutMode::kColumn;
  box.layout.width = SizeSpec::Grow();
  box.layout.padding = Padding::Xy(24, 28);
  box.layout.gap = 8;
  box.layout.align = Align::kCenter;
  ctx.Open(box);
  ctx.PushId(box.id);
  LabelOptions title;
  title.bold = true;
  title.align = TextAlign::kCenter;
  Label(ctx, spec.title, title);
  if (!spec.hint.empty()) {
    LabelOptions hint;
    hint.muted = true;
    hint.wrap = true;
    hint.align = TextAlign::kCenter;
    hint.width = SizeSpec::Fit(0.0F, 420.0F);
    Label(ctx, spec.hint, hint);
  }
  bool taken = false;
  if (!spec.action.empty()) {
    Spacer(ctx, 4.0F);
    taken = Button(ctx, spec.action, ButtonKind::kPrimary);
  }
  ctx.PopId();
  ctx.Close();
  (void)t;
  return taken;
}

auto SegmentedControl(Context &ctx, std::string_view label, int &selected,
                      std::span<const Segment> segments) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const int count = static_cast<int>(segments.size());
  const int before = selected;
  if (count > 0 && ctx.HasFocus(id)) {
    const int at = std::clamp(selected, 0, count - 1);
    if (ctx.ConsumeKey(Key::kLeft)) {
      selected = (at + count - 1) % count;
    }
    if (ctx.ConsumeKey(Key::kRight)) {
      selected = (selected < 0 || selected >= count) ? 0 : (at + 1) % count;
    }
  }
  NodeDesc bar;
  bar.id = id;
  bar.layout.mode = LayoutMode::kRow;
  bar.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  bar.layout.padding = Padding::All(2);
  bar.layout.gap = 2;
  bar.layout.align = Align::kStretch;
  bar.style.fill = t.colors.surface_alt;
  bar.style.border = t.colors.border;
  bar.style.border_width = t.metrics.border_width;
  bar.style.radii = gfx::CornerRadii::All(t.metrics.radius + 1);
  bar.flags = kNodeFocusable | kNodeFocusRing;
  ctx.Open(bar);
  ctx.PushId(id);
  for (int i = 0; i < count; ++i) {
    const Segment &seg = segments[static_cast<std::size_t>(i)];
    const Id seg_id = ctx.GetId(static_cast<std::uint64_t>(i));
    const Interaction it = ctx.Interact(seg_id);
    if (it.clicked) {
      selected = i;
      ctx.SetFocus(id, false);
    }
    const bool on = i == selected;
    const bool empty = seg.count == 0;
    NodeDesc cell;
    cell.id = seg_id;
    cell.layout.mode = LayoutMode::kRow;
    cell.layout.padding = Padding::Xy(t.metrics.padding_x + 2, 0);
    cell.layout.gap = 5;
    cell.layout.align = Align::kCenter;
    cell.style.radii = gfx::CornerRadii::All(t.metrics.radius);
    if (on) {
      cell.style.fill = t.colors.primary.WithAlpha(0.32F);
      cell.style.border = t.colors.primary;
      cell.style.border_width = t.metrics.border_width;
    } else if (it.hovered) {
      cell.style.fill = t.colors.hover;
    }
    cell.flags = kNodeInteractive;
    ctx.Open(cell);
    TextSpec text;
    text.text = seg.text;
    text.font = on ? t.fonts.bold : t.fonts.sans;
    text.size = t.metrics.font_size;
    text.color = on      ? t.colors.text
                 : empty ? t.colors.text_disabled
                         : t.colors.text_muted;
    ctx.Text(text);
    if (seg.count >= 0) {
      const std::string n = std::to_string(seg.count);
      ctx.Text(SmallText(t,
                         n,
                         on      ? t.colors.text
                         : empty ? t.colors.text_disabled
                                 : t.colors.accent));
    }
    ctx.Close();
    if (!seg.tooltip.empty()) {
      Tooltip(ctx, seg_id, seg.tooltip);
    }
  }
  ctx.PopId();
  ctx.Close();
  return selected != before;
}

auto TabMemory::Get(std::string_view group,
                    std::span<const std::string> labels) const -> int {
  const auto at = chosen_.find(group);
  if (at == chosen_.end()) {
    return 0;
  }
  const auto tab = std::find(labels.begin(), labels.end(), at->second);
  return tab == labels.end() ? 0 : static_cast<int>(tab - labels.begin());
}

void TabMemory::Set(std::string_view group, std::string_view tab) {
  const auto at = chosen_.find(group);
  if (at != chosen_.end()) {
    at->second = tab;
  } else {
    chosen_.emplace(std::string(group), std::string(tab));
  }
}

auto RememberedTabs(Context &ctx, std::string_view label, TabMemory &memory,
                    std::string_view group, std::span<const std::string> labels)
    -> int {
  int selected = memory.Get(group, labels);
  if (Tabs(ctx, label, selected, labels) && selected >= 0 &&
      selected < static_cast<int>(labels.size())) {
    memory.Set(group, labels[static_cast<std::size_t>(selected)]);
  }
  return selected;
}

}  // namespace gloomy::ui
