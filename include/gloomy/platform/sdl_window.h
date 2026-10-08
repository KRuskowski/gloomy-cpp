#ifndef GLOOMY_PLATFORM_SDL_WINDOW_H_
#define GLOOMY_PLATFORM_SDL_WINDOW_H_

#include <vulkan/vulkan.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gloomy/ui/context.h"
#include "gloomy/ui/input.h"

struct SDL_Window;
union SDL_Event;

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

  /// Drains SDL's event queue into `out`.
  /// @return False when the window was asked to close.
  auto PollEvents(std::vector<ui::InputEvent> &out) -> bool;

  /// Pixels per window coordinate (HiDPI).
  auto PixelScale() const -> float;

  /// Posts a quit event, as closing the window does.
  static void PostQuit();

  /// Routes the context's clipboard to the system clipboard.
  static void InstallClipboard(ui::Context &ctx);

  auto window() const -> SDL_Window * { return window_; }

 private:
  explicit SdlWindow(SDL_Window *window) : window_(window) {}

  SDL_Window *window_;
};

/// Translates one SDL event; std::nullopt for events the UI ignores.
/// @param[in] scale Pixels per window coordinate.
auto TranslateEvent(const SDL_Event &event, float scale)
    -> std::optional<ui::InputEvent>;

}  // namespace gloomy::platform

#endif  // GLOOMY_PLATFORM_SDL_WINDOW_H_
