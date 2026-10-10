#include "gloomy/ui/overlays.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

namespace {

struct MenuState {
  Vec2 at;
  int highlight = 0;
  // Items declared in the last frame, for keyboard movement.
  int count = 0;
  int counter = 0;
  int activate = -1;
  bool close = false;
};

// The menu being declared (menus do not nest), per context.
struct ActiveMenu {
  Id id = kNoId;
  MenuState *state = nullptr;
};

struct SplitFrame {
  Id id;
  bool stacked;
  float first;
};

// Fixed ids of per-context bookkeeping in the state store.
constexpr Id kActiveMenuId = 0x6D656E7561637469ULL;
constexpr Id kSplitStackId = 0x73706C6974737463ULL;

auto Menu(Context &ctx) -> ActiveMenu & {
  return ctx.State<ActiveMenu>(kActiveMenuId);
}

auto Splits(Context &ctx) -> std::vector<SplitFrame> & {
  return ctx.State<std::vector<SplitFrame>>(kSplitStackId);
}

constexpr float kDivider = 6.0F;

}  // namespace

void OpenContextMenu(Context &ctx, std::string_view label) {
  const Id id = ctx.GetId(label);
  MenuState &st = ctx.State<MenuState>(id);
  st = MenuState{};
  st.at = ctx.mouse();
  ctx.OpenPopup(id);
}

namespace {

// Keyboard movement in an open menu, and makes it the one being declared.
void StartMenu(Context &ctx, Id id, MenuState &st, const Rect &anchor) {
  st.count = std::max(st.count, st.counter);
  st.counter = 0;
  st.activate = -1;
  if (st.count > 0) {
    if (ctx.ConsumeKey(Key::kDown)) {
      st.highlight = (st.highlight + 1) % st.count;
    }
    if (ctx.ConsumeKey(Key::kUp)) {
      st.highlight = (st.highlight + st.count - 1) % st.count;
    }
    if (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace)) {
      st.activate = st.highlight;
    }
  }
  Menu(ctx) = {id, &st};
  ctx.BeginLayer(LayerKind::kPopup, id, anchor);
  ctx.PushId(id);
}

void FinishMenu(Context &ctx) {
  ActiveMenu &menu = Menu(ctx);
  MenuState &st = *menu.state;
  ctx.PopId();
  ctx.EndLayer();
  st.count = st.counter;
  if (st.close) {
    ctx.ClosePopup(menu.id);
  }
  menu = {};
}

}  // namespace

auto BeginContextMenu(Context &ctx, std::string_view label) -> bool {
  const Id id = ctx.GetId(label);
  if (!ctx.IsPopupOpen(id)) {
    return false;
  }
  MenuState &st = ctx.State<MenuState>(id);
  StartMenu(ctx, id, st, Rect{st.at.x, st.at.y, st.at.x + 1, st.at.y});
  return true;
}

auto MenuItem(Context &ctx, std::string_view text, std::string_view shortcut,
              bool enabled) -> bool {
  return MenuItem(ctx, MenuItemSpec{text, shortcut, {}, enabled});
}

auto MenuItem(Context &ctx, const MenuItemSpec &item) -> bool {
  const Theme &t = ctx.theme();
  MenuState &st = *Menu(ctx).state;
  const int index = st.counter++;
  const Id id = ctx.GetId(static_cast<std::uint64_t>(index));
  const Interaction it = ctx.Interact(id);
  const bool enabled = item.enabled;
  if (it.hovered && enabled) {
    st.highlight = index;
  }
  const bool chosen = enabled && (it.clicked || st.activate == index);
  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.height = item.hint.empty()
                          ? SizeSpec::Fixed(t.metrics.row_height + 2)
                          : SizeSpec::Fit(t.metrics.row_height + 2);
  row.layout.padding = Padding::Xy(10, item.hint.empty() ? 0.0F : 4.0F);
  row.layout.gap = 24;
  row.layout.align = Align::kCenter;
  row.style.radii = gfx::CornerRadii::All(t.metrics.radius - 1);
  if (index == st.highlight && enabled) {
    row.style.fill = t.colors.selection;
  }
  row.flags = kNodeInteractive;
  ctx.Open(row);
  LayoutSpec lines;
  lines.width = SizeSpec::Grow();
  lines.gap = 1;
  BeginColumn(ctx, lines);
  LabelOptions label;
  label.color = enabled ? t.colors.text : t.colors.text_disabled;
  Label(ctx, item.text, label);
  if (!item.hint.empty()) {
    LabelOptions hint;
    hint.muted = true;
    hint.size = t.metrics.small_font_size;
    hint.wrap = true;
    hint.width = SizeSpec::Fit(0.0F, 340.0F);
    Label(ctx, item.hint, hint);
  }
  End(ctx);
  if (!item.shortcut.empty()) {
    LabelOptions sc;
    sc.muted = true;
    sc.mono = true;
    sc.size = t.metrics.small_font_size;
    Label(ctx, item.shortcut, sc);
  }
  ctx.Close();
  if (chosen) {
    st.close = true;
  }
  return chosen;
}

