#include "gloomy/gfx/ui_renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "gfx/barriers.h"
#include "shaders/ui.frag.h"
#include "shaders/ui.vert.h"

namespace gloomy::gfx {

namespace {

constexpr VkDeviceSize kMinInstanceBytes = 64 * 1024;
constexpr VkDeviceSize kMinStagingBytes = 256 * 1024;

auto CreateShader(VkDevice device, const std::uint32_t *code, std::size_t size)
    -> VkShaderModule {
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = size;
  info.pCode = code;
  VkShaderModule module = VK_NULL_HANDLE;
  CheckVk(vkCreateShaderModule(device, &info, nullptr, &module),
          "vkCreateShaderModule");
  return module;
}

// Rounds up to the next power of two, at least `min`.
auto GrowSize(VkDeviceSize needed, VkDeviceSize min) -> VkDeviceSize {
  VkDeviceSize size = min;
  while (size < needed) {
    size *= 2;
  }
  return size;
}

}  // namespace

UiRenderer::UiRenderer(DeviceContext &ctx, VkFormat color_format, Atlas &atlas,
                       std::uint32_t frames_in_flight)
    : ctx_(ctx), atlas_(atlas), frames_(frames_in_flight) {
  CreateAtlasTexture();
  CreatePipeline(color_format);
}

UiRenderer::~UiRenderer() {
  VkDevice device = ctx_.device();
  vkDeviceWaitIdle(device);
  vkDestroyPipeline(device, pipeline_, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
  vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
  vkDestroyDescriptorSetLayout(device, set_layout_, nullptr);
  vkDestroyDescriptorSetLayout(device, backdrop_layout_, nullptr);
  vkDestroySampler(device, sampler_, nullptr);
}

void UiRenderer::CreateAtlasTexture() {
  const auto size = static_cast<std::uint32_t>(atlas_.page_size());
  const auto layers = static_cast<std::uint32_t>(atlas_.max_pages());
  atlas_image_ = ctx_.CreateImage(
      {size, size},
      layers,
      VK_FORMAT_R8_UNORM,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY);
  ctx_.SubmitAndWait([&](VkCommandBuffer cmd) {
    ImageBarrier(cmd,
                 atlas_image_.handle(),
                 layers,
                 VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_PIPELINE_STAGE_2_NONE,
                 VK_ACCESS_2_NONE,
                 VK_PIPELINE_STAGE_2_CLEAR_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT);
    const VkClearColorValue zero{};
    const VkImageSubresourceRange range{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
    vkCmdClearColorImage(cmd,
                         atlas_image_.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &zero,
                         1,
                         &range);
    ImageBarrier(cmd,
                 atlas_image_.handle(),
                 layers,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_PIPELINE_STAGE_2_CLEAR_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  });

  placeholder_ = ctx_.CreateImage(
      {1, 1},
      1,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  ctx_.SubmitAndWait([&](VkCommandBuffer cmd) {
    ImageBarrier(cmd,
                 placeholder_.handle(),
                 1,
                 VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_PIPELINE_STAGE_2_NONE,
                 VK_ACCESS_2_NONE,
                 VK_PIPELINE_STAGE_2_CLEAR_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT);
    const VkClearColorValue zero{};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd,
                         placeholder_.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &zero,
                         1,
                         &range);
    ImageBarrier(cmd,
                 placeholder_.handle(),
                 1,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_PIPELINE_STAGE_2_CLEAR_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  });

  VkSamplerCreateInfo sampler{};
  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0.0F;
  CheckVk(vkCreateSampler(ctx_.device(), &sampler, nullptr, &sampler_),
          "vkCreateSampler");
}

void UiRenderer::CreatePipeline(VkFormat color_format) {
  VkDevice device = ctx_.device();

  // Descriptor set: the atlas.
  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo set_info{};
  set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set_info.bindingCount = 1;
  set_info.pBindings = &binding;
  CheckVk(vkCreateDescriptorSetLayout(device, &set_info, nullptr, &set_layout_),
          "vkCreateDescriptorSetLayout");

  CheckVk(vkCreateDescriptorSetLayout(
              device, &set_info, nullptr, &backdrop_layout_),
          "vkCreateDescriptorSetLayout");

  const auto sets = static_cast<std::uint32_t>(1 + frames_.size());
  VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 sets};
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = sets;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  CheckVk(
      vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool_),
      "vkCreateDescriptorPool");

  VkDescriptorSetAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc.descriptorPool = descriptor_pool_;
  alloc.descriptorSetCount = 1;
  alloc.pSetLayouts = &set_layout_;
  CheckVk(vkAllocateDescriptorSets(device, &alloc, &descriptor_set_),
          "vkAllocateDescriptorSets");

  VkDescriptorImageInfo image_info{};
  image_info.sampler = sampler_;
  image_info.imageView = atlas_image_.view();
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptor_set_;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image_info;
  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  // One backdrop set per frame slot, pointing at the placeholder until a
  // host scene is set.
  for (FrameData &frame : frames_) {
    alloc.pSetLayouts = &backdrop_layout_;
    CheckVk(vkAllocateDescriptorSets(device, &alloc, &frame.backdrop_set),
            "vkAllocateDescriptorSets");
    WriteBackdrop(frame, placeholder_.view());
  }

  // Layout: the atlas and backdrop sets plus viewport size and backdrop
  // flag (ui.vert, ui.frag).
  VkPushConstantRange push{
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(float) * 4};
  const VkDescriptorSetLayout set_layouts[] = {set_layout_, backdrop_layout_};
  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 2;
  layout_info.pSetLayouts = set_layouts;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &push;
  CheckVk(
      vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout_),
      "vkCreatePipelineLayout");

  VkShaderModule vert =
      CreateShader(device, kUiVertSpirv, sizeof(kUiVertSpirv));
  VkShaderModule frag =
      CreateShader(device, kUiFragSpirv, sizeof(kUiFragSpirv));
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";

  // One instance-rate binding; attributes follow Instance.
  VkVertexInputBindingDescription vbinding{
      0, sizeof(Instance), VK_VERTEX_INPUT_RATE_INSTANCE};
  const VkVertexInputAttributeDescription attributes[] = {
      {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instance, rect)},
      {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instance, uv)},
      {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instance, radii)},
      {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instance, clip)},
      {4, 0, VK_FORMAT_R32_UINT, offsetof(Instance, color)},
      {5, 0, VK_FORMAT_R32_UINT, offsetof(Instance, color2)},
      {6, 0, VK_FORMAT_R32_SFLOAT, offsetof(Instance, stroke)},
      {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(Instance, soft)},
      {8, 0, VK_FORMAT_R32_UINT, offsetof(Instance, kind)},
  };
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &vbinding;
  vertex_input.vertexAttributeDescriptionCount = std::size(attributes);
  vertex_input.pVertexAttributeDescriptions = attributes;

  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
  raster.lineWidth = 1.0F;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Premultiplied alpha.
  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.blendEnable = VK_TRUE;
  blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blend_attachment;

  const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                           VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = std::size(dynamic_states);
  dynamic.pDynamicStates = dynamic_states;

  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &color_format;

  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = &rendering;
  info.stageCount = 2;
  info.pStages = stages;
  info.pVertexInputState = &vertex_input;
  info.pInputAssemblyState = &assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &raster;
  info.pMultisampleState = &multisample;
  info.pColorBlendState = &blend;
  info.pDynamicState = &dynamic;
  info.layout = pipeline_layout_;
  CheckVk(vkCreateGraphicsPipelines(
              device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_),
          "vkCreateGraphicsPipelines");
  vkDestroyShaderModule(device, vert, nullptr);
  vkDestroyShaderModule(device, frag, nullptr);
}

