/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <sstream>

#include "memory.h"

namespace memory {

int Buffer::validateBufferCreateInfo(const std::vector<uint32_t>& queueFams) {
  if (!info.size || !info.usage) {
    logE("Buffer::ctorError found uninitialized fields\n");
    return 1;
  }

  if (queueFams.size()) {
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;
  }
  info.queueFamilyIndexCount = queueFams.size();
  info.pQueueFamilyIndices = queueFams.data();
  return 0;
}

int Buffer::ctorError(VkMemoryPropertyFlags props,
                      const std::vector<uint32_t>& queueFams) {
  if (validateBufferCreateInfo(queueFams)) {
    return 1;
  }
  // mem.reset() frees the memory. vk.reset() destroys the VkBuffer handle.
  mem.reset();
  vk.reset();
  VkResult v = vkCreateBuffer(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateBuffer", v);
  }
  vk.allocator = mem.dev.dev.allocator;
  vk.onCreate();

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  MemoryRequirements req(mem.dev, *this);
  mem.vmaAlloc.requiredProps = props;
#else
  MemoryRequirements req(mem.dev, *this, VMA_MEMORY_USAGE_UNKNOWN);
  req.info.requiredFlags = props;
#endif
  return mem.alloc(req);
}

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
int Buffer::ctorError(VmaMemoryUsage usage,
                      const std::vector<uint32_t>& queueFams) {
  if (validateBufferCreateInfo(queueFams)) {
    return 1;
  }
  vk.reset();
  VkResult v = vkCreateBuffer(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateBuffer", v);
  }
  vk.allocator = mem.dev.dev.allocator;
  vk.onCreate();
  return mem.alloc({mem.dev, *this, usage});
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

int Buffer::bindMemory(
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    VkDeviceSize offset /*= 0*/
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
) {
  auto& dev = mem.dev;
  VkResult v;
  const char* functionName;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  functionName = "vmaBindBufferMemory";
  v = vmaBindBufferMemory(dev.vmaAllocator, mem.vmaAlloc, vk);
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
    functionName = "vkBindBufferMemory";
    v = vkBindBufferMemory(dev.dev, vk, mem.vmaAlloc.vk, offset);
  } else {
    // Use Vulkan 1.1 features if supported.
    functionName = "vkBindBufferMemory2";
    VkBindBufferMemoryInfo infos[1];
    memset(&infos[0], 0, sizeof(infos[0]));
    infos[0].sType = autoSType(infos[0]);
    infos[0].buffer = vk;
    infos[0].memory = mem.vmaAlloc.vk;
    infos[0].memoryOffset = offset;
    v = vkBindBufferMemory2(dev.dev, sizeof(infos) / sizeof(infos[0]), infos);
  }
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  if (v != VK_SUCCESS) {
    return explainVkResult(functionName, v);
  }
  return 0;
}

int Buffer::reset() {
  mem.reset();
  vk.reset();
  return 0;
}

}  // namespace memory
