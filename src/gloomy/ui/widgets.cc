#include "gloomy/ui/widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "gloomy/core/utf8.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

using detail::InteractiveFill;
using detail::ScrollBar;
using detail::Tint;

namespace {

// Pixels scrolled per wheel line.
constexpr float kWheelLinePixels = 40.0F;
// Seconds before a tooltip appears.
constexpr double kTooltipDelay = 0.5;
// Seconds after which a type-to-jump prefix starts over.
constexpr double kPrefixTimeout = 1.0;

auto TextOf(const Context &ctx, std::string_view text, const Color &color,
            bool bold = false) -> TextSpec {
  const Theme &t = ctx.theme();
  TextSpec spec;
  spec.text = text;
  spec.font = bold ? t.fonts.bold : t.fonts.sans;
  spec.size = t.metrics.font_size;
  spec.color = color;
  return spec;
}

// Draws a small downward chevron centred at `c`.
void Chevron(gfx::DrawList &list, Vec2 c, const Color &color) {
  list.Line({c.x - 4, c.y - 2}, {c.x, c.y + 2}, 1.5F, color);
  list.Line({c.x, c.y + 2}, {c.x + 4, c.y - 2}, 1.5F, color);
}

// Next code point boundary after `pos`.
auto NextBoundary(std::string_view s, std::size_t pos) -> std::size_t {
  if (pos >= s.size()) {
    return s.size();
  }
  DecodeUtf8(s, pos);
  return pos;
}

auto IsWordByte(char c) -> bool {
  return c != ' ' && c != '\t' && c != '.' && c != ',' && c != '-' &&
         c != '/' && c != ':' && c != ';';
}

auto NextWord(std::string_view s, std::size_t pos) -> std::size_t {
  while (pos < s.size() && !IsWordByte(s[pos])) {
    ++pos;
  }
  while (pos < s.size() && IsWordByte(s[pos])) {
    ++pos;
  }
  return pos;
}

auto PrevWord(std::string_view s, std::size_t pos) -> std::size_t {
  while (pos > 0 && !IsWordByte(s[pos - 1])) {
    --pos;
  }
  while (pos > 0 && IsWordByte(s[pos - 1])) {
    --pos;
  }
  return pos;
}

// Case-insensitive prefix test on folded text.
auto StartsWithFolded(std::string_view text, std::string_view folded_prefix)
    -> bool {
  std::string folded;
  FoldCaseUtf8(text.substr(0, std::min(text.size(), folded_prefix.size())),
               folded);
  return folded == folded_prefix;
}

}  // namespace

auto DisplayText(std::string_view label) -> std::string_view {
  const std::size_t hidden = label.find("##");
  return hidden == std::string_view::npos ? label : label.substr(0, hidden);
}

// Containers.

auto BeginRow(Context &ctx, LayoutSpec layout) -> int {
  layout.mode = LayoutMode::kRow;
  NodeDesc desc;
  desc.layout = layout;
  return ctx.Open(desc);
}

auto BeginColumn(Context &ctx, LayoutSpec layout) -> int {
  layout.mode = LayoutMode::kColumn;
  NodeDesc desc;
  desc.layout = layout;
  return ctx.Open(desc);
}

auto BeginPanel(Context &ctx, LayoutSpec layout) -> int {
  const Theme &t = ctx.theme();
  layout.mode = LayoutMode::kColumn;
  if (layout.padding.left == 0 && layout.padding.top == 0) {
    layout.padding = Padding::All(t.metrics.spacing);
  }
  if (layout.gap == 0) {
    layout.gap = t.metrics.spacing;
  }
  NodeDesc desc;
  desc.layout = layout;
  desc.style.fill = t.colors.surface;
  desc.style.material = t.MaterialOf(Surface::kPanel);
  desc.style.border = t.colors.border;
  desc.style.border_width = t.metrics.border_width;
  desc.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  return ctx.Open(desc);
}

void End(Context &ctx) {
  ctx.Close();
}

void Spacer(Context &ctx, float size) {
  NodeDesc desc;
  if (size > 0.0F) {
    desc.layout.width = SizeSpec::Fixed(size);
    desc.layout.height = SizeSpec::Fixed(size);
  } else {
    desc.layout.width = SizeSpec::Grow();
    desc.layout.height = SizeSpec::Grow();
  }
  ctx.Open(desc);
  ctx.Close();
}

void Icon(Context &ctx, icon::IconId icon, float size, Color color) {
  NodeDesc desc;
  desc.layout.width = SizeSpec::Fixed(size);
  desc.layout.height = SizeSpec::Fixed(size);
  const int node = ctx.Open(desc);
  ctx.Close();
  if (icon == icon::kNoIcon) {
    return;
  }
  if (color.a <= 0.0F) {
    color = ctx.theme().colors.text;
  }
  ctx.AddPainter(node, [icon, color](PaintContext &pc, const Rect &r) {
    if (pc.icons != nullptr) {
      pc.icons->DrawInRect(pc.list, icon, r, color);
    }
  });
}

void Separator(Context &ctx, bool vertical) {
  const Theme &t = ctx.theme();
  NodeDesc desc;
  desc.layout.width = vertical ? SizeSpec::Fixed(1) : SizeSpec::Grow();
  desc.layout.height = vertical ? SizeSpec::Grow() : SizeSpec::Fixed(1);
  desc.style.fill = t.colors.divider;
  ctx.Open(desc);
  ctx.Close();
}

// Text.