void UiRenderer::UploadAtlas(VkCommandBuffer cmd, FrameData &frame) {
  const std::vector<AtlasDirtyRect> dirty = atlas_.TakeDirty();
  if (dirty.empty()) {
    return;
  }
  VkDeviceSize bytes = 0;
  for (const AtlasDirtyRect &r : dirty) {
    bytes += static_cast<VkDeviceSize>(r.x1 - r.x0) * (r.y1 - r.y0);
  }
  if (frame.staging.size() < bytes) {
    frame.staging = ctx_.CreateBuffer(GrowSize(bytes, kMinStagingBytes),
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      true);
  }
  std::vector<VkBufferImageCopy> copies;
  auto *dst = static_cast<std::uint8_t *>(frame.staging.mapped());
  VkDeviceSize offset = 0;
  const int page_size = atlas_.page_size();
  for (const AtlasDirtyRect &r : dirty) {
    const int w = r.x1 - r.x0;
    const std::uint8_t *page = atlas_.PagePixels(r.page);
    for (int y = r.y0; y < r.y1; ++y) {
      std::memcpy(dst + offset + static_cast<VkDeviceSize>(y - r.y0) * w,
                  page + static_cast<std::size_t>(y) * page_size + r.x0,
                  static_cast<std::size_t>(w));
    }
    VkBufferImageCopy copy{};
    copy.bufferOffset = offset;
    copy.imageSubresource = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<std::uint32_t>(r.page), 1};
    copy.imageOffset = {r.x0, r.y0, 0};
    copy.imageExtent = {static_cast<std::uint32_t>(w),
                        static_cast<std::uint32_t>(r.y1 - r.y0),
                        1};
    copies.push_back(copy);
    offset += static_cast<VkDeviceSize>(w) * (r.y1 - r.y0);
  }
  // Earlier frames may still sample the atlas: wait for their fragment
  // shaders before overwriting.
  ImageBarrier(cmd,
               atlas_image_.handle(),
               atlas_image_.layers(),
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
               VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
               VK_PIPELINE_STAGE_2_COPY_BIT,
               VK_ACCESS_2_TRANSFER_WRITE_BIT);
  vkCmdCopyBufferToImage(cmd,
                         frame.staging.handle(),
                         atlas_image_.handle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<std::uint32_t>(copies.size()),
                         copies.data());
  ImageBarrier(cmd,
               atlas_image_.handle(),
               atlas_image_.layers(),
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_2_COPY_BIT,
               VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
               VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

void UiRenderer::WriteBackdrop(FrameData &frame, VkImageView view) {
  VkDescriptorImageInfo image_info{};
  image_info.sampler = sampler_;
  image_info.imageView = view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = frame.backdrop_set;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image_info;
  vkUpdateDescriptorSets(ctx_.device(), 1, &write, 0, nullptr);
  frame.backdrop = view;
}

void UiRenderer::SetBackdrop(std::uint32_t slot, VkImageView view) {
  FrameData &frame = frames_[slot];
  frame.has_backdrop = view != VK_NULL_HANDLE;
  const VkImageView bound = frame.has_backdrop ? view : placeholder_.view();
  if (bound != frame.backdrop) {
    WriteBackdrop(frame, bound);
  }
}

void UiRenderer::Prepare(VkCommandBuffer cmd, std::uint32_t slot,
                         std::span<const DrawList *const> lists) {
  FrameData &frame = frames_[slot];
  UploadAtlas(cmd, frame);

  std::size_t count = 0;
  for (const DrawList *list : lists) {
    count += list->instances().size();
  }
  const VkDeviceSize bytes = count * sizeof(Instance);
  if (frame.instances.size() < bytes || frame.instances.handle() == nullptr) {
    frame.instances = ctx_.CreateBuffer(GrowSize(bytes, kMinInstanceBytes),
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        true);
  }
  auto *dst = static_cast<Instance *>(frame.instances.mapped());
  for (const DrawList *list : lists) {
    const std::vector<Instance> &src = list->instances();
    std::copy(src.begin(), src.end(), dst);
    dst += src.size();
  }
  frame.count = static_cast<std::uint32_t>(count);
}

void UiRenderer::Draw(VkCommandBuffer cmd, std::uint32_t slot,
                      VkExtent2D extent) {
  const FrameData &frame = frames_[slot];
  if (frame.count == 0) {
    return;
  }
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout_,
                          0,
                          1,
                          &descriptor_set_,
                          0,
                          nullptr);
  vkCmdBindDescriptorSets(cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout_,
                          1,
                          1,
                          &frame.backdrop_set,
                          0,
                          nullptr);
  const VkViewport viewport{0.0F,
                            0.0F,
                            static_cast<float>(extent.width),
                            static_cast<float>(extent.height),
                            0.0F,
                            1.0F};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  const VkRect2D scissor{{0, 0}, extent};
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  const float push[4] = {static_cast<float>(extent.width),
                         static_cast<float>(extent.height),
                         frame.has_backdrop ? 1.0F : 0.0F,
                         0.0F};
  vkCmdPushConstants(cmd,
                     pipeline_layout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0,
                     sizeof(push),
                     push);
  const VkBuffer buffer = frame.instances.handle();
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
  vkCmdDraw(cmd, 6, frame.count, 0, 0);
}

}  // namespace gloomy::gfx
