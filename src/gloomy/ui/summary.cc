#include "gloomy/ui/summary.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

namespace {

constexpr double kTipDelay = 0.5;

struct TipState {
  double since = -1.0;
};

// Shows `why` as a tooltip when node `id` is hovered long enough or has
// keyboard focus.
void WhyTooltip(Context &ctx, Id id, std::string_view why) {
  TipState &st = ctx.State<TipState>(HashCombine(id, 0x776879ULL));
  const bool hover = ctx.Interact(id).hovered && !ctx.mouse_down();
  const bool focus = ctx.HasFocus(id) && ctx.focus_visible();
  if (!hover && !focus) {
    st.since = -1.0;
    return;
  }
  if (st.since < 0.0) {
    st.since = ctx.time();
  }
  if (!focus && ctx.time() - st.since < kTipDelay) {
    return;
  }
  ctx.BeginLayer(LayerKind::kTooltip, HashCombine(id, 0x746970ULL));
  LabelOptions opt;
  opt.size = ctx.theme().metrics.small_font_size + 1;
  Label(ctx, why, opt);
  ctx.EndLayer();
}

}  // namespace

auto ToneColor(const Theme &theme, Tone tone) -> Color {
  switch (tone) {
    case Tone::kGood:
      return theme.colors.success;
    case Tone::kWarning:
      return theme.colors.warning;
    case Tone::kBad:
      return theme.colors.error;
    case Tone::kNeutral:
      break;
  }
  return theme.colors.text;
}

void ExplainedLabel(Context &ctx, std::string_view label,
                    const ExplainedValue &value, Tone tone, bool inline_note) {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const bool explained = !value.why.empty();
  NodeDesc box;
  box.id = id;
  box.layout.mode = LayoutMode::kColumn;
  box.flags = explained ? (kNodeInteractive | kNodeFocusable | kNodeFocusRing)
                        : kNodeNone;
  box.style.radii = gfx::CornerRadii::All(2);
  const int node = ctx.Open(box);
  TextSpec spec;
  spec.text = value.value;
  spec.font = t.fonts.sans;
  spec.size = t.metrics.font_size;
  spec.color = ToneColor(t, tone);
  const int text_node = ctx.Text(spec);
  if (explained && inline_note) {
    LabelOptions note;
    note.muted = true;
    note.size = t.metrics.small_font_size;
    note.wrap = true;
    Label(ctx, value.why, note);
  }
  ctx.Close();
  (void)node;
  if (explained) {
    // Dotted underline: the value has a reason.
    const Color dots = t.colors.text_muted;
    ctx.AddPainter(text_node, [=](PaintContext &pc, const Rect &r) {
      const float y = std::round(r.y1) - 1.0F;
      for (float x = r.x0; x < r.x1; x += 4.0F) {
        pc.list.FillRect({x, y, std::min(x + 2.0F, r.x1), y + 1.0F}, dots);
      }
    });
    if (!inline_note) {
      WhyTooltip(ctx, id, value.why);
    }
  }
}

void Sparkline(Context &ctx, std::span<const float> values, SizeSpec width,
               float height, Tone tone) {
  const Theme &t = ctx.theme();
  NodeDesc desc;
  desc.layout.width = width;
  desc.layout.height = SizeSpec::Fixed(height);
  const int node = ctx.Open(desc);
  ctx.Close();
  if (values.size() < 2) {
    return;
  }
  const std::vector<float> v(values.begin(), values.end());
  const Color line =
      tone == Tone::kNeutral ? t.colors.accent : ToneColor(t, tone);
  ctx.AddPainter(node, [v, line](PaintContext &pc, const Rect &r) {
    const auto [lo_it, hi_it] = std::minmax_element(v.begin(), v.end());
    const float lo = *lo_it;
    const float span = std::max(1e-6F, *hi_it - lo);
    auto point = [&](std::size_t i) -> Vec2 {
      const float x = r.x0 + 1 +
                      (r.Width() - 2) * static_cast<float>(i) /
                          static_cast<float>(v.size() - 1);
      const float y = r.y1 - 2 - (r.Height() - 4) * (v[i] - lo) / span;
      return {x, y};
    };
    for (std::size_t i = 1; i < v.size(); ++i) {
      pc.list.Line(point(i - 1), point(i), 1.5F, line);
    }
    pc.list.FillCircle(point(v.size() - 1), 2.5F, line);
  });
}

