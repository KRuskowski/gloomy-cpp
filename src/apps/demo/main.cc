// gloomy_demo: the widget gallery in a window, or headless into a PNG.
//
//   gloomy_demo                          open a window (SDL2)
//   gloomy_demo --screenshot out.png     render offscreen and exit
//   gloomy_demo --size 960x600           window or screenshot size

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "demo_ui.h"
#include "gloomy/core/atlas.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/swapchain.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/text/font_set.h"
#include "gloomy/text/text_renderer.h"
#include "gloomy/ui/context.h"
#include "gloomy/ui/theme.h"

#ifdef GLOOMY_HAVE_SDL2
#include "gloomy/platform/sdl_window.h"
#endif

namespace {

using gloomy::demo::DemoState;

// Applies the theme picked in the demo UI.
void SyncTheme(gloomy::ui::Context &ctx, const gloomy::text::DefaultFonts &f,
               int theme) {
  const char *names[] = {"Dark", "Light", "High Contrast"};
  if (ctx.theme().name == names[theme]) {
    return;
  }
  switch (theme) {
    case 1:
      ctx.SetTheme(gloomy::ui::Theme::Light(f));
      break;
    case 2:
      ctx.SetTheme(gloomy::ui::Theme::HighContrast(f));
      break;
    default:
      ctx.SetTheme(gloomy::ui::Theme::Dark(f));
      break;
  }
}

auto Screenshot(const std::string &path, VkExtent2D size) -> int {
  gloomy::gfx::DeviceOptions options;
  options.validation = true;
  auto ctx = gloomy::gfx::DeviceContext::Create(options);
  if (!ctx) {
    std::fprintf(stderr, "%s\n", ctx.error().c_str());
    return 1;
  }
  std::printf("device: %s\n", (*ctx)->device_name().c_str());
  gloomy::text::FontSet fonts;
  const gloomy::text::DefaultFonts ids = fonts.AddDefaultFonts();
  gloomy::Atlas atlas(1024, 4);
  gloomy::text::TextSystem text(fonts, atlas);
  gloomy::ui::Context ui(text, gloomy::ui::Theme::Dark(ids));
  DemoState state;
  const gloomy::Vec2 viewport{static_cast<float>(size.width),
                              static_cast<float>(size.height)};
  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(viewport, {}, frame / 60.0);
    gloomy::demo::DemoUi(ui, state);
    ui.EndFrame();
  }
  gloomy::gfx::UiRenderer renderer(
      **ctx, gloomy::gfx::OffscreenTarget::kFormat, atlas);
  gloomy::gfx::OffscreenTarget target(**ctx, size);
  const gloomy::Image image = gloomy::gfx::RenderToImage(
      **ctx, renderer, target, ui.draw_lists(), ui.theme().colors.background);
  if (auto saved = gloomy::SavePng(image, path); !saved) {
    std::fprintf(stderr, "%s\n", saved.error().c_str());
    return 1;
  }
  return (*ctx)->validation_message_count() == 0 ? 0 : 2;
}

#ifdef GLOOMY_HAVE_SDL2
auto RunWindow(VkExtent2D size) -> int {
  auto window =
      gloomy::platform::SdlWindow::Create("gloomy demo",
                                          static_cast<int>(size.width),
                                          static_cast<int>(size.height));
  if (!window) {
    std::fprintf(stderr, "%s\n", window.error().c_str());
    return 1;
  }
  gloomy::gfx::DeviceOptions options;
  options.validation = true;
  options.instance_extensions = (*window)->RequiredInstanceExtensions();
  options.create_surface = [&](VkInstance instance) {
    return (*window)->CreateSurface(instance);
  };
  auto ctx = gloomy::gfx::DeviceContext::Create(options);
  if (!ctx) {
    std::fprintf(stderr, "%s\n", ctx.error().c_str());
    return 1;
  }
  gloomy::text::FontSet fonts;
  const gloomy::text::DefaultFonts ids = fonts.AddDefaultFonts();
  gloomy::Atlas atlas(1024, 4);
  gloomy::text::TextSystem text(fonts, atlas);
  gloomy::ui::Context ui(text, gloomy::ui::Theme::Dark(ids));
  gloomy::platform::SdlWindow::InstallClipboard(ui);
  gloomy::gfx::Swapchain swapchain(**ctx, (*window)->DrawableExtent());
  gloomy::gfx::UiRenderer renderer(**ctx,
                                   swapchain.format(),
                                   atlas,
                                   gloomy::gfx::Swapchain::kFramesInFlight);
  DemoState state;
  const auto start = std::chrono::steady_clock::now();
  std::vector<gloomy::ui::InputEvent> events;
  bool running = true;
  while (running) {
    events.clear();
    running = (*window)->PollEvents(events);
    const VkExtent2D extent = (*window)->DrawableExtent();
    auto frame = swapchain.BeginFrame(extent);
    if (!frame) {
      continue;
    }
    const double now =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    SyncTheme(ui, ids, state.theme);
    ui.BeginFrame({static_cast<float>(frame->extent.width),
                   static_cast<float>(frame->extent.height)},
                  events,
                  now);
    gloomy::demo::DemoUi(ui, state);
    ui.EndFrame();
    renderer.Prepare(frame->cmd, frame->slot, ui.draw_lists());
    swapchain.BeginRendering(ui.theme().colors.background);
    renderer.Draw(frame->cmd, frame->slot, frame->extent);
    swapchain.EndFrame();
  }
  return 0;
}
#endif

}  // namespace

auto main(int argc, char **argv) -> int {
  VkExtent2D size{960, 600};
  std::string screenshot;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      screenshot = argv[++i];
    } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
      unsigned w = 0;
      unsigned h = 0;
      if (std::sscanf(argv[++i], "%ux%u", &w, &h) == 2) {
        size = {w, h};
      }
    }
  }
  if (!screenshot.empty()) {
    return Screenshot(screenshot, size);
  }
#ifdef GLOOMY_HAVE_SDL2
  return RunWindow(size);
#else
  std::fprintf(stderr, "built without SDL2; use --screenshot <file.png>\n");
  return 1;
#endif
}
