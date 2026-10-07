#include "gloomy/platform/sdl_window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

namespace gloomy::platform {

auto SdlWindow::Create(const std::string &title, int width, int height)
    -> std::expected<std::unique_ptr<SdlWindow>, std::string> {
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    return std::unexpected(std::string("SDL_Init: ") + SDL_GetError());
  }
  SDL_Window *window = SDL_CreateWindow(
      title.c_str(),
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      width,
      height,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == nullptr) {
    return std::unexpected(std::string("SDL_CreateWindow: ") + SDL_GetError());
  }
  return std::unique_ptr<SdlWindow>(new SdlWindow(window));
}

SdlWindow::~SdlWindow() {
  SDL_DestroyWindow(window_);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

auto SdlWindow::RequiredInstanceExtensions() const -> std::vector<std::string> {
  unsigned count = 0;
  SDL_Vulkan_GetInstanceExtensions(window_, &count, nullptr);
  std::vector<const char *> names(count);
  SDL_Vulkan_GetInstanceExtensions(window_, &count, names.data());
  return {names.begin(), names.end()};
}

auto SdlWindow::CreateSurface(VkInstance instance) const -> VkSurfaceKHR {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (SDL_Vulkan_CreateSurface(window_, instance, &surface) == SDL_FALSE) {
    return VK_NULL_HANDLE;
  }
  return surface;
}

auto SdlWindow::DrawableExtent() const -> VkExtent2D {
  int w = 0;
  int h = 0;
  SDL_Vulkan_GetDrawableSize(window_, &w, &h);
  return {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
}

}  // namespace gloomy::platform
