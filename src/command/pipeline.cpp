/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

PipelineAttachment::PipelineAttachment(VkFormat format,
                                       VkImageLayout refLayout) {
  VkOverwrite(refvk);
  VkOverwrite(vk);
  refvk.layout = refLayout;

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

Pipeline::Pipeline(language::Device& dev_)
    : dev(dev_),
      info(dev_),
      pipelineLayout(dev_.dev, vkDestroyPipelineLayout),
      vk(dev_.dev, vkDestroyPipeline) {
  pipelineLayout.allocator = dev.dev.allocator;
  vk.allocator = dev.dev.allocator;
};

int Pipeline::init(RenderPass& renderPass, size_t subpass_i) {
  if (subpass_i >= renderPass.pipelines.size()) {
    fprintf(
        stderr,
        "Pipeline::init(): subpass_i=%zu when renderPass.pipeline.size=%zu\n",
        subpass_i, renderPass.pipelines.size());
    return 1;
  }
  if (info.asci.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) {
    // TODO: validate components swizzle in src/languag/imageview.cpp. But this
    // all belongs in a portability assistance layer, not in Volcano.
    logW("TRIANGLE_FAN is not supported by MoltenVK or DX12 portability.\n");
    logW("See https://www.knrhonos.org/vulkan-portability-initiative\n");
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

  VkPipelineLayoutCreateInfo VkInit(plci);
  plci.setLayoutCount = info.setLayouts.size();
  plci.pSetLayouts = info.setLayouts.data();
  plci.pushConstantRangeCount = info.pushConstants.size();
  plci.pPushConstantRanges = info.pushConstants.data();

  //
  // Create pipelineLayout.
  //
  pipelineLayout.reset();
  VkResult v = vkCreatePipelineLayout(dev.dev, &plci, nullptr, &pipelineLayout);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreatePipelineLayout", v,
         string_VkResult(v));
    return 1;
  }

  VkGraphicsPipelineCreateInfo VkInit(p);
  p.flags = info.flags;
  std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfo;
  stageName.resize(info.stages.size());
  for (size_t i = 0; i < info.stages.size(); i++) {
    auto& stage = info.stages.at(i);
    stageName.at(i) = stage.entryPointName;
    stage.info.module = stage.shader->vk;
    stage.info.pName = stageName.at(i).c_str();
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
  VkPipelineDynamicStateCreateInfo VkInit(dsci);
  if (info.dynamicStates.size()) {
    dsci.dynamicStateCount = info.dynamicStates.size();
    dsci.pDynamicStates = info.dynamicStates.data();
    p.pDynamicState = &dsci;
  }
  p.layout = pipelineLayout;
  p.renderPass = renderPass.vk;
  p.subpass = subpass_i;

  vk.reset();
  v = vkCreateGraphicsPipelines(dev.dev, VK_NULL_HANDLE, 1, &p, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateGraphicsPipelines", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

}  // namespace command
