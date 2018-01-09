/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int Semaphore::ctorError(language::Device& dev) {
  VkSemaphoreCreateInfo VkInit(sci);
  VkResult v = vkCreateSemaphore(dev.dev, &sci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSemaphore", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Fence::ctorError(language::Device& dev) {
  VkFenceCreateInfo VkInit(fci);
  VkResult v = vkCreateFence(dev.dev, &fci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateFence", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Fence::reset(language::Device& dev) {
  VkFence fences[] = {vk};
  VkResult v =
      vkResetFences(dev.dev, sizeof(fences) / sizeof(fences[0]), fences);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkResetFences", v, string_VkResult(v));
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
    logE("%s failed: %d (%s)\n", "vkCreateEvent", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

}  // namespace command
