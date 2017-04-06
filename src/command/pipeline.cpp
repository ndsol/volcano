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
      vk.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      vk.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      vk.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      // This depth attachment can also throw away any previous data:
      vk.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      // And after the RenderPass, VkImage should have a depth buffer format:
      vk.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      // (These are default values. Customize as needed for your application.)
      break;

    default:
      fprintf(stderr, "PipelineAttachment(%s (%d)): not supported.\n",
              string_VkImageLayout(refLayout), refLayout);
      break;
  }
}

PipelineCreateInfo::PipelineCreateInfo(language::Device& dev) {
  VkOverwrite(vertsci);
  VkOverwrite(asci);
  asci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  asci.primitiveRestartEnable = VK_FALSE;

  VkOverwrite(viewsci);
  VkExtent2D& scExtent = dev.swapChainInfo.imageExtent;
  viewports.push_back(VkViewport{
      /*x:*/ 0.0f,
      /*y:*/ 0.0f,
      /*width:*/ (float)scExtent.width,
      /*height:*/ (float)scExtent.height,
      /*minDepth:*/ 0.0f,
      /*maxDepth:*/ 1.0f,
  });
  scissors.push_back(VkRect2D{
      /*offset:*/ {0, 0},
      /*extent:*/ scExtent,
  });

  VkOverwrite(rastersci);
  rastersci.depthClampEnable = VK_FALSE;
  rastersci.rasterizerDiscardEnable = VK_FALSE;
  rastersci.polygonMode = VK_POLYGON_MODE_FILL;
  rastersci.lineWidth = 1.0f;
  rastersci.cullMode = VK_CULL_MODE_BACK_BIT;
  rastersci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rastersci.depthBiasEnable = VK_FALSE;

  VkOverwrite(multisci);
  multisci.sampleShadingEnable = VK_FALSE;
  multisci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkOverwrite(depthsci);
  // depthsci.depthTestEnable = VK_FALSE;
  // depthsci.depthWriteEnable = VK_FALSE;
  depthsci.depthCompareOp = VK_COMPARE_OP_LESS;
  // depthsci.depthBoundsTestEnable = VK_FALSE;
  // depthsci.stencilTestEnable = VK_FALSE;

  VkOverwrite(cbsci);
  cbsci.logicOpEnable = VK_FALSE;
  cbsci.logicOp = VK_LOGIC_OP_COPY;
  cbsci.blendConstants[0] = 0.0f;
  cbsci.blendConstants[1] = 0.0f;
  cbsci.blendConstants[2] = 0.0f;
  cbsci.blendConstants[3] = 0.0f;
  perFramebufColorBlend.push_back(withDisabledAlpha());

  // Default PipelineCreateInfo contains a lone color attachment using the
  // VkSurfaceFormatKHR of the device (dev.format).
  attach.emplace_back(dev.swapChainInfo.imageFormat,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkOverwrite(subpassDesc);
  subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

VkPipelineColorBlendAttachmentState PipelineCreateInfo::withDisabledAlpha() {
  VkPipelineColorBlendAttachmentState VkInit(state);
  state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  state.blendEnable = VK_FALSE;
  return state;
}

VkPipelineColorBlendAttachmentState PipelineCreateInfo::withEnabledAlpha() {
  VkPipelineColorBlendAttachmentState VkInit(state);
  state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  state.blendEnable = VK_TRUE;
  state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  state.colorBlendOp = VK_BLEND_OP_ADD;
  state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  state.alphaBlendOp = VK_BLEND_OP_ADD;
  return state;
}

Pipeline::Pipeline(language::Device& dev)
    : info{dev},
      pipelineLayout{dev.dev, vkDestroyPipelineLayout},
      vk{dev.dev, vkDestroyPipeline} {
  pipelineLayout.allocator = dev.dev.allocator;
  vk.allocator = dev.dev.allocator;
};

int Pipeline::init(language::Device& dev, RenderPass& renderPass,
                   size_t subpass_i) {
  if (subpass_i >= renderPass.pipelines.size()) {
    fprintf(
        stderr,
        "Pipeline::init(): subpass_i=%zu when renderPass.pipeline.size=%zu\n",
        subpass_i, renderPass.pipelines.size());
    return 1;
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
  // TODO: Add pushConstants.
  plci.setLayoutCount = info.setLayouts.size();
  plci.pSetLayouts = info.setLayouts.data();
  plci.pushConstantRangeCount = 0;
  plci.pPushConstantRanges = 0;

  //
  // Create pipelineLayout.
  //
  pipelineLayout.reset();
  VkResult v = vkCreatePipelineLayout(dev.dev, &plci, nullptr, &pipelineLayout);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreatePipelineLayout", v,
            string_VkResult(v));
    return 1;
  }

  VkGraphicsPipelineCreateInfo VkInit(p);
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
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateGraphicsPipelines", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

Pipeline::~Pipeline() {}

}  // namespace command