void Label(Context &ctx, std::string_view text, const LabelOptions &opt) {
  const Theme &t = ctx.theme();
  TextSpec spec;
  spec.text = text;
  spec.font =
      opt.mono ? t.fonts.mono : (opt.bold ? t.fonts.bold : t.fonts.sans);
  spec.size = opt.size > 0.0F ? opt.size : t.metrics.font_size;
  spec.color = opt.color.a > 0.0F
                   ? opt.color
                   : (opt.muted ? t.colors.text_muted : t.colors.text);
  spec.align = opt.align;
  spec.wrap = opt.wrap;
  LayoutSpec layout;
  layout.width = opt.wrap && opt.width.kind == SizeKind::kFit ? SizeSpec::Grow()
                                                              : opt.width;
  ctx.Text(spec, layout);
}

void Heading(Context &ctx, std::string_view text) {
  LabelOptions opt;
  opt.bold = true;
  opt.size = ctx.theme().metrics.heading_font_size;
  Label(ctx, text, opt);
}

// Controls.

auto Button(Context &ctx, std::string_view label, ButtonKind kind) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  bool activated = it.clicked;
  if (ctx.HasFocus(id) &&
      (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace))) {
    activated = true;
  }
  Color base = t.colors.surface_raised;
  Color fg = t.colors.text;
  if (kind == ButtonKind::kPrimary) {
    base = t.colors.primary;
    fg = t.colors.on_primary;
  } else if (kind == ButtonKind::kFlat) {
    base = t.colors.surface;
  }
  NodeDesc desc;
  desc.id = id;
  desc.layout.mode = LayoutMode::kRow;
  desc.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  desc.layout.padding = Padding::Xy(t.metrics.padding_x * 1.5F, 0);
  desc.layout.align = Align::kCenter;
  desc.layout.justify = Justify::kCenter;
  desc.style.fill = InteractiveFill(t, base, it);
  if (kind == ButtonKind::kFlat && !it.hovered && !it.pressed) {
    desc.style.fill.a = 0.0F;
  }
  if (kind == ButtonKind::kNormal) {
    desc.style.border = t.colors.border;
    desc.style.border_width = t.metrics.border_width;
  }
  desc.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  desc.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  ctx.Open(desc);
  ctx.Text(TextOf(ctx, DisplayText(label), fg));
  ctx.Close();
  return activated;
}

void DisabledButton(Context &ctx, std::string_view label,
                    std::string_view reason, ButtonKind kind) {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  // Interact only for the tooltip's hover.
  ctx.Interact(id);
  NodeDesc desc;
  desc.id = id;
  desc.layout.mode = LayoutMode::kRow;
  desc.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  desc.layout.padding = Padding::Xy(t.metrics.padding_x * 1.5F, 0);
  desc.layout.align = Align::kCenter;
  desc.layout.justify = Justify::kCenter;
  // A flat button stays flat: only its text greys out.
  if (kind != ButtonKind::kFlat) {
    desc.style.fill = t.colors.surface;
    desc.style.border = t.colors.border;
    desc.style.border_width = t.metrics.border_width;
  }
  desc.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  desc.flags = kNodeInteractive;
  ctx.Open(desc);
  ctx.Text(TextOf(ctx, DisplayText(label), t.colors.text_disabled));
  ctx.Close();
  if (!reason.empty()) {
    Tooltip(ctx, id, reason);
  }
}

namespace {

// Shared body of checkbox and radio button.
auto ToggleRow(Context &ctx, std::string_view label, bool checked, bool round)
    -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  bool toggled = it.clicked;
  if (ctx.HasFocus(id) && ctx.ConsumeKey(Key::kSpace)) {
    toggled = true;
  }
  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.gap = 6;
  row.layout.align = Align::kCenter;
  row.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  row.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  row.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  ctx.Open(row);

  NodeDesc box;
  box.layout.width = SizeSpec::Fixed(16);
  box.layout.height = SizeSpec::Fixed(16);
  box.style.radii = gfx::CornerRadii::All(round ? 8.0F : 3.0F);
  const Color fill =
      checked && !round ? t.colors.primary : t.colors.surface_alt;
  box.style.fill = InteractiveFill(t, fill, it);
  box.style.border = checked ? t.colors.primary : t.colors.border;
  box.style.border_width = t.metrics.border_width;
  const int box_node = ctx.Open(box);
  ctx.Close();
  if (checked) {
    const Color mark = round ? t.colors.primary : t.colors.on_primary;
    ctx.AddPainter(box_node, [=](PaintContext &pc, const Rect &r) {
      if (round) {
        pc.list.FillCircle(r.Center(), 4.0F, mark);
      } else {
        pc.list.Line({r.x0 + 3.5F, r.y0 + 8.0F},
                     {r.x0 + 6.5F, r.y0 + 11.0F},
                     2.0F,
                     mark);
        pc.list.Line({r.x0 + 6.0F, r.y0 + 11.0F},
                     {r.x0 + 12.5F, r.y0 + 4.5F},
                     2.0F,
                     mark);
      }
    });
  }
  ctx.Text(TextOf(ctx, DisplayText(label), t.colors.text));
  ctx.Close();
  return toggled;
}

}  // namespace

auto Checkbox(Context &ctx, std::string_view label, bool &value) -> bool {
  if (ToggleRow(ctx, label, value, false)) {
    value = !value;
    return true;
  }
  return false;
}

auto RadioButton(Context &ctx, std::string_view label, int &value, int option)
    -> bool {
  if (ToggleRow(ctx, label, value == option, true) && value != option) {
    value = option;
    return true;
  }
  return false;
}

