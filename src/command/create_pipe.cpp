/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

PipelineCreateInfo::PipelineCreateInfo(RenderPass& pass) {
  VkOverwrite(vertsci);
  VkOverwrite(asci);
  asci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  asci.primitiveRestartEnable = VK_FALSE;

  VkOverwrite(viewsci);
  VkExtent3D targetExtent = pass.getTargetExtent();
  viewports.push_back(VkViewport{
      /*x:*/ 0.0f,
      /*y:*/ 0.0f,
      /*width:*/ (float)targetExtent.width,
      /*height:*/ (float)targetExtent.height,
      /*minDepth:*/ 0.0f,
      /*maxDepth:*/ 1.0f,
  });
  scissors.push_back(VkRect2D{
      /*offset:*/ {0, 0},
      /*extent:*/ {targetExtent.width, targetExtent.height},
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

  // Default PipelineCreateInfo has one color attachment.
  attach.emplace_back(pass.getTargetFormat(),
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
  state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  state.alphaBlendOp = VK_BLEND_OP_ADD;
  return state;
}

int PipelineCreateInfo::addShader(RenderPass& pass,
                                  std::shared_ptr<Shader> shader,
                                  VkShaderStageFlagBits stageBits,
                                  std::string entryPointName /*= "main"*/) {
  stages.emplace_back();
  auto& pipelinestage = stages.back();
  pipelinestage.info.stage = stageBits;
  pipelinestage.entryPointName = entryPointName;

  // pass.shaders is a set, so shader will not be duplicated in it.
  auto result = pass.shaders.insert(shader);

  // result.first is a std::iterator<std::shared_ptr<Shader>> pointing to
  // the Shader in the set.
  // result.second is (bool) false if the shader was a duplicate -- this
  // is not an error: shaders can be reused.
  pipelinestage.shader = *result.first;
  return 0;
}

}  // namespace command
