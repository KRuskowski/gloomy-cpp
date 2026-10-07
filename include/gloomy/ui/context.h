#ifndef GLOOMY_UI_CONTEXT_H_
#define GLOOMY_UI_CONTEXT_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/core/geometry.h"
#include "gloomy/core/id.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/input.h"
#include "gloomy/ui/layout.h"
#include "gloomy/ui/theme.h"

namespace gloomy::ui {

/// How a node is painted before its children.
struct NodeStyle {
  Color fill{0, 0, 0, 0};
  /// Bottom colour of a vertical gradient; ignored when alpha is 0.
  Color fill_bottom{0, 0, 0, 0};
  gfx::CornerRadii radii;
  Color border{0, 0, 0, 0};
  float border_width = 0.0F;
  Color shadow{0, 0, 0, 0};
  float shadow_blur = 0.0F;
  Vec2 shadow_offset;
};

enum class TextAlign : std::uint8_t { kLeft, kCenter, kRight };

/// Text content of a leaf node.
struct TextSpec {
  std::string_view text;
  text::FontId font = 0;
  float size = 13.0F;
  Color color;
  TextAlign align = TextAlign::kLeft;
  /// Wrap at the node's width; otherwise one line, cut with an ellipsis
  /// when too long.
  bool wrap = false;
};

/// Node behaviour bits.
enum NodeFlag : std::uint16_t {
  kNodeNone = 0,
  /// Takes part in hover and click hit testing.
  kNodeInteractive = 1U << 0U,
  /// Reachable with Tab; clicking it focuses it.
  kNodeFocusable = 1U << 1U,
  /// Receives mouse wheel input (scroll containers).
  kNodeWheel = 1U << 2U,
  /// Clips its children to its rectangle.
  kNodeClip = 1U << 3U,
  /// Draws a focus ring when it has keyboard focus.
  kNodeFocusRing = 1U << 4U,
};

/// Everything Open() needs to create a node.
struct NodeDesc {
  Id id = kNoId;
  LayoutSpec layout;
  NodeStyle style;
  std::uint16_t flags = kNodeNone;
};

/// What a widget sees of its node's interaction this frame, resolved
/// against the previous frame's layout.
struct Interaction {
  bool hovered = false;
  /// The pointer was pressed on the node and is still down.
  bool pressed = false;
  /// Pressed and released over the node this frame.
  bool clicked = false;
  bool double_clicked = false;
  bool right_clicked = false;
  bool focused = false;
  /// Pointer position relative to the node's top-left corner.
  Vec2 local;
  /// Pointer movement since the press, while pressed.
  Vec2 drag;
  /// The node's rectangle in the previous frame.
  Rect rect;
};

/// Where an overlay layer goes.
enum class LayerKind : std::uint8_t {
  /// Below (or above, if there is no room) an anchor rectangle; closes on
  /// clicks outside and on Escape.
  kPopup,
  /// Near the pointer; never takes input.
  kTooltip,
  /// Centred over a dimmed screen; blocks input to everything below and
  /// keeps keyboard focus inside.
  kModal,
};

/// What custom painters get.
struct PaintContext {
  gfx::DrawList &list;
  text::TextSystem &text;
  const Theme &theme;
};

/// Paints part of a node; called once the node's rectangle is known.
using Painter = std::function<void(PaintContext &, const Rect &)>;

/// Replaceable clipboard access (the platform layer provides one).
struct Clipboard {
  std::function<std::string()> get;
  std::function<void(const std::string &)> set;
};

/// The immediate-mode UI context: collects the node tree declared each
/// frame, resolves input against the previous frame's layout, keeps
/// per-id widget state, lays out and paints. See docs/design.md section 4.
class Context {
 public:
  Context(text::TextSystem &text, Theme theme);
  Context(const Context &) = delete;
  auto operator=(const Context &) -> Context & = delete;
  ~Context();

  // Frame.

  /// Starts a frame: applies `events` to the previous frame's layout and
  /// opens the root node covering `viewport`.
  /// @param[in] time Monotonic seconds, for double clicks, blinking and
  ///     timeouts.
  void BeginFrame(Vec2 viewport, std::span<const InputEvent> events,
                  double time);

  /// Closes the tree, lays out every layer and paints into the draw lists.
  void EndFrame();