auto Slider(Context &ctx, std::string_view label, float &value, float min,
            float max) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  const float before = value;
  constexpr float kThumb = 7.0F;
  if (it.pressed && it.rect.Width() > 2 * kThumb) {
    const float f =
        (ctx.mouse().x - it.rect.x0 - kThumb) / (it.rect.Width() - 2 * kThumb);
    value = min + std::clamp(f, 0.0F, 1.0F) * (max - min);
  }
  if (ctx.HasFocus(id)) {
    const float step = (max - min) / 100.0F;
    if (ctx.ConsumeKey(Key::kLeft)) {
      value -= step;
    }
    if (ctx.ConsumeKey(Key::kRight)) {
      value += step;
    }
    if (ctx.ConsumeKey(Key::kPageDown)) {
      value -= step * 10;
    }
    if (ctx.ConsumeKey(Key::kPageUp)) {
      value += step * 10;
    }
    if (ctx.ConsumeKey(Key::kHome)) {
      value = min;
    }
    if (ctx.ConsumeKey(Key::kEnd)) {
      value = max;
    }
  }
  value = std::clamp(value, min, max);

  LayoutSpec row_layout;
  row_layout.gap = t.metrics.spacing;
  row_layout.align = Align::kCenter;
  row_layout.width = SizeSpec::Grow();
  BeginRow(ctx, row_layout);
  if (!DisplayText(label).empty()) {
    ctx.Text(TextOf(ctx, DisplayText(label), t.colors.text));
  }
  NodeDesc track;
  track.id = id;
  track.layout.width = SizeSpec::Grow();
  track.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  track.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  track.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  const int node = ctx.Open(track);
  ctx.Close();
  const float f = max > min ? (value - min) / (max - min) : 0.0F;
  const Color rail = t.colors.surface_alt;
  const Color filled = t.colors.primary;
  const Color thumb = InteractiveFill(t, t.colors.text, it);
  ctx.AddPainter(node, [=](PaintContext &pc, const Rect &r) {
    const float cy = std::round(r.Center().y);
    const float x0 = r.x0 + kThumb;
    const float x1 = r.x1 - kThumb;
    const float x = x0 + (x1 - x0) * f;
    pc.list.FillRect({x0, cy - 2, x1, cy + 2}, rail, gfx::CornerRadii::All(2));
    pc.list.FillRect({x0, cy - 2, x, cy + 2}, filled, gfx::CornerRadii::All(2));
    pc.list.FillCircle({x, cy}, kThumb, thumb);
  });
  char text[32];
  std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(value));
  TextSpec value_text = TextOf(ctx, text, t.colors.text_muted);
  value_text.font = t.fonts.mono;
  value_text.align = TextAlign::kRight;
  LayoutSpec value_layout;
  value_layout.width = SizeSpec::Fixed(56);
  ctx.Text(value_text, value_layout);
  End(ctx);
  return value != before;
}

void ProgressBar(Context &ctx, float fraction, SizeSpec width) {
  const Theme &t = ctx.theme();
  NodeDesc desc;
  desc.layout.width = width;
  desc.layout.height = SizeSpec::Fixed(8);
  desc.style.fill = t.colors.surface_alt;
  desc.style.radii = gfx::CornerRadii::All(4);
  const int node = ctx.Open(desc);
  ctx.Close();
  const float f = std::clamp(fraction, 0.0F, 1.0F);
  const Color c = t.colors.primary;
  ctx.AddPainter(node, [=](PaintContext &pc, const Rect &r) {
    pc.list.FillRect(
        {r.x0, r.y0, r.x0 + r.Width() * f, r.y1}, c, gfx::CornerRadii::All(4));
  });
}

namespace {

struct TextEditState {
  std::size_t cursor = 0;
  std::size_t anchor = 0;
  float scroll_x = 0.0F;
  double last_change = 0.0;
  bool dragging = false;
  // Hash of the text as the field last left it, to notice edits by the
  // app (the caret then moves to the end).
  std::uint64_t known = 0;
};

}  // namespace

