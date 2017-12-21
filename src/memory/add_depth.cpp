/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <src/science/science.h>
#include "memory.h"

namespace command {

int Pipeline::addDepthImage(const std::vector<VkFormat>& formatChoices,
                            RenderPass& pass) {
  if (info.depthsci.depthTestEnable) {
    // Advanced use cases like dynamic shadowmaps need to customize even more.
    logE("Pipeline::addDepthImage can only be called once.\n");
    logE("Only vanilla depth testing is supported.\n");
    return 1;
  }
  // Turn on the fixed-function depthTestEnable and depthWriteEnable.
  info.depthsci.depthTestEnable = VK_TRUE;
  info.depthsci.depthWriteEnable = VK_TRUE;

  // RenderPass will clear the depth buffer along with any color buffers.
  pass.clearColors.emplace_back(pass.depthClear);

  VkFormat choice = dev.chooseFormat(
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
      formatChoices);
  if (choice == VK_FORMAT_UNDEFINED) {
    logE("Pipeline::addDepthImage: none of formatChoices chosen.\n");
    return 1;
  }
  if (dev.depthFormat == VK_FORMAT_UNDEFINED || dev.depthFormat == choice) {
    dev.depthFormat = choice;
  } else {
    logE("Pipeline::addDepthImage chose format %u. But a previous\n", choice);
    logE("call to Pipeline::addDepthImage chose %u.\n", dev.depthFormat);
    return 1;
  }

  // Add a PipelineAttachment which will set some defaults based on knowing
  // this is a VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL attachment.
  info.attach.emplace_back(dev.depthFormat,
                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  return 0;
}

}  // namespace command
namespace language {

int Device::addOrUpdateFramebufs(std::vector<VkImage>& images,
                                 command::CommandPool& cpool,
                                 size_t poolQindex) {
  science::SmartCommandBuffer setup{cpool, poolQindex};
  if (setup.ctorError() || setup.autoSubmit()) {
    logE("addOrUpdateFramebufs: SmartCommandBuffer failed\n");
    return 1;
  }

  for (size_t i = 0; i < images.size(); i++) {
    if (i >= framebufs.size()) {
      // Create new framebufs using existing framebufs.at(0) as a template.
      framebufs.emplace_back(*this);
      auto& framebuf = framebufs.back();
      framebuf.image.emplace_back((VkImage)VK_NULL_HANDLE);
      framebuf.attachments.emplace_back(*this);
    } else {
      auto& framebuf = framebufs.at(i);
      if (framebuf.image.size() < 1) {
        logE("Updating framebuf[%zu]: image.size=%zu\n", i,
             framebuf.image.size());
        return 1;
      }
      if (framebuf.attachments.size() < 1) {
        logE("Updating framebuf[%zu]: attachments.size=%zu\n", i,
             framebuf.image.size());
        return 1;
      }
    }

    auto& framebuf = framebufs.at(i);
    framebuf.image.at(0) = images.at(i);
    auto& attachment = framebuf.attachments.at(0);
    // If i==0 (this already is framebufs.at(0)), this is a no-op. Defaults as
    // defined in Language::ImageView will be used.
    attachment.info = framebufs.at(0).attachments.at(0).info;
    if (attachment.ctorError(*this, images.at(i), swapChainInfo.imageFormat)) {
      return 1;
    }

    // Add depth image to attachments if Pipeline::addDepthImage() asked for it.
    if (depthFormat == VK_FORMAT_UNDEFINED) {
      continue;
    }

    // If depthImage is outdated, delete it.
    if (depthImage &&
        (depthImage->info.extent.width != swapChainInfo.imageExtent.width ||
         depthImage->info.extent.height != swapChainInfo.imageExtent.height ||
         depthImage->info.extent.depth != 1)) {
      for (size_t depthI = 0; depthI < framebuf.image.size(); depthI++) {
        if (framebuf.image.at(depthI) == depthImage->vk) {
          framebuf.image.erase(framebuf.image.begin() + depthI);
          break;
        }
      }
      delete depthImage;
      depthImage = nullptr;
    }

    // Now depthImage is correct and already exists, or it is time to make it.
    if (!depthImage) {
      depthImage = new memory::Image(*this);
      depthImage->info.format = depthFormat;
      depthImage->info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      depthImage->info.tiling = VK_IMAGE_TILING_OPTIMAL;
      depthImage->info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      depthImage->info.extent = {1, 1, 1};
      depthImage->info.extent.width = swapChainInfo.imageExtent.width;
      depthImage->info.extent.height = swapChainInfo.imageExtent.height;
      if (depthImage->ctorDeviceLocal(*this) || depthImage->bindMemory(*this)) {
        logE("depthImage->ctorError failed\n");
        return 1;
      }

      command::CommandBuffer::BarrierSet bset;
      bset.img.emplace_back(depthImage->makeTransition(
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
      depthImage->currentLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      // Transitioning to a depth format only affects the depth test stage
      // fixed function, so use VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT.
      if (setup.barrier(bset, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)) {
        return 1;
      }
    }

    framebuf.image.emplace_back(depthImage->vk);

    // It is also possible to have one ImageView and share just the vkImageView
    // handle among all the Framebufs. But an ImageView per framebuf isn't much
    // memory to allocate.
    if (!framebuf.depthImageViewAt1) {
      if (framebuf.attachments.size() < 2) {
        framebuf.attachments.emplace_back(*this);
      }
      framebuf.depthImageViewAt1 = true;
    }

    auto& view = framebuf.attachments.at(1);
    view.info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (view.ctorError(*this, depthImage->vk, depthFormat)) {
      logE("addOrUpdateFramebufs: depth image view.ctorError failed\n");
      return 1;
    }
  }

  // Delete any more framebufs. Make framebufs.size() match images.size().
  while (framebufs.size() > images.size()) {
    framebufs.pop_back();
  }
  return 0;
}

Device::~Device() {
  if (depthImage) {
    delete depthImage;
    depthImage = nullptr;
  }
}

}  // namespace language
