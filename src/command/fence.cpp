/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int Semaphore::ctorError(language::Device& dev) {
  VkSemaphoreCreateInfo VkInit(sci);
  VkResult v = vkCreateSemaphore(dev.dev, &sci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateSemaphore", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int PresentSemaphore::ctorError() {
  if (Semaphore::ctorError(dev)) {
    return 1;
  }

  auto qfam_i = dev.getQfamI(language::PRESENT);
  if (qfam_i == (decltype(qfam_i)) - 1) {
    return 1;
  }
  auto& qfam = dev.qfams.at(qfam_i);
  if (qfam.queues.size() < 1) {
    fprintf(stderr, "BUG: queue family PRESENT with %zu queues\n",
            qfam.queues.size());
    return 1;
  }

  // Assume that any queue in this family is acceptable.
  q = qfam.queues.back();
  return 0;
};

int PresentSemaphore::present(uint32_t image_i) {
  VkSemaphore semaphores[] = {vk};
  VkSwapchainKHR swapChains[] = {dev.swapChain};

  VkPresentInfoKHR VkInit(presentInfo);
  presentInfo.waitSemaphoreCount = sizeof(semaphores) / sizeof(semaphores[0]);
  presentInfo.pWaitSemaphores = semaphores;
  presentInfo.swapchainCount = sizeof(swapChains) / sizeof(swapChains[0]);
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &image_i;

  VkResult v = vkQueuePresentKHR(q, &presentInfo);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkQueuePresentKHR", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
};

int Fence::ctorError(language::Device& dev) {
  VkFenceCreateInfo VkInit(fci);
  VkResult v = vkCreateFence(dev.dev, &fci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateFence", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int Fence::reset(language::Device& dev) {
  VkFence fences[] = {vk};
  VkResult v =
      vkResetFences(dev.dev, sizeof(fences) / sizeof(fences[0]), fences);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkResetFences", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

VkResult Fence::wait(language::Device& dev, uint64_t timeoutNanos) {
  VkFence fences[] = {vk};
  return vkWaitForFences(dev.dev, sizeof(fences) / sizeof(fences[0]), fences,
                         VK_FALSE, timeoutNanos);
}

VkResult Fence::getStatus(language::Device& dev) {
  return vkGetFenceStatus(dev.dev, vk);
}

int Event::ctorError(language::Device& dev) {
  VkEventCreateInfo VkInit(eci);
  VkResult v = vkCreateEvent(dev.dev, &eci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateEvent", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

}  // namespace command
