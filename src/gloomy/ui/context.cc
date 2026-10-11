#include "gloomy/ui/context.h"

#include <algorithm>
#include <cmath>

namespace gloomy::ui {

namespace {

// Seconds between two clicks that still count as a double click.
constexpr double kDoubleClickSeconds = 0.35;
// Frames a state may go unused before it is dropped.
constexpr std::uint64_t kStateLifetimeFrames = 120;

}  // namespace

// Measures text leaves for the layout engine.
class Context::TextMeasurer : public Measurer {
 public:
  explicit TextMeasurer(Context &ctx) : ctx_(ctx) {}

  auto MeasureWidth(int m) -> WidthRange override {
    const TextItem &item = ctx_.texts_[m];
    const std::string_view text = ctx_.TextOf(item);
    const text::ShapedText &shaped =
        ctx_.text_.Shape(text, item.spec.font, item.spec.size);
    if (!item.spec.wrap) {
      // Single lines may shrink to a few characters plus an ellipsis.
      return {std::min(shaped.width, item.spec.size * 3.0F), shaped.width};
    }
    // Longest word is the narrowest a wrapped text can get.
    float longest = 0.0F;
    float word_start = 0.0F;
    for (const text::ShapedGlyph &g : shaped.glyphs) {
      if (text[g.cluster] == ' ' || text[g.cluster] == '\n') {
        longest = std::max(longest, g.x - word_start);
        word_start = g.x + g.advance;
      }
    }
    longest = std::max(longest, shaped.width - word_start);
    return {std::ceil(longest), std::ceil(shaped.width)};
  }

  auto MeasureHeight(int m, float width) -> float override {
    TextItem &item = ctx_.texts_[m];
    const float line =
        ctx_.text_.fonts().Metrics(item.spec.font, item.spec.size).LineHeight();
    if (!item.spec.wrap) {
      return std::ceil(line);
    }
    item.wrapped = ctx_.text_.Wrap(
        ctx_.TextOf(item), item.spec.font, item.spec.size, width + 0.5F);
    return std::ceil(item.wrapped.Height());
  }

