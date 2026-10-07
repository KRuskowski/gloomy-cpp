#include "app_shell.h"

#include <chrono>
#include <cstdio>
#include <cstring>
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

namespace gloomy::apps {

namespace {

using Clock = std::chrono::steady_clock;

auto Ms(Clock::duration d) -> double {
  return std::chrono::duration<double, std::milli>(d).count();
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
  ui::Context ui(text_system, ui::Theme::Dark(info.fonts));
  const Vec2 viewport{static_cast<float>(options.size.width),
                      static_cast<float>(options.size.height)};
  int frame = 0;
  auto run_frame = [&] {
    const auto start = Clock::now();
    ui.BeginFrame(viewport, {}, 1.0 + frame / 60.0);
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
  ui::Context ui(text_system, ui::Theme::Dark(info.fonts));
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
    running = (*window)->PollEvents(events);
    auto frame = swapchain.BeginFrame((*window)->DrawableExtent());
    if (!frame) {
      continue;
    }
    const auto now = Clock::now();
    info.frame_ms = Ms(now - last);
    last = now;
    ui.BeginFrame({static_cast<float>(frame->extent.width),
                   static_cast<float>(frame->extent.height)},
                  events,
                  std::chrono::duration<double>(now - start).count());
    options.frame(ui, info);
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

}  // namespace gloomy::apps