auto TextField(Context &ctx, std::string_view label, std::string &text,
               const TextFieldOptions &opt) -> TextFieldResult {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  TextEditState &st = ctx.State<TextEditState>(id);
  TextFieldResult result;
  const text::FontId font = opt.mono ? t.fonts.mono : t.fonts.sans;
  const float size = t.metrics.font_size;
  const float pad = t.metrics.padding_x;
  if (HashString(text) != st.known) {
    st.cursor = st.anchor = text.size();
  }
  st.cursor = std::min(st.cursor, text.size());
  st.anchor = std::min(st.anchor, text.size());

  auto has_selection = [&] {
    return st.cursor != st.anchor;
  };
  auto delete_selection = [&] {
    const std::size_t a = std::min(st.cursor, st.anchor);
    const std::size_t b = std::max(st.cursor, st.anchor);
    text.erase(a, b - a);
    st.cursor = st.anchor = a;
    result.changed = true;
  };
  auto insert = [&](std::string_view s) {
    if (has_selection()) {
      delete_selection();
    }
    std::string clean;
    for (const char c : s) {
      if (c != '\n' && c != '\r') {
        clean.push_back(c);
      }
    }
    text.insert(st.cursor, clean);
    st.cursor += clean.size();
    st.anchor = st.cursor;
    result.changed = true;
  };

  // Mouse: place the caret, drag to select, double click selects all.
  if (it.pressed) {
    const text::ShapedText &shaped = ctx.text().Shape(text, font, size);
    const std::size_t at = shaped.HitTest(it.local.x - pad + st.scroll_x);
    if (!st.dragging) {
      st.dragging = true;
      st.anchor = at;
    }
    st.cursor = at;
  } else {
    st.dragging = false;
  }
  if (it.double_clicked) {
    st.anchor = 0;
    st.cursor = text.size();
  }

  if (ctx.HasFocus(id)) {
    const std::string typed = ctx.TakeText();
    if (!typed.empty()) {
      insert(typed);
    }
    std::uint8_t mods = 0;
    auto move = [&](std::size_t to, std::uint8_t m) {
      st.cursor = to;
      if ((m & kModShift) == 0) {
        st.anchor = to;
      }
    };
    while (ctx.ConsumeKeyAnyMods(Key::kLeft, mods)) {
      if (has_selection() && (mods & kModShift) == 0) {
        move(std::min(st.cursor, st.anchor), mods);
      } else {
        move((mods & kModCtrl) != 0 ? PrevWord(text, st.cursor)
                                    : PrevUtf8(text, st.cursor),
             mods);
      }
    }
    while (ctx.ConsumeKeyAnyMods(Key::kRight, mods)) {
      if (has_selection() && (mods & kModShift) == 0) {
        move(std::max(st.cursor, st.anchor), mods);
      } else {
        move((mods & kModCtrl) != 0 ? NextWord(text, st.cursor)
                                    : NextBoundary(text, st.cursor),
             mods);
      }
    }
    while (ctx.ConsumeKeyAnyMods(Key::kHome, mods)) {
      move(0, mods);
    }
    while (ctx.ConsumeKeyAnyMods(Key::kEnd, mods)) {
      move(text.size(), mods);
    }
    while (ctx.ConsumeKeyAnyMods(Key::kBackspace, mods)) {
      if (has_selection()) {
        delete_selection();
      } else if (st.cursor > 0) {
        st.anchor = (mods & kModCtrl) != 0 ? PrevWord(text, st.cursor)
                                           : PrevUtf8(text, st.cursor);
        delete_selection();
      }
    }
    while (ctx.ConsumeKeyAnyMods(Key::kDelete, mods)) {
      if (has_selection()) {
        delete_selection();
      } else if (st.cursor < text.size()) {
        st.anchor = (mods & kModCtrl) != 0 ? NextWord(text, st.cursor)
                                           : NextBoundary(text, st.cursor);
        delete_selection();
      }
    }
    if (ctx.ConsumeKey(Key::kA, kModCtrl)) {
      st.anchor = 0;
      st.cursor = text.size();
    }
    const auto selected = [&] {
      const std::size_t a = std::min(st.cursor, st.anchor);
      return text.substr(a, std::max(st.cursor, st.anchor) - a);
    };
    if (ctx.ConsumeKey(Key::kC, kModCtrl) && has_selection()) {
      ctx.clipboard().set(selected());
    }
    if (ctx.ConsumeKey(Key::kX, kModCtrl) && has_selection()) {
      ctx.clipboard().set(selected());
      delete_selection();
    }
    if (ctx.ConsumeKey(Key::kV, kModCtrl)) {
      insert(ctx.clipboard().get());
    }
    if (ctx.ConsumeKey(Key::kEnter)) {
      result.submitted = true;
    }
    // Space arrives as text; drop the key so nothing else acts on it.
    ctx.ConsumeKey(Key::kSpace, kModNone, true);
  }
  if (result.changed) {
    st.last_change = ctx.time();
  }
  st.known = HashString(text);

  // Keep the caret visible.
  const text::ShapedText &shaped = ctx.text().Shape(text, font, size);
  const float inner = it.rect.Width() > 0 ? it.rect.Width() - 2 * pad
                                          : opt.width.value - 2 * pad;
  const float caret_x = shaped.CaretX(static_cast<std::uint32_t>(st.cursor));
  if (caret_x - st.scroll_x < 0.0F) {
    st.scroll_x = caret_x;
  } else if (caret_x - st.scroll_x > inner - 1.0F) {
    st.scroll_x = caret_x - inner + 1.0F;
  }
  st.scroll_x = std::clamp(
      st.scroll_x, 0.0F, std::max(0.0F, shaped.width - inner + 1.0F));

  LayoutSpec row_layout;
  row_layout.gap = t.metrics.spacing;
  row_layout.align = Align::kCenter;
  const bool has_label = !DisplayText(label).empty();
  if (has_label) {
    BeginRow(ctx, row_layout);
    ctx.Text(TextOf(ctx, DisplayText(label), t.colors.text));
  }
  const bool focused = ctx.HasFocus(id);
  NodeDesc field;
  field.id = id;
  field.layout.width = opt.width;
  field.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  if (!opt.frameless) {
    field.style.fill = t.colors.surface_alt;
    field.style.border = focused ? t.colors.focus : t.colors.border;
    field.style.border_width = t.metrics.border_width;
    field.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  }
  field.flags = kNodeInteractive | kNodeFocusable | kNodeClip;
  const int node = ctx.Open(field);
  ctx.Close();

  const std::string shown = text;
  const std::string placeholder(opt.placeholder);
  const float scroll = st.scroll_x;
  const std::size_t sel_a = std::min(st.cursor, st.anchor);
  const std::size_t sel_b = std::max(st.cursor, st.anchor);
  const bool caret_on =
      focused && std::fmod(ctx.time() - st.last_change, 1.0) < 0.6;
  ctx.AddPainter(node, [=](PaintContext &pc, const Rect &r) {
    const text::ShapedText &s = pc.text.Shape(shown, font, size);
    const text::FontMetrics m = pc.text.fonts().Metrics(font, size);
    const float x0 = r.x0 + pad - scroll;
    const float top = r.y0 + std::round((r.Height() - m.LineHeight()) * 0.5F);
    const float baseline = top + std::round(m.ascent);
    pc.list.PushClip(r.Inset(1));
    if (sel_a != sel_b) {
      const float a = s.CaretX(static_cast<std::uint32_t>(sel_a));
      const float b = s.CaretX(static_cast<std::uint32_t>(sel_b));
      pc.list.FillRect({x0 + a, top, x0 + b, top + m.LineHeight()},
                       pc.theme.colors.selection);
    }
    if (shown.empty() && !placeholder.empty()) {
      pc.text.DrawText(pc.list,
                       placeholder,
                       font,
                       size,
                       {r.x0 + pad, top},
                       pc.theme.colors.text_disabled);
    } else {
      pc.text.Draw(pc.list, s, {x0, baseline}, pc.theme.colors.text);
    }
    pc.list.PopClip();
  });
  if (caret_on) {
    const std::size_t cursor = st.cursor;
    ctx.AddPainter(
        node,
        [=](PaintContext &pc, const Rect &r) {
          const text::ShapedText &s = pc.text.Shape(shown, font, size);
          const text::FontMetrics m = pc.text.fonts().Metrics(font, size);
          const float top =
              r.y0 + std::round((r.Height() - m.LineHeight()) * 0.5F);
          const float cx =
              std::round(r.x0 + pad - scroll +
                         s.CaretX(static_cast<std::uint32_t>(cursor)));
          pc.list.FillRect({cx, top, cx + 1, top + m.LineHeight()},
                           pc.theme.colors.text);
        },
        true);
  }
  if (has_label) {
    End(ctx);
  }
  return result;
}