 private:
  Context &ctx_;
};

Context::Context(text::TextSystem &text, Theme theme)
    : text_(text),
      theme_(std::move(theme)),
      icon_cache_(icons_, text.glyphs().atlas()) {
  // In-memory clipboard until the platform installs a real one.
  auto storage = std::make_shared<std::string>();
  clipboard_.get = [storage] {
    return *storage;
  };
  clipboard_.set = [storage](const std::string &s) {
    *storage = s;
  };
}

Context::~Context() = default;

// Frame.

void Context::BeginFrame(Vec2 viewport, std::span<const InputEvent> events,
                         double time) {
  ++frame_;
  time_ = time;
  viewport_ = viewport;
  text_.BeginFrame();

  focusables_.clear();
  prev_want_tab_ = want_tab_;
  want_tab_ = false;

  clicked_ = kNoId;
  double_clicked_ = kNoId;
  right_clicked_ = kNoId;
  right_pressed_ = kNoId;
  right_released_ = false;
  wheel_.clear();
  keys_.clear();
  text_input_.clear();

  tree_.Clear();
  nodes_.clear();
  texts_.clear();
  text_arena_.clear();
  painters_.clear();
  layers_.clear();
  layer_stack_.assign(1, 0);
  stack_.clear();
  id_stack_.assign(1, 0x676C6F6F6D79ULL);

  ProcessEvents(events);

  if (frame_ % 60 == 0) {
    std::erase_if(states_, [&](const auto &item) {
      return item.second.last_frame + kStateLifetimeFrames < frame_;
    });
  }

  // The base layer's root covers the viewport.
  layers_.push_back(Layer{LayerKind::kPopup, kNoId, {}, 0});
  stack_.push_back(-1);
  NodeDesc root;
  root.id = GetId("##root");
  root.layout.width = SizeSpec::Fixed(viewport.x);
  root.layout.height = SizeSpec::Fixed(viewport.y);
  root.layout.align = Align::kStretch;
  Open(root);
}

void Context::ProcessEvents(std::span<const InputEvent> events) {
  for (const InputEvent &e : events) {
    if (e.type != InputEvent::Type::kPointerMove &&
        e.type != InputEvent::Type::kText) {
      mods_ = e.mods;
    }
    switch (e.type) {
      case InputEvent::Type::kPointerMove:
        mouse_ = e.pos;
        break;
      case InputEvent::Type::kPointerDown: {
        mouse_ = e.pos;
        if (e.button == PointerButton::kRight) {
          right_pressed_ = HitTest(mouse_, kNodeInteractive | kNodeFocusable);
          right_down_ = true;
          right_press_pos_ = mouse_;
          break;
        }
        if (e.button != PointerButton::kLeft) {
          break;
        }
        const Id hit = HitTest(mouse_, kNodeInteractive | kNodeFocusable);
        // A press outside the topmost popups closes them.
        popup_closed_by_press_ = kNoId;
        while (!open_popups_.empty()) {
          const Id popup = open_popups_.back();
          bool inside = false;
          for (std::size_t i = 0; i < prev_layer_ids_.size(); ++i) {
            if (prev_layer_ids_[i] == popup &&
                prev_layer_rects_[i].Contains(mouse_)) {
              inside = true;
            }
          }
          if (inside) {
            break;
          }
          popup_closed_by_press_ = popup;
          open_popups_.pop_back();
        }
        mouse_down_ = true;
        press_pos_ = mouse_;
        press_mods_ = e.mods;
        active_ = hit;
        std::uint16_t flags = 0;
        for (const HitEntry &h : prev_hits_) {
          if (h.id == hit) {
            flags = h.flags;
          }
        }
        if ((flags & kNodeFocusable) != 0) {
          focus_ = hit;
          focus_visible_ = false;
        } else if (hit == kNoId) {
          focus_ = kNoId;
        }
        break;
      }
      case InputEvent::Type::kPointerUp: {
        mouse_ = e.pos;
        const Id hit = HitTest(mouse_, kNodeInteractive | kNodeFocusable);
        if (e.button == PointerButton::kRight) {
          right_clicked_ = hit;
          right_down_ = false;
          right_released_ = true;
          break;
        }
        if (e.button != PointerButton::kLeft) {
          break;
        }
        mouse_down_ = false;
        if (active_ != kNoId && hit == active_) {
          clicked_ = active_;
          if (last_click_id_ == active_ &&
              time_ - last_click_time_ < kDoubleClickSeconds) {
            double_clicked_ = active_;
            last_click_id_ = kNoId;
          } else {
            last_click_id_ = active_;
            last_click_time_ = time_;
          }
        }
        active_ = kNoId;
        break;
      }
      case InputEvent::Type::kWheel: {
        const Id target = WheelTarget(mouse_, e.wheel, e.mods);
        if (target != kNoId) {
          Vec2 &w = wheel_[target];
          w = w + e.wheel;
        }
        break;
      }
      case InputEvent::Type::kKeyDown:
        // A modifier on its own (Shift pressed or released) changes only
        // mods(): it is no key press.
        if (e.key == Key::kNone) {
          break;
        }
        focus_visible_ = true;
        if (e.key == Key::kTab && !prev_want_tab_ &&
            (e.mods & (kModCtrl | kModAlt)) == 0) {
          MoveFocus((e.mods & kModShift) != 0);
          break;
        }
        held_keys_[static_cast<std::size_t>(e.key)] = true;
        keys_.push_back({e.key, e.mods, false});
        break;
      case InputEvent::Type::kKeyUp:
        if (e.key == Key::kNone) {
          break;
        }
        held_keys_[static_cast<std::size_t>(e.key)] = false;
        break;
      case InputEvent::Type::kText:
        text_input_ += e.text;
        break;
    }
  }
  hovered_ = active_ != kNoId
                 ? active_
                 : HitTest(mouse_, kNodeInteractive | kNodeFocusable);
  if (active_ != kNoId &&
      HitTest(mouse_, kNodeInteractive | kNodeFocusable) != active_) {
    // Pressed widgets stay "active" while dragged off, but not hovered.
    hovered_ = kNoId;
  }
}

auto Context::WheelTarget(Vec2 p, Vec2 wheel, std::uint8_t mods) const -> Id {
  // Bit the turn needs: up for positive y (content moves down), down for
  // negative; horizontal turns go to the innermost node.
  const std::uint8_t need = wheel.y > 0.0F ? 1U : (wheel.y < 0.0F ? 2U : 0U);
  const bool ctrl = (mods & kModCtrl) != 0;
  Id innermost = kNoId;
  Id zoom = kNoId;
  // Whether a wheel node around the pointer scrolls at all.
  bool scrolls = false;
  for (auto it = prev_hits_.rbegin(); it != prev_hits_.rend(); ++it) {
    if (prev_modal_layer_ >= 0 && it->layer < prev_modal_layer_) {
      break;
    }
    if ((it->flags & kNodeWheel) == 0 || !it->rect.Contains(p)) {
      continue;
    }
    if ((it->flags & kNodeWheelZoom) != 0) {
      if (ctrl) {
        return it->id;
      }
      if (zoom == kNoId) {
        zoom = it->id;
      }
      continue;
    }
    if (innermost == kNoId) {
      innermost = it->id;
    }
    const auto range = prev_wheel_ranges_.find(it->id);
    const bool declared = range != prev_wheel_ranges_.end();
    if (declared && range->second != 0) {
      scrolls = true;
    }
    if (need == 0 || !declared || (range->second & need) != 0) {
      if (zoom != kNoId && declared && range->second == 0) {
        // Nothing to scroll here: the zoom node inside may take it.
        continue;
      }
      return it->id;
    }
  }
  // A zoom node with nothing scrolling around it zooms; otherwise the
  // innermost scrolling node gets the turn it cannot take (and clamps).
  if (zoom != kNoId && !scrolls) {
    return zoom;
  }
  return innermost != kNoId ? innermost : zoom;
}

void Context::WheelRange(Id id, bool can_up, bool can_down) {
  wheel_ranges_[id] =
      static_cast<std::uint8_t>((can_up ? 1U : 0U) | (can_down ? 2U : 0U));
}

auto Context::HitTest(Vec2 p, std::uint16_t flags) const -> Id {
  for (auto it = prev_hits_.rbegin(); it != prev_hits_.rend(); ++it) {
    if (prev_modal_layer_ >= 0 && it->layer < prev_modal_layer_) {
      break;
    }
    if ((it->flags & flags) != 0 && it->rect.Contains(p)) {
      return it->id;
    }
  }
  return kNoId;
}

void Context::MoveFocus(bool backwards) {
  std::vector<Id> order;
  for (const auto &[id, layer] : prev_focusables_) {
    if (prev_modal_layer_ < 0 || layer == prev_modal_layer_) {
      order.push_back(id);
    }
  }
  if (order.empty()) {
    return;
  }
  const auto it = std::find(order.begin(), order.end(), focus_);
  const auto n = static_cast<std::ptrdiff_t>(order.size());
  std::ptrdiff_t i = 0;
  if (it == order.end()) {
    i = backwards ? n - 1 : 0;
  } else {
    i = (it - order.begin() + (backwards ? n - 1 : 1)) % n;
  }
  focus_ = order[static_cast<std::size_t>(i)];
  focus_visible_ = true;
}

void Context::EndFrame() {
  // Close the base root.
  stack_.resize(1);

  // Escape nobody handled closes the topmost popup, else drops focus.
  for (KeyPress &k : keys_) {
    if (k.key == Key::kEscape && !k.consumed) {
      k.consumed = true;
      if (!open_popups_.empty()) {
        open_popups_.pop_back();
      } else {
        focus_ = kNoId;
      }
    }
  }

  for (const Layer &layer : layers_) {
    LayoutLayer(layer);
  }
  CollectOverflows();

  hits_.clear();
  infos_.clear();
  while (lists_.size() < layers_.size()) {
    lists_.push_back(std::make_unique<gfx::DrawList>());
  }
  list_ptrs_.clear();
  prev_layer_rects_.clear();
  prev_layer_ids_.clear();
  prev_modal_layer_ = -1;
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    gfx::DrawList &list = *lists_[i];
    list.Clear();
    PaintNode(layers_[i].root, list);
    list_ptrs_.push_back(&list);
    prev_layer_rects_.push_back(tree_.node(layers_[i].root).rect);
    prev_layer_ids_.push_back(layers_[i].id);
    if (i > 0 && (layers_[i].kind == LayerKind::kModal ||
                  layers_[i].kind == LayerKind::kOverlay)) {
      prev_modal_layer_ = static_cast<int>(i);
    }
  }

