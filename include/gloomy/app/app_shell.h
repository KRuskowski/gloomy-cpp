#ifndef GLOOMY_APP_APP_SHELL_H_
#define GLOOMY_APP_APP_SHELL_H_

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gloomy/text/font_set.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/input.h"
#include "gloomy/ui/theme.h"

namespace gloomy::app {

/// Per-frame information for the app's UI function.
struct FrameInfo {
  text::DefaultFonts fonts;
  /// CPU time of the previous frame (UI build, layout, paint) in ms.
  double ui_ms = 0.0;
  /// Wall time between the previous two frames in ms.
  double frame_ms = 0.0;
  /// Seconds since the start: wall time in a window; 1/60 s per frame
  /// headless, so that headless runs are deterministic.
  double time = 0.0;
  /// Seconds since the previous frame (0 for the first).
  double dt = 0.0;
  /// Frames run before this one.
  std::int64_t frame = 0;
  /// Rendering offscreen for a screenshot.
  bool headless = false;
  std::string device_name;
};

/// How to run an app.
struct ShellOptions {
  std::string title = "gloomy";
  VkExtent2D size{1280, 800};
  /// Non-empty: render headless into this PNG and exit.
  std::string screenshot;
  /// Frames run before the screenshot is taken.
  int screenshot_frames = 4;
  bool validation = true;
  /// The theme to start with; Theme::Dark() when empty.
  std::function<ui::Theme(const text::DefaultFonts &)> theme;
  /// Headless only: frames continue (a few ms apart, up to 5 s) until this
  /// returns true, so background queries finish before the screenshot.
  std::function<bool()> settled;
  /// Headless only: synthetic input for a frame (scripted key presses for
  /// screenshots of screens reached by keyboard).
  std::function<void(std::int64_t frame, std::vector<ui::InputEvent> &)>
      headless_input;
  /// Windowed only: the app asks to quit when this returns true.
  std::function<bool()> quit;
  /// Declares the UI for one frame.
  std::function<void(ui::Context &, const FrameInfo &)> frame;
};

/// Parses --screenshot <png>, --size WxH, --frames N and --no-validation
/// into `options`; other arguments are left for the app.
void ParseShellArgs(int argc, char **argv, ShellOptions &options);

/// Runs the app in an SDL2 window, or headless for a screenshot.
/// @return Process exit code; 2 if validation reported problems.
auto RunApp(const ShellOptions &options) -> int;

/// Parses a key name as used in scripts and keymaps: "Down", "Enter",
/// "PageUp", "F5", "A", "Period", "Ctrl+K", "Shift+Tab"; "Release+W" is
/// the key's release (a key pressed by name stays held until then).
/// @return The key press, or a KeyDown of Key::kNone for an unknown name.
auto ParseKeyName(std::string_view name) -> ui::InputEvent;

/// Parses a comma-separated list of key names ("Down,Down,Enter").
/// @return False (and `out` partly filled) if a name is unknown.
auto ParseKeyList(std::string_view list, std::vector<ui::InputEvent> &out)
    -> bool;

}  // namespace gloomy::app

#endif  // GLOOMY_APP_APP_SHELL_H_