void MenuHeading(Context &ctx, std::string_view text) {
  const Theme &t = ctx.theme();
  NodeDesc row;
  row.layout.mode = LayoutMode::kRow;
  row.layout.height = SizeSpec::Fixed(t.metrics.row_height);
  row.layout.padding = {10, 6, 10, 0};
  row.layout.align = Align::kEnd;
  ctx.Open(row);
  LabelOptions o;
  o.muted = true;
  o.bold = true;
  o.size = t.metrics.small_font_size;
  Label(ctx, text, o);
  ctx.Close();
}

void MenuSeparator(Context &ctx) {
  NodeDesc line;
  line.layout.width = SizeSpec::Grow();
  line.layout.height = SizeSpec::Fixed(7);
  const int node = ctx.Open(line);
  ctx.Close();
  const Color color = ctx.theme().colors.border;
  ctx.AddPainter(node, [color](PaintContext &pc, const Rect &r) {
    const float y = std::round(r.Center().y);
    pc.list.FillRect({r.x0 + 6, y, r.x1 - 6, y + 1}, color);
  });
}

void EndContextMenu(Context &ctx) {
  FinishMenu(ctx);
}

auto BeginMenuButton(Context &ctx, std::string_view label, ButtonKind kind)
    -> bool {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  const Id popup = HashCombine(id, 0x6D656E75ULL);
  const Interaction it = ctx.Interact(id);
  MenuState &st = ctx.State<MenuState>(popup);
  bool open = ctx.IsPopupOpen(popup);
  const auto start = [&] {
    st = MenuState{};
    ctx.OpenPopup(popup);
  };
  if (it.clicked && !ctx.PopupClosedByPress(popup)) {
    if (open) {
      ctx.ClosePopup(popup);
    } else {
      start();
    }
    ctx.SetFocus(id, false);
  } else if (!open && ctx.HasFocus(id) &&
             (ctx.ConsumeKey(Key::kEnter) || ctx.ConsumeKey(Key::kSpace) ||
              ctx.ConsumeKey(Key::kDown))) {
    start();
  }
  open = ctx.IsPopupOpen(popup);
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
  desc.layout.padding = {t.metrics.padding_x * 1.5F, 0, 22, 0};
  desc.layout.align = Align::kCenter;
  desc.style.fill = detail::InteractiveFill(t, base, it);
  if (kind == ButtonKind::kFlat && !it.hovered && !it.pressed && !open) {
    desc.style.fill.a = 0.0F;
  }
  if (kind == ButtonKind::kNormal) {
    desc.style.border = open ? t.colors.focus : t.colors.border;
    desc.style.border_width = t.metrics.border_width;
  }
  desc.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  desc.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const int node = ctx.Open(desc);
  LabelOptions text;
  text.color = fg;
  Label(ctx, DisplayText(label), text);
  ctx.Close();
  const Color chevron = fg.WithAlpha(0.75F);
  ctx.AddPainter(node, [chevron](PaintContext &pc, const Rect &r) {
    const Vec2 c{r.x1 - 12, std::round(r.Center().y)};
    pc.list.Line({c.x - 4, c.y - 2}, {c.x, c.y + 2}, 1.5F, chevron);
    pc.list.Line({c.x, c.y + 2}, {c.x + 4, c.y - 2}, 1.5F, chevron);
  });
  if (!open) {
    return false;
  }
  StartMenu(ctx, popup, st, ctx.LastRect(id).value_or(Rect{}));
  return true;
}

void EndMenuButton(Context &ctx) {
  FinishMenu(ctx);
}

auto BeginDialog(Context &ctx, std::string_view label, std::string_view title,
                 bool &open, float width) -> bool {
  DialogOptions opt;
  opt.width = width;
  return BeginDialog(ctx, label, title, open, opt);
}

