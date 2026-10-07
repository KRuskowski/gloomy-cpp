#ifndef GLOOMY_APPS_COMMON_APP_SHELL_H_
#define GLOOMY_APPS_COMMON_APP_SHELL_H_

#include <vulkan/vulkan.h>

#include <functional>
#include <string>
#include <thread>

#include "gloomy/text/font_set.h"
#include "gloomy/ui/context.h"

namespace gloomy::apps {

/// Per-frame information for the app's UI function.
struct FrameInfo {
  text::DefaultFonts fonts;
  /// CPU time of the previous frame (UI build, layout, paint) in ms.
  double ui_ms = 0.0;
  /// Wall time between the previous two frames in ms.
  double frame_ms = 0.0;
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
  /// Headless only: frames continue (a few ms apart, up to 5 s) until this
  /// returns true, so background queries finish before the screenshot.
  std::function<bool()> settled;
  /// Declares the UI for one frame.
  std::function<void(ui::Context &, const FrameInfo &)> frame;
};

/// Parses --screenshot <png>, --size WxH and --frames N into `options`.
void ParseShellArgs(int argc, char **argv, ShellOptions &options);

/// Runs the app in an SDL2 window, or headless for a screenshot.
/// @return Process exit code; 2 if validation reported problems.
auto RunApp(const ShellOptions &options) -> int;

}  // namespace gloomy::apps

#endif  // GLOOMY_APPS_COMMON_APP_SHELL_H_
