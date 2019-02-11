/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <src/core/VkEnum.h>
#include <src/core/VkInit.h>

#include <algorithm>

#include "language.h"

namespace language {
using namespace VkEnum;

namespace {  // an anonymous namespace hides its contents outside this file

uint32_t calculateMinRequestedImages(const VkSurfaceCapabilitiesKHR& scap) {
  // An optimal number of images is one more than the minimum. For example:
  // double buffering minImageCount = 1. imageCount = 2.
  // triple buffering minImageCount = 2. imageCount = 3.
  uint32_t imageCount = scap.minImageCount + 1;

  // maxImageCount = 0 means "there is no maximum except device memory limits".
  if (scap.maxImageCount > 0 && imageCount > scap.maxImageCount) {
    imageCount = scap.maxImageCount;
  }

  // Note: The GPU driver can create more than the number returned here.
  // Device::images.size() gives the actual number created by the GPU driver.
  //
  // https://forums.khronos.org/showthread.php/13489-Number-of-images-created-in-a-swapchain
  return imageCount;
}

VkExtent2D calculateSurfaceExtent2D(const VkSurfaceCapabilitiesKHR& scap,
                                    VkExtent2D sizeRequest) {
  // If currentExtent != { UINT32_MAX, UINT32_MAX } then Vulkan is telling us:
  // "this is the right extent: you already created a surface and Vulkan
  // computed the right size to match it."
  if (scap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return scap.currentExtent;
  }

  // Vulkan is telling us "choose width, height from scap.minImageExtent
  // to scap.maxImageExtent." Attempt to satisfy sizeRequest.
  const VkExtent2D &lo = scap.minImageExtent, hi = scap.maxImageExtent;
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
                               const VkSurfaceCapabilitiesKHR& scap) {
  // Use the currentTransform value for preTransform.
  swapChainInfo.preTransform = scap.currentTransform;
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
  VkSurfaceCapabilitiesKHR scap;
  VkResult v = getSurfaceCapabilities(scap);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", v);
  }

  swapChainInfo.imageExtent =
      calculateSurfaceExtent2D(scap, swapChainInfo.imageExtent);
  calculateSurfaceTransform(swapChainInfo, scap);
  swapChainInfo.minImageCount = calculateMinRequestedImages(scap);
#ifdef __ANDROID__
  // Want OPAQUE (i.e. no-op), but the only option is INHERIT (i.e. unknown)
  if (swapChainInfo.compositeAlpha == VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR &&
      scap.supportedCompositeAlpha == VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
    logD("Android workaround: compositeAlpha now INHERIT (was OPAQUE).\n");
    swapChainInfo.compositeAlpha =
        (VkCompositeAlphaFlagBitsKHR)scap.supportedCompositeAlpha;
  }
#endif
  if ((swapChainInfo.compositeAlpha & scap.supportedCompositeAlpha) == 0) {
    logE("compositeAlpha %x not in scap.supportedCompositeAlpha\n",
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

  auto* vkImages = Vk::getSwapchainImages(dev, swapChain);
  if (!vkImages) {
    return 1;
  }

  // Update framebufs preserving existing FrameBuf elements and adding new ones.
  if (addOrUpdateFramebufs(*vkImages, cpool, poolQindex)) {
    delete vkImages;
    return 1;
  }
  delete vkImages;
  return 0;
}

}  // namespace language