  // Focus on a widget that disappeared is dropped.
  if (focus_ != kNoId &&
      std::none_of(focusables_.begin(), focusables_.end(), [&](auto &f) {
        return f.first == focus_;
      })) {
    focus_ = kNoId;
  }
  // Popups that were not declared this frame are closed.
  std::erase_if(open_popups_, [&](Id id) {
    return std::none_of(
        layers_.begin(), layers_.end(), [&](auto &l) { return l.id == id; });
  });

  // This frame's results are what the next frame's input resolves against
  // (and what LastRect() reports from now on).
  prev_hits_.swap(hits_);
  prev_wheel_ranges_.swap(wheel_ranges_);
  wheel_ranges_.clear();
  prev_infos_.swap(infos_);
  prev_overflows_.swap(overflows_);
  prev_focusables_.swap(focusables_);
}

void Context::LayoutLayer(const Layer &layer) {
  TextMeasurer measurer(*this);
  const Rect screen{0, 0, viewport_.x, viewport_.y};
  if (layer.root == 0) {
    tree_.Compute(layer.root, screen, measurer);
    return;
  }
  switch (layer.kind) {
    case LayerKind::kModal:
    case LayerKind::kOverlay:
      tree_.Compute(layer.root, screen, measurer);
      break;
    case LayerKind::kFloating:
      tree_.Compute(
          layer.root,
          Rect{layer.anchor.x0, layer.anchor.y0, viewport_.x, viewport_.y},
          measurer);
      break;
    case LayerKind::kTooltip: {
      Rect avail{mouse_.x + 12, mouse_.y + 18, viewport_.x, viewport_.y};
      tree_.Compute(layer.root, avail, measurer);
      const Rect r = tree_.node(layer.root).rect;
      if (r.x1 > viewport_.x || r.y1 > viewport_.y) {
        avail.x0 = std::max(0.0F, std::min(avail.x0, viewport_.x - r.Width()));
        avail.y0 = r.y1 > viewport_.y
                       ? std::max(0.0F, mouse_.y - 4 - r.Height())
                       : avail.y0;
        tree_.Compute(layer.root, avail, measurer);
      }
      break;
    }
    case LayerKind::kPopup: {
      const Rect &a = layer.anchor;
      Rect avail{a.x0, a.y1, viewport_.x, viewport_.y};
      tree_.Compute(layer.root, avail, measurer);
      const LayoutNode &root = tree_.node(layer.root);
      const float below = viewport_.y - a.y1;
      const float above = a.y0;
      Rect placed = avail;
      if (root.pref_h > below && above > below) {
        const float h = std::min(root.pref_h, above);
        placed = Rect{a.x0, a.y0 - h, viewport_.x, a.y0};
      }
      // The popup's own width, not what is left right of the anchor: one
      // anchored near the right edge moves left instead of squeezing.
      const float w =
          std::min(std::max(root.pref_w, root.rect.Width()), viewport_.x);
      if (placed.x0 + w > viewport_.x) {
        placed.x0 = std::max(0.0F, viewport_.x - w);
      }
      if (placed != avail) {
        tree_.Compute(layer.root, placed, measurer);
      }
      break;
    }
  }
}

