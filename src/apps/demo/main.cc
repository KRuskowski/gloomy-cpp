// gloomy_demo: draws a small scene in a window, or headless into a PNG.
//
//   gloomy_demo                       open a window (SDL2)
//   gloomy_demo --screenshot out.png  render offscreen and exit

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/gfx/offscreen_target.h"
#include "gloomy/gfx/swapchain.h"
#include "gloomy/gfx/ui_renderer.h"
#include "gloomy/text/font_set.h"
#include "gloomy/text/text_renderer.h"

#ifdef GLOOMY_HAVE_SDL2
#include <SDL.h>

#include "gloomy/platform/sdl_window.h"
#endif

namespace {

using gloomy::Color;
using gloomy::Rect;

void DrawScene(gloomy::gfx::DrawList &list, gloomy::text::TextSystem &text,
               const gloomy::text::DefaultFonts &fonts, float width,
               float height) {
  list.Clear();
  const Rect panel{16, 16, width - 16, height - 16};
  list.Shadow(panel,
              Color::Rgba(0x00000080),
              12,
              {0, 6},
              gloomy::gfx::CornerRadii::All(10));
  list.FillRect(panel, Color::Rgb(0x313244), gloomy::gfx::CornerRadii::All(10));
  list.StrokeRect(
      panel, Color::Rgb(0x45475A), 1, gloomy::gfx::CornerRadii::All(10));
  text.DrawText(
      list, "gloomy-cpp", fonts.bold, 22, {32, 28}, Color::Rgb(0x89B4FA));
  text.DrawText(list,
                "Raw Vulkan, SDF primitives, HarfBuzz text",
                fonts.sans,
                14,
                {32, 60},
                Color::Rgb(0xCDD6F4));
  float y = 96;
  for (int row = 0; row < 8; ++row) {
    const Rect r{32, y, width - 32, y + 24};
    if (row % 2 == 1) {
      list.FillRect(r, Color::Rgb(0x2A2B3C));
    }
    char line[96];
    std::snprintf(line,
                  sizeof(line),
                  "Station %02d   Ore   %7d Cr",
                  row + 1,
                  1200 + row * 137);
    text.DrawText(
        list, line, fonts.mono, 13, {40, y + 4}, Color::Rgb(0xBAC2DE));
    y += 24;
  }
}

auto Screenshot(const std::string &path) -> int {
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
  gloomy::gfx::UiRenderer renderer(
      **ctx, gloomy::gfx::OffscreenTarget::kFormat, atlas);
  gloomy::gfx::OffscreenTarget target(**ctx, {480, 320});
  gloomy::gfx::DrawList list;
  DrawScene(list, text, ids, 480, 320);
  const std::array<const gloomy::gfx::DrawList *, 1> lists{&list};
  const gloomy::Image image = gloomy::gfx::RenderToImage(
      **ctx, renderer, target, lists, Color::Rgb(0x1E1E2E));
  if (auto saved = gloomy::SavePng(image, path); !saved) {
    std::fprintf(stderr, "%s\n", saved.error().c_str());
    return 1;
  }
  return (*ctx)->validation_message_count() == 0 ? 0 : 2;
}

#ifdef GLOOMY_HAVE_SDL2
auto RunWindow() -> int {
  auto window = gloomy::platform::SdlWindow::Create("gloomy demo", 960, 640);
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
  gloomy::gfx::Swapchain swapchain(**ctx, (*window)->DrawableExtent());
  gloomy::gfx::UiRenderer renderer(**ctx,
                                   swapchain.format(),
                                   atlas,
                                   gloomy::gfx::Swapchain::kFramesInFlight);
  gloomy::gfx::DrawList list;
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      }
    }
    const VkExtent2D extent = (*window)->DrawableExtent();
    auto frame = swapchain.BeginFrame(extent);
    if (!frame) {
      SDL_Delay(10);
      continue;
    }
    text.BeginFrame();
    DrawScene(list,
              text,
              ids,
              static_cast<float>(frame->extent.width),
              static_cast<float>(frame->extent.height));
    const std::array<const gloomy::gfx::DrawList *, 1> lists{&list};
    renderer.Prepare(frame->cmd, frame->slot, lists);
    swapchain.BeginRendering(Color::Rgb(0x1E1E2E));
    renderer.Draw(frame->cmd, frame->slot, frame->extent);
    swapchain.EndFrame();
  }
  return 0;
}
#endif

}  // namespace

auto main(int argc, char **argv) -> int {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      return Screenshot(argv[i + 1]);
    }
  }
#ifdef GLOOMY_HAVE_SDL2
  return RunWindow();
#else
  std::fprintf(stderr, "built without SDL2; use --screenshot <file.png>\n");
  return 1;
#endif
}
