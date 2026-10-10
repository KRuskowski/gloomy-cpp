#ifndef GLOOMY_GFX_UI_RENDERER_H_
#define GLOOMY_GFX_UI_RENDERER_H_

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>
#include <vector>

#include "gloomy/core/atlas.h"
#include "gloomy/gfx/device_context.h"
#include "gloomy/gfx/draw_list.h"

namespace gloomy::gfx {

/// Draws DrawLists into a colour attachment the caller provides. Owns only
/// its pipeline, the GPU copy of the glyph atlas and per-frame buffers, so
/// it can render into a host engine's frame. See docs/design.md section 6.1.
///
/// Per frame, for a frame slot whose previous use has completed:
/// Prepare() outside rendering (uploads), then Draw() inside a dynamic
/// rendering scope with a colour attachment of `color_format`.
class UiRenderer {
 public:
  /// @param[in] atlas The atlas glyphs are drawn from; its page size and
  ///     page limit fix the GPU texture size. Must outlive the renderer.
  /// @param[in] frames_in_flight Number of frame slots.
  UiRenderer(DeviceContext &ctx, VkFormat color_format, Atlas &atlas,
             std::uint32_t frames_in_flight = 2);

  UiRenderer(const UiRenderer &) = delete;
  auto operator=(const UiRenderer &) -> UiRenderer & = delete;
  ~UiRenderer();

  /// Records atlas uploads and copies the instances of `lists` (drawn in
  /// order) into the buffer of `slot`. Call outside a rendering scope.
  void Prepare(VkCommandBuffer cmd, std::uint32_t slot,
               std::span<const DrawList *const> lists);

  /// The blurred host scene glass shows on frame slot `slot`
  /// (BackdropBlur::Record()'s result, in SHADER_READ_ONLY_OPTIMAL while
  /// drawn), or VK_NULL_HANDLE for none: glass then draws its solid
  /// fallback. Call before Draw(), once the slot's previous use completed.
  void SetBackdrop(std::uint32_t slot, VkImageView view);

  /// Records the draw for what the last Prepare() of `slot` uploaded.
  /// Sets viewport and scissor to `extent`.
  void Draw(VkCommandBuffer cmd, std::uint32_t slot, VkExtent2D extent);

  /// Instances uploaded by the last Prepare() of `slot`.
  auto instance_count(std::uint32_t slot) const -> std::uint32_t {
    return frames_[slot].count;
  }

 private:
  struct FrameData {
    Buffer instances;
    Buffer staging;
    std::uint32_t count = 0;
    /// Set 1: the backdrop glass samples.
    VkDescriptorSet backdrop_set = VK_NULL_HANDLE;
    VkImageView backdrop = VK_NULL_HANDLE;
    bool has_backdrop = false;
  };

  void CreatePipeline(VkFormat color_format);

  void CreateAtlasTexture();

  void UploadAtlas(VkCommandBuffer cmd, FrameData &frame);

  void WriteBackdrop(FrameData &frame, VkImageView view);

  DeviceContext &ctx_;
  Atlas &atlas_;
  std::vector<FrameData> frames_;
  Image2D atlas_image_;
  /// What glass samples without a backdrop (never shown: the shader draws
  /// the fallback colour then), so the set is always valid.
  Image2D placeholder_;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout backdrop_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_UI_RENDERER_H_
