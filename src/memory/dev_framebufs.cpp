/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

namespace command {

VkExtent3D RenderPass::getTargetExtent() const {
  if (image) {
    return image->info.extent;
  }
  VkExtent3D r;
  r.width = vk.dev.swapChainInfo.imageExtent.width;
  r.height = vk.dev.swapChainInfo.imageExtent.height;
  r.depth = 1;
  return r;
}

VkFormat RenderPass::getTargetFormat() const {
  if (image) {
    return image->info.format;
  }
  return vk.dev.swapChainInfo.imageFormat;
}

}  // namespace command

namespace language {

int Device::addOrUpdateFramebufs(std::vector<VkImage>& images,
                                 command::CommandPool& cpool,
                                 size_t poolQindex) {
  command::CommandBuffer setup{cpool};
  char name[256];
  for (size_t i = 0; i < images.size(); i++) {
    if (i >= framebufs.size()) {
      // Create new framebufs using existing framebufs.at(0) as a template.
      framebufs.emplace_back(*this);
      auto& framebuf = framebufs.back();
      framebuf.image.emplace_back((VkImage)VK_NULL_HANDLE);
      framebuf.attachments.emplace_back(*this);

      // Default name is not the empty string. Your app can change this.
      snprintf(name, sizeof(name), "framebuf[%zu]", framebufs.size() - 1);
      if (framebuf.setName(name)) {
        logE("framebuf[%zu].setName failed\n", framebufs.size() - 1);
        return 1;
      }
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
    snprintf(name, sizeof(name), "framebuf[%zu] ImageView", i);
    // If i==0 (this already is framebufs.at(0)), this is a no-op. Defaults as
    // defined in Language::ImageView will be used.
    attachment.info = framebufs.at(0).attachments.at(0).info;
    if (attachment.setName(name) ||
        attachment.ctorError(images.at(i), swapChainInfo.imageFormat)) {
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
      snprintf(name, sizeof(name), "framebuf[%zu] depthImage", i);
      if (depthImage->setName(name)) {
        logE("%s.setName failed\n", name);
        return 1;
      }
      depthImage->info.format = depthFormat;
      depthImage->info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      depthImage->info.tiling = VK_IMAGE_TILING_OPTIMAL;
      depthImage->info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      depthImage->info.extent = {1, 1, 1};
      depthImage->info.extent.width = swapChainInfo.imageExtent.width;
      depthImage->info.extent.height = swapChainInfo.imageExtent.height;
      if (depthImage->ctorAndBindDeviceLocal()) {
        logE("depthImage->ctorAndBindError failed\n");
        return 1;
      }

      if (!setup.vk) {
        setup.vk = cpool.borrowOneTimeBuffer();
        if (!setup.vk || setup.beginOneTimeUse()) {
          logE("Device::addOrUpdateFramebufs: borrow or begin failed\n");
          return 1;
        }
      }
      if (setup.barrier(*depthImage,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
        return 1;
      }
      // Transitioning to a depth format only affects the depth test stage
      // fixed function, so use VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT.
      //
      // Changing the srcStageMask or dstStageMask requires some care -
      // setup.lazyBarriers is not added immediately (hence it is "lazy"). That
      // allows setup to be customized here:
      setup.lazyBarriers.dstStageMask =
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
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

    snprintf(name, sizeof(name), "framebuf[%zu] depthImage.ImageView", i);
    auto& view = framebuf.attachments.at(1);
    view.info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (view.setName(name) || view.ctorError(depthImage->vk, depthFormat)) {
      logE("addOrUpdateFramebufs: depth image view.ctorError failed\n");
      return 1;
    }
  }

  // Delete any more framebufs. Make framebufs.size() match images.size().
  while (framebufs.size() > images.size()) {
    framebufs.pop_back();
  }
  // setup.vk is only non-null if barriers were added.
  if (!setup.vk) {
    return 0;
  }
  // Flush any commands in setup.vk.
  if (setup.end() || cpool.submitAndWait(poolQindex, setup)) {
    (void)cpool.unborrowOneTimeBuffer(setup.vk);
    logE("Device::addOrUpdateFramebufs: end or submitAndWait failed\n");
    return 1;
  }
  if (cpool.unborrowOneTimeBuffer(setup.vk)) {
    logE("Device::addOrUpdateFramebufs: unborrowOneTimeBuffer failed\n");
    return 1;
  }
  return 0;
}

Device::~Device() {
  if (depthImage) {
    delete depthImage;
    depthImage = nullptr;
  }
  if (vmaAllocator) {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    logE("~Device: vmaAllocator should be NULL. Memory corruption detected.");
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
    vmaDestroyAllocator(vmaAllocator);
    vmaAllocator = VK_NULL_HANDLE;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  }
}

}  // namespace language
