/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

#include <src/science/science.h>
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
    logE("BUG: Buffer::copyFromHost(len=0x%lx, dstOffset=0x%llx).\n", len,
         (unsigned long long)dstOffset);
    logE("BUG: when Buffer.info.size=0x%llx\n", (unsigned long long)info.size);
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

int UniformBuffer::copyAndKeepMmap(command::CommandPool& pool, const void* src,
                                   size_t len, VkDeviceSize dstOffset /*= 0*/) {
  if (!(stage.info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
    logW("WARNING: UniformBuffer::copyAndKeepMmap on a Buffer where\n");
    logW("neither ctorHostVisible nor ctorHostCoherent was used.\n");
    std::ostringstream msg;
    msg << std::hex << stage.info.usage;
    auto s = msg.str();
    logW("usage = 0x%s\n", s.c_str());
    return 1;
  }

  if (dstOffset + len > stage.info.size) {
    logE("BUG: UniformBuffer::copyAndKeepMmap(len=0x%lx, dstOffset=0x%llx).\n",
         len, (unsigned long long)dstOffset);
    logE("BUG: when Buffer.info.size=0x%llx\n",
         (unsigned long long)stage.info.size);
    return 1;
  }

  if (!stageMmap) {
    if (stage.mem.mmap(pool.dev, &stageMmap)) {
      logE("UniformBuffer::copyAndKeepMmap: stage.mem.mmap failed\n");
      return 1;
    }
  }

  memcpy(reinterpret_cast<char*>(stageMmap) + dstOffset, src, len);
  return Buffer::copy(pool, stage);
}

}  // namespace memory
