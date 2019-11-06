/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <src/core/VkEnum.h>

#include <algorithm>

#include "language.h"

namespace language {
using namespace VkEnum;

namespace {  // an anonymous namespace hides its contents outside this file

uint32_t calculateMinRequestedImages(const VkSurfaceCapabilitiesKHR& cap) {
  // An optimal number of images is one more than the minimum. For example:
  // double buffering minImageCount = 1. imageCount = 2.
  // triple buffering minImageCount = 2. imageCount = 3.
  uint32_t imageCount = cap.minImageCount + 1;

  // maxImageCount = 0 means "there is no maximum except device memory limits".
  if (cap.maxImageCount > 0 && imageCount > cap.maxImageCount) {
    imageCount = cap.maxImageCount;
  }

  // Note: The GPU driver can create more than the number returned here.
  // Device::images.size() gives the actual number created by the GPU driver.
  //
  // https://forums.khronos.org/showthread.php/13489-Number-of-images-created-in-a-swapchain
  return imageCount;
}

VkExtent2D calculateSurfaceExtent2D(const VkSurfaceCapabilitiesKHR& cap,
                                    VkExtent2D sizeRequest) {
  // If currentExtent != { UINT32_MAX, UINT32_MAX } then Vulkan is telling us:
  // "this is the right extent: you already created a surface and Vulkan
  // computed the right size to match it."
  if (cap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return cap.currentExtent;
  }

  // Vulkan is telling us "choose width, height from cap.minImageExtent
  // to cap.maxImageExtent." Attempt to satisfy sizeRequest.
  const VkExtent2D &lo = cap.minImageExtent, hi = cap.maxImageExtent;
  if (hi.width == 0 || hi.height == 0 || hi.width < lo.width ||
      hi.height < lo.height) {
    logF("calculateSurfaceExtent2D: window is minimized, will fail.\n");
  }
  return {
      /*width:*/ std::max(lo.width, std::min(hi.width, sizeRequest.width)),
      /*height:*/
      std::max(lo.height, std::min(hi.height, sizeRequest.height)),
  };
}

void calculateSurfaceTransform(VkSwapchainCreateInfoKHR& swapChainInfo,
                               const VkSurfaceCapabilitiesKHR& cap) {
  // Use the currentTransform value for preTransform.
  swapChainInfo.preTransform = cap.currentTransform;
  // To do this requires your app to rotate content to match.
  uint32_t t;
  switch (swapChainInfo.preTransform) {
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
      t = swapChainInfo.imageExtent.width;
      swapChainInfo.imageExtent.width = swapChainInfo.imageExtent.height;
      swapChainInfo.imageExtent.height = t;
      break;
    default:
      break;
  }
}

}  // anonymous namespace

int Device::resetSwapChain(command::CommandPool& cpool, size_t poolQindex) {
  VkResult v = scap.getProperties(*this, getSurface());
  if (v != VK_SUCCESS) {
    return explainVkResult("vkGetPhysicalDeviceSurfaceCapabilities", v);
  }

  auto& cap = scap.surfaceCapabilities;

  swapChainInfo.imageExtent =
      calculateSurfaceExtent2D(cap, swapChainInfo.imageExtent);
  calculateSurfaceTransform(swapChainInfo, cap);
  swapChainInfo.minImageCount = calculateMinRequestedImages(cap);
#ifdef __ANDROID__
  // Want OPAQUE (i.e. no-op), but the only option is INHERIT (i.e. unknown)
  if (swapChainInfo.compositeAlpha == VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR &&
      cap.supportedCompositeAlpha == VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
    logD("Android workaround: compositeAlpha now INHERIT (was OPAQUE).\n");
    swapChainInfo.compositeAlpha =
        (VkCompositeAlphaFlagBitsKHR)cap.supportedCompositeAlpha;
  }
#endif
  if ((swapChainInfo.compositeAlpha & cap.supportedCompositeAlpha) == 0) {
    logE("compositeAlpha %x not in cap.supportedCompositeAlpha\n",
         swapChainInfo.compositeAlpha);
    return 1;
  }

  VkSwapchainCreateInfoKHR scci = swapChainInfo;
  scci.surface = getSurface();
  scci.oldSwapchain = VK_NULL_HANDLE;
  if (swapChain) {
    scci.oldSwapchain = swapChain;
  }
  uint32_t qfamIndices[] = {
      (uint32_t)getQfamI(PRESENT),
      (uint32_t)getQfamI(GRAPHICS),
  };
  if (qfamIndices[0] == qfamIndices[1]) {
    // Device queues were set up such that one QueueFamily does both
    // PRESENT and GRAPHICS.
    scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    scci.queueFamilyIndexCount = 0;
    scci.pQueueFamilyIndices = nullptr;
  } else {
    // Device queues were set up such that a different QueueFamily does PRESENT
    // and a different QueueFamily does GRAPHICS.
    logW(
        "SHARING_MODE_CONCURRENT: what GPU is this? It has never been seen.\n");
    logW("TODO: Test a per-resource barrier (queue ownership transfer).\n");
    // Is a queue ownership transfer faster than SHARING_MODE_CONCURRENT?
    // Measure, measure, measure!
    //
    // Note also that a CONCURRENT swapchain, if moved to a different queue in
    // the same QueueFamily, must be done by an ownership barrier.
    scci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    scci.queueFamilyIndexCount = 2;
    scci.pQueueFamilyIndices = qfamIndices;
  }
  VkSwapchainKHR newSwapChain;
  v = vkCreateSwapchainKHR(dev, &scci, dev.allocator, &newSwapChain);
  if (v != VK_SUCCESS) {
#ifdef __ANDROID__
    if (v == VK_ERROR_SURFACE_LOST_KHR) {
      destroySurface();  // This is recoverable but surface must be redone.
      return 0;
    }
#endif /*__ANDROID__*/
    return explainVkResult("vkCreateSwapchainKHR", v);
  }
  // swapChain.inst == VK_NULL_HANDLE the first time through,
  // swapChain needs to be reset to use dev.
  //
  // Also, calling reset here avoids deleting dev.swapChain until after
  // vkCreateSwapchainKHR().
  swapChain.reset();             // Delete the old dev.swapChain.
  *(&swapChain) = newSwapChain;  // Install the new dev.swapChain.
  swapChain.allocator = dev.allocator;
  swapChain.onCreate();

  std::vector<VkImage> vkImages;
  return Vk::getSwapchainImages(dev, swapChain, vkImages) ||
         // Preserve existing FrameBuf elements, add any new ones.
         addOrUpdateFramebufs(vkImages, cpool, poolQindex);
}

void Device::destroySurface() {
  // Destroy swapChain before destroying surface.
  swapChain.reset();
  scap.reset();
  inst->surface.reset(inst->vk);
}

}  // namespace language
