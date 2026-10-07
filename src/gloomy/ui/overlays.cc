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

auto BeginContextMenu(Context &ctx, std::string_view label) -> bool {
  const Id id = ctx.GetId(label);
  if (!ctx.IsPopupOpen(id)) {
    return false;
  }
  MenuState &st = ctx.State<MenuState>(id);
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
  ctx.BeginLayer(
      LayerKind::kPopup, id, Rect{st.at.x, st.at.y, st.at.x + 1, st.at.y});
  ctx.PushId(id);
  return true;
}

auto MenuItem(Context &ctx, std::string_view text, std::string_view shortcut,
              bool enabled) -> bool {
  const Theme &t = ctx.theme();
  MenuState &st = *Menu(ctx).state;
  const int index = st.counter++;
  const Id id = ctx.GetId(static_cast<std::uint64_t>(index));
  const Interaction it = ctx.Interact(id);
  if (it.hovered && enabled) {
    st.highlight = index;
  }
  const bool chosen = enabled && (it.clicked || st.activate == index);
  NodeDesc row;
  row.id = id;
  row.layout.mode = LayoutMode::kRow;
  row.layout.height = SizeSpec::Fixed(t.metrics.row_height + 2);
  row.layout.padding = Padding::Xy(10, 0);
  row.layout.gap = 24;
  row.layout.align = Align::kCenter;
  row.style.radii = gfx::CornerRadii::All(t.metrics.radius - 1);
  if (index == st.highlight && enabled) {
    row.style.fill = t.colors.selection;
  }
  row.flags = kNodeInteractive;
  ctx.Open(row);
  LabelOptions label;
  label.color = enabled ? t.colors.text : t.colors.text_disabled;
  label.width = SizeSpec::Grow();
  Label(ctx, text, label);
  if (!shortcut.empty()) {
    LabelOptions sc;
    sc.muted = true;
    sc.mono = true;
    sc.size = t.metrics.small_font_size;
    Label(ctx, shortcut, sc);
  }
  ctx.Close();
  if (chosen) {
    st.close = true;
  }
  return chosen;
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

auto BeginDialog(Context &ctx, std::string_view label, std::string_view title,
                 bool &open, float width) -> bool {
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
  panel.layout.width = SizeSpec::Fixed(width);
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
