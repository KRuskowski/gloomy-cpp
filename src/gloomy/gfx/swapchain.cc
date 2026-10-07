#include "gloomy/gfx/swapchain.h"

#include <algorithm>

#include "gfx/barriers.h"

namespace gloomy::gfx {

Swapchain::Swapchain(DeviceContext &ctx, VkExtent2D extent, bool vsync)
    : ctx_(ctx), vsync_(vsync) {
  VkDevice device = ctx_.device();
  for (Slot &slot : slots_) {
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = ctx_.queue_family();
    CheckVk(vkCreateCommandPool(device, &pool, nullptr, &slot.pool),
            "vkCreateCommandPool");
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = slot.pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    CheckVk(vkAllocateCommandBuffers(device, &alloc, &slot.cmd),
            "vkAllocateCommandBuffers");
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    CheckVk(vkCreateFence(device, &fence, nullptr, &slot.fence),
            "vkCreateFence");
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CheckVk(vkCreateSemaphore(device, &semaphore, nullptr, &slot.acquired),
            "vkCreateSemaphore");
  }
  Create(extent);
}

Swapchain::~Swapchain() {
  VkDevice device = ctx_.device();
  vkDeviceWaitIdle(device);
  Destroy();
  for (Slot &slot : slots_) {
    vkDestroySemaphore(device, slot.acquired, nullptr);
    vkDestroyFence(device, slot.fence, nullptr);
    vkDestroyCommandPool(device, slot.pool, nullptr);
  }
}

void Swapchain::Create(VkExtent2D extent) {
  VkPhysicalDevice physical = ctx_.physical_device();
  VkSurfaceKHR surface = ctx_.surface();
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

  std::uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical, surface, &count, formats.data());
  // UNORM: the UI blends in gamma space (docs/design.md section 6.2).
  VkSurfaceFormatKHR chosen = formats.front();
  for (const VkSurfaceFormatKHR &f : formats) {
    if ((f.format == VK_FORMAT_B8G8R8A8_UNORM ||
         f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = f;
      break;
    }
  }
  format_ = chosen.format;

  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr);
  std::vector<VkPresentModeKHR> modes(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical, surface, &count, modes.data());
  VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
  if (!vsync_) {
    for (VkPresentModeKHR m :
         {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
      if (std::find(modes.begin(), modes.end(), m) != modes.end()) {
        mode = m;
        break;
      }
    }
  }

  if (caps.currentExtent.width != UINT32_MAX) {
    extent = caps.currentExtent;
  } else {
    extent.width = std::clamp(
        extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(
        extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
  }
  extent_ = extent;
  if (extent.width == 0 || extent.height == 0) {
    return;
  }

  std::uint32_t image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0) {
    image_count = std::min(image_count, caps.maxImageCount);
  }
  VkSwapchainCreateInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = surface;
  info.minImageCount = image_count;
  info.imageFormat = chosen.format;
  info.imageColorSpace = chosen.colorSpace;
  info.imageExtent = extent;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = caps.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = mode;
  info.clipped = VK_TRUE;
  CheckVk(vkCreateSwapchainKHR(ctx_.device(), &info, nullptr, &swapchain_),
          "vkCreateSwapchainKHR");

  vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &count, nullptr);
  images_.resize(count);
  vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &count, images_.data());
  for (VkImage image : images_) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format_;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView handle = VK_NULL_HANDLE;
    CheckVk(vkCreateImageView(ctx_.device(), &view, nullptr, &handle),
            "vkCreateImageView");
    views_.push_back(handle);
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore rendered = VK_NULL_HANDLE;
    CheckVk(vkCreateSemaphore(ctx_.device(), &semaphore, nullptr, &rendered),
            "vkCreateSemaphore");
    rendered_.push_back(rendered);
  }
}

void Swapchain::Destroy() {
  VkDevice device = ctx_.device();
  for (VkImageView view : views_) {
    vkDestroyImageView(device, view, nullptr);
  }
  for (VkSemaphore semaphore : rendered_) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }
  views_.clear();
  rendered_.clear();
  images_.clear();
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

auto Swapchain::BeginFrame(VkExtent2D extent) -> std::optional<Frame> {
  VkDevice device = ctx_.device();
  if (extent.width == 0 || extent.height == 0) {
    return std::nullopt;
  }
  if (needs_recreate_ || swapchain_ == VK_NULL_HANDLE ||
      extent.width != extent_.width || extent.height != extent_.height) {
    vkDeviceWaitIdle(device);
    Destroy();
    Create(extent);
    needs_recreate_ = false;
    if (swapchain_ == VK_NULL_HANDLE) {
      return std::nullopt;
    }
  }
  Slot &slot = slots_[slot_];
  CheckVk(vkWaitForFences(device, 1, &slot.fence, VK_TRUE, UINT64_MAX),
          "vkWaitForFences");
  const VkResult acquired = vkAcquireNextImageKHR(device,
                                                  swapchain_,
                                                  UINT64_MAX,
                                                  slot.acquired,
                                                  VK_NULL_HANDLE,
                                                  &image_index_);
  if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
    needs_recreate_ = true;
    return std::nullopt;
  }
  if (acquired != VK_SUBOPTIMAL_KHR) {
    CheckVk(acquired, "vkAcquireNextImageKHR");
  }
  vkResetFences(device, 1, &slot.fence);
  vkResetCommandBuffer(slot.cmd, 0);
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CheckVk(vkBeginCommandBuffer(slot.cmd, &begin), "vkBeginCommandBuffer");
  return Frame{slot.cmd, slot_, extent_};
}

void Swapchain::BeginRendering(const Color &clear) {
  VkCommandBuffer cmd = slots_[slot_].cmd;
  ImageBarrier(cmd,
               images_[image_index_],
               1,
               VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_NONE,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  VkRenderingAttachmentInfo color{};
  color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color.imageView = views_[image_index_];
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.clearValue.color = {{clear.r, clear.g, clear.b, 1.0F}};
  VkRenderingInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  info.renderArea = {{0, 0}, extent_};
  info.layerCount = 1;
  info.colorAttachmentCount = 1;
  info.pColorAttachments = &color;
  vkCmdBeginRendering(cmd, &info);
}

void Swapchain::EndFrame() {
  Slot &slot = slots_[slot_];
  vkCmdEndRendering(slot.cmd);
  ImageBarrier(slot.cmd,
               images_[image_index_],
               1,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_2_NONE,
               VK_ACCESS_2_NONE);
  CheckVk(vkEndCommandBuffer(slot.cmd), "vkEndCommandBuffer");

  VkSemaphoreSubmitInfo wait{};
  wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait.semaphore = slot.acquired;
  wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSemaphoreSubmitInfo signal{};
  signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal.semaphore = rendered_[image_index_];
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkCommandBufferSubmitInfo cmd_info{};
  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  cmd_info.commandBuffer = slot.cmd;
  VkSubmitInfo2 submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cmd_info;
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  CheckVk(vkQueueSubmit2(ctx_.queue(), 1, &submit, slot.fence),
          "vkQueueSubmit2");

  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &rendered_[image_index_];
  present.swapchainCount = 1;
  present.pSwapchains = &swapchain_;
  present.pImageIndices = &image_index_;
  const VkResult presented = vkQueuePresentKHR(ctx_.queue(), &present);
  if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
    needs_recreate_ = true;
  } else {
    CheckVk(presented, "vkQueuePresentKHR");
  }
  slot_ = (slot_ + 1) % kFramesInFlight;
}

}  // namespace gloomy::gfx
