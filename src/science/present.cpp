/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

CommandPoolContainer::~CommandPoolContainer(){};

int CommandPoolContainer::acquireNextImage(
    uint32_t frameNumber, uint32_t* next_image_i,
    command::Semaphore& imageAvailableSemaphore,
    uint64_t timeout /*= std::numeric_limits<uint64_t>::max()*/,
    VkFence fence /*= VK_NULL_HANDLE*/,
    size_t poolQindex /*= memory::ASSUME_POOL_QINDEX*/) {
  auto& dev = cpool.vk.dev;
  if (!dev.swapChain) {
    // if swapChain was reset (happens at any time on Android) do not proceed.
    *next_image_i = (uint32_t)-1;
    return 0;
  }
  dev.setFrameNumber(frameNumber);
  VkResult result =
      vkAcquireNextImageKHR(dev.dev, dev.swapChain, timeout,
                            imageAvailableSemaphore.vk, fence, next_image_i);
  switch (result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
      break;

    case VK_ERROR_OUT_OF_DATE_KHR:
      if (
#ifdef __ANDROID__
          // On Android, surface being destroyed may return OUT_OF_DATE.
          dev.getSurface() &&
#endif
          onResized(dev.swapChainInfo.imageExtent, poolQindex)) {
        logE("acquireNextImage: OUT_OF_DATE, but onResized failed\n");
        return 1;
      }
      *next_image_i = (uint32_t)-1;
      return 0;

    case VK_ERROR_SURFACE_LOST_KHR:
      // VK_ERROR_SURFACE_LOST_KHR can be recovered if the device is recreated.
      // On Android, glfw asks the app nicely to exit, then loops around again.
      *next_image_i = (uint32_t)-1;
      break;

    default:
      return explainVkResult("vkAcquireNextImageKHR", result);
  }
  return 0;
}

int PresentSemaphore::ctorError() {
  if (Semaphore::ctorError()) {
    return 1;
  }

  auto qfam_i = vk.dev.getQfamI(queueFamily);
  if (qfam_i == (decltype(qfam_i))(-1)) {
    logE("PresentSemaphore::ctorError: dev.getQfamI(%d) failed\n", queueFamily);
    return 1;
  }
  auto& qfam = vk.dev.qfams.at(qfam_i);
  if (qfam.queues.size() < 1) {
    logE("BUG: queue family PRESENT with %zu queues\n", qfam.queues.size());
    return 1;
  }
  q = qfam.queues.at(memory::ASSUME_PRESENT_QINDEX);
  return 0;
}

int PresentSemaphore::present(
    uint32_t* image_i, size_t poolQindex /*= memory::ASSUME_POOL_QINDEX*/) {
  auto& dev = parent.cpool.vk.dev;
  VkSemaphore semaphores[] = {vk};
  VkSwapchainKHR swapChains[] = {dev.swapChain};
  if (dev.framebufs.at(*image_i).dirty) {
    logW("framebuf[%u] dirty and has not been rebuilt before present\n",
         *image_i);
  }

  VkPresentInfoKHR VkInit(presentInfo);
  presentInfo.waitSemaphoreCount = sizeof(semaphores) / sizeof(semaphores[0]);
  presentInfo.pWaitSemaphores = semaphores;
  presentInfo.swapchainCount = sizeof(swapChains) / sizeof(swapChains[0]);
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = image_i;

  VkResult v = vkQueuePresentKHR(q, &presentInfo);
  switch (v) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
      break;

    case VK_ERROR_OUT_OF_DATE_KHR:
      if (parent.onResized(dev.swapChainInfo.imageExtent, poolQindex)) {
        logE("present: OUT_OF_DATE, but onResized failed\n");
        return 1;
      }
      *image_i = (uint32_t)-1;
      break;

    case VK_ERROR_SURFACE_LOST_KHR:
      // VK_ERROR_SURFACE_LOST_KHR can be recovered if the device is recreated.
      // On Android, glfw asks the app nicely to exit, then loops around again.
      *image_i = (uint32_t)-1;
      break;

    default:
      return explainVkResult("vkQueuePresentKHR", v);
  }
  return 0;
}

int PresentSemaphore::waitIdle() {
  VkResult v = vkQueueWaitIdle(q);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkQueueWaitIdle", v);
  }
  return 0;
}

}  // namespace science
