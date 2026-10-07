#ifndef GLOOMY_GFX_SWAPCHAIN_H_
#define GLOOMY_GFX_SWAPCHAIN_H_

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "gloomy/core/color.h"
#include "gloomy/gfx/device_context.h"

namespace gloomy::gfx {

/// A swapchain on the context's surface plus the per-frame command
/// buffers and synchronisation of a standalone render loop. Hosts that own
/// their frame loop do not need this; they call UiRenderer directly.
class Swapchain {
 public:
  /// Frames recorded while earlier ones are still on the GPU.
  static constexpr std::uint32_t kFramesInFlight = 2;

  /// What BeginFrame() hands out for recording.
  struct Frame {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    /// Frame slot in [0, kFramesInFlight) for UiRenderer.
    std::uint32_t slot = 0;
    VkExtent2D extent{};
  };

  /// @param[in] vsync FIFO presentation if true, else MAILBOX or IMMEDIATE
  ///     when available.
  Swapchain(DeviceContext &ctx, VkExtent2D extent, bool vsync = true);
  Swapchain(const Swapchain &) = delete;
  auto operator=(const Swapchain &) -> Swapchain & = delete;
  ~Swapchain();

  auto format() const -> VkFormat { return format_; }

  /// Waits for the slot, acquires an image and begins the command buffer.
  /// @param[in] extent The window's current drawable size; the swapchain
  ///     is recreated when it changed.
  /// @return std::nullopt when nothing can be drawn (minimised window).
  auto BeginFrame(VkExtent2D extent) -> std::optional<Frame>;

  /// Transitions the acquired image and begins dynamic rendering on it.
  void BeginRendering(const Color &clear);

  /// Ends rendering, submits and presents.
  void EndFrame();

 private:
  struct Slot {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
  };

  void Create(VkExtent2D extent);

  void Destroy();

  DeviceContext &ctx_;
  bool vsync_;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<VkImageView> views_;
  // One per image: a semaphore may only be reused once its present is done,
  // which is known when the image is acquired again.
  std::vector<VkSemaphore> rendered_;
  Slot slots_[kFramesInFlight];
  std::uint32_t slot_ = 0;
  std::uint32_t image_index_ = 0;
  bool needs_recreate_ = false;
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_SWAPCHAIN_H_
