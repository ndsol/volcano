/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"
#include <src/science/science.h>

namespace memory {

int DeviceMemory::alloc(MemoryRequirements req, VkMemoryPropertyFlags props) {
  // This relies on the fact that req.ofProps() updates req.vkalloc.
  if (!req.ofProps(props)) {
    fprintf(stderr, "DeviceMemory::alloc: indexOf returned not found\n");
    return 1;
  }
  vk.reset();
  VkResult v =
      vkAllocateMemory(req.dev.dev, &req.vkalloc, req.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkAllocateMemory", v,
            string_VkResult(v));
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
    fprintf(stderr, "%s failed: %d (%s)\n", "vkMapMemory", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

void DeviceMemory::munmap(language::Device& dev) { vkUnmapMemory(dev.dev, vk); }

int Image::ctorError(language::Device& dev, VkMemoryPropertyFlags props) {
  if (!info.extent.width || !info.extent.height || !info.extent.depth ||
      !info.format || !info.usage) {
    fprintf(stderr, "Image::ctorError found uninitialized fields\n");
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateImage(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateImage", v,
            string_VkResult(v));
    return 1;
  }
  currentLayout = info.initialLayout;

  return mem.alloc({dev, vk}, props);
}

int Image::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindImageMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkBindImageMemory", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int Buffer::ctorError(language::Device& dev, VkMemoryPropertyFlags props) {
  if (!info.size || !info.usage) {
    fprintf(stderr, "Buffer::ctorError found uninitialized fields\n");
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateBuffer(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateBuffer", v,
            string_VkResult(v));
    return 1;
  }

  return mem.alloc({dev, vk}, props);
}

int Buffer::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindBufferMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkBindBufferMemory", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int Buffer::copyFromHost(language::Device& dev, const void* src, size_t len,
                         VkDeviceSize dstOffset /*= 0*/) {
  if (!(info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
    fprintf(stderr,
            "WARNING: Buffer::copyFromHost on a Buffer where neither "
            "ctorHostVisible nor ctorHostCoherent was used.\n"
            "WARNING: usage = 0x%x",
            info.usage);

    // Dump info.usage bits.
    const char* firstPrefix = " (";
    const char* prefix = firstPrefix;
    for (uint64_t bit = 1; bit < VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
         bit <<= 1) {
      if (((uint64_t)info.usage) & bit) {
        fprintf(stderr, "%s%s", prefix,
                string_VkBufferUsageFlagBits((VkBufferUsageFlagBits)bit));
        prefix = " | ";
      }
    }
    if (prefix != firstPrefix) {
      fprintf(stderr, ")");
    }
    fprintf(stderr, "\n");
    return 1;
  }

  if (dstOffset + len > info.size) {
    fprintf(stderr,
            "BUG: Buffer::copyFromHost(len=0x%lx, dstOffset=0x%llx).\n"
            "BUG: when Buffer.info.size=0x%llx\n",
            len, (unsigned long long)dstOffset, (unsigned long long)info.size);
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

MemoryRequirements::MemoryRequirements(language::Device& dev, VkImage img)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetImageMemoryRequirements(dev.dev, img, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, Image& img)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetImageMemoryRequirements(dev.dev, img.vk, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, VkBuffer buf)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetBufferMemoryRequirements(dev.dev, buf, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, Buffer& buf)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetBufferMemoryRequirements(dev.dev, buf.vk, &vk);
}

int MemoryRequirements::indexOf(VkMemoryPropertyFlags props) const {
  for (uint32_t i = 0; i < dev.memProps.memoryTypeCount; i++) {
    if ((vk.memoryTypeBits & (1 << i)) &&
        (dev.memProps.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }

  fprintf(stderr, "MemoryRequirements::indexOf(%x): not found in %x\n", props,
          vk.memoryTypeBits);
  return -1;
}

VkMemoryAllocateInfo* MemoryRequirements::ofProps(VkMemoryPropertyFlags props) {
  int i = indexOf(props);
  if (i == -1) {
    return nullptr;
  }

  vkalloc.memoryTypeIndex = i;
  vkalloc.allocationSize = vk.size;

  return &vkalloc;
}

}  // namespace memory