namespace {

struct DropdownState {
  int highlight = 0;
};

}  // namespace

auto Dropdown(Context &ctx, std::string_view label, int &selected,
              std::span<const std::string> items) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Id popup = HashCombine(id, 0x706F707570ULL);
  const Interaction it = ctx.Interact(id);
  DropdownState &st = ctx.State<DropdownState>(id);
  const int count = static_cast<int>(items.size());
  bool changed = false;
  bool open = ctx.IsPopupOpen(popup);
  if (it.clicked && !ctx.PopupClosedByPress(popup)) {
    if (open) {
      ctx.ClosePopup(popup);
    } else {
      ctx.OpenPopup(popup);
      st.highlight = std::max(0, selected);
    }
    ctx.SetFocus(id, false);
  }
  if (ctx.HasFocus(id)) {
    if (!open &&
        (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace) ||
         ctx.ConsumeKey(Key::kDown, kModAlt) || ctx.ConsumeKey(Key::kDown))) {
      ctx.OpenPopup(popup);
      st.highlight = std::max(0, selected);
    } else if (open) {
      if (ctx.ConsumeKey(Key::kDown)) {
        st.highlight = std::min(count - 1, st.highlight + 1);
      }
      if (ctx.ConsumeKey(Key::kUp)) {
        st.highlight = std::max(0, st.highlight - 1);
      }
      if (ctx.ConsumeKey(Key::kHome)) {
        st.highlight = 0;
      }
      if (ctx.ConsumeKey(Key::kEnd)) {
        st.highlight = count - 1;
      }
      if ((ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace)) &&
          st.highlight >= 0 && st.highlight < count) {
        changed = selected != st.highlight;
        selected = st.highlight;
        ctx.ClosePopup(popup);
      }
    }
  }
  open = ctx.IsPopupOpen(popup);

  LayoutSpec row_layout;
  row_layout.gap = t.metrics.spacing;
  row_layout.align = Align::kCenter;
  const bool has_label = !DisplayText(label).empty();
  if (has_label) {
    BeginRow(ctx, row_layout);
    ctx.Text(TextOf(ctx, DisplayText(label), t.colors.text));
  }
  NodeDesc button;
  button.id = id;
  button.layout.mode = LayoutMode::kRow;
  button.layout.width = SizeSpec::Fit(140);
  button.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  button.layout.padding = {t.metrics.padding_x, 0, 24, 0};
  button.layout.align = Align::kCenter;
  button.style.fill = InteractiveFill(t, t.colors.surface_raised, it);
  button.style.border = open ? t.colors.focus : t.colors.border;
  button.style.border_width = t.metrics.border_width;
  button.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  button.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const int node = ctx.Open(button);
  const std::string_view current = selected >= 0 && selected < count
                                       ? std::string_view(items[selected])
                                       : std::string_view();
  ctx.Text(TextOf(ctx, current, t.colors.text));
  ctx.Close();
  const Color chevron = t.colors.text_muted;
  ctx.AddPainter(node, [=](PaintContext &pc, const Rect &r) {
    Chevron(pc.list, {r.x1 - 12, std::round(r.Center().y)}, chevron);
  });
  if (has_label) {
    End(ctx);
  }

  if (open) {
    const Rect anchor = ctx.LastRect(id).value_or(Rect{});
    ctx.BeginLayer(LayerKind::kPopup, popup, anchor);
    ctx.PushId(popup);
    for (int i = 0; i < count; ++i) {
      const Id item_id = ctx.GetId(static_cast<std::uint64_t>(i));
      const Interaction item = ctx.Interact(item_id);
      if (item.hovered) {
        st.highlight = i;
      }
      if (item.clicked) {
        changed = selected != i;
        selected = i;
        ctx.ClosePopup(popup);
      }
      NodeDesc row;
      row.id = item_id;
      row.layout.mode = LayoutMode::kRow;
      row.layout.width = SizeSpec::Grow();
      row.layout.height = SizeSpec::Fixed(t.metrics.row_height);
      row.layout.padding = Padding::Xy(t.metrics.padding_x, 0);
      row.layout.align = Align::kCenter;
      row.style.radii = gfx::CornerRadii::All(t.metrics.radius - 1);
      if (i == st.highlight) {
        row.style.fill = t.colors.selection;
      }
      row.flags = kNodeInteractive;
      ctx.Open(row);
      ctx.Text(TextOf(ctx, items[i], t.colors.text, i == selected));
      ctx.Close();
    }
    ctx.PopId();
    ctx.EndLayer();
  }
  return changed;
}