void Context::PaintNode(int index, gfx::DrawList &list) {
  const Node &node = nodes_[index];
  const LayoutNode &ln = tree_.node(index);
  const Rect &r = ln.rect;
  const LayoutSpec &spec = ln.spec;
  const Vec2 inner{r.Width() - spec.padding.left - spec.padding.right,
                   r.Height() - spec.padding.top - spec.padding.bottom};
  if (node.id != kNoId) {
    infos_[node.id] = {r, ln.content, inner};
    const bool tooltip =
        layers_[node.layer].kind == LayerKind::kTooltip && node.layer > 0;
    if (!tooltip &&
        (node.flags & (kNodeInteractive | kNodeFocusable | kNodeWheel)) != 0) {
      hits_.push_back(
          {node.id, r.Intersect(list.clip()), node.flags, node.layer});
    }
  }

  const NodeStyle &s = node.style;
  if (s.shadow.a > 0.0F && s.shadow_blur > 0.0F) {
    list.Shadow(r, s.shadow, s.shadow_blur, s.shadow_offset, s.radii);
  }
  if (s.material == Material::kGlass) {
    const GlassStyle &g = theme_.glass;
    list.FillGlass(
        r, g.Look(), g.fallback.a > 0.0F ? g.fallback : s.fill, s.radii);
  } else if (s.fill.a > 0.0F || s.fill_bottom.a > 0.0F) {
    if (s.fill_bottom.a > 0.0F) {
      list.FillRectGradient(r, s.fill, s.fill_bottom, s.radii);
    } else {
      list.FillRect(r, s.fill, s.radii);
    }
  }
  if (s.border_width > 0.0F && s.border.a > 0.0F) {
    list.StrokeRect(r, s.border, s.border_width, s.radii);
  }
  if (node.text >= 0) {
    const Rect content{r.x0 + spec.padding.left,
                       r.y0 + spec.padding.top,
                       r.x1 - spec.padding.right,
                       r.y1 - spec.padding.bottom};
    PaintText(texts_[node.text], content, list);
  }
  PaintContext pc{list, text_, theme_, &icon_cache_};
  if (node.painter_before >= 0) {
    painters_[node.painter_before](pc, r);
  }
  const bool clip =
      (node.flags & kNodeClip) != 0 || spec.scroll_x || spec.scroll_y;
  if (clip) {
    list.PushClip(r);
  }
  for (int c = ln.first_child; c >= 0; c = tree_.node(c).next_sibling) {
    PaintNode(c, list);
  }
  if (clip) {
    list.PopClip();
  }
  if (node.painter_after >= 0) {
    painters_[node.painter_after](pc, r);
  }
  if ((node.flags & kNodeFocusRing) != 0 && HasFocus(node.id) &&
      focus_visible_) {
    const float w = theme_.metrics.focus_width;
    gfx::CornerRadii radii = s.radii;
    radii.top_left += w;
    radii.top_right += w;
    radii.bottom_right += w;
    radii.bottom_left += w;
    list.StrokeRect(r.Inset(-w), theme_.colors.focus, w, radii);
  }
}