auto SummaryCard(Context &ctx, std::string_view label, const SummarySpec &spec)
    -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  bool drill = it.clicked;
  if (ctx.HasFocus(id) &&
      (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace))) {
    drill = true;
  }
  NodeDesc card;
  card.id = id;
  card.layout.mode = LayoutMode::kColumn;
  card.layout.width = SizeSpec::Grow();
  card.layout.padding = {14, 10, 12, 10};
  card.layout.gap = 4;
  card.style.fill = detail::InteractiveFill(t, t.colors.surface, it);
  card.style.material = t.MaterialOf(Surface::kCard);
  card.style.border = t.colors.border;
  card.style.border_width = t.metrics.border_width;
  card.style.radii = gfx::CornerRadii::All(t.metrics.radius + 2);
  card.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const int node = ctx.Open(card);
  const Color edge = spec.status == Tone::kNeutral ? t.colors.border
                                                   : ToneColor(t, spec.status);
  ctx.AddPainter(
      node,
      [edge](PaintContext &pc, const Rect &r) {
        pc.list.FillRect({r.x0 + 1, r.y0 + 6, r.x0 + 4, r.y1 - 6},
                         edge,
                         gfx::CornerRadii::All(1.5F));
      },
      true);
  LabelOptions title;
  title.muted = true;
  title.size = t.metrics.small_font_size + 1;
  Label(ctx, spec.title, title);
  LayoutSpec row;
  row.gap = 10;
  row.align = Align::kCenter;
  row.width = SizeSpec::Grow();
  BeginRow(ctx, row);
  {
    TextSpec value;
    value.text = spec.value.value;
    value.font = t.fonts.bold;
    value.size = t.metrics.heading_font_size + 4;
    value.color = t.colors.text;
    ctx.Text(value);
  }
  Spacer(ctx);
  if (!spec.sparkline.empty()) {
    Sparkline(ctx, spec.sparkline, SizeSpec::Fixed(84), 26, spec.delta_tone);
  }
  End(ctx);
  if (!spec.delta.empty() || !spec.value.why.empty()) {
    LayoutSpec foot;
    foot.gap = 8;
    BeginRow(ctx, foot);
    if (!spec.delta.empty()) {
      LabelOptions delta;
      delta.size = t.metrics.small_font_size + 1;
      delta.color = ToneColor(t, spec.delta_tone);
      Label(ctx, spec.delta, delta);
    }
    if (!spec.value.why.empty()) {
      LabelOptions why;
      why.muted = true;
      why.size = t.metrics.small_font_size + 1;
      Label(ctx, spec.value.why, why);
    }
    End(ctx);
  }
  ctx.Close();
  return drill;
}

auto AnswerLine(Context &ctx, std::string_view label, std::string_view answer,
                std::string_view reason, std::string_view action) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  NodeDesc box;
  box.id = id;
  box.layout.mode = LayoutMode::kRow;
  box.layout.width = SizeSpec::Grow();
  box.layout.padding = Padding::Xy(12, 8);
  box.layout.gap = 12;
  box.layout.align = Align::kCenter;
  box.style.fill = t.colors.surface_raised;
  box.style.border = t.colors.border;
  box.style.border_width = t.metrics.border_width;
  box.style.radii = gfx::CornerRadii::All(t.metrics.radius + 2);
  ctx.Open(box);
  LayoutSpec text_col;
  text_col.width = SizeSpec::Grow();
  text_col.gap = 2;
  BeginColumn(ctx, text_col);
  {
    LabelOptions a;
    a.bold = true;
    a.size = t.metrics.font_size + 2;
    Label(ctx, answer, a);
    LabelOptions r;
    r.muted = true;
    Label(ctx, reason, r);
  }
  End(ctx);
  ctx.PushId(id);
  const bool act = Button(ctx, action, ButtonKind::kPrimary);
  ctx.PopId();
  ctx.Close();
  return act;
}

