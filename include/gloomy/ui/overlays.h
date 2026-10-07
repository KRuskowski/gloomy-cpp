#ifndef GLOOMY_UI_OVERLAYS_H_
#define GLOOMY_UI_OVERLAYS_H_

#include <deque>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/summary.h"

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

void MenuSeparator(Context &ctx);

void EndContextMenu(Context &ctx);

/// Declares a modal dialog when `open`: a titled panel over a dimmed
/// screen, keyboard focus kept inside. The close button, or an Escape no
/// widget inside used, sets `open` to false. Content follows; then
/// EndDialog() with the same flag.
/// @return True if the dialog is open (only then call EndDialog()).
auto BeginDialog(Context &ctx, std::string_view label, std::string_view title,
                 bool &open, float width = 420.0F) -> bool;

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
