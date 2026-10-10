#ifndef GLOOMY_GFX_BACKDROP_BLUR_H_
#define GLOOMY_GFX_BACKDROP_BLUR_H_

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

#include "gloomy/gfx/device_context.h"

namespace gloomy::gfx {

/// A host scene's colour image that glass shows blurred. The host leaves it
/// in SHADER_READ_ONLY_OPTIMAL with its writes made visible to fragment
/// shader reads (a barrier from its last write to
/// VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT / SHADER_SAMPLED_READ) before
/// BackdropBlur::Record(). See docs/design.md section 6.5.
struct Backdrop {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkExtent2D extent{};
};

/// Blurs a Backdrop once per frame for frosted glass: dual-filter (Kawase)
/// downsampling to 1/2, 1/4, ... of its size and upsampling back to 1/2,
/// so a large radius costs a few passes over small images (cheap at 4K).
/// The result is what UiRenderer::SetBackdrop() takes.
class BackdropBlur {
 public:
  /// Most downsampling levels; each roughly doubles the blur radius.
  static constexpr int kMaxLevels = 6;

  /// The format of the blurred images.
  static constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;

  /// @param[in] frames_in_flight Frame slots, as for UiRenderer.
  explicit BackdropBlur(DeviceContext &ctx, std::uint32_t frames_in_flight = 2);
  BackdropBlur(const BackdropBlur &) = delete;
  auto operator=(const BackdropBlur &) -> BackdropBlur & = delete;
  ~BackdropBlur();

  /// Downsampling levels for a blur radius in pixels of the source:
  /// 1 up to 4 px, one more per doubling, at most kMaxLevels.
  [[nodiscard]] static auto LevelsForRadius(float radius) -> int;

  /// Records the blur of `source` on frame slot `slot`, outside any
  /// rendering scope. The images follow the source's size (recreated after
  /// waiting for the device when it changes).
  /// @return The blurred image's view, half the source's size, in
  ///     SHADER_READ_ONLY_OPTIMAL and visible to fragment shaders.
  auto Record(VkCommandBuffer cmd, std::uint32_t slot, const Backdrop &source,
              int levels) -> VkImageView;

  /// Size of the result for the last source.
  [[nodiscard]] auto result_extent() const -> VkExtent2D {
    return levels_.empty() ? VkExtent2D{} : levels_[0].extent();
  }

 private:
  void CreatePipelines();

  void Resize(VkExtent2D source);

  /// One full-screen pass from `from` (size `from_extent`) into level
  /// `to`, with the down or up filter.
  void Pass(VkCommandBuffer cmd, VkDescriptorSet set, VkImageView from,
            VkExtent2D from_extent, int to, bool up);

  DeviceContext &ctx_;
  std::uint32_t frames_;
  VkExtent2D source_extent_{};
  /// Level k holds the source at 1 / 2^(k + 1).
  std::vector<Image2D> levels_;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  /// Per frame slot, one set per pass (kMaxLevels down, kMaxLevels up).
  std::vector<std::array<VkDescriptorSet, 2 * kMaxLevels>> sets_;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkPipeline down_ = VK_NULL_HANDLE;
  VkPipeline up_ = VK_NULL_HANDLE;
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_BACKDROP_BLUR_H_
