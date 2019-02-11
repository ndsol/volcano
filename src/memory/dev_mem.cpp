/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This contains the implementation of the DeviceMemory class.
 */
#include "memory.h"
#include "vulkan/vk_format_utils.h"

namespace memory {

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
DeviceMemory::~DeviceMemory() { reset(); }

void DeviceMemory::reset() {
  DeviceMemory::lock_guard_t lock(lockmutex);
  if (vmaAlloc) {
    if (!dev.vmaAllocator) {
      logF("~DeviceMemory: Device destroyed already or not created yet.\n");
      return;
    }
    VmaAllocationInfo info;
    if (getAllocInfo(info)) {
      logF("~DeviceMemory: BUG: getAllocInfo failed\n");
    }
    if (info.pMappedData) {
      vmaUnmapMemory(dev.vmaAllocator, vmaAlloc);
    }
    vmaFreeMemory(dev.vmaAllocator, vmaAlloc);
    vmaAlloc = nullptr;
  }
}

int DeviceMemory::setName(const std::string& name) {
  this->name = name;
  if (vmaAlloc) {
    DeviceMemory::lock_guard_t lock(lockmutex);
    vmaSetAllocationUserData(dev.vmaAllocator, vmaAlloc,
                             const_cast<char*>(name.c_str()));
  }
  return 0;
}

const std::string& DeviceMemory::getName() {
  if (vmaAlloc) {
    VmaAllocationInfo info;
    if (getAllocInfo(info)) {
      // If getter fails, just return last-known value.
      return name;
    }
    name = static_cast<const char*>(info.pUserData);
  }
  return name;
}

int DeviceMemory::alloc(MemoryRequirements req) {
  isImage = !!req.vkimg;
  if (!dev.vmaAllocator) {
    DeviceMemory::lock_guard_t lock(dev.lockmutex);
    if (!dev.phys || !dev.dev) {
      logE("alloc: device not created yet\n");
      return 1;
    }
    VmaAllocatorCreateInfo allocatorInfo;
    memset(&allocatorInfo, 0, sizeof(allocatorInfo));
    allocatorInfo.physicalDevice = dev.phys;
    allocatorInfo.device = dev.dev;
    allocatorInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
#ifdef __ANDROID__
    if (!vkGetPhysicalDeviceProperties) {
      logF("InitVulkan in glfwglue.cpp was not called yet.\n");
    }

    VmaVulkanFunctions vulkanFns;
    memset(&vulkanFns, 0, sizeof(vulkanFns));

    vulkanFns.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFns.vkGetPhysicalDeviceMemoryProperties =
        vkGetPhysicalDeviceMemoryProperties;
    vulkanFns.vkAllocateMemory = vkAllocateMemory;
    vulkanFns.vkFreeMemory = vkFreeMemory;
    vulkanFns.vkMapMemory = vkMapMemory;
    vulkanFns.vkUnmapMemory = vkUnmapMemory;
    vulkanFns.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFns.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFns.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFns.vkBindImageMemory = vkBindImageMemory;
    vulkanFns.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFns.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFns.vkCreateBuffer = vkCreateBuffer;
    vulkanFns.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFns.vkCreateImage = vkCreateImage;
    vulkanFns.vkDestroyImage = vkDestroyImage;
    vulkanFns.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION
    vulkanFns.vkGetBufferMemoryRequirements2KHR =
        vkGetBufferMemoryRequirements2KHR;
    vulkanFns.vkGetImageMemoryRequirements2KHR =
        vkGetImageMemoryRequirements2KHR;
#endif

    allocatorInfo.pVulkanFunctions = &vulkanFns;
#endif /*__ANDROID__*/

    VkResult r = vmaCreateAllocator(&allocatorInfo, &dev.vmaAllocator);
    if (r != VK_SUCCESS) {
      return explainVkResult("vmaCreateAllocator", r);
    }
  }

  VmaAllocationCreateInfo* pInfo = &req.info;
  if (pInfo->usage == VMA_MEMORY_USAGE_UNKNOWN && !pInfo->requiredFlags) {
    logE("Please set MemoryRequirements::info.usage before calling alloc.\n");
    return 1;
  }
  DeviceMemory::lock_guard_t lock(lockmutex);
  void* oldUserData = pInfo->pUserData;
  VkResult r;
  if (req.vkbuf) {
    if (req.vkimg) {
      logE("MemoryRequirements with both vkbuf and vkimg is invalid.\n");
      return 1;
    }
    pInfo->pUserData = const_cast<char*>(name.c_str());
    r = vmaAllocateMemoryForBuffer(dev.vmaAllocator, req.vkbuf, pInfo,
                                   &vmaAlloc, NULL);
  } else if (req.vkimg) {
    pInfo->pUserData = const_cast<char*>(name.c_str());
    r = vmaAllocateMemoryForImage(dev.vmaAllocator, req.vkimg, pInfo, &vmaAlloc,
                                  NULL);
  } else {
    pInfo->pUserData = oldUserData;
    logE("MemoryRequirements::get not called yet.\n");
    return 1;
  }
  pInfo->pUserData = oldUserData;
  if (r != VK_SUCCESS) {
    return explainVkResult("vmaAllocateMemoryFor(Buffer or Image)", r);
  }
  return 0;
}

int DeviceMemory::getAllocInfo(VmaAllocationInfo& info) {
  if (!dev.vmaAllocator) {
    logE("getAllocInfo: alloc not called yet.\n");
    return 1;
  }
  memset(&info, 0, sizeof(info));
  lock_guard_t lock(lockmutex);
  vmaGetAllocationInfo(dev.vmaAllocator, vmaAlloc, &info);
  return 0;
}

int DeviceMemory::mmap(void** pData, VkDeviceSize offset /*= 0*/,
                       VkDeviceSize size /*= VK_WHOLE_SIZE*/,
                       VkMemoryMapFlags flags /*= 0*/) {
  if (isImage) {
    // Can assume the image is a linear, host coherent image.
    logW("MoltenVK will munmap ALL other mappings when this is munmapped!\n");
    logW("  **   https://github.com/KhronosGroup/MoltenVK/issues/175   **\n");
#ifdef __APPLE__
    // Workarounds would be ugly (global count of all mmaps). Refuse to do it.
    logE("Only safe if there is no other active mmap at this time!\n");
    return 1;
#endif
  }
  VmaAllocationInfo info;
  if (getAllocInfo(info)) {
    logE("mmap: getAllocInfo failed\n");
    return 1;
  }
  if (info.pMappedData) {
    logE("mmap: already mapped at %p\n", info.pMappedData);
    return 1;
  }
  lock_guard_t lock(lockmutex);
  (void)size;
  (void)flags;
  VkResult v = vmaMapMemory(dev.vmaAllocator, vmaAlloc, pData);
  if (v != VK_SUCCESS) {
    return explainVkResult("vmaMapMemory", v);
  }
  if (offset) {
    logW("mmap: offset != 0 when using VulkanMemoryAllocator - SLOW!\n");
    *pData = reinterpret_cast<void*>(reinterpret_cast<char*>(*pData) + offset);
  }
  return 0;
}

void DeviceMemory::makeRange(VkMappedMemoryRange& range,
                             const VmaAllocationInfo& info) {
  VkOverwrite(range);
  range.memory = info.deviceMemory;
  range.offset = info.offset;
  range.size = info.size;
}

int DeviceMemory::flush() {
  VmaAllocationInfo info;
  if (getAllocInfo(info)) {
    logE("flush: getAllocInfo failed\n");
    return 1;
  }
  VkResult v =
      vmaFlushAllocation(dev.vmaAllocator, vmaAlloc, info.offset, info.size);
  if (v != VK_SUCCESS) {
    return explainVkResult("vmaFlushAllocation", v);
  }
  return 0;
}

int DeviceMemory::invalidate() {
  VmaAllocationInfo info;
  if (getAllocInfo(info)) {
    logE("flush: getAllocInfo failed\n");
    return 1;
  }
  VkResult v = vmaInvalidateAllocation(dev.vmaAllocator, vmaAlloc, info.offset,
                                       info.size);
  if (v != VK_SUCCESS) {
    return explainVkResult("vmaInvalidateAllocation", v);
  }
  return 0;
}

void DeviceMemory::munmap() {
  lock_guard_t lock(lockmutex);
  vmaUnmapMemory(dev.vmaAllocator, vmaAlloc);
}

#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

DeviceMemory::~DeviceMemory() {}

void DeviceMemory::reset() {
  if (vmaAlloc.mapped) {
    vmaAlloc.mapped = 0;
    vkUnmapMemory(dev.dev, vmaAlloc.vk);
  }
  vmaAlloc.allocSize = 0;
  vmaAlloc.vk.reset();
}

int DeviceMemory::setName(const std::string& name) {
  return vmaAlloc.vk.setName(name);
}

const std::string& DeviceMemory::getName() { return vmaAlloc.vk.getName(); }

int DeviceMemory::alloc(MemoryRequirements req) {
  isImage = req.isImage;
  if (req.findVkalloc(vmaAlloc.requiredProps)) {
    return 1;
  }
  vmaAlloc.allocSize = req.vkalloc.allocationSize;
  vmaAlloc.vk.reset();
  VkResult v = vkAllocateMemory(req.dev.dev, &req.vkalloc,
                                req.dev.dev.allocator, &vmaAlloc.vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkAllocateMemory", v);
  }
  vmaAlloc.vk.allocator = req.dev.dev.allocator;
  vmaAlloc.vk.onCreate();
  return 0;
}

int DeviceMemory::mmap(void** pData, VkDeviceSize offset /*= 0*/,
                       VkDeviceSize size /*= VK_WHOLE_SIZE*/,
                       VkMemoryMapFlags flags /*= 0*/) {
  VkResult v = vkMapMemory(dev.dev, vmaAlloc.vk, offset, size, flags, pData);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkMapMemory", v);
  }
  vmaAlloc.mapped = *pData;
  return 0;
}