void Context::CollectOverflows() {
  overflows_.clear();
  // Half a pixel of rounding is no overflow.
  constexpr float kSlack = 0.5F;
  const Rect screen{0, 0, viewport_.x, viewport_.y};
  auto outside = [](const Rect &r, const Rect &b, bool x, bool y) {
    return (x && (r.x0 < b.x0 - kSlack || r.x1 > b.x1 + kSlack)) ||
           (y && (r.y0 < b.y0 - kSlack || r.y1 > b.y1 + kSlack));
  };
  auto owner = [&](int i) {
    for (; i >= 0; i = tree_.node(i).parent) {
      if (nodes_[i].id != kNoId) {
        return nodes_[i].id;
      }
    }
    return kNoId;
  };
  auto add = [&](int i, const Rect &bounds) {
    overflows_.push_back(
        {nodes_[i].id, owner(i), tree_.node(i).rect, bounds, FirstText(i)});
  };
  for (int i = 0; i < tree_.size(); ++i) {
    const LayoutNode &n = tree_.node(i);
    if (n.rect.Empty()) {
      // Nothing drawn.
      continue;
    }
    if (n.parent < 0) {
      // Tooltips place themselves; other roots stay on the screen.
      if (layers_[nodes_[i].layer].kind != LayerKind::kTooltip &&
          outside(n.rect, screen, true, true)) {
        add(i, screen);
      }
    } else {
      const LayoutNode &p = tree_.node(n.parent);
      const Padding &pad = p.spec.padding;
      const Rect inner{p.rect.x0 + pad.left,
                       p.rect.y0 + pad.top,
                       p.rect.x1 - pad.right,
                       p.rect.y1 - pad.bottom};
      // A scrolled axis may hold more than shows: scrolling reaches it.
      if (outside(n.rect, inner, !p.spec.scroll_x, !p.spec.scroll_y)) {
        add(i, inner);
        continue;
      }
    }
    if (nodes_[i].text >= 0) {
      const TextItem &item = texts_[nodes_[i].text];
      const float h =
          n.rect.Height() - n.spec.padding.top - n.spec.padding.bottom;
      if (item.spec.wrap && item.wrapped.Height() > h + kSlack) {
        add(i, n.rect);
      }
    }
  }
}

