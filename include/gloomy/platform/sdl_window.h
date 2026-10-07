#ifndef GLOOMY_PLATFORM_SDL_WINDOW_H_
#define GLOOMY_PLATFORM_SDL_WINDOW_H_

#include <vulkan/vulkan.h>

#include <expected>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace gloomy::platform {

/// An SDL2 window that Vulkan can present to. The only place gloomy sees
/// SDL; a host engine replaces this module.
class SdlWindow {
 public:
  /// Initialises SDL video and opens a resizable Vulkan window.
  static auto Create(const std::string &title, int width, int height)
      -> std::expected<std::unique_ptr<SdlWindow>, std::string>;

  SdlWindow(const SdlWindow &) = delete;
  auto operator=(const SdlWindow &) -> SdlWindow & = delete;
  ~SdlWindow();

  /// Instance extensions the surface needs.
  auto RequiredInstanceExtensions() const -> std::vector<std::string>;

  /// Creates the presentation surface; VK_NULL_HANDLE on failure.
  auto CreateSurface(VkInstance instance) const -> VkSurfaceKHR;

  /// Size of the drawable area in pixels.
  auto DrawableExtent() const -> VkExtent2D;

  auto window() const -> SDL_Window * { return window_; }

 private:
  explicit SdlWindow(SDL_Window *window) : window_(window) {}

  SDL_Window *window_;
};

}  // namespace gloomy::platform

#endif  // GLOOMY_PLATFORM_SDL_WINDOW_H_
