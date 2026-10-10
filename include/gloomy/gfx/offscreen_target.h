#ifndef GLOOMY_GFX_OFFSCREEN_TARGET_H_
#define GLOOMY_GFX_OFFSCREEN_TARGET_H_

#include <vulkan/vulkan.h>

#include <span>

#include "gloomy/core/color.h"
#include "gloomy/core/image.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/draw_list.h"
#include "gloomy/gfx/scene_renderer.h"

namespace gloomy::gfx {

class UiRenderer;

/// A colour image to render into without a window, plus a host buffer the
/// result is copied to for reading back. Used by headless tests and
/// screenshots.
class OffscreenTarget {
 public:
  /// The format UiRenderer pipelines for offscreen targets must use.
  static constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;

  OffscreenTarget(DeviceContext &ctx, VkExtent2D extent);

  /// Transitions the image for a host scene to draw into before the UI
  /// (SceneRenderer); Begin() must then load it.
  auto BeginScene(VkCommandBuffer cmd) -> ColorTarget;

  /// Transitions the image and begins dynamic rendering, cleared to
  /// `clear`; with `load` (after BeginScene()) it keeps what the scene
  /// drew instead.
  void Begin(VkCommandBuffer cmd, const Color &clear, bool load = false);

  /// Ends rendering and, with `readback`, records the copy into the
  /// readback buffer.
  void End(VkCommandBuffer cmd, bool readback = true);

  /// The pixels of the last completed End(). Call after the submission
  /// finished.
  auto Read() const -> Image;

  auto extent() const -> VkExtent2D { return image_.extent(); }

 private:
  Image2D image_;
  Buffer readback_;
};

/// Renders `lists` into `target` on frame slot 0 and waits for the result.
/// For tests and screenshots.
auto RenderToImage(DeviceContext &ctx, UiRenderer &renderer,
                   OffscreenTarget &target,
                   std::span<const DrawList *const> lists, const Color &clear)
    -> Image;

/// Renders `scene` into `target`, blurs its backdrop with `blur` at
/// `blur_levels` for glass, draws `lists` over it and waits for the
/// result. For screenshots of a host scene under the UI.
auto RenderSceneToImage(DeviceContext &ctx, UiRenderer &renderer,
                        BackdropBlur &blur, SceneRenderer &scene,
                        OffscreenTarget &target,
                        std::span<const DrawList *const> lists, int blur_levels)
    -> Image;

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_OFFSCREEN_TARGET_H_
