#include "gloomy/app/app_shell.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/swapchain.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/theme.h"

#ifdef GLOOMY_HAVE_SDL2
#include "gloomy/platform/sdl_window.h"
#endif

namespace gloomy::app {

namespace {

using Clock = std::chrono::steady_clock;

auto Ms(Clock::duration d) -> double {
  return std::chrono::duration<double, std::milli>(d).count();
}

auto StartTheme(const ShellOptions &options, const text::DefaultFonts &fonts)
    -> ui::Theme {
  return options.theme ? options.theme(fonts) : ui::Theme::Dark(fonts);
}

auto Headless(const ShellOptions &options) -> int {
  gfx::DeviceOptions device_options;
  device_options.app_name = options.title;
  device_options.validation = options.validation;
  auto ctx = gfx::DeviceContext::Create(device_options);
  if (!ctx) {
    std::fprintf(stderr, "%s\n", ctx.error().c_str());
    return 1;
  }
  text::FontSet fonts;
  FrameInfo info;
  info.fonts = fonts.AddDefaultFonts();
  info.device_name = (*ctx)->device_name();
  Atlas atlas(1024, 4);
  text::TextSystem text_system(fonts, atlas);
  ui::Context ui(text_system, StartTheme(options, info.fonts));
  info.headless = true;
  const Vec2 viewport{static_cast<float>(options.size.width),
                      static_cast<float>(options.size.height)};
  std::int64_t frame = 0;
  std::vector<ui::InputEvent> events;
  auto run_frame = [&] {
    const auto start = Clock::now();
    events.clear();
    if (options.headless_input) {
      options.headless_input(frame, events);
    }
    const double time = 1.0 + static_cast<double>(frame) / 60.0;
    info.dt = frame == 0 ? 0.0 : time - info.time;
    info.time = time;
    info.frame = frame;
    ui.BeginFrame(viewport, events, time);
    options.frame(ui, info);
    ui.EndFrame();
    info.ui_ms = Ms(Clock::now() - start);
    ++frame;
  };
  for (int i = 0; i < options.screenshot_frames; ++i) {
    run_frame();
  }
  if (options.settled) {
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    while (!options.settled() && Clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      run_frame();
    }
    // A few more so results that arrived are laid out.
    for (int i = 0; i < 3; ++i) {
      run_frame();
    }
  }
  gfx::UiRenderer renderer(**ctx, gfx::OffscreenTarget::kFormat, atlas);
  gfx::OffscreenTarget target(**ctx, options.size);
  const Image image = gfx::RenderToImage(
      **ctx, renderer, target, ui.draw_lists(), ui.theme().colors.background);
  if (auto saved = SavePng(image, options.screenshot); !saved) {
    std::fprintf(stderr, "%s\n", saved.error().c_str());
    return 1;
  }
  std::printf("%s: %s, last frame %.2f ms UI\n",
              options.screenshot.c_str(),
              info.device_name.c_str(),
              info.ui_ms);
  return (*ctx)->validation_message_count() == 0 ? 0 : 2;
}

#ifdef GLOOMY_HAVE_SDL2
auto Windowed(const ShellOptions &options) -> int {
  auto window =
      platform::SdlWindow::Create(options.title,
                                  static_cast<int>(options.size.width),
                                  static_cast<int>(options.size.height));
  if (!window) {
    std::fprintf(stderr, "%s\n", window.error().c_str());
    return 1;
  }
  gfx::DeviceOptions device_options;
  device_options.app_name = options.title;
  device_options.validation = options.validation;
  device_options.instance_extensions = (*window)->RequiredInstanceExtensions();
  device_options.create_surface = [&](VkInstance instance) {
    return (*window)->CreateSurface(instance);
  };
  auto ctx = gfx::DeviceContext::Create(device_options);
  if (!ctx) {
    std::fprintf(stderr, "%s\n", ctx.error().c_str());
    return 1;
  }
  text::FontSet fonts;
  FrameInfo info;
  info.fonts = fonts.AddDefaultFonts();
  info.device_name = (*ctx)->device_name();
  Atlas atlas(1024, 4);
  text::TextSystem text_system(fonts, atlas);
  ui::Context ui(text_system, StartTheme(options, info.fonts));
  platform::SdlWindow::InstallClipboard(ui);
  gfx::Swapchain swapchain(**ctx, (*window)->DrawableExtent());
  gfx::UiRenderer renderer(
      **ctx, swapchain.format(), atlas, gfx::Swapchain::kFramesInFlight);
  const auto start = Clock::now();
  auto last = start;
  std::vector<ui::InputEvent> events;
  bool running = true;
  while (running) {
    events.clear();
    running =
        (*window)->PollEvents(events) && !(options.quit && options.quit());
    auto frame = swapchain.BeginFrame((*window)->DrawableExtent());
    if (!frame) {
      continue;
    }
    const auto now = Clock::now();
    info.frame_ms = Ms(now - last);
    info.dt = info.frame == 0 ? 0.0 : info.frame_ms / 1000.0;
    last = now;
    info.time = std::chrono::duration<double>(now - start).count();
    ui.BeginFrame({static_cast<float>(frame->extent.width),
                   static_cast<float>(frame->extent.height)},
                  events,
                  info.time);
    options.frame(ui, info);
    ++info.frame;
    ui.EndFrame();
    info.ui_ms = Ms(Clock::now() - now);
    renderer.Prepare(frame->cmd, frame->slot, ui.draw_lists());
    swapchain.BeginRendering(ui.theme().colors.background);
    renderer.Draw(frame->cmd, frame->slot, frame->extent);
    swapchain.EndFrame();
  }
  return (*ctx)->validation_message_count() == 0 ? 0 : 2;
}
#endif

}  // namespace

void ParseShellArgs(int argc, char **argv, ShellOptions &options) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      options.screenshot = argv[++i];
    } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
      unsigned w = 0;
      unsigned h = 0;
      if (std::sscanf(argv[++i], "%ux%u", &w, &h) == 2) {
        options.size = {w, h};
      }
    } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      options.screenshot_frames = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--no-validation") == 0) {
      options.validation = false;
    }
  }
}

