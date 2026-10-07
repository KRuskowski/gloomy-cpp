#include "gloomy/gfx/offscreen_target.h"

#include <cstring>

#include "gfx/barriers.h"
#include "gloomy/gfx/ui_renderer.h"

namespace gloomy::gfx {

OffscreenTarget::OffscreenTarget(DeviceContext &ctx, VkExtent2D extent)
    : image_(ctx.CreateImage(extent, 1, kFormat,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT)),
      readback_(ctx.CreateBuffer(
          static_cast<VkDeviceSize>(extent.width) * extent.height * 4,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT, true)) {}

void OffscreenTarget::Begin(VkCommandBuffer cmd, const Color &clear) {
  ImageBarrier(cmd,
               image_.handle(),
               1,
               VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_PIPELINE_STAGE_2_COPY_BIT,
               VK_ACCESS_2_TRANSFER_READ_BIT,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  VkRenderingAttachmentInfo color{};
  color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color.imageView = image_.view();
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // Premultiplied, like everything the renderer writes.
  color.clearValue.color = {
      {clear.r * clear.a, clear.g * clear.a, clear.b * clear.a, clear.a}};
  VkRenderingInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  info.renderArea = {{0, 0}, image_.extent()};
  info.layerCount = 1;
  info.colorAttachmentCount = 1;
  info.pColorAttachments = &color;
  vkCmdBeginRendering(cmd, &info);
}

void OffscreenTarget::End(VkCommandBuffer cmd, bool readback) {
  vkCmdEndRendering(cmd);
  if (!readback) {
    // Leave the image in a layout the next Begin() accepts.
    ImageBarrier(cmd,
                 image_.handle(),
                 1,
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_ACCESS_2_NONE);
    return;
  }
  ImageBarrier(cmd,
               image_.handle(),
               1,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_2_COPY_BIT,
               VK_ACCESS_2_TRANSFER_READ_BIT);
  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {image_.extent().width, image_.extent().height, 1};
  vkCmdCopyImageToBuffer(cmd,
                         image_.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         readback_.handle(),
                         1,
                         &copy);
  VkBufferMemoryBarrier2 host{};
  host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
  host.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  host.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  host.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
  host.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
  host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  host.buffer = readback_.handle();
  host.size = VK_WHOLE_SIZE;
  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.bufferMemoryBarrierCount = 1;
  dep.pBufferMemoryBarriers = &host;
  vkCmdPipelineBarrier2(cmd, &dep);
}

auto OffscreenTarget::Read() const -> Image {
  Image image = Image::Make(static_cast<int>(image_.extent().width),
                            static_cast<int>(image_.extent().height));
  std::memcpy(image.pixels.data(), readback_.mapped(), image.pixels.size());
  return image;
}

auto RenderToImage(DeviceContext &ctx, UiRenderer &renderer,
                   OffscreenTarget &target,
                   std::span<const DrawList *const> lists, const Color &clear)
    -> Image {
  ctx.SubmitAndWait([&](VkCommandBuffer cmd) {
    renderer.Prepare(cmd, 0, lists);
    target.Begin(cmd, clear);
    renderer.Draw(cmd, 0, target.extent());
    target.End(cmd);
  });
  return target.Read();
}

}  // namespace gloomy::gfx