  /// Draw lists of the last frame, bottom layer first.
  auto draw_lists() const -> std::span<const gfx::DrawList *const> {
    return list_ptrs_;
  }

  // Ids.

  /// Id of `label` under the current id stack. Text after "##" in a label
  /// is part of the id but not shown.
  auto GetId(std::string_view label) const -> Id;

  auto GetId(std::uint64_t value) const -> Id;

  void PushId(std::string_view label);

  void PushId(std::uint64_t value);

  void PopId();

  // Tree.

  /// Opens a node as child of the current one; Close() ends it.
  /// @return The node index (valid until EndFrame()).
  auto Open(const NodeDesc &desc) -> int;

  void Close();

  /// Adds a text leaf.
  auto Text(const TextSpec &text, const LayoutSpec &layout = {}, Id id = kNoId)
      -> int;

  /// Adds a painter to node `node` that runs after its children (or
  /// before them).
  void AddPainter(int node, Painter painter, bool before_children = false);

  /// Opens an overlay layer with its own root; EndLayer() closes it.
  /// @param[in] anchor For popups: the rectangle to attach to.
  void BeginLayer(LayerKind kind, Id id, const Rect &anchor = {});

  void EndLayer();

  // Interaction.

  /// Interaction state of node `id` this frame.
  auto Interact(Id id) const -> Interaction;

  /// Rectangle of `id` in the previous frame.
  auto LastRect(Id id) const -> std::optional<Rect>;

  /// Content and inner size of a scroll node in the previous frame.
  auto LastContentSize(Id id) const -> Vec2;

  auto LastInnerSize(Id id) const -> Vec2;

  auto mouse() const -> Vec2 { return mouse_; }

  auto mouse_down() const -> bool { return mouse_down_; }

  /// Where the pointer was last pressed.
  auto press_pos() const -> Vec2 { return press_pos_; }

  /// Modifier keys of the latest input event.
  auto mods() const -> std::uint8_t { return mods_; }

  /// Modifier keys held at the last pointer press.
  auto press_mods() const -> std::uint8_t { return press_mods_; }

  /// Wheel movement (lines) over the innermost wheel node `id` this frame.
  auto Wheel(Id id) const -> Vec2;

  auto time() const -> double { return time_; }

  auto frame() const -> std::uint64_t { return frame_; }

  // Keyboard.

  auto focused() const -> Id { return focus_; }

  auto HasFocus(Id id) const -> bool { return focus_ == id && id != kNoId; }

  /// Moves keyboard focus to `id`.
  /// @param[in] visible Show the focus ring (keyboard-driven focus).
  void SetFocus(Id id, bool visible = true);

  /// True if the focus came from the keyboard (draw focus rings).
  auto focus_visible() const -> bool { return focus_visible_; }

  /// Takes an unconsumed key press of `key` with exactly `mods` (shift is
  /// ignored if `ignore_shift`) from this frame's events.
  auto ConsumeKey(Key key, std::uint8_t mods = kModNone,
                  bool ignore_shift = false) -> bool;

  /// Like ConsumeKey(), but reports the modifiers instead of matching them.
  auto ConsumeKeyAnyMods(Key key, std::uint8_t &mods) -> bool;

  /// Text typed this frame; taking it empties it.
  auto TakeText() -> std::string;

  /// Text typed this frame without taking it.
  auto PeekText() const -> const std::string & { return text_input_; }

  /// Claims Tab for the focused widget this frame (text areas).
  void WantTab() { want_tab_ = true; }

  // Popups.

  /// True while popup `id` is open.
  auto IsPopupOpen(Id id) const -> bool;

  void OpenPopup(Id id);

  void ClosePopup(Id id);

  /// True if the current (or last) pointer press closed popup `id` by
  /// landing outside it; the popup's owner must not reopen it on the
  /// click that follows.
  auto PopupClosedByPress(Id id) const -> bool {
    return popup_closed_by_press_ == id && id != kNoId;
  }

  // Widget state.

  /// Persistent state of type T for `id`, created on first use and dropped
  /// when `id` is not used for a while.
  template <typename T>
  auto State(Id id) -> T & {
    StateSlot &slot = states_[id];
    if (!slot.value || slot.type != &typeid(T)) {
      slot.value = std::make_unique<StateHolder<T>>();
      slot.type = &typeid(T);
    }
    slot.last_frame = frame_;
    return static_cast<StateHolder<T> *>(slot.value.get())->value;
  }

