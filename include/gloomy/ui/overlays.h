#ifndef GLOOMY_UI_OVERLAYS_H_
#define GLOOMY_UI_OVERLAYS_H_

#include <deque>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/summary.h"
#include "gloomy/ui/widgets.h"

namespace gloomy::ui {

// Menus, dialogs, toasts and split panes (docs/design.md section 11.1).

/// Opens context menu `label` at the pointer; call on a right click.
void OpenContextMenu(Context &ctx, std::string_view label);

/// Declares context menu `label` if it is open; then MenuItem()s and
/// EndContextMenu(). Up/Down move, Enter activates, Escape closes.
/// @return True if the menu is open (only then call EndContextMenu()).
auto BeginContextMenu(Context &ctx, std::string_view label) -> bool;

/// @return True when chosen; the menu then closes.
auto MenuItem(Context &ctx, std::string_view text,
              std::string_view shortcut = {}, bool enabled = true) -> bool;

/// A menu item with a second, muted line under its text.
struct MenuItemSpec {
  std::string_view text;
  std::string_view shortcut;
  /// What the item does or, when it is disabled, why not.
  std::string_view hint;
  bool enabled = true;
};

/// @return True when chosen; the menu then closes.
auto MenuItem(Context &ctx, const MenuItemSpec &item) -> bool;

void MenuSeparator(Context &ctx);

/// A small muted heading inside a menu, grouping the items after it.
void MenuHeading(Context &ctx, std::string_view text);

void EndContextMenu(Context &ctx);

/// A button that opens a menu under it ("More" actions, "Views"):
/// declares the button with a chevron and, while the menu is open, starts
/// it; then MenuItem()s and EndMenuButton(). Enter, Space or Down on the
/// focused button open it; Up/Down move, Enter chooses, Escape or a click
/// outside closes it.
/// @return True if the menu is open (only then call EndMenuButton()).
auto BeginMenuButton(Context &ctx, std::string_view label,
                     ButtonKind kind = ButtonKind::kNormal) -> bool;

void EndMenuButton(Context &ctx);

/// Declares a modal dialog when `open`: a titled panel over a dimmed
/// screen, keyboard focus kept inside. The close button, or an Escape no
/// widget inside used, sets `open` to false. Content follows; then
/// EndDialog() with the same flag.
/// @return True if the dialog is open (only then call EndDialog()).
auto BeginDialog(Context &ctx, std::string_view label, std::string_view title,
                 bool &open, float width = 420.0F) -> bool;

/// How a dialog looks: its width and, answer first, a summary under the
/// title before the inputs.
struct DialogOptions {
  float width = 420.0F;
  /// One line under the title: what the form will do, or what is missing.
  std::string_view summary;
  Tone tone = Tone::kNeutral;
  /// A muted line under the summary.
  std::string_view detail;
};

/// BeginDialog() with a summary: the answer first, then the inputs.
auto BeginDialog(Context &ctx, std::string_view label, std::string_view title,
                 bool &open, const DialogOptions &opt) -> bool;

void EndDialog(Context &ctx, bool &open);

/// Short notifications shown at the bottom right.
class ToastQueue {
 public:
  void Show(std::string text, Tone tone = Tone::kNeutral, double seconds = 3.0);

 private:
  friend void Toasts(Context &ctx, ToastQueue &queue);

  struct Toast {
    std::string text;
    Tone tone;
    double seconds;
    double until = -1.0;
  };

  std::deque<Toast> toasts_;
};

/// Draws the queue's toasts and drops expired ones; call once per frame.
void Toasts(Context &ctx, ToastQueue &queue);

/// Divider position of a split pane.
struct SplitState {
  /// Share of the first pane, 0..1.
  float ratio = 0.5F;
  bool dragging = false;
  float start_ratio = 0.5F;
};

/// Opens two panes side by side (or stacked when `stacked`), separated by
/// a draggable divider; keyboard: focus the divider, Left/Right (Up/Down)
/// move it. Declare the first pane, call SplitNext(), the second pane,
/// then EndSplit().
void BeginSplit(Context &ctx, std::string_view label, SplitState &state,
                bool stacked = false, LayoutSpec layout = {});

void SplitNext(Context &ctx, SplitState &state);

void EndSplit(Context &ctx);

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_OVERLAYS_H_