auto Context::FirstText(int index) const -> std::string {
  if (nodes_[index].text >= 0) {
    return std::string(TextOf(texts_[nodes_[index].text]));
  }
  for (int c = tree_.node(index).first_child; c >= 0;
       c = tree_.node(c).next_sibling) {
    std::string text = FirstText(c);
    if (!text.empty()) {
      return text;
    }
  }
  return {};
}

void Context::PaintText(const TextItem &item, const Rect &rect,
                        gfx::DrawList &list) {
  const std::string_view text = TextOf(item);
  const TextSpec &spec = item.spec;
  const text::ShapedText &shaped = text_.Shape(text, spec.font, spec.size);
  const text::FontMetrics m = text_.fonts().Metrics(spec.font, spec.size);
  const float line_h = m.LineHeight();
  auto x_for = [&](float width) -> float {
    switch (spec.align) {
      case TextAlign::kCenter:
        return std::round(rect.x0 + (rect.Width() - width) * 0.5F);
      case TextAlign::kRight:
        return std::round(rect.x1 - width);
      default:
        return rect.x0;
    }
  };
  if (spec.wrap && !item.wrapped.lines.empty()) {
    float y = rect.y0 + std::round(m.ascent);
    for (const text::TextLine &line : item.wrapped.lines) {
      text_.DrawRange(list,
                      shaped,
                      line.begin,
                      line.end,
                      {x_for(line.width), y},
                      spec.color);
      y += line_h;
    }
    return;
  }
  const text::HAlign align =
      spec.align == TextAlign::kCenter
          ? text::HAlign::kCenter
          : (spec.align == TextAlign::kRight ? text::HAlign::kRight
                                             : text::HAlign::kLeft);
  text_.DrawFitted(list, text, spec.font, spec.size, rect, spec.color, align);
}

// Ids.

auto Context::GetId(std::string_view label) const -> Id {
  return HashString(label, id_stack_.back());
}

