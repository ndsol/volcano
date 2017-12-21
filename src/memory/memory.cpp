/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"
#include <src/science/science.h>
#include <vulkan/vk_format_utils.h>
#include <sstream>

namespace memory {
int Buffer::copy(command::CommandPool& pool, Buffer& src) {
  if (src.info.size > info.size) {
    logE("Buffer::copy: src.info.size=0x%llx is larger than my size 0x%llx\n",
         (unsigned long long)src.info.size, (unsigned long long)info.size);
    return 1;
  }

  science::SmartCommandBuffer cmdBuffer{pool, ASSUME_POOL_QINDEX};
  return cmdBuffer.ctorError() || cmdBuffer.autoSubmit() ||
         copy(cmdBuffer, src);
}

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

int Image::ctorError(language::Device& dev, VkMemoryPropertyFlags props) {
  if (!info.extent.width || !info.extent.height || !info.extent.depth ||
      !info.format || !info.usage || !info.mipLevels || !info.arrayLayers) {
    logE("Image::ctorError found uninitialized fields\n");
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateImage(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateImage", v, string_VkResult(v));
    return 1;
  }
  currentLayout = info.initialLayout;

  if (mem.alloc({dev, *this}, props)) {
    return 1;
  }
  // NOTE: vkGetImageSubresourceLayout is only valid for an image with
  // LINEAR tiling.
  //
  // Vulkan Spec: "The layout of a subresource (mipLevel/arrayLayer) of an
  // image created with linear tiling is queried by calling
  // vkGetImageSubresourceLayout".
  if (info.tiling != VK_IMAGE_TILING_LINEAR) {
    return 0;
  }
  VkImageSubresource s = {};
  if (FormatIsColor(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        colorMem.emplace_back();
        vkGetImageSubresourceLayout(dev.dev, vk, &s, &colorMem.back());
      }
    }
  }
  if (FormatHasDepth(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        depthMem.emplace_back();
        vkGetImageSubresourceLayout(dev.dev, vk, &s, &depthMem.back());
      }
    }
  }
  if (FormatHasStencil(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        stencilMem.emplace_back();
        vkGetImageSubresourceLayout(dev.dev, vk, &s, &stencilMem.back());
      }
    }
  }
  return 0;
}

int Image::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindImageMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkBindImageMemory", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Image::reset() {
  mem.allocSize = 0;
  mem.vk.reset();
  vk.reset();
  return 0;
}

int Buffer::ctorError(language::Device& dev, VkMemoryPropertyFlags props,
                      const std::vector<uint32_t>& queueFams) {
  if (!info.size || !info.usage) {
    logE("Buffer::ctorError found uninitialized fields\n");
    return 1;
  }

  if (queueFams.size()) {
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;
  }
  info.queueFamilyIndexCount = queueFams.size();
  info.pQueueFamilyIndices = queueFams.data();

  vk.reset();
  VkResult v = vkCreateBuffer(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateBuffer", v, string_VkResult(v));
    return 1;
  }

  return mem.alloc({dev, *this}, props);
}

int Buffer::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindBufferMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkBindBufferMemory", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Buffer::reset() {
  mem.allocSize = 0;
  mem.vk.reset();
  vk.reset();
  return 0;
}

int Buffer::copyFromHost(language::Device& dev, const void* src, size_t len,
                         VkDeviceSize dstOffset /*= 0*/) {
  if (!(info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
    logW("WARNING: Buffer::copyFromHost on a Buffer where neither\n");
    logW("ctorHostVisible nor ctorHostCoherent was used.\n");
    std::ostringstream msg;
    msg << "usage = 0x" << std::hex << info.usage;

    // Dump info.usage bits.
    const char* firstPrefix = " (";
    const char* prefix = firstPrefix;
    for (uint64_t bit = 1; bit < VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
         bit <<= 1) {
      if (((uint64_t)info.usage) & bit) {
        msg << prefix
            << string_VkBufferUsageFlagBits((VkBufferUsageFlagBits)bit);
        prefix = " | ";
      }
    }
    if (prefix != firstPrefix) {
      msg << ")";
    }
    auto s = msg.str();
    logW("%s\n", s.c_str());
    return 1;
  }

  if (dstOffset + len > info.size) {
    logE("BUG: Buffer::copyFromHost(len=0x%lx, dstOffset=0x%llx).\n", len);
    logE("BUG: when Buffer.info.size=0x%llx\n", (unsigned long long)dstOffset,
         (unsigned long long)info.size);
    return 1;
  }

  void* mapped;
  if (mem.mmap(dev, &mapped)) {
    return 1;
  }
  memcpy(reinterpret_cast<char*>(mapped) + dstOffset, src, len);
  mem.munmap(dev);
  return 0;
}

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
