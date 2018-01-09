/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

Pipeline::~Pipeline() {}

int RenderPass::getSubpassDeps(size_t subpass_i,
                               std::vector<VkSubpassDependency>& subpassdeps) {
  // Link this subpass to the previous one.
  VkSubpassDependency VkInit(fromprev);
  fromprev.srcSubpass =
      (subpass_i == 0) ? VK_SUBPASS_EXTERNAL : (subpass_i - 1);
  fromprev.dstSubpass = subpass_i;
  fromprev.dependencyFlags = 0;
  if (subpass_i == 0) {
    fromprev.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    fromprev.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  } else {
    fromprev.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    fromprev.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if (subpass_i == pipelines.size() - 1) {
    // TODO: This can be COLOR_ATTACHMENT_OUTPUT also? When?
    fromprev.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    fromprev.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  } else {
    fromprev.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    fromprev.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  subpassdeps.push_back(fromprev);
  return 0;
}

int RenderPass::updateFormat() { return 0; }

}  // namespace command
