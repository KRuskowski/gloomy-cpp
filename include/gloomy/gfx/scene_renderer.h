#ifndef GLOOMY_GFX_SCENE_RENDERER_H_
#define GLOOMY_GFX_SCENE_RENDERER_H_

#include <vulkan/vulkan.h>

#include <cstdint>

#include "gloomy/gfx/backdrop_blur.h"

namespace gloomy::gfx {

/// The frame's colour image a host scene renders into, under the UI.
struct ColorTarget {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{};
};

/// A host engine's scene drawn under the UI in gloomy's own render loop
/// (app::ShellOptions::scene). Per frame the shell asks Active(); if so it
/// transitions the frame's image to COLOR_ATTACHMENT_OPTIMAL, calls
/// Record() outside any rendering scope, blurs the returned backdrop for
/// glass and draws the UI over the image with LOAD. See docs/design.md
/// section 6.5.
class SceneRenderer {
 public:
  virtual ~SceneRenderer() = default;

  /// Whether there is a scene this frame; if not the frame is cleared to
  /// the theme's background as without a scene, and glass is solid.
  [[nodiscard]] virtual auto Active() const -> bool = 0;

  /// Records the scene into every pixel of `target`, which is in
  /// COLOR_ATTACHMENT_OPTIMAL with undefined contents and must be left
  /// there with the writes done before COLOR_ATTACHMENT_OUTPUT (the UI
  /// loads it). `slot` is the frame slot (its previous use has completed).
  /// @return The scene image glass blurs (see Backdrop for its state), or
  ///     a Backdrop with a null view for solid glass.
  virtual auto Record(VkCommandBuffer cmd, std::uint32_t slot,
                      const ColorTarget &target) -> Backdrop = 0;
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_SCENE_RENDERER_H_