auto Tabs(Context &ctx, std::string_view label, int &selected,
          std::span<const std::string> labels) -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const int count = static_cast<int>(labels.size());
  const int before = selected;
  if (ctx.HasFocus(id)) {
    if (ctx.ConsumeKey(Key::kLeft)) {
      selected = (selected + count - 1) % count;
    }
    if (ctx.ConsumeKey(Key::kRight)) {
      selected = (selected + 1) % count;
    }
  }
  NodeDesc bar;
  bar.id = id;
  bar.layout.mode = LayoutMode::kRow;
  bar.layout.width = SizeSpec::Grow();
  bar.layout.height = SizeSpec::Fixed(t.metrics.control_height + 4);
  bar.flags = kNodeFocusable | kNodeFocusRing;
  const int bar_node = ctx.Open(bar);
  const Color divider = t.colors.divider;
  ctx.AddPainter(
      bar_node,
      [=](PaintContext &pc, const Rect &r) {
        pc.list.FillRect({r.x0, r.y1 - 1, r.x1, r.y1}, divider);
      },
      true);
  ctx.PushId(id);
  for (int i = 0; i < count; ++i) {
    const Id tab_id = ctx.GetId(static_cast<std::uint64_t>(i));
    const Interaction it = ctx.Interact(tab_id);
    if (it.clicked) {
      selected = i;
      ctx.SetFocus(id, false);
    }
    const bool active = i == selected;
    NodeDesc tab;
    tab.id = tab_id;
    tab.layout.mode = LayoutMode::kRow;
    tab.layout.height = SizeSpec::Grow();
    tab.layout.padding = Padding::Xy(t.metrics.padding_x * 1.5F, 0);
    tab.layout.align = Align::kCenter;
    tab.flags = kNodeInteractive;
    if (it.hovered && !active) {
      tab.style.fill = t.colors.hover;
    }
    const int node = ctx.Open(tab);
    ctx.Text(TextOf(
        ctx, labels[i], active ? t.colors.text : t.colors.text_muted, active));
    ctx.Close();
    if (active) {
      const Color line = t.colors.primary;
      ctx.AddPainter(node, [=](PaintContext &pc, const Rect &r) {
        pc.list.FillRect({r.x0, r.y1 - 2, r.x1, r.y1}, line);
      });
    }
  }
  ctx.PopId();
  ctx.Close();
  return selected != before;
}

auto SearchFieldId(Context &ctx, std::string_view label) -> Id {
  ctx.PushId(label);
  const Id id = ctx.GetId("##q");
  ctx.PopId();
  return id;
}

auto SearchField(Context &ctx, std::string_view label, std::string &query,
                 const SearchFieldOptions &opt) -> SearchFieldResult {
  const Theme &t = ctx.theme();
  SearchFieldResult result;
  const Id id = ctx.GetId(label);
  ctx.PushId(label);
  const Id field_id = ctx.GetId("##q");
  const Id clear_id = ctx.GetId("##clear");
  const bool focused = ctx.HasFocus(field_id);
  if (focused) {
    if (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kF3)) {
      result.next = true;
    }
    if (ctx.ConsumeKey(Key::kEnter, kModShift) ||
        ctx.ConsumeKey(Key::kF3, kModShift)) {
      result.previous = true;
    }
    if (!query.empty() && ctx.ConsumeKey(Key::kEscape)) {
      query.clear();
      result.cleared = true;
      result.changed = true;
    }
  }
  if (ctx.Interact(clear_id).clicked && !query.empty()) {
    query.clear();
    result.cleared = true;
    result.changed = true;
    ctx.SetFocus(field_id, false);
  }

  NodeDesc box;
  box.id = id;
  box.layout.mode = LayoutMode::kRow;
  box.layout.width = opt.width;
  box.layout.height = SizeSpec::Fixed(t.metrics.control_height);
  box.layout.padding = Padding::Xy(6, 0);
  box.layout.gap = 4;
  box.layout.align = Align::kCenter;
  box.style.fill = t.colors.surface_alt;
  box.style.border = focused ? t.colors.focus : t.colors.border;
  box.style.border_width = t.metrics.border_width;
  box.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  ctx.Open(box);

  NodeDesc icon;
  icon.layout.width = SizeSpec::Fixed(14);
  icon.layout.height = SizeSpec::Fixed(14);
  const int icon_node = ctx.Open(icon);
  ctx.Close();
  const Color icon_color = focused ? t.colors.text : t.colors.text_muted;
  ctx.AddPainter(icon_node, [=](PaintContext &pc, const Rect &r) {
    pc.list.StrokeCircle({r.x0 + 6, r.y0 + 6}, 4.5F, icon_color, 1.5F);
    pc.list.Line(
        {r.x0 + 9.5F, r.y0 + 9.5F}, {r.x0 + 13, r.y0 + 13}, 1.8F, icon_color);
  });

  TextFieldOptions field;
  field.width = SizeSpec::Grow();
  field.placeholder = opt.placeholder;
  field.frameless = true;
  if (TextField(ctx, "##q", query, field).changed) {
    result.changed = true;
  }
  if (opt.busy || !opt.status.empty()) {
    TextSpec status;
    status.text = opt.busy && opt.status.empty() ? "\xE2\x80\xA6" : opt.status;
    status.font = t.fonts.sans;
    status.size = t.metrics.small_font_size + 1;
    status.color = t.colors.text_muted;
    ctx.Text(status);
  }
  if (!query.empty()) {
    const Interaction it = ctx.Interact(clear_id);
    NodeDesc clear;
    clear.id = clear_id;
    clear.layout.width = SizeSpec::Fixed(16);
    clear.layout.height = SizeSpec::Fixed(16);
    clear.style.radii = gfx::CornerRadii::All(8);
    clear.style.fill = it.hovered ? t.colors.hover : Color{0, 0, 0, 0};
    clear.flags = kNodeInteractive;
    const int clear_node = ctx.Open(clear);
    ctx.Close();
    const Color x_color = it.hovered ? t.colors.text : t.colors.text_muted;
    ctx.AddPainter(clear_node, [=](PaintContext &pc, const Rect &r) {
      const Vec2 c = r.Center();
      pc.list.Line(
          {c.x - 3.5F, c.y - 3.5F}, {c.x + 3.5F, c.y + 3.5F}, 1.5F, x_color);
      pc.list.Line(
          {c.x - 3.5F, c.y + 3.5F}, {c.x + 3.5F, c.y - 3.5F}, 1.5F, x_color);
    });
  }
  ctx.Close();
  ctx.PopId();
  return result;
}

