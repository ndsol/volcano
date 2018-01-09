/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

namespace memory {

int DeviceMemory::alloc(MemoryRequirements req, VkMemoryPropertyFlags props) {
  // This relies on the fact that req.ofProps() updates req.vkalloc.
  if (!req.ofProps(props)) {
    return 1;
  }
  allocSize = req.vkalloc.allocationSize;
  vk.reset();
  VkResult v =
      vkAllocateMemory(req.dev.dev, &req.vkalloc, req.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkAllocateMemory", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int DeviceMemory::mmap(language::Device& dev, void** pData,
                       VkDeviceSize offset /*= 0*/,
                       VkDeviceSize size /*= VK_WHOLE_SIZE*/,
                       VkMemoryMapFlags flags /*= 0*/) {
  VkResult v = vkMapMemory(dev.dev, vk, offset, size, flags, pData);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkMapMemory", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

void DeviceMemory::makeRange(VkMappedMemoryRange& range, VkDeviceSize offset,
                             VkDeviceSize size) {
  VkOverwrite(range);
  range.memory = vk;
  range.offset = offset;
  range.size = size;
}

int DeviceMemory::flush(language::Device& dev,
                        std::vector<VkMappedMemoryRange> mem) {
  if (!mem.size()) {
    logE("DeviceMemory::flush: vector<VkMappedMemoryRange>.size=0\n");
    return 1;
  }
  for (auto i = mem.begin(); i != mem.end(); i++) {  // Force .memory to be vk.
    i->memory = vk;
  }
  VkResult v = vkFlushMappedMemoryRanges(dev.dev, mem.size(), mem.data());
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkFlushMappedMemoryRanges", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

int DeviceMemory::invalidate(language::Device& dev,
                             const std::vector<VkMappedMemoryRange>& mem) {
  VkResult v = vkInvalidateMappedMemoryRanges(dev.dev, mem.size(), mem.data());
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkInvalidateMappedMemoryRanges", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

void DeviceMemory::munmap(language::Device& dev) { vkUnmapMemory(dev.dev, vk); }

int MemoryRequirements::indexOf(VkMemoryPropertyFlags props) const {
  for (uint32_t i = 0; i < dev.memProps.memoryTypeCount; i++) {
    if ((vk.memoryTypeBits & (1 << i)) &&
        (dev.memProps.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  return -1;
}

VkMemoryAllocateInfo* MemoryRequirements::ofProps(VkMemoryPropertyFlags props) {
  // TODO: optionally accept another props with fewer bits (i.e. first props are
  // the most optimal, second props are the bare minimum).
  int i = indexOf(props);
  if (i == -1) {
    logE("MemoryRequirements::indexOf(%x): not found in %x\n", props,
         vk.memoryTypeBits);
    return nullptr;
  }

  vkalloc.memoryTypeIndex = i;
  vkalloc.allocationSize = vk.size;

  return &vkalloc;
}

}  // namespace memory
