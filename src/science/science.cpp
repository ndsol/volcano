/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

int CommandPoolContainer::onResized(VkExtent2D newSize, size_t poolQindex) {
  if (!pass.vk) {
    // Call RenderPass::ctorError the first time. It will set pass.vk.
    if (pass.ctorError()) {
      return 1;
    }
  }
  language::Device& dev = cpool.vk.dev;
  prevSize = dev.swapChainInfo.imageExtent;
  dev.swapChainInfo.imageExtent = newSize;
  if (cpool.deviceWaitIdle()) {
    logE("onResized: deviceWaitIdle failed\n");
    return 1;
  }
  if (pass.isTargetDefault()) {
    // If pass is using default framebuf of dev, then update the framebuf now
    // RenderPass::ctorError creates a Framebuf for a non-default target.
    if (dev.resetSwapChain(cpool, poolQindex)) {
      logE("onResized: resetSwapChain failed\n");
      return 1;
    }

    auto& extent = dev.swapChainInfo.imageExtent;
    for (size_t i = 0; i < dev.framebufs.size(); i++) {
      if (dev.framebufs.at(i).ctorError(pass.vk, extent.width, extent.height)) {
        logE("CommandPoolContainer::onResized(): framebuf[%zu] failed\n", i);
        return 1;
      }
    }
  }

  for (auto cb : resizeFramebufListeners) {
    for (size_t i = 0; i < dev.framebufs.size(); i++) {
      if (cb.first(cb.second, dev.framebufs.at(i), i, poolQindex)) {
        return 1;
      }
    }
  }
  return 0;
}

int SmartCommandBuffer::submit() {
  if (end() || cpool.submitAndWait(poolQindex, *this)) {
    logE("SmartCommandBuffer::submit: end or submitAndWait failed\n");
    return 1;
  }

  // Clear wantAutoSubmit now that the command buffer has been submitted.
  wantAutoSubmit = false;
  return 0;
}

SmartCommandBuffer::~SmartCommandBuffer() {
  if (wantAutoSubmit) {
    if (submit()) {
      logF("~SmartCommandBuffer: submit failed\n");
    }
  }
  if (ctorErrorSuccess) {
    if (cpool.unborrowOneTimeBuffer(vk)) {
      logF("~SmartCommandBuffer: unborrowOneTimeBuffer failed\n");
    }
  }
  vk = VK_NULL_HANDLE;
}

int SmartCommandBuffer::ctorError() {
  vk = cpool.borrowOneTimeBuffer();
  if (!vk) {
    logE("SmartCommandBuffer:%s failed\n", " borrowOneTimeBuffer");
    return 1;
  }
  if (beginOneTimeUse()) {
    logE("SmartCommandBuffer:%s failed\n", " beginOneTimeUse");
    return 1;
  }
  ctorErrorSuccess = true;
  return 0;
}

int SmartCommandBuffer::autoSubmit() {
  if (!ctorErrorSuccess) {
    logE("SmartCommandBuffer:%s failed\n", " ctorError not called, autoSubmit");
    return 1;
  }
  wantAutoSubmit = true;
  return 0;
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