auto BeginDialog(Context &ctx, std::string_view label, std::string_view title,
                 bool &open, const DialogOptions &opt) -> bool {
  if (!open) {
    return false;
  }
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  ctx.BeginLayer(LayerKind::kModal, id);
  ctx.PushId(id);
  NodeDesc panel;
  panel.id = ctx.GetId("##panel");
  panel.layout.mode = LayoutMode::kColumn;
  panel.layout.width = SizeSpec::Fixed(opt.width);
  panel.layout.padding = Padding::All(16);
  panel.layout.gap = 10;
  panel.layout.align = Align::kStretch;
  panel.style.fill = t.colors.surface_raised;
  panel.style.border = t.colors.border;
  panel.style.border_width = t.metrics.border_width;
  panel.style.radii = gfx::CornerRadii::All(t.metrics.radius + 4);
  panel.style.shadow = t.colors.shadow;
  panel.style.shadow_blur = 18;
  panel.style.shadow_offset = {0, 6};
  panel.flags = kNodeInteractive;
  ctx.Open(panel);
  LayoutSpec head;
  head.align = Align::kCenter;
  head.width = SizeSpec::Grow();
  BeginRow(ctx, head);
  Heading(ctx, title);
  Spacer(ctx);
  if (Button(ctx, "Close##dialog", ButtonKind::kFlat)) {
    open = false;
  }
  End(ctx);
  if (!opt.summary.empty()) {
    NodeDesc answer;
    answer.layout.mode = LayoutMode::kColumn;
    answer.layout.width = SizeSpec::Grow();
    answer.layout.padding = {10, 6, 8, 6};
    answer.layout.gap = 2;
    const Color edge =
        opt.tone == Tone::kNeutral ? t.colors.accent : ToneColor(t, opt.tone);
    answer.style.fill = edge.WithAlpha(0.10F);
    answer.style.radii = gfx::CornerRadii::All(t.metrics.radius);
    const int node = ctx.Open(answer);
    LabelOptions line;
    line.wrap = true;
    line.width = SizeSpec::Grow();
    line.color =
        opt.tone == Tone::kNeutral ? t.colors.text : ToneColor(t, opt.tone);
    Label(ctx, opt.summary, line);
    if (!opt.detail.empty()) {
      LabelOptions more;
      more.wrap = true;
      more.muted = true;
      more.width = SizeSpec::Grow();
      more.size = t.metrics.small_font_size + 1;
      Label(ctx, opt.detail, more);
    }
    ctx.Close();
    ctx.AddPainter(node, [edge](PaintContext &pc, const Rect &r) {
      pc.list.FillRect({r.x0, r.y0, r.x0 + 3, r.y1}, edge);
    });
  }
  return true;
}

void EndDialog(Context &ctx, bool &open) {
  ctx.Close();
  ctx.PopId();
  ctx.EndLayer();
  if (ctx.ConsumeKey(Key::kEscape)) {
    open = false;
  }
}

void ToastQueue::Show(std::string text, Tone tone, double seconds) {
  toasts_.push_back({std::move(text), tone, seconds});
}

void Toasts(Context &ctx, ToastQueue &queue) {
  const Theme &t = ctx.theme();
  auto &toasts = queue.toasts_;
  for (auto &toast : toasts) {
    if (toast.until < 0.0) {
      toast.until = ctx.time() + toast.seconds;
    }
  }
  std::erase_if(toasts,
                [&](const auto &toast) { return toast.until < ctx.time(); });
  if (toasts.empty()) {
    return;
  }
  const Vec2 vp = ctx.viewport();
  const float width = 340.0F;
  const float height =
      static_cast<float>(toasts.size()) * (t.metrics.row_height + 30.0F);
  ctx.BeginLayer(LayerKind::kFloating,
                 ctx.GetId("##toasts"),
                 Rect{vp.x - width - 40, vp.y - height - 40, vp.x, vp.y});
  for (const auto &toast : toasts) {
    NodeDesc box;
    box.layout.mode = LayoutMode::kRow;
    box.layout.width = SizeSpec::Fixed(width);
    box.layout.padding = Padding::Xy(10, 8);
    box.layout.gap = 8;
    box.layout.align = Align::kCenter;
    const int node = ctx.Open(box);
    const Color edge = toast.tone == Tone::kNeutral ? t.colors.primary
                                                    : ToneColor(t, toast.tone);
    ctx.AddPainter(
        node,
        [edge](PaintContext &pc, const Rect &r) {
          pc.list.FillRect({r.x0, r.y0 + 4, r.x0 + 3, r.y1 - 4},
                           edge,
                           gfx::CornerRadii::All(1.5F));
        },
        true);
    LabelOptions text;
    text.wrap = true;
    Label(ctx, toast.text, text);
    ctx.Close();
  }
  ctx.EndLayer();
}