auto ParseKeyName(std::string_view name) -> ui::InputEvent {
  using ui::Key;
  // "Release+W" lets go of a held key (scripted steering).
  constexpr std::string_view kRelease = "Release+";
  if (name.starts_with(kRelease)) {
    ui::InputEvent e = ParseKeyName(name.substr(kRelease.size()));
    e.type = ui::InputEvent::Type::kKeyUp;
    return e;
  }
  std::uint8_t mods = ui::kModNone;
  // Modifier prefixes: "Ctrl+", "Shift+", "Alt+".
  for (;;) {
    if (name.starts_with("Ctrl+")) {
      mods |= ui::kModCtrl;
      name.remove_prefix(5);
    } else if (name.starts_with("Shift+")) {
      mods |= ui::kModShift;
      name.remove_prefix(6);
    } else if (name.starts_with("Alt+")) {
      mods |= ui::kModAlt;
      name.remove_prefix(4);
    } else {
      break;
    }
  }

  struct Named {
    std::string_view name;
    Key key;
  };

  static constexpr Named kNames[] = {
      {"Tab", Key::kTab},
      {"Enter", Key::kEnter},
      {"Escape", Key::kEscape},
      {"Esc", Key::kEscape},
      {"Backspace", Key::kBackspace},
      {"Delete", Key::kDelete},
      {"Insert", Key::kInsert},
      {"Left", Key::kLeft},
      {"Right", Key::kRight},
      {"Up", Key::kUp},
      {"Down", Key::kDown},
      {"Home", Key::kHome},
      {"End", Key::kEnd},
      {"PageUp", Key::kPageUp},
      {"PageDown", Key::kPageDown},
      {"Space", Key::kSpace},
      {"Slash", Key::kSlash},
      {"Period", Key::kPeriod},
      {"Comma", Key::kComma},
      {"Minus", Key::kMinus},
      {"Equals", Key::kEquals},
      {"Plus", Key::kEquals},
  };
  for (const Named &n : kNames) {
    if (name == n.name) {
      return ui::InputEvent::KeyDown(n.key, mods);
    }
  }
  if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') {
    return ui::InputEvent::KeyDown(
        static_cast<Key>(static_cast<int>(Key::kA) + (name[0] - 'A')), mods);
  }
  if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') {
    return ui::InputEvent::KeyDown(
        static_cast<Key>(static_cast<int>(Key::k0) + (name[0] - '0')), mods);
  }
  if (name.size() >= 2 && name.size() <= 3 && name[0] == 'F') {
    const int n = std::atoi(std::string(name.substr(1)).c_str());
    if (n >= 1 && n <= 12) {
      return ui::InputEvent::KeyDown(
          static_cast<Key>(static_cast<int>(Key::kF1) + n - 1), mods);
    }
  }
  return ui::InputEvent::KeyDown(Key::kNone, mods);
}

auto ParseKeyList(std::string_view list, std::vector<ui::InputEvent> &out)
    -> bool {
  std::size_t start = 0;
  while (start <= list.size()) {
    const std::size_t end = std::min(list.find(',', start), list.size());
    const ui::InputEvent e = ParseKeyName(list.substr(start, end - start));
    if (e.key == ui::Key::kNone) {
      return false;
    }
    out.push_back(e);
    start = end + 1;
  }
  return true;
}

auto RunApp(const ShellOptions &options) -> int {
  if (!options.screenshot.empty()) {
    return Headless(options);
  }
#ifdef GLOOMY_HAVE_SDL2
  return Windowed(options);
#else
  std::fprintf(stderr, "built without SDL2; use --screenshot <file.png>\n");
  return 1;
#endif
}

}  // namespace gloomy::app