// Scrolling.

namespace {

struct ScrollAreaState {
  double offset = 0.0;
  /// The content node of this frame (EndScroll() adds its edge fades).
  int node = -1;
};

/// Height of the fade at an edge with more content beyond it.
constexpr float kEdgeFade = 18.0F;

// The state lives under the content node's id, which BeginScroll() and
// EndScroll() can both compute.
auto ScrollState(Context &ctx) -> ScrollAreaState & {
  return ctx.State<ScrollAreaState>(ctx.GetId("##content"));
}

}  // namespace

auto ScrollOffset(Context &ctx, std::string_view label) -> double & {
  ctx.PushId(label);
  double &offset = ScrollState(ctx).offset;
  ctx.PopId();
  return offset;
}

void BeginScroll(Context &ctx, std::string_view label, LayoutSpec layout) {
  const Id id = ctx.GetId(label);
  ctx.PushId(label);
  const Id content_id = ctx.GetId("##content");
  ScrollAreaState &st = ScrollState(ctx);
  const double max_offset = std::max(0.0,
                                     double{ctx.LastContentSize(content_id).y} -
                                         ctx.LastInnerSize(content_id).y);
  st.offset -= ctx.Wheel(content_id).y * kWheelLinePixels;
  st.offset = std::clamp(st.offset, 0.0, max_offset);
  ctx.WheelRange(content_id, st.offset > 0.0, st.offset < max_offset);

  NodeDesc outer;
  outer.id = id;
  outer.layout = layout;
  outer.layout.mode = LayoutMode::kRow;
  outer.layout.padding = {};
  outer.layout.gap = 0;
  outer.layout.align = Align::kStretch;
  ctx.Open(outer);

  NodeDesc area;
  area.id = content_id;
  area.layout.mode = LayoutMode::kColumn;
  area.layout.width = SizeSpec::Grow();
  area.layout.height = SizeSpec::Grow();
  area.layout.scroll_y = true;
  area.layout.scroll = {0, static_cast<float>(st.offset)};
  area.layout.gap = layout.gap;
  area.layout.padding = layout.padding;
  area.layout.align = layout.align;
  area.flags = kNodeWheel | kNodeClip;
  st.node = ctx.Open(area);
}

void EndScroll(Context &ctx) {
  ctx.Close();
  const Id content_id = ctx.GetId("##content");
  ScrollAreaState &st = ScrollState(ctx);
  const double content = ctx.LastContentSize(content_id).y;
  const double inner = ctx.LastInnerSize(content_id).y;
  // Fade the edges that have more beyond them, so that content cut by a
  // few pixels reads as "scroll for more" even where the bar is nearly
  // full.
  const bool above = st.offset > 0.5;
  const bool below = content - inner - st.offset > 0.5;
  if ((above || below) && st.node >= 0) {
    const Color surface = ctx.OpenBackground();
    const Color clear{surface.r, surface.g, surface.b, 0.0F};
    ctx.AddPainter(st.node, [=](PaintContext &pc, const Rect &r) {
      const float h = std::min(kEdgeFade, r.Height() * 0.25F);
      if (above) {
        pc.list.FillRectGradient({r.x0, r.y0, r.x1, r.y0 + h}, surface, clear);
      }
      if (below) {
        pc.list.FillRectGradient({r.x0, r.y1 - h, r.x1, r.y1}, clear, surface);
      }
    });
  }
  st.node = -1;
  ScrollBar(ctx, ctx.GetId("##bar"), st.offset, content, inner);
  ctx.PopId();
  ctx.Close();
}

// Lists.

