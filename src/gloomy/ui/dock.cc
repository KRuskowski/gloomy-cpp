#include "gloomy/ui/dock.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

#include "gloomy/ui/overlays.h"
#include "gloomy/ui/widgets.h"
#include "ui/widget_util.h"

namespace gloomy::ui {

namespace {

constexpr float kDragStart = 8.0F;
// Share of a stack's width or height near an edge that docks to that side.
constexpr float kEdgeZone = 0.25F;

auto Escape(std::string_view s) -> std::string {
  std::string out;
  for (const char c : s) {
    if (c == ',' || c == '(' || c == ')' || c == ':' || c == '%') {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      out += buf;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

auto Unescape(std::string_view s) -> std::string {
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      unsigned v = 0;
      std::from_chars(s.data() + i + 1, s.data() + i + 3, v, 16);
      out.push_back(static_cast<char>(v));
      i += 2;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

// Recursive-descent parser of DockLayout::Encode() output.
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {}

  auto Node(std::vector<DockNode> &nodes) -> int {
    if (text_.starts_with("s:")) {
      text_.remove_prefix(2);
      DockNode node;
      node.split = true;
      if (text_.empty()) {
        return -1;
      }
      node.stacked = text_[0] == 'v';
      text_.remove_prefix(1);
      if (!Eat(':')) {
        return -1;
      }
      const std::size_t paren = text_.find('(');
      if (paren == std::string_view::npos) {
        return -1;
      }
      float ratio = 0.5F;
      std::from_chars(text_.data(), text_.data() + paren, ratio);
      node.ratio = std::clamp(ratio, 0.05F, 0.95F);
      text_.remove_prefix(paren);
      const int index = static_cast<int>(nodes.size());
      nodes.push_back(node);
      if (!Eat('(')) {
        return -1;
      }
      const int first = Node(nodes);
      if (first < 0 || !Eat(')') || !Eat('(')) {
        return -1;
      }
      const int second = Node(nodes);
      if (second < 0 || !Eat(')')) {
        return -1;
      }
      nodes[static_cast<std::size_t>(index)].first = first;
      nodes[static_cast<std::size_t>(index)].second = second;
      return index;
    }
    if (text_.starts_with("t:")) {
      text_.remove_prefix(2);
      DockNode node;
      const std::size_t colon = text_.find(':');
      if (colon == std::string_view::npos) {
        return -1;
      }
      std::from_chars(text_.data(), text_.data() + colon, node.active);
      text_.remove_prefix(colon + 1);
      std::size_t end = 0;
      while (end < text_.size() && text_[end] != ')') {
        ++end;
      }
      std::string_view list = text_.substr(0, end);
      text_.remove_prefix(end);
      while (!list.empty()) {
        const std::size_t comma = list.find(',');
        node.panels.push_back(Unescape(list.substr(0, comma)));
        if (comma == std::string_view::npos) {
          break;
        }
        list.remove_prefix(comma + 1);
      }
      node.active =
          std::clamp(node.active,
                     0,
                     std::max(0, static_cast<int>(node.panels.size()) - 1));
      nodes.push_back(node);
      return static_cast<int>(nodes.size() - 1);
    }
    return -1;
  }

  auto done() const -> bool { return text_.empty(); }

 private:
  auto Eat(char c) -> bool {
    if (text_.empty() || text_[0] != c) {
      return false;
    }
    text_.remove_prefix(1);
    return true;
  }

  std::string_view text_;
};

}  // namespace

// DockLayout

DockLayout::DockLayout(std::vector<std::string> panels) {
  DockNode stack;
  stack.panels = std::move(panels);
  nodes_.push_back(std::move(stack));
}

auto DockLayout::StackOf(std::string_view panel) const -> int {
  for (const int s : Stacks()) {
    const auto &p = nodes_[static_cast<std::size_t>(s)].panels;
    if (std::find(p.begin(), p.end(), panel) != p.end()) {
      return s;
    }
  }
  return -1;
}

auto DockLayout::Stacks() const -> std::vector<int> {
  std::vector<int> out;
  std::vector<int> todo{root_};
  while (!todo.empty()) {
    const int n = todo.back();
    todo.pop_back();
    const DockNode &node = nodes_[static_cast<std::size_t>(n)];
    if (node.split) {
      todo.push_back(node.second);
      todo.push_back(node.first);
    } else {
      out.push_back(n);
    }
  }
  return out;
}

void DockLayout::Compact() {
  std::vector<DockNode> nodes;
  // Copies the subtree at `n` and returns its new index.
  std::function<int(int)> copy = [&](int n) -> int {
    const DockNode node = nodes_[static_cast<std::size_t>(n)];
    const int index = static_cast<int>(nodes.size());
    nodes.push_back(node);
    if (node.split) {
      const int first = copy(node.first);
      const int second = copy(node.second);
      nodes[static_cast<std::size_t>(index)].first = first;
      nodes[static_cast<std::size_t>(index)].second = second;
    }
    return index;
  };
  copy(root_);
  nodes_ = std::move(nodes);
  root_ = 0;
}

void DockLayout::Remove(int stack) {
  if (stack == root_) {
    nodes_[static_cast<std::size_t>(stack)].panels.clear();
    return;
  }
  for (DockNode &node : nodes_) {
    if (!node.split || (node.first != stack && node.second != stack)) {
      continue;
    }
    // The sibling takes the split's place.
    const int sibling = node.first == stack ? node.second : node.first;
    node = nodes_[static_cast<std::size_t>(sibling)];
    break;
  }
  Compact();
}

void DockLayout::Close(const std::string &panel) {
  const int s = StackOf(panel);
  if (s < 0) {
    return;
  }
  DockNode &node = nodes_[static_cast<std::size_t>(s)];
  const auto it = std::find(node.panels.begin(), node.panels.end(), panel);
  const auto index = static_cast<int>(it - node.panels.begin());
  node.panels.erase(it);
  if (node.active > index ||
      node.active >= static_cast<int>(node.panels.size())) {
    node.active = std::max(0, node.active - 1);
  }
  if (node.panels.empty()) {
    Remove(s);
  }
}

void DockLayout::Activate(const std::string &panel) {
  const int s = StackOf(panel);
  if (s < 0) {
    return;
  }
  DockNode &node = nodes_[static_cast<std::size_t>(s)];
  node.active = static_cast<int>(
      std::find(node.panels.begin(), node.panels.end(), panel) -
      node.panels.begin());
}

void DockLayout::Dock(const std::string &panel, const std::string &target,
                      DockSide side, float ratio) {
  if (panel == target) {
    return;
  }
  const int from = StackOf(panel);
  if (from >= 0) {
    const int to = StackOf(target);
    // Already where it would go.
    if (from == to && side == DockSide::kCenter) {
      return;
    }
    Close(panel);
  }
  const int t = StackOf(target);
  if (t < 0) {
    // An empty layout: the panel becomes its only stack.
    DockNode &root = nodes_[static_cast<std::size_t>(root_)];
    if (!root.split) {
      root.panels.push_back(panel);
      root.active = static_cast<int>(root.panels.size()) - 1;
    }
    return;
  }
  if (side == DockSide::kCenter) {
    DockNode &node = nodes_[static_cast<std::size_t>(t)];
    node.panels.push_back(panel);
    node.active = static_cast<int>(node.panels.size()) - 1;
    return;
  }
  DockNode fresh;
  fresh.panels = {panel};
  const int new_index = static_cast<int>(nodes_.size());
  nodes_.push_back(std::move(fresh));
  const int old_index = static_cast<int>(nodes_.size());
  // Copy first: push_back may reallocate under a reference to an element.
  DockNode old = nodes_[static_cast<std::size_t>(t)];
  nodes_.push_back(std::move(old));
  DockNode &split = nodes_[static_cast<std::size_t>(t)];
  split = DockNode{};
  split.split = true;
  split.stacked = side == DockSide::kTop || side == DockSide::kBottom;
  const bool before = side == DockSide::kLeft || side == DockSide::kTop;
  split.first = before ? new_index : old_index;
  split.second = before ? old_index : new_index;
  split.ratio = before ? ratio : 1.0F - ratio;
}

auto DockLayout::Encode() const -> std::string {
  std::function<std::string(int)> enc = [&](int n) -> std::string {
    const DockNode &node = nodes_[static_cast<std::size_t>(n)];
    if (node.split) {
      char ratio[16];
      std::snprintf(
          ratio, sizeof(ratio), "%.3f", static_cast<double>(node.ratio));
      return std::string("s:") + (node.stacked ? "v" : "h") + ":" + ratio +
             "(" + enc(node.first) + ")(" + enc(node.second) + ")";
    }
    std::string out = "t:" + std::to_string(node.active) + ":";
    for (std::size_t i = 0; i < node.panels.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      out += Escape(node.panels[i]);
    }
    return out;
  };
  return enc(root_);
}

auto DockLayout::Decode(std::string_view text) -> std::optional<DockLayout> {
  DockLayout layout;
  layout.nodes_.clear();
  Parser parser(text);
  const int root = parser.Node(layout.nodes_);
  if (root < 0 || !parser.done()) {
    return std::nullopt;
  }
  layout.root_ = root;
  layout.Compact();
  return layout;
}

// DockSpace

namespace {

// Where a drop at `p` inside `r` docks.
auto SideAt(const Rect &r, Vec2 p) -> DockSide {
  const float fx = (p.x - r.x0) / std::max(1.0F, r.Width());
  const float fy = (p.y - r.y0) / std::max(1.0F, r.Height());
  const float edge = std::min({fx, 1 - fx, fy, 1 - fy});
  if (edge > kEdgeZone) {
    return DockSide::kCenter;
  }
  if (edge == fx) {
    return DockSide::kLeft;
  }
  if (edge == 1 - fx) {
    return DockSide::kRight;
  }
  return edge == fy ? DockSide::kTop : DockSide::kBottom;
}

auto PreviewRect(const Rect &r, DockSide side) -> Rect {
  switch (side) {
    case DockSide::kLeft:
      return {r.x0, r.y0, r.Center().x, r.y1};
    case DockSide::kRight:
      return {r.Center().x, r.y0, r.x1, r.y1};
    case DockSide::kTop:
      return {r.x0, r.y0, r.x1, r.Center().y};
    case DockSide::kBottom:
      return {r.x0, r.Center().y, r.x1, r.y1};
    case DockSide::kCenter:
      break;
  }
  return r;
}

// Ids of dock parts derive from the dock's id, not from the id stack, so
// they stay the same however deeply a stack sits in splits.
auto PartId(Id base, std::string_view part) -> Id {
  return HashString(part, base);
}

struct DockFrame {
  Id base;
  Context &ctx;
  DockLayout &layout;
  DockState &state;
  const PanelFn &draw;
  const PanelTitleFn &title;
  bool closable;
  std::string close_request;
  std::string activate_request;
  bool released = false;
};

void RenderNode(DockFrame &f, int index);

void RenderStack(DockFrame &f, int index) {
  Context &ctx = f.ctx;
  const Theme &t = ctx.theme();
  DockNode &node = f.layout.nodes()[static_cast<std::size_t>(index)];
  NodeDesc stack;
  stack.id = PartId(f.base, "##stack" + std::to_string(index));
  stack.layout.mode = LayoutMode::kColumn;
  stack.layout.width = SizeSpec::Grow();
  stack.layout.height = SizeSpec::Grow();
  stack.layout.align = Align::kStretch;
  stack.style.fill = t.colors.surface;
  stack.style.border = t.colors.border;
  stack.style.border_width = t.metrics.border_width;
  stack.style.radii = gfx::CornerRadii::All(t.metrics.radius);
  stack.layout.padding = Padding::All(1);
  stack.flags = kNodeClip;
  ctx.Open(stack);

  // Tab bar.
  const Id bar_id = PartId(f.base, "##bar" + std::to_string(index));
  NodeDesc bar;
  bar.id = bar_id;
  bar.layout.mode = LayoutMode::kRow;
  bar.layout.width = SizeSpec::Grow();
  bar.layout.height = SizeSpec::Fixed(t.metrics.control_height + 2);
  bar.layout.padding = Padding::Xy(4, 0);
  bar.layout.gap = 2;
  bar.layout.align = Align::kEnd;
  bar.style.fill = t.colors.surface_alt;
  bar.flags = kNodeFocusable | kNodeFocusRing;
  const int bar_node = ctx.Open(bar);
  const Color divider = t.colors.border;
  ctx.AddPainter(bar_node, [divider](PaintContext &pc, const Rect &r) {
    pc.list.FillRect({r.x0, r.y1 - 1, r.x1, r.y1}, divider);
  });
  if (ctx.HasFocus(bar_id)) {
    f.state.focused_stack = index;
  }
  for (std::size_t i = 0; i < node.panels.size(); ++i) {
    const std::string &panel = node.panels[i];
    const Id tab_id = PartId(f.base, "##tab" + panel);
    const Id close_id = PartId(f.base, "##close" + panel);
    const Interaction it = ctx.Interact(tab_id);
    const bool active = static_cast<int>(i) == node.active;
    // Press: a candidate drag; release without moving: activate.
    if (it.pressed && f.state.dragging.empty()) {
      f.state.dragging = panel;
      f.state.drag_active = false;
    }
    if (f.state.dragging == panel) {
      if (it.pressed && std::hypot(it.drag.x, it.drag.y) > kDragStart) {
        f.state.drag_active = true;
      }
      if (!it.pressed) {
        f.released = true;
      }
    }
    if (it.clicked && !f.state.drag_active) {
      f.activate_request = panel;
      ctx.SetFocus(bar_id, false);
    }
    NodeDesc tab;
    tab.id = tab_id;
    tab.layout.mode = LayoutMode::kRow;
    tab.layout.height = SizeSpec::Fixed(t.metrics.control_height - 2);
    tab.layout.padding = {10, 0, f.closable ? 4.0F : 10.0F, 0};
    tab.layout.gap = 6;
    tab.layout.align = Align::kCenter;
    tab.style.radii = {t.metrics.radius, t.metrics.radius, 0, 0};
    tab.style.fill = active ? t.colors.surface
                            : (it.hovered ? t.colors.hover : Color{0, 0, 0, 0});
    tab.flags = kNodeInteractive;
    const int tab_node = ctx.Open(tab);
    TextSpec text;
    const std::string shown = f.title ? f.title(panel) : panel;
    text.text = shown;
    text.font = active ? t.fonts.bold : t.fonts.sans;
    text.size = t.metrics.font_size;
    text.color = active ? t.colors.text : t.colors.text_muted;
    ctx.Text(text);
    if (f.closable) {
      const Interaction close_it = ctx.Interact(close_id);
      if (close_it.clicked) {
        f.close_request = panel;
      }
      NodeDesc close;
      close.id = close_id;
      close.layout.width = SizeSpec::Fixed(16);
      close.layout.height = SizeSpec::Fixed(16);
      close.style.radii = gfx::CornerRadii::All(8);
      close.style.fill = close_it.hovered ? t.colors.hover : Color{0, 0, 0, 0};
      close.flags = kNodeInteractive;
      const int close_node = ctx.Open(close);
      ctx.Close();
      const Color x = active || it.hovered || close_it.hovered
                          ? t.colors.text_muted
                          : Color{0, 0, 0, 0};
      ctx.AddPainter(close_node, [x](PaintContext &pc, const Rect &r) {
        const Vec2 c = r.Center();
        pc.list.Line({c.x - 3, c.y - 3}, {c.x + 3, c.y + 3}, 1.3F, x);
        pc.list.Line({c.x - 3, c.y + 3}, {c.x + 3, c.y - 3}, 1.3F, x);
      });
    }
    ctx.Close();
    if (active) {
      const Color line = t.colors.primary;
      ctx.AddPainter(tab_node, [line](PaintContext &pc, const Rect &r) {
        pc.list.FillRect({r.x0, r.y0, r.x1, r.y0 + 2}, line);
      });
    }
  }
  ctx.Close();

  // Content of the active panel.
  NodeDesc content;
  content.id = PartId(f.base, "##content" + std::to_string(index));
  content.layout.mode = LayoutMode::kColumn;
  content.layout.width = SizeSpec::Grow();
  content.layout.height = SizeSpec::Grow();
  content.layout.padding = Padding::All(8);
  content.layout.gap = 6;
  content.layout.align = Align::kStretch;
  content.flags = kNodeClip;
  ctx.Open(content);
  if (!node.panels.empty()) {
    const std::string panel =
        node.panels[static_cast<std::size_t>(node.active)];
    ctx.PushId(panel);
    f.draw(ctx, panel);
    ctx.PopId();
  }
  ctx.Close();
  ctx.Close();
}

void RenderNode(DockFrame &f, int index) {
  Context &ctx = f.ctx;
  const DockNode node = f.layout.nodes()[static_cast<std::size_t>(index)];
  if (!node.split) {
    RenderStack(f, index);
    return;
  }
  const std::string label = "##split" + std::to_string(index);
  SplitState &split = ctx.State<SplitState>(ctx.GetId(label + "##state"));
  if (!split.dragging) {
    split.ratio = node.ratio;
  }
  BeginSplit(ctx, label, split, node.stacked);
  RenderNode(f, node.first);
  SplitNext(ctx, split);
  RenderNode(f, node.second);
  EndSplit(ctx);
  f.layout.nodes()[static_cast<std::size_t>(index)].ratio = split.ratio;
}

}  // namespace

auto DockTabId(Context &ctx, std::string_view label, std::string_view panel)
    -> Id {
  return PartId(ctx.GetId(label), std::string("##tab") + std::string(panel));
}

auto DockCloseId(Context &ctx, std::string_view label, std::string_view panel)
    -> Id {
  return PartId(ctx.GetId(label), std::string("##close") + std::string(panel));
}

void DockSpace(Context &ctx, std::string_view label, DockLayout &layout,
               DockState &state, const PanelFn &draw, const PanelTitleFn &title,
               bool closable, LayoutSpec layout_spec) {
  const Theme &t = ctx.theme();
  const Id id = ctx.GetId(label);
  ctx.PushId(id);

  // Keyboard: F6 cycles stacks, Ctrl+Tab cycles tabs of the focused one.
  const std::vector<int> stacks = layout.Stacks();
  if (!stacks.empty()) {
    const bool next = ctx.ConsumeKey(Key::kF6);
    const bool prev = ctx.ConsumeKey(Key::kF6, kModShift);
    if (next || prev) {
      auto it = std::find(stacks.begin(), stacks.end(), state.focused_stack);
      std::ptrdiff_t i = it == stacks.end() ? -1 : it - stacks.begin();
      const auto n = static_cast<std::ptrdiff_t>(stacks.size());
      i = next ? (i + 1) % n : (i <= 0 ? n - 1 : i - 1);
      state.focused_stack = stacks[static_cast<std::size_t>(i)];
      ctx.SetFocus(PartId(id, "##bar" + std::to_string(state.focused_stack)));
    }
    if (state.focused_stack >= 0 &&
        state.focused_stack < static_cast<int>(layout.nodes().size()) &&
        !layout.nodes()[static_cast<std::size_t>(state.focused_stack)].split) {
      DockNode &node =
          layout.nodes()[static_cast<std::size_t>(state.focused_stack)];
      const int n = static_cast<int>(node.panels.size());
      if (n > 0 && ctx.ConsumeKey(Key::kTab, kModCtrl)) {
        node.active = (node.active + 1) % n;
      }
      if (n > 0 && ctx.ConsumeKey(Key::kTab, kModCtrl | kModShift)) {
        node.active = (node.active + n - 1) % n;
      }
    }
  }

  if (layout_spec.width.kind == SizeKind::kFit) {
    layout_spec.width = SizeSpec::Grow();
  }
  if (layout_spec.height.kind == SizeKind::kFit) {
    layout_spec.height = SizeSpec::Grow();
  }
  NodeDesc root;
  root.id = ctx.GetId("##root");
  root.layout = layout_spec;
  root.layout.mode = LayoutMode::kColumn;
  root.layout.align = Align::kStretch;
  const int root_node = ctx.Open(root);
  DockFrame f{id, ctx, layout, state, draw, title, closable, {}, {}, false};
  RenderNode(f, layout.root());
  ctx.Close();

  // Drag-to-dock: find the stack under the pointer (last frame's rects).
  int target = -1;
  DockSide side = DockSide::kCenter;
  Rect preview;
  if (state.drag_active) {
    for (const int s : stacks) {
      const auto rect = ctx.LastRect(PartId(id, "##stack" + std::to_string(s)));
      if (rect && rect->Contains(ctx.mouse())) {
        target = s;
        side = SideAt(*rect, ctx.mouse());
        preview = PreviewRect(*rect, side);
      }
    }
    if (target >= 0) {
      const Color fill = t.colors.primary.WithAlpha(0.22F);
      const Color edge = t.colors.primary;
      ctx.AddPainter(
          root_node, [preview, fill, edge](PaintContext &pc, const Rect &) {
            pc.list.FillRect(preview.Inset(3), fill, gfx::CornerRadii::All(6));
            pc.list.StrokeRect(
                preview.Inset(3), edge, 2.0F, gfx::CornerRadii::All(6));
          });
    }
  }
  if (f.released) {
    if (state.drag_active && target >= 0) {
      const DockNode &node = layout.nodes()[static_cast<std::size_t>(target)];
      if (!node.panels.empty()) {
        layout.Dock(state.dragging,
                    node.panels[static_cast<std::size_t>(node.active)],
                    side);
        layout.Activate(state.dragging);
      }
    }
    state.dragging.clear();
    state.drag_active = false;
  }
  if (!f.activate_request.empty()) {
    layout.Activate(f.activate_request);
  }
  if (!f.close_request.empty()) {
    layout.Close(f.close_request);
  }
  ctx.PopId();
}

}  // namespace gloomy::ui
