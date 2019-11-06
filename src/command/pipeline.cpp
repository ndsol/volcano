/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * Implements PipelineAttachment and Pipeline::Pipeline constructor.
 */
#include <gli/gli.hpp>

#include "command.h"

namespace command {

VkImageAspectFlags PipelineAttachment::aspectMaskFromFormat(VkFormat format) {
  if (format > VK_FORMAT_ASTC_12x12_SRGB_BLOCK) {
    // VK_IMAGE_ASPECT_METADATA_BIT is not handled. See Image::getAllAspects().
    logE("format %u not supported by Volcano\n", format);
    return 0;
  }
  gli::format f = static_cast<gli::format>(format);
  bool isDepth = gli::is_depth(f);
  bool isStencil = gli::is_stencil(f);
  VkImageAspectFlags r =
      (!(isDepth || isStencil) ? VK_IMAGE_ASPECT_COLOR_BIT : 0) |
      (isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
      (isStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
#if 0
  // These formats need custom support:
  ((FormatPlaneCount(format) > 0) ? VK_IMAGE_ASPECT_PLANE_0_BIT : 0);
  ((FormatPlaneCount(format) > 1) ? VK_IMAGE_ASPECT_PLANE_1_BIT : 0);
  ((FormatPlaneCount(format) > 2) ? VK_IMAGE_ASPECT_PLANE_2_BIT : 0);
#endif
  return r;
}

PipelineAttachment::PipelineAttachment(VkFormat format,
                                       VkImageLayout refLayout) {
  memset(&refvk, 0, sizeof(refvk));
  refvk.sType = autoSType(refvk);
  refvk.layout = refLayout;
  refvk.aspectMask = PipelineAttachment::aspectMaskFromFormat(format);
  memset(&vk, 0, sizeof(vk));
  vk.sType = autoSType(vk);

  switch (refLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      // Set no defaults. The caller must do all PipelineAttachment setup.
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Set up vk to be a color attachment.
      vk.format = format;
      vk.samples = VK_SAMPLE_COUNT_1_BIT;
      vk.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      vk.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      vk.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      vk.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

      // This color attachment (the VkImage in the Framebuffer) will be
      // transitioned automatically just before the RenderPass. It will be
      // transitioned from...
      vk.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      // (VK_IMAGE_LAYOUT_UNDEFINED meaning throw away any data in the
      // Framebuffer)
      //
      // Then the RenderPass is performed.
      //
      // Then after the RenderPass ends, the Framebuffer gets transitioned
      // automatically to a VkImage with:
      vk.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      // (These are default values. Customize as needed for your application.)
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      // Set up vk to be a depth attachment.
      vk.format = format;
      vk.samples = VK_SAMPLE_COUNT_1_BIT;
      vk.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      vk.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      vk.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      vk.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      // This depth attachment can also throw away any previous data:
      vk.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      // And after the RenderPass, VkImage should have a depth buffer format:
      vk.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      // (These are default values. Customize as needed for your application.)
      break;

    default:
      logE("PipelineAttachment(%s (%d)): not supported.\n",
           string_VkImageLayout(refLayout), refLayout);
      break;
  }
}

Pipeline::Pipeline(RenderPass& pass_)
    : info(pass_),
      pipelineLayout(pass_.vk.dev, vkDestroyPipelineLayout),
      vk(pass_.vk.dev, vkDestroyPipeline) {
  pipelineLayout.allocator = pass_.vk.dev.dev.allocator;
  vk.allocator = pass_.vk.dev.dev.allocator;
  clearColors.emplace_back(VkClearValue({0, 0, 0, 1}));
};

Pipeline::~Pipeline() {}

int Pipeline::ctorError(RenderPass& pass, size_t subpass_i) {
  if (subpass_i >= pass.pipelines.size()) {
    logE("Pipeline::init(): subpass_i=%zu when pass.pipeline.size=%zu\n",
         subpass_i, pass.pipelines.size());
    return 1;
  }
  if (info.asci.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) {
    // TODO: validate components swizzle in src/languag/imageview.cpp. But this
    // all belongs in a portability assistance layer, not in Volcano.
    logW("TRIANGLE_FAN is not supported by MoltenVK or DX12 portability.\n");
    logW("See https://www.khronos.org/vulkan-portability-initiative\n");
#ifdef __APPLE__ /* neither iOS nor macOS via MoltenVK can do TRIANGLE_FAN */
    logE("This apple device does not support TRIANGLE_FAN (MoltenVK).\n");
    return 1;
#endif
  }

  //
  // Collect PipelineCreateInfo structures into native Vulkan structures.
  //
  info.viewsci.viewportCount = info.viewports.size();
  info.viewsci.pViewports = info.viewports.data();
  info.viewsci.scissorCount = info.scissors.size();
  info.viewsci.pScissors = info.scissors.data();

  info.cbsci.attachmentCount = info.perFramebufColorBlend.size();
  info.cbsci.pAttachments = info.perFramebufColorBlend.data();

  VkPipelineLayoutCreateInfo plci;
  memset(&plci, 0, sizeof(plci));
  plci.sType = autoSType(plci);
  plci.setLayoutCount = info.setLayouts.size();
  plci.pSetLayouts = info.setLayouts.data();
  plci.pushConstantRangeCount = info.pushConstants.size();
  plci.pPushConstantRanges = info.pushConstants.data();

  //
  // Create pipelineLayout.
  //
  pipelineLayout.reset();
  VkResult v = vkCreatePipelineLayout(
      pass.vk.dev.dev, &plci, pass.vk.dev.dev.allocator, &pipelineLayout);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreatePipelineLayout", v);
  }
  pipelineLayout.onCreate();

  VkGraphicsPipelineCreateInfo p;
  memset(&p, 0, sizeof(p));
  p.sType = autoSType(p);
  p.flags = info.flags;
  std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfo;
  std::vector<VkSpecializationInfo> specInfo(info.stages.size());
  stageName.resize(info.stages.size());
  for (size_t i = 0; i < info.stages.size(); i++) {
    auto& stage = info.stages.at(i);
    stageName.at(i) = stage.entryPointName;
    stage.info.module = stage.shader->vk;
    stage.info.pName = stageName.at(i).c_str();
    if (!stage.specialization.empty()) {
      stage.info.pSpecializationInfo = &specInfo.at(i);
      specInfo.at(i).mapEntryCount = stage.specializationMap.size();
      specInfo.at(i).pMapEntries = stage.specializationMap.data();
      specInfo.at(i).dataSize = stage.specialization.size();
      specInfo.at(i).pData = stage.specialization.data();
    }
    stageCreateInfo.push_back(stage.info);
  }
  p.stageCount = stageCreateInfo.size();
  p.pStages = stageCreateInfo.data();
  p.pVertexInputState = &info.vertsci;
  p.pInputAssemblyState = &info.asci;
  p.pViewportState = &info.viewsci;
  p.pRasterizationState = &info.rastersci;
  p.pMultisampleState = &info.multisci;
  p.pDepthStencilState = &info.depthsci;
  p.pColorBlendState = &info.cbsci;
  VkPipelineDynamicStateCreateInfo dsci;
  memset(&dsci, 0, sizeof(dsci));
  dsci.sType = autoSType(dsci);
  if (info.dynamicStates.size()) {
    dsci.dynamicStateCount = info.dynamicStates.size();
    dsci.pDynamicStates = info.dynamicStates.data();
    p.pDynamicState = &dsci;
  }
  p.layout = pipelineLayout;
  p.renderPass = pass.vk;
  p.subpass = subpass_i;

  // TODO: Pipeline Cache.
  vk.reset();
  v = vkCreateGraphicsPipelines(pass.vk.dev.dev, VK_NULL_HANDLE, 1, &p,
                                pass.vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateGraphicsPipelines", v);
  }
  vk.onCreate();
  return 0;
}

}  // namespace command
