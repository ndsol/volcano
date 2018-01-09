/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

void ImageCopies::addSrc(memory::Image& src) {
  for (uint32_t mipLevel = 0; mipLevel < src.info.mipLevels; mipLevel++) {
    addSrcAtMipLevel(src, mipLevel);
    SubresUpdate<VkImageSubresourceLayers>(back().dstSubresource)
        .setMipLevel(mipLevel);
  }
}

void ImageCopies::addSrcAtMipLevel(memory::Image& src, uint32_t mipLevel) {
  emplace_back();
  VkImageCopy& region = back();
  Subres(region.srcSubresource).addColor().setMipLevel(mipLevel);
  Subres(region.dstSubresource).addColor();

  region.srcOffset = {0, 0, 0};
  region.dstOffset = {0, 0, 0};

  region.extent = src.info.extent;
  region.extent.width >>= mipLevel;
  region.extent.height >>= mipLevel;
}

SmartCommandBuffer::~SmartCommandBuffer() {
  if (wantAutoSubmit) {
    if (end()) {
      logF("~SmartCommandBuffer: end failed\n");
    }
    if (submit(poolQindex)) {
      logF("~SmartCommandBuffer: submit(%zu) failed\n", poolQindex);
    }
  }
  if (ctorErrorSuccess) {
    if (cpool.unborrowOneTimeBuffer(vk)) {
      logF("~SmartCommandBuffer: unborrowOneTimeBuffer failed\n");
    }
  }
  vk = VK_NULL_HANDLE;
  if (wantAutoSubmit) {
    VkResult v = vkQueueWaitIdle(cpool.q(poolQindex));
    if (v != VK_SUCCESS) {
      logF("%s failed: %d (%s)\n", "vkQueueWaitIdle", v, string_VkResult(v));
    }
  }
}

int PipeBuilder::alphaBlendWithPreviousPass(
    const command::PipelineCreateInfo& prevPipeInfo) {
  auto& pipeInfo = info();
  if (pipeInfo.attach.size() > prevPipeInfo.attach.size()) {
    logE("alphaBlendWithPreviousPass: %zu attachments when prevPipe has %zu\n",
         pipeInfo.attach.size(), prevPipeInfo.attach.size());
    return 1;
  }

  // Tell pipeline to alpha blend with what is already in framebuffer.
  pipeInfo.perFramebufColorBlend.at(0) =
      command::PipelineCreateInfo::withEnabledAlpha();

  // Update the loadOp to load data from the framebuffer, instead of a CLEAR_OP.
  for (size_t i = 0; i < pipeInfo.attach.size(); i++) {
    auto& attach = pipeInfo.attach.at(i);
    attach.vk.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attach.vk.initialLayout = prevPipeInfo.attach.at(i).vk.finalLayout;
  }

  // Autodetect if the device is using a depth buffer.
  if (dev.GetDepthFormat() != VK_FORMAT_UNDEFINED) {
    // Match the format of the framebuf.
    if (addDepthImage({dev.GetDepthFormat()})) {
      logE("addDepthImage failed (trying to match framebuf format)\n");
      return 1;
    }
  }
  return 0;
}

}  // namespace science