auto Context::GetId(std::uint64_t value) const -> Id {
  return HashCombine(id_stack_.back(), value);
}

void Context::PushId(std::string_view label) {
  id_stack_.push_back(GetId(label));
}

void Context::PushId(std::uint64_t value) {
  id_stack_.push_back(GetId(value));
}

void Context::PopId() {
  if (id_stack_.size() > 1) {
    id_stack_.pop_back();
  }
}

// Tree.

auto Context::Open(const NodeDesc &desc) -> int {
  const int parent = stack_.back();
  const int index = tree_.Add(parent, desc.layout);
  Node &node = nodes_.emplace_back();
  node.id = desc.id;
  node.style = desc.style;
  node.flags = desc.flags;
  node.layer = layer_stack_.back();
  if ((desc.flags & kNodeFocusable) != 0 && desc.id != kNoId) {
    focusables_.emplace_back(desc.id, node.layer);
  }
  stack_.push_back(index);
  return index;
}

void Context::Close() {
  if (stack_.size() > 1) {
    stack_.pop_back();
  }
}

auto Context::OpenBackground() const -> Color {
  for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
    if (*it >= 0 && nodes_[*it].style.fill.a >= 1.0F &&
        nodes_[*it].style.material != Material::kGlass) {
      return nodes_[*it].style.fill;
    }
  }
  return theme_.colors.background;
}

auto Context::Text(const TextSpec &spec, const LayoutSpec &layout, Id id)
    -> int {
  TextItem item;
  item.offset = static_cast<std::uint32_t>(text_arena_.size());
  item.length = static_cast<std::uint32_t>(spec.text.size());
  item.spec = spec;
  item.spec.text = {};
  text_arena_.append(spec.text);
  texts_.push_back(std::move(item));
  const int index =
      tree_.Add(stack_.back(), layout, static_cast<int>(texts_.size() - 1));
  Node &node = nodes_.emplace_back();
  node.id = id;
  node.text = static_cast<int>(texts_.size() - 1);
  node.layer = layer_stack_.back();
  return index;
}

void Context::AddPainter(int node, Painter painter, bool before_children) {
  painters_.push_back(std::move(painter));
  const int index = static_cast<int>(painters_.size() - 1);
  if (before_children) {
    nodes_[node].painter_before = index;
  } else {
    nodes_[node].painter_after = index;
  }
}

void Context::BeginLayer(LayerKind kind, Id id, const Rect &anchor) {
  const int layer = static_cast<int>(layers_.size());
  layers_.push_back(Layer{kind, id, anchor, -1});
  layer_stack_.push_back(layer);
  stack_.push_back(-1);
  const Palette &c = theme_.colors;
  NodeDesc root;
  root.id = HashCombine(id, 0x6C61796572ULL);
  switch (kind) {
    case LayerKind::kModal:
      root.layout.mode = LayoutMode::kStack;
      root.layout.width = SizeSpec::Fixed(viewport_.x);
      root.layout.height = SizeSpec::Fixed(viewport_.y);
      root.layout.align = Align::kCenter;
      root.style.fill = c.scrim;
      // The scrim swallows clicks.
      root.flags = kNodeInteractive;
      break;
    case LayerKind::kOverlay:
      root.layout.mode = LayoutMode::kStack;
      root.layout.width = SizeSpec::Fixed(viewport_.x);
      root.layout.height = SizeSpec::Fixed(viewport_.y);
      // Transparent, but it swallows clicks.
      root.flags = kNodeInteractive;
      break;
    case LayerKind::kPopup:
    case LayerKind::kTooltip:
    case LayerKind::kFloating:
      root.layout.width =
          SizeSpec::Fit(kind == LayerKind::kPopup ? anchor.Width() : 0.0F);
      root.layout.height = SizeSpec::Fit();
      root.layout.padding = Padding::All(kind == LayerKind::kPopup ? 4 : 6);
      root.layout.align = Align::kStretch;
      root.style.fill = c.surface_raised;
      root.style.material = theme_.MaterialOf(Surface::kMenu);
      root.style.border = c.border;
      root.style.border_width = theme_.metrics.border_width;
      root.style.radii = gfx::CornerRadii::All(theme_.metrics.radius);
      root.style.shadow = c.shadow;
      root.style.shadow_blur = 8;
      root.style.shadow_offset = {0, 3};
      root.flags = kind == LayerKind::kTooltip ? kNodeNone : kNodeInteractive;
      break;
  }
  layers_.back().root = Open(root);
}