auto List(Context &ctx, std::string_view label, ListState &state, int count,
          const ItemText &item, LayoutSpec layout) -> ListResult {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Interaction it = ctx.Interact(id);
  ListResult result;
  const double row_h = t.metrics.row_height;
  const double viewport = ctx.LastInnerSize(id).y;
  const int page = std::max(1, static_cast<int>(viewport / row_h) - 1);
  const int before = state.selected;
  bool keyboard_moved = false;

  if (ctx.HasFocus(id) && count > 0) {
    auto go = [&](int to) {
      state.selected = std::clamp(to, 0, count - 1);
      keyboard_moved = true;
    };
    if (ctx.ConsumeKey(Key::kDown)) {
      go(state.selected + 1);
    }
    if (ctx.ConsumeKey(Key::kUp)) {
      go(std::max(0, state.selected - 1));
    }
    if (ctx.ConsumeKey(Key::kPageDown)) {
      go(state.selected + page);
    }
    if (ctx.ConsumeKey(Key::kPageUp)) {
      go(state.selected - page);
    }
    if (ctx.ConsumeKey(Key::kHome)) {
      go(0);
    }
    if (ctx.ConsumeKey(Key::kEnd)) {
      go(count - 1);
    }
    if (ctx.ConsumeKey(Key::kEnter) && state.selected >= 0) {
      result.activated = true;
    }
    // Type to jump.
    const std::string typed = ctx.TakeText();
    if (!typed.empty() && typed != " ") {
      if (ctx.time() - state.prefix_time > kPrefixTimeout) {
        state.prefix.clear();
      }
      std::string folded;
      FoldCaseUtf8(typed, folded);
      const bool repeat = state.prefix.empty() || state.prefix == folded;
      state.prefix = repeat ? folded : state.prefix + folded;
      state.prefix_time = ctx.time();
      // A new single-letter prefix searches from the next item, so typing
      // the same letter again cycles through matches.
      const int start = std::max(0, state.selected + (repeat ? 1 : 0));
      for (int k = 0; k < count; ++k) {
        const int i = (start + k) % count;
        if (StartsWithFolded(item(i), state.prefix)) {
          go(i);
          break;
        }
      }
    }
  }

  // Scrolling.
  const double content = row_h * count;
  state.scroll -= ctx.Wheel(id).y * row_h * 3.0;
  if (keyboard_moved && state.selected >= 0 && viewport > 0) {
    const double top = state.selected * row_h;
    if (top < state.scroll) {
      state.scroll = top;
    } else if (top + row_h > state.scroll + viewport) {
      state.scroll = top + row_h - viewport;
    }
  }
  state.scroll =
      std::clamp(state.scroll, 0.0, std::max(0.0, content - viewport));
  ctx.WheelRange(id,
                 state.scroll > 0.0,
                 viewport > 0.0 && state.scroll < content - viewport);

  layout.mode = LayoutMode::kRow;
  if (layout.height.kind == SizeKind::kFit) {
    layout.height = SizeSpec::Grow();
  }
  if (layout.width.kind == SizeKind::kFit) {
    layout.width = SizeSpec::Grow();
  }
  NodeDesc outer;
  outer.layout = layout;
  outer.style.fill = t.colors.surface;
  outer.style.border = t.colors.border;
  outer.style.border_width = t.metrics.border_width;
  outer.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  ctx.Open(outer);

  const int first = static_cast<int>(state.scroll / row_h);
  const double frac = state.scroll - first * row_h;
  const int visible =
      static_cast<int>(std::ceil((viewport > 0 ? viewport : 400.0) / row_h)) +
      1;
  NodeDesc view;
  view.id = id;
  view.layout.mode = LayoutMode::kColumn;
  view.layout.width = SizeSpec::Grow();
  view.layout.height = SizeSpec::Grow();
  view.layout.scroll_y = true;
  view.layout.scroll = {0, static_cast<float>(frac)};
  view.layout.padding = Padding::All(1);
  view.layout.align = Align::kStretch;
  view.flags = kNodeInteractive | kNodeFocusable | kNodeWheel | kNodeClip |
               kNodeFocusRing;
  view.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  ctx.Open(view);
  const bool focused = ctx.HasFocus(id);
  ctx.PushId(id);
  for (int i = first; i < std::min(count, first + visible); ++i) {
    const Id row_id = ctx.GetId(static_cast<std::uint64_t>(i));
    const Interaction row_it = ctx.Interact(row_id);
    if (row_it.clicked || (row_it.pressed && state.selected != i)) {
      state.selected = i;
      ctx.SetFocus(id, false);
    }
    if (row_it.double_clicked) {
      result.activated = true;
    }
    NodeDesc row;
    row.id = row_id;
    row.layout.mode = LayoutMode::kRow;
    row.layout.height = SizeSpec::Fixed(static_cast<float>(row_h));
    row.layout.padding = Padding::Xy(t.metrics.padding_x, 0);
    row.layout.align = Align::kCenter;
    row.flags = kNodeInteractive;
    if (i == state.selected) {
      row.style.fill =
          focused ? t.colors.selection
                  : t.colors.selection.WithAlpha(t.colors.selection.a * 0.6F);
    } else if (row_it.hovered) {
      row.style.fill = t.colors.hover;
    }
    ctx.Open(row);
    ctx.Text(TextOf(ctx, item(i), t.colors.text));
    ctx.Close();
  }
  ctx.PopId();
  ctx.Close();
  ScrollBar(ctx, HashCombine(id, 0x626172ULL), state.scroll, content, viewport);
  ctx.Close();
  (void)it;
  result.selection_changed = state.selected != before;
  return result;
}

// Overlays.

namespace {

struct TooltipState {
  double since = -1.0;
};

}  // namespace

void Tooltip(Context &ctx, Id for_id, std::string_view text) {
  TooltipState &st = ctx.State<TooltipState>(HashCombine(for_id, 0x7469ULL));
  if (!ctx.Interact(for_id).hovered || ctx.mouse_down()) {
    st.since = -1.0;
    return;
  }
  if (st.since < 0.0) {
    st.since = ctx.time();
  }
  if (ctx.time() - st.since < kTooltipDelay) {
    return;
  }
  ctx.BeginLayer(LayerKind::kTooltip, HashCombine(for_id, 0x7470ULL));
  LabelOptions opt;
  opt.size = ctx.theme().metrics.small_font_size + 1;
  Label(ctx, text, opt);
  ctx.EndLayer();
}

}  // namespace gloomy::ui
