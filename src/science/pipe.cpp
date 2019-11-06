/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */

#include "science.h"

namespace science {

int PipeBuilder::deriveFrom(PipeBuilder& other) {
  pipe = std::make_shared<command::Pipeline>(pass);
  pipe->info = other.info();
  pipe->commandBufferType = other.pipe->commandBufferType;
  pipe->clearColors = other.pipe->clearColors;
  return 0;
}

int PipeBuilder::swap(PipeBuilder& other) {
  if (!pipe) {
    logE("PipeBuilder::swap: this pipe is NULL\n");
    return 1;
  }
  if (&pass != &other.pass) {
    logE("PipeBuilder::swap: this.pass=%p other.pass=%p - cannot differ.\n",
         &pass, &other.pass);
    return 1;
  }

  // Take it for granted that *this and other are compatible. Validating that
  // is a big job best left to the Vulkan validation layers.
  //
  // If *this and other are not compatible, swap() produces undefined behavior
  // up to and including a hard lockup.
  for (size_t i = 0; i < pass.pipelines.size(); i++) {
    if (pass.pipelines.at(i) == other.pipe) {
      pass.pipelines.at(i) = pipe;
      return 0;
    }
  }

  logE("PipeBuilder::swap: PipeBuilder %p with pipe %p not found in pass %p\n",
       &other, &*other.pipe, pass.vk ? pass.vk.printf() : NULL);
  return 1;
}

int PipeBuilder::alphaBlendWith(const command::PipelineCreateInfo& prevPipeInfo,
                                VkObjectType boundary) {
  auto& pipeInfo = info();
  if (pipeInfo.attach.size() > prevPipeInfo.attach.size()) {
    // To make pipeInfo compatible with prevPipeInfo, this can add attachments
    // but only your app knows what to do if attachments must be removed.
    logE("alphaBlendWith: %zu attachments when prevPipe has %zu\n",
         pipeInfo.attach.size(), prevPipeInfo.attach.size());
    return 1;
  }

  // Tell pipeline to alpha blend with what is already in framebuffer.
  pipeInfo.perFramebufColorBlend.at(0) =
      command::PipelineCreateInfo::withEnabledAlpha();

  // Update the loadOp to load data from the framebuffer, instead of a CLEAR_OP.
  for (size_t i = 0; i < pipeInfo.attach.size(); i++) {
    auto& attach = pipeInfo.attach.at(i);
    switch (boundary) {
      case VK_OBJECT_TYPE_RENDER_PASS:
        attach.vk.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attach.vk.initialLayout = prevPipeInfo.attach.at(i).vk.finalLayout;
        break;
      case VK_OBJECT_TYPE_PIPELINE:
        // Copy from prevPipeInfo. Attachments must match exactly.
        attach.vk = prevPipeInfo.attach.at(i).vk;
        break;
      default:
        logE("alphaBlendWith(boundary %u) not supported.\n", boundary);
        return 1;
    }
  }

  if (pass.vk.dev.GetDepthFormat() != VK_FORMAT_UNDEFINED) {
    // If pass is using a depth buffer, add it to this pipeline so it matches.
    if (addDepthImage({pass.vk.dev.GetDepthFormat()})) {
      logE("alphaBlendWith: addDepthImage failed (trying to match format)\n");
      return 1;
    }
  }
  return 0;
}

}  // namespace science