auto SummaryPanel(Context &ctx, std::string_view label,
                  const data::TableView &view, const SummaryPanelSpec &spec)
    -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  bool drill = false;
  if (ctx.HasFocus(id) && ctx.ConsumeKey(Key::kEnter)) {
    drill = true;
  }
  NodeDesc panel;
  panel.id = id;
  panel.layout.mode = LayoutMode::kColumn;
  panel.layout.width = SizeSpec::Grow();
  panel.layout.padding = Padding::All(12);
  panel.layout.gap = 8;
  panel.layout.align = Align::kStretch;
  panel.style.fill = t.colors.surface;
  panel.style.material = t.MaterialOf(Surface::kCard);
  panel.style.border = t.colors.border;
  panel.style.border_width = t.metrics.border_width;
  panel.style.radii = gfx::CornerRadii::All(t.metrics.radius + 2);
  panel.flags = kNodeFocusable | kNodeFocusRing;
  ctx.Open(panel);
  ctx.PushId(id);
  Heading(ctx, spec.title);
  if (!spec.numbers.empty()) {
    LayoutSpec numbers;
    numbers.gap = 20;
    BeginRow(ctx, numbers);
    for (std::size_t i = 0; i < spec.numbers.size(); ++i) {
      const KeyNumber &k = spec.numbers[i];
      LayoutSpec col;
      col.gap = 2;
      BeginColumn(ctx, col);
      LabelOptions name;
      name.muted = true;
      name.size = t.metrics.small_font_size + 1;
      Label(ctx, k.label, name);
      ExplainedLabel(ctx, "##k" + std::to_string(i), k.value, k.tone);
      End(ctx);
    }
    End(ctx);
  }
  // Compact table of the first rows.
  const data::TableSource &src = view.source();
  const auto rows = view.rows();
  const auto infos = src.Columns();
  LayoutSpec table;
  table.gap = 0;
  table.align = Align::kStretch;
  BeginColumn(ctx, table);
  float weights = 0.0F;
  for (const int col : spec.columns) {
    weights += infos[col].IsNumeric() ? 1.0F : 2.0F;
  }
  // The gaps take a little; leave them out of the shares.
  const float usable = 0.97F;
  auto Share = [&](const data::ColumnInfo &info) {
    return usable * (info.IsNumeric() ? 1.0F : 2.0F) / weights;
  };
  auto row_of_cells = [&](auto &&cell_text, bool header, bool stripe) {
    NodeDesc row;
    row.layout.mode = LayoutMode::kRow;
    row.layout.height = SizeSpec::Fixed(t.metrics.row_height);
    row.layout.padding = Padding::Xy(1, 0);
    row.layout.gap = 0;
    row.layout.align = Align::kCenter;
    if (stripe) {
      row.style.fill = t.colors.surface_alt;
    }
    ctx.Open(row);
    for (std::size_t c = 0; c < spec.columns.size(); ++c) {
      const data::ColumnInfo &info = infos[spec.columns[c]];
      TextSpec text;
      const std::string value = cell_text(spec.columns[c]);
      text.text = value;
      text.font = header ? t.fonts.bold : t.fonts.sans;
      text.size = header ? t.metrics.small_font_size + 1 : t.metrics.font_size;
      text.color = header ? t.colors.text_muted : t.colors.text;
      text.align = info.IsNumeric() ? TextAlign::kRight : TextAlign::kLeft;
      // Fixed shares keep columns aligned across rows; text columns get
      // twice the room of numbers.
      LayoutSpec cell;
      cell.width = SizeSpec::Percent(Share(info));
      cell.padding = Padding::Xy(5, 0);
      ctx.Text(text, cell);
    }
    ctx.Close();
  };
  row_of_cells([&](int col) { return infos[col].title; }, true, false);
  std::string cell;
  for (int r = 0; r < spec.rows && r < static_cast<int>(rows.size()); ++r) {
    const data::RowIndex row = rows[static_cast<std::size_t>(r)];
    row_of_cells(
        [&](int col) {
          src.FormatCell(row, col, cell);
          return cell;
        },
        false,
        r % 2 == 1);
  }
  End(ctx);
  LayoutSpec foot;
  foot.align = Align::kCenter;
  foot.width = SizeSpec::Grow();
  BeginRow(ctx, foot);
  LabelOptions more;
  more.muted = true;
  more.size = t.metrics.small_font_size + 1;
  const std::int64_t rest = static_cast<std::int64_t>(rows.size()) - spec.rows;
  std::string more_text;
  if (view.busy()) {
    more_text = "Updating\xE2\x80\xA6";
  } else if (rest > 0) {
    data::FormatInt(rest, true, more_text);
    more_text += " more";
  }
  Label(ctx, more_text, more);
  Spacer(ctx);
  if (Button(ctx, spec.action, ButtonKind::kFlat)) {
    drill = true;
  }
  End(ctx);
  ctx.PopId();
  ctx.Close();
  return drill;
}

}  // namespace gloomy::ui