void DeviceMemory::makeRange(VkMappedMemoryRange& range, VkDeviceSize offset,
                             VkDeviceSize size) {
  VkOverwrite(range);
  range.memory = vmaAlloc.vk;
  range.offset = offset;
  range.size = size;
}

int DeviceMemory::flush(std::vector<VkMappedMemoryRange> mem) {
  if (!mem.size()) {
    logE("DeviceMemory::flush: vector<VkMappedMemoryRange>.size=0\n");
    return 1;
  }
  for (auto i = mem.begin(); i != mem.end(); i++) {  // Force .memory to be vk.
    i->memory = vmaAlloc.vk;
  }
  VkResult v = vkFlushMappedMemoryRanges(dev.dev, mem.size(), mem.data());
  if (v != VK_SUCCESS) {
    return explainVkResult("vkFlushMappedMemoryRanges", v);
  }
  return 0;
}

int DeviceMemory::invalidate(const std::vector<VkMappedMemoryRange>& mem) {
  VkResult v = vkInvalidateMappedMemoryRanges(dev.dev, mem.size(), mem.data());
  if (v != VK_SUCCESS) {
    return explainVkResult("vkInvalidateMappedMemoryRanges", v);
  }
  return 0;
}

void DeviceMemory::munmap() {
  vkUnmapMemory(dev.dev, vmaAlloc.vk);
  vmaAlloc.mapped = 0;
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

}  // namespace memory
