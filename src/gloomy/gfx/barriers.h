#ifndef GLOOMY_GFX_BARRIERS_H_
#define GLOOMY_GFX_BARRIERS_H_

#include <vulkan/vulkan.h>

#include <cstdint>

namespace gloomy::gfx {

/// Records a layout transition and memory dependency for all layers of the
/// colour image `image` (synchronization2).
inline void ImageBarrier(VkCommandBuffer cmd, VkImage image,
                         std::uint32_t layers, VkImageLayout old_layout,
                         VkImageLayout new_layout,
                         VkPipelineStageFlags2 src_stage,
                         VkAccessFlags2 src_access,
                         VkPipelineStageFlags2 dst_stage,
                         VkAccessFlags2 dst_access) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask = src_stage;
  barrier.srcAccessMask = src_access;
  barrier.dstStageMask = dst_stage;
  barrier.dstAccessMask = dst_access;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd, &dep);
}

}  // namespace gloomy::gfx

#endif  // GLOOMY_GFX_BARRIERS_H_
