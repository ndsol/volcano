/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

CommandPoolContainer::~CommandPoolContainer(){};

int CommandPoolContainer::onResized(VkExtent2D newSize, size_t poolQindex) {
  if (!pass.vk) {
    // Call RenderPass::ctorError the first time. It will set pass.vk.
    if (pass.ctorError()) {
      return 1;
    }
  }
  language::Device& dev = cpool.vk.dev;
#ifdef __ANDROID__
  if (!dev.getSurface()) {
    return 0;  // Resize timing is wrong, ignore and it will fire again.
  }
#endif /*__ANDROID__*/
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
#ifdef __ANDROID__
    if (!dev.getSurface()) {
      return 0;  // resetSwapChain destroyed surface.
    }
#endif /*__ANDROID__*/

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

}  // namespace science