  // Services.

  auto theme() const -> const Theme & { return theme_; }

  void SetTheme(Theme theme) { theme_ = std::move(theme); }

  auto text() -> text::TextSystem & { return text_; }

  auto clipboard() -> Clipboard & { return clipboard_; }

  /// The current parent node index.
  auto current() const -> int { return stack_.back(); }

  /// Number of nodes declared this frame.
  auto node_count() const -> int { return tree_.size(); }

 private:
  struct Node {
    Id id = kNoId;
    NodeStyle style;
    std::uint16_t flags = kNodeNone;
    int text = -1;
    int painter_before = -1;
    int painter_after = -1;
    int layer = 0;
  };

  struct TextItem {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    TextSpec spec;
    text::WrappedText wrapped;
  };

  struct Layer {
    LayerKind kind = LayerKind::kPopup;
    Id id = kNoId;
    Rect anchor;
    int root = -1;
  };

  struct HitEntry {
    Id id;
    Rect rect;
    std::uint16_t flags;
    int layer;
  };

  struct NodeInfo {
    Rect rect;
    Vec2 content;
    Vec2 inner;
  };

  struct StateBase {
    virtual ~StateBase() = default;
  };

  template <typename T>
  struct StateHolder : StateBase {
    T value{};
  };

  struct StateSlot {
    std::unique_ptr<StateBase> value;
    const std::type_info *type = nullptr;
    std::uint64_t last_frame = 0;
  };

  struct KeyPress {
    Key key;
    std::uint8_t mods;
    bool consumed;
  };

  class TextMeasurer;

  void ProcessEvents(std::span<const InputEvent> events);

  auto HitTest(Vec2 p, std::uint16_t flags) const -> Id;

  void MoveFocus(bool backwards);

  void LayoutLayer(const Layer &layer);

  void PaintNode(int index, gfx::DrawList &list);

  void PaintText(const TextItem &item, const Rect &rect, gfx::DrawList &list);

  auto TextOf(const TextItem &item) const -> std::string_view {
    return {text_arena_.data() + item.offset, item.length};
  }

  text::TextSystem &text_;
  Theme theme_;
  Clipboard clipboard_;

  // Frame being built.
  LayoutTree tree_;
  std::vector<Node> nodes_;
  std::vector<int> stack_;
  std::vector<Id> id_stack_;
  std::vector<TextItem> texts_;
  std::string text_arena_;
  std::vector<Painter> painters_;
  std::vector<Layer> layers_;
  std::vector<int> layer_stack_;
  std::vector<std::pair<Id, int>> focusables_;
  Vec2 viewport_;
  std::uint64_t frame_ = 0;
  double time_ = 0.0;

  // Results of the previous frame, used for input.
  std::vector<HitEntry> hits_;
  std::vector<HitEntry> prev_hits_;
  std::unordered_map<Id, NodeInfo> infos_;
  std::unordered_map<Id, NodeInfo> prev_infos_;
  std::vector<std::pair<Id, int>> prev_focusables_;
  int prev_modal_layer_ = -1;
  std::vector<Rect> prev_layer_rects_;
  std::vector<Id> prev_layer_ids_;

  // Input state.
  Vec2 mouse_;
  bool mouse_down_ = false;
  Vec2 press_pos_;
  std::uint8_t press_mods_ = kModNone;
  std::uint8_t mods_ = kModNone;
  Id hovered_ = kNoId;
  Id active_ = kNoId;
  Id clicked_ = kNoId;
  Id double_clicked_ = kNoId;
  Id right_clicked_ = kNoId;
  Id last_click_id_ = kNoId;
  double last_click_time_ = -1.0;
  std::unordered_map<Id, Vec2> wheel_;
  std::vector<KeyPress> keys_;
  std::string text_input_;
  Id focus_ = kNoId;
  bool focus_visible_ = false;
  bool want_tab_ = false;
  bool prev_want_tab_ = false;
  std::vector<Id> open_popups_;
  Id popup_closed_by_press_ = kNoId;

  // State store.
  std::unordered_map<Id, StateSlot> states_;

  // Output.
  std::vector<std::unique_ptr<gfx::DrawList>> lists_;
  std::vector<const gfx::DrawList *> list_ptrs_;
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_CONTEXT_H_
