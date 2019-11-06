/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int Pipeline::addDepthImage(const std::vector<VkFormat>& formatChoices,
                            VkClearValue depthClear /*= {1.0f, 0}*/) {
  if (info.depthsci.depthTestEnable) {
    // Advanced use cases like dynamic shadowmaps need to customize even more.
    logE("Pipeline::addDepthImage can only be called once.\n");
    logE("Only vanilla depth testing is supported.\n");
    return 1;
  }
  if (info.depthsci.sType == 0 ||
      (!info.stages.empty() &&
       info.stages.at(0).info.stage == VK_SHADER_STAGE_COMPUTE_BIT)) {
    logE("Pipeline::addDepthImage cannot be called for a %s\n",
         "compute pipeline");
    return 1;
  }
  // Turn on the fixed-function depthTestEnable and depthWriteEnable.
  info.depthsci.depthTestEnable = VK_TRUE;
  info.depthsci.depthWriteEnable = VK_TRUE;

  // This pipeline should clear the depth buffer along with any color buffers.
  clearColors.emplace_back(depthClear);

  VkFormat choice = vk.dev.chooseFormat(
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_IMAGE_TYPE_2D, formatChoices);
  if (choice == VK_FORMAT_UNDEFINED) {
    logE("Pipeline::addDepthImage: none of formatChoices chosen.\n");
    return 1;
  }
  if (vk.dev.depthFormat == VK_FORMAT_UNDEFINED ||
      vk.dev.depthFormat == choice) {
    vk.dev.depthFormat = choice;
  } else {
    logE("Pipeline::addDepthImage chose format %u. But a previous\n", choice);
    logE("call to Pipeline::addDepthImage chose %u.\n", vk.dev.depthFormat);
    return 1;
  }

  // Add a PipelineAttachment which will set some defaults based on knowing
  // this is a VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL attachment.
  info.attach.emplace_back(vk.dev.depthFormat,
                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  return 0;
}

}  // namespace command
