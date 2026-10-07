#ifndef GLOOMY_UI_DOCK_H_
#define GLOOMY_UI_DOCK_H_

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// Where a panel goes relative to a tab stack.
enum class DockSide : std::uint8_t { kCenter, kLeft, kRight, kTop, kBottom };

/// A node of a dock layout: a split of two children, or a stack of tabbed
/// panels.
struct DockNode {
  bool split = false;
  /// Split: children above each other instead of side by side.
  bool stacked = false;
  /// Split: share of the first child.
  float ratio = 0.5F;
  int first = -1;
  int second = -1;
  /// Stack: panel ids in tab order and the active one.
  std::vector<std::string> panels;
  int active = 0;
};

/// The arrangement of panels: a tree of splits and tab stacks. A plain
/// value: save it with Encode(), restore it with Decode()
/// (docs/design.md section 11.1).
class DockLayout {
 public:
  /// One stack holding `panels`.
  explicit DockLayout(std::vector<std::string> panels = {});

  /// Docks `panel` next to (or, with kCenter, into) the stack that holds
  /// `target`. A panel already in the layout moves.
  void Dock(const std::string &panel, const std::string &target, DockSide side,
            float ratio = 0.5F);

  /// Removes `panel`; an emptied stack disappears and its sibling takes
  /// the space.
  void Close(const std::string &panel);

  /// Makes `panel` the active tab of its stack.
  void Activate(const std::string &panel);

  /// Index of the stack holding `panel`, -1 if absent.
  auto StackOf(std::string_view panel) const -> int;

  /// Stacks in layout order (left to right, top to bottom).
  auto Stacks() const -> std::vector<int>;

  auto nodes() -> std::vector<DockNode> & { return nodes_; }

  auto nodes() const -> const std::vector<DockNode> & { return nodes_; }

  auto root() const -> int { return root_; }

  /// One line, e.g. "s:h:0.3(t:0:markets,ships)(t:0:contract)".
  auto Encode() const -> std::string;

  static auto Decode(std::string_view text) -> std::optional<DockLayout>;

 private:
  void Remove(int stack);

  void Compact();

  std::vector<DockNode> nodes_;
  int root_ = 0;
};

/// Drag-to-dock and splitter state of a DockSpace; owned by the app.
struct DockState {
  /// Panel being dragged by its tab, if any.
  std::string dragging;
  bool drag_active = false;
  /// Split dragging, per split node.
  int split_dragging = -1;
  float split_start = 0.0F;
  /// Stack that last had keyboard focus (F6 cycles).
  int focused_stack = -1;
};

/// Draws the content of panel `id`.
using PanelFn = std::function<void(Context &, const std::string &id)>;

/// Title of panel `id` for its tab.
using PanelTitleFn = std::function<std::string(const std::string &id)>;

/// Lays out `layout`: splits with draggable dividers, tab stacks whose
/// panels `draw` declares. Tabs switch on click, close with their x button
/// (when `closable`), and drag onto another stack to dock there (center:
/// as a tab, edges: as a split), with a preview while dragging. F6 and
/// Shift+F6 move between stacks, Ctrl+Tab and Ctrl+Shift+Tab between the
/// tabs of the active stack.
void DockSpace(Context &ctx, std::string_view label, DockLayout &layout,
               DockState &state, const PanelFn &draw,
               const PanelTitleFn &title = {}, bool closable = true,
               LayoutSpec layout_spec = {});

/// Id of the tab of `panel` in DockSpace `label` (stable while the panel
/// moves around the layout).
auto DockTabId(Context &ctx, std::string_view label, std::string_view panel)
    -> Id;

/// Id of the close button of `panel`'s tab.
auto DockCloseId(Context &ctx, std::string_view label, std::string_view panel)
    -> Id;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_DOCK_H_
