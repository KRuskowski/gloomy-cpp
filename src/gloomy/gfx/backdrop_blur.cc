#include "gloomy/gfx/backdrop_blur.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "gfx/barriers.h"
#include "shaders/blur_down.frag.h"
#include "shaders/blur_up.frag.h"
#include "shaders/fullscreen.vert.h"

namespace gloomy::gfx {

namespace {

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

auto Half(VkExtent2D e) -> VkExtent2D {
  return {std::max(1U, e.width / 2), std::max(1U, e.height / 2)};
}

}  // namespace

BackdropBlur::BackdropBlur(DeviceContext &ctx, std::uint32_t frames_in_flight)
    : ctx_(ctx), frames_(frames_in_flight), sets_(frames_in_flight) {
  CreatePipelines();
}

BackdropBlur::~BackdropBlur() {
  VkDevice device = ctx_.device();
  vkDeviceWaitIdle(device);
  vkDestroyPipeline(device, down_, nullptr);
  vkDestroyPipeline(device, up_, nullptr);
  vkDestroyPipelineLayout(device, layout_, nullptr);
  vkDestroyDescriptorPool(device, pool_, nullptr);
  vkDestroyDescriptorSetLayout(device, set_layout_, nullptr);
  vkDestroySampler(device, sampler_, nullptr);
}

auto BackdropBlur::LevelsForRadius(float radius) -> int {
  if (!(radius > 4.0F)) {
    return 1;
  }
  const int levels = static_cast<int>(std::ceil(std::log2(radius))) - 1;
  return std::clamp(levels, 1, kMaxLevels);
}

void BackdropBlur::CreatePipelines() {
  VkDevice device = ctx_.device();
  VkSamplerCreateInfo sampler{};
  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  CheckVk(vkCreateSampler(device, &sampler, nullptr, &sampler_),
          "vkCreateSampler");

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

  const std::uint32_t count = frames_ * 2 * kMaxLevels;
  VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 count};
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = count;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  CheckVk(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool_),
          "vkCreateDescriptorPool");
  const std::vector<VkDescriptorSetLayout> layouts(2 * kMaxLevels, set_layout_);
  for (auto &slot : sets_) {
    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = pool_;
    alloc.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    alloc.pSetLayouts = layouts.data();
    CheckVk(vkAllocateDescriptorSets(device, &alloc, slot.data()),
            "vkAllocateDescriptorSets");
  }

  VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 2};
  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 1;
  layout_info.pSetLayouts = &set_layout_;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &push;
  CheckVk(vkCreatePipelineLayout(device, &layout_info, nullptr, &layout_),
          "vkCreatePipelineLayout");

  VkShaderModule vert =
      CreateShader(device, kFullscreenVertSpirv, sizeof(kFullscreenVertSpirv));
  VkShaderModule down =
      CreateShader(device, kBlurDownFragSpirv, sizeof(kBlurDownFragSpirv));
  VkShaderModule up =
      CreateShader(device, kBlurUpFragSpirv, sizeof(kBlurUpFragSpirv));

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
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
  raster.lineWidth = 1.0F;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineColorBlendAttachmentState blend_attachment{};
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
  const VkFormat format = kFormat;
  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &format;

  for (const bool is_up : {false, true}) {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = is_up ? up : down;
    stages[1].pName = "main";
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
    info.layout = layout_;
    CheckVk(
        vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &info, nullptr, is_up ? &up_ : &down_),
        "vkCreateGraphicsPipelines");
  }
  vkDestroyShaderModule(device, vert, nullptr);
  vkDestroyShaderModule(device, down, nullptr);
  vkDestroyShaderModule(device, up, nullptr);
}

void BackdropBlur::Resize(VkExtent2D source) {
  // Earlier frames may still read the old images.
  ctx_.WaitIdle();
  levels_.clear();
  VkExtent2D e = source;
  for (int k = 0; k < kMaxLevels; ++k) {
    e = Half(e);
    levels_.push_back(ctx_.CreateImage(
        e,
        1,
        kFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
  }
  source_extent_ = source;
}

void BackdropBlur::Pass(VkCommandBuffer cmd, VkDescriptorSet set,
                        VkImageView from, VkExtent2D from_extent, int to,
                        bool up) {
  const Image2D &target = levels_[static_cast<std::size_t>(to)];
  VkDescriptorImageInfo image_info{};
  image_info.sampler = sampler_;
  image_info.imageView = from;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image_info;
  vkUpdateDescriptorSets(ctx_.device(), 1, &write, 0, nullptr);

  // The target's old contents are not needed; wait for whoever read it
  // (an earlier pass or frame) before writing.
  ImageBarrier(cmd,
               target.handle(),
               1,
               VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
               VK_ACCESS_2_NONE,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  VkRenderingAttachmentInfo color{};
  color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color.imageView = target.view();
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  info.renderArea = {{0, 0}, target.extent()};
  info.layerCount = 1;
  info.colorAttachmentCount = 1;
  info.pColorAttachments = &color;
  vkCmdBeginRendering(cmd, &info);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, up ? up_ : down_);
  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set, 0, nullptr);
  const VkViewport viewport{0.0F,
                            0.0F,
                            static_cast<float>(target.extent().width),
                            static_cast<float>(target.extent().height),
                            0.0F,
                            1.0F};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  const VkRect2D scissor{{0, 0}, target.extent()};
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  const float half_texel[2] = {0.5F / static_cast<float>(from_extent.width),
                               0.5F / static_cast<float>(from_extent.height)};
  vkCmdPushConstants(cmd,
                     layout_,
                     VK_SHADER_STAGE_FRAGMENT_BIT,
                     0,
                     sizeof(half_texel),
                     half_texel);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
  ImageBarrier(cmd,
               target.handle(),
               1,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
               VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

auto BackdropBlur::Record(VkCommandBuffer cmd, std::uint32_t slot,
                          const Backdrop &source, int levels) -> VkImageView {
  if (levels_.empty() || source.extent.width != source_extent_.width ||
      source.extent.height != source_extent_.height) {
    Resize(source.extent);
  }
  levels = std::clamp(levels, 1, kMaxLevels);
  auto &sets = sets_[slot];
  // Down: source -> 1/2 -> 1/4 ...
  Pass(cmd, sets[0], source.view, source.extent, 0, false);
  for (int k = 1; k < levels; ++k) {
    const Image2D &from = levels_[static_cast<std::size_t>(k - 1)];
    Pass(cmd,
         sets[static_cast<std::size_t>(k)],
         from.view(),
         from.extent(),
         k,
         false);
  }
  // Up: ... 1/4 -> 1/2.
  for (int k = levels - 2; k >= 0; --k) {
    const Image2D &from = levels_[static_cast<std::size_t>(k + 1)];
    Pass(cmd,
         sets[static_cast<std::size_t>(kMaxLevels + k)],
         from.view(),
         from.extent(),
         k,
         true);
  }
  return levels_[0].view();
}

}  // namespace gloomy::gfx
