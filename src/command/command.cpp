/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int CommandPool::ctorError(VkCommandPoolCreateFlags flags) {
  auto qfam_i = dev.getQfamI(queueFamily);
  if (qfam_i == (decltype(qfam_i)) - 1) {
    return 1;
  }

  // Cache QueueFamily, as all commands in this pool must be submitted here.
  qf_ = &dev.qfams.at(qfam_i);

  VkCommandPoolCreateInfo VkInit(cpci);
  cpci.queueFamilyIndex = qfam_i;
  cpci.flags = flags;
  VkResult v = vkCreateCommandPool(dev.dev, &cpci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateCommandPool", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int CommandPool::alloc(std::vector<VkCommandBuffer>& buf,
                       VkCommandBufferLevel level) {
  VkCommandBufferAllocateInfo VkInit(ai);
  ai.commandPool = vk;
  ai.level = level;
  ai.commandBufferCount = (decltype(ai.commandBufferCount))buf.size();

  VkResult v = vkAllocateCommandBuffers(dev.dev, &ai, buf.data());
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkAllocateCommandBuffers", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

CommandBuilder::~CommandBuilder() { cpool.free(bufs); }

}  // namespace command