void Context::EndLayer() {
  // Pop the layer root and the -1 sentinel.
  if (stack_.size() > 2) {
    stack_.pop_back();
    stack_.pop_back();
  }
  if (layer_stack_.size() > 1) {
    layer_stack_.pop_back();
  }
}

// Interaction.

auto Context::Interact(Id id) const -> Interaction {
  Interaction result;
  if (id == kNoId) {
    return result;
  }
  auto it = prev_infos_.find(id);
  if (it != prev_infos_.end()) {
    result.rect = it->second.rect;
  }
  result.hovered = hovered_ == id;
  result.pressed = active_ == id && mouse_down_;
  result.clicked = clicked_ == id;
  result.double_clicked = double_clicked_ == id;
  result.right_clicked = right_clicked_ == id;
  result.right_pressed = right_pressed_ == id;
  result.focused = focus_ == id;
  result.local = mouse_ - result.rect.Min();
  if (result.pressed) {
    result.drag = mouse_ - press_pos_;
  }
  return result;
}

auto Context::LastRect(Id id) const -> std::optional<Rect> {
  auto it = prev_infos_.find(id);
  if (it == prev_infos_.end()) {
    return std::nullopt;
  }
  return it->second.rect;
}

auto Context::LastContentSize(Id id) const -> Vec2 {
  auto it = prev_infos_.find(id);
  return it == prev_infos_.end() ? Vec2{} : it->second.content;
}

auto Context::LastInnerSize(Id id) const -> Vec2 {
  auto it = prev_infos_.find(id);
  return it == prev_infos_.end() ? Vec2{} : it->second.inner;
}

auto Context::Wheel(Id id) const -> Vec2 {
  auto it = wheel_.find(id);
  return it == wheel_.end() ? Vec2{} : it->second;
}

// Keyboard.

void Context::SetFocus(Id id, bool visible) {
  focus_ = id;
  focus_visible_ = visible;
}

auto Context::ConsumeKey(Key key, std::uint8_t mods, bool ignore_shift)
    -> bool {
  for (KeyPress &k : keys_) {
    std::uint8_t m = k.mods;
    if (ignore_shift) {
      m &= static_cast<std::uint8_t>(~kModShift);
    }
    if (!k.consumed && k.key == key && m == mods) {
      k.consumed = true;
      return true;
    }
  }
  return false;
}

auto Context::IsKeyDown(Key key) const -> bool {
  return held_keys_[static_cast<std::size_t>(key)];
}

auto Context::ConsumeKeyAnyMods(Key key, std::uint8_t &mods) -> bool {
  for (KeyPress &k : keys_) {
    if (!k.consumed && k.key == key) {
      k.consumed = true;
      mods = k.mods;
      return true;
    }
  }
  return false;
}

auto Context::TakeText() -> std::string {
  std::string text;
  text.swap(text_input_);
  return text;
}

// Popups.

auto Context::IsPopupOpen(Id id) const -> bool {
  return std::find(open_popups_.begin(), open_popups_.end(), id) !=
         open_popups_.end();
}

void Context::OpenPopup(Id id) {
  if (!IsPopupOpen(id)) {
    open_popups_.push_back(id);
  }
}

void Context::ClosePopup(Id id) {
  std::erase(open_popups_, id);
}

}  // namespace gloomy::ui
