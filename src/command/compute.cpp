/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * Implements PipelineCreateInfo and Pipeline::Pipeline constructor for
 * compute shaders.
 */

#include "command.h"

namespace command {

PipelineCreateInfo::PipelineCreateInfo(
    std::shared_ptr<command::Shader> computeShader,
    std::string entryPointName /*= "main"*/) {
  // Intentionally fill these with zeros. The incorrect sType should block
  // any attempt to pass them to Vulkan APIs.
  memset(&vertsci, 0, sizeof(vertsci));
  memset(&asci, 0, sizeof(asci));
  memset(&viewsci, 0, sizeof(viewsci));
  memset(&rastersci, 0, sizeof(rastersci));
  memset(&multisci, 0, sizeof(multisci));
  memset(&depthsci, 0, sizeof(depthsci));
  memset(&cbsci, 0, sizeof(cbsci));
  memset(&subpassDesc, 0, sizeof(subpassDesc));

  stages.emplace_back();
  auto& pipelinestage = stages.back();
  pipelinestage.info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelinestage.entryPointName = entryPointName;
  pipelinestage.shader = computeShader;
}

Pipeline::Pipeline(CommandPool& computeCommandPool,
                   std::shared_ptr<command::Shader> computeShader,
                   std::string entryPointName /*= "main"*/)
    : info(computeShader, entryPointName),
      pipelineLayout(computeCommandPool.vk.dev, vkDestroyPipelineLayout),
      vk(computeCommandPool.vk.dev, vkDestroyPipeline) {
  pipelineLayout.allocator = computeCommandPool.vk.dev.dev.allocator;
  vk.allocator = computeCommandPool.vk.dev.dev.allocator;
}

int Pipeline::ctorError(CommandPool& computeCommandPool) {
  if (info.stages.size() != 1) {
    logE("Pipeline has %zu stages. Only 1 allowed for a %s.\n",
         info.stages.size(), "compute pipeline");
    return 1;
  }
  stageName.resize(info.stages.size());
  auto& stage = info.stages.at(0);
  if (info.depthsci.sType != 0 ||
      stage.info.stage != VK_SHADER_STAGE_COMPUTE_BIT) {
    logE("Pipeline::ctorError for a %s, is this a %s?\n", "compute pipeline",
         "compute pipeline");
    return 1;
  }
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
  VkResult v = vkCreatePipelineLayout(computeCommandPool.vk.dev.dev, &plci,
                                      computeCommandPool.vk.dev.dev.allocator,
                                      &pipelineLayout);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreatePipelineLayout", v);
  }
  pipelineLayout.onCreate();

  VkComputePipelineCreateInfo p;
  memset(&p, 0, sizeof(p));
  p.sType = autoSType(p);
  p.flags = info.flags;
  stageName.at(0) = stage.entryPointName;
  p.stage = stage.info;
  p.stage.module = stage.shader->vk;
  p.stage.pName = stageName.at(0).c_str();
  VkSpecializationInfo specInfo;
  memset(&specInfo, 0, sizeof(specInfo));
  if (!stage.specialization.empty()) {
    p.stage.pSpecializationInfo = &specInfo;
    specInfo.mapEntryCount = stage.specializationMap.size();
    specInfo.pMapEntries = stage.specializationMap.data();
    specInfo.dataSize = stage.specialization.size();
    specInfo.pData = stage.specialization.data();
  }
  p.layout = pipelineLayout;

  // TODO: Pipeline Cache.
  vk.reset();
  v = vkCreateComputePipelines(computeCommandPool.vk.dev.dev, VK_NULL_HANDLE, 1,
                               &p, computeCommandPool.vk.dev.dev.allocator,
                               &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateComputePipelines", v);
  }
  vk.onCreate();
  return 0;
}

}  // namespace command