void BeginSplit(Context &ctx, std::string_view label, SplitState &state,
                bool stacked, LayoutSpec layout) {
  const Id id = ctx.GetId(label);
  if (layout.width.kind == SizeKind::kFit) {
    layout.width = SizeSpec::Grow();
  }
  if (layout.height.kind == SizeKind::kFit) {
    layout.height = SizeSpec::Grow();
  }
  layout.mode = stacked ? LayoutMode::kColumn : LayoutMode::kRow;
  layout.align = Align::kStretch;
  layout.gap = 0;
  NodeDesc outer;
  outer.id = id;
  outer.layout = layout;
  ctx.Open(outer);
  ctx.PushId(id);
  // Sizes come from the last frame's rectangle; the first frame splits
  // by weight.
  float first = -1.0F;
  if (const auto rect = ctx.LastRect(id)) {
    const float total = (stacked ? rect->Height() : rect->Width()) - kDivider;
    first = std::round(std::clamp(state.ratio, 0.05F, 0.95F) * total);
  }
  Splits(ctx).push_back({id, stacked, first});
  NodeDesc pane;
  pane.layout.mode = LayoutMode::kColumn;
  pane.layout.align = Align::kStretch;
  const SizeSpec main =
      first >= 0 ? SizeSpec::Fixed(first) : SizeSpec::Grow(state.ratio);
  pane.layout.width = stacked ? SizeSpec::Grow() : main;
  pane.layout.height = stacked ? main : SizeSpec::Grow();
  pane.flags = kNodeClip;
  ctx.Open(pane);
}

void SplitNext(Context &ctx, SplitState &state) {
  const Theme &t = ctx.theme();
  ctx.Close();
  const SplitFrame f = Splits(ctx).back();
  const Id divider = ctx.GetId("##divider");
  const Interaction it = ctx.Interact(divider);
  const auto rect = ctx.LastRect(f.id);
  const float total =
      rect ? (f.stacked ? rect->Height() : rect->Width()) - kDivider : 0.0F;
  if (it.pressed && total > 0) {
    if (!state.dragging) {
      state.dragging = true;
      state.start_ratio = state.ratio;
    }
    state.ratio =
        state.start_ratio + (f.stacked ? it.drag.y : it.drag.x) / total;
  } else {
    state.dragging = false;
  }
  if (ctx.HasFocus(divider)) {
    if (ctx.ConsumeKey(f.stacked ? Key::kUp : Key::kLeft)) {
      state.ratio -= 0.05F;
    }
    if (ctx.ConsumeKey(f.stacked ? Key::kDown : Key::kRight)) {
      state.ratio += 0.05F;
    }
  }
  state.ratio = std::clamp(state.ratio, 0.05F, 0.95F);
  NodeDesc bar;
  bar.id = divider;
  bar.layout.width = f.stacked ? SizeSpec::Grow() : SizeSpec::Fixed(kDivider);
  bar.layout.height = f.stacked ? SizeSpec::Fixed(kDivider) : SizeSpec::Grow();
  bar.flags = kNodeInteractive | kNodeFocusable | kNodeFocusRing;
  const int node = ctx.Open(bar);
  ctx.Close();
  const Color line =
      it.pressed || it.hovered ? t.colors.primary : t.colors.divider;
  const bool stacked = f.stacked;
  ctx.AddPainter(node, [line, stacked](PaintContext &pc, const Rect &r) {
    const Vec2 c = r.Center();
    if (stacked) {
      pc.list.FillRect({r.x0, std::round(c.y) - 1, r.x1, std::round(c.y)},
                       line);
    } else {
      pc.list.FillRect({std::round(c.x) - 1, r.y0, std::round(c.x), r.y1},
                       line);
    }
  });
  NodeDesc pane;
  pane.layout.mode = LayoutMode::kColumn;
  pane.layout.align = Align::kStretch;
  pane.layout.width = SizeSpec::Grow();
  pane.layout.height = SizeSpec::Grow();
  pane.flags = kNodeClip;
  ctx.Open(pane);
}

void EndSplit(Context &ctx) {
  ctx.Close();
  ctx.PopId();
  ctx.Close();
  Splits(ctx).pop_back();
}

}  // namespace gloomy::ui
