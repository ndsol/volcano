/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file contains CommandBuffer methods that access namespace memory objects
 * for convenience. These are methods that exist in namespace command but have a
 * dependency on code in memory (so this is part of memory).
 */
#include "memory.h"

namespace command {

int CommandBuffer::barrier(memory::Image& img, VkImageLayout newLayout) {
  if (img.currentLayout == newLayout) {
    // Silently discard no-op transitions.
    return 0;
  }
  CommandPool::lock_guard_t lock(cpool.lockmutex);
  lazyBarriers.img.emplace_back(img.makeTransition(newLayout));
  VkImageMemoryBarrier& b = lazyBarriers.img.back();
  b.subresourceRange = img.getSubresourceRange();
  img.currentLayout = newLayout;
  return 0;
}

int CommandBuffer::barrier(memory::Image& img, VkImageLayout newLayout,
                           const VkImageSubresourceRange& range) {
  CommandPool::lock_guard_t lock(cpool.lockmutex);
  lazyBarriers.img.emplace_back(img.makeTransition(newLayout));
  lazyBarriers.img.back().subresourceRange = range;
  // FIXME: Bug workaround for validation layers: if 'range' and the very next
  // barrier use the same image and the next barrier affects the full image,
  // validation fails to record this barrier. Force all 'range' barriers to
  // flush and call vkCmdPipelineBarrier.
  return flushLazyBarriers(lock);
}

int CommandBuffer::copyImage(memory::Image& src, memory::Image& dst,
                             const std::vector<VkImageCopy>& regions) {
  return copyImage(src.vk, src.currentLayout, dst.vk, dst.currentLayout,
                   regions);
}

int CommandBuffer::copyImage(memory::Buffer& src, memory::Image& dst,
                             const std::vector<VkBufferImageCopy>& regions) {
  return copyBufferToImage(src.vk, dst.vk, dst.currentLayout, regions);
}

int CommandBuffer::copyImage(memory::Image& src, memory::Buffer& dst,
                             const std::vector<VkBufferImageCopy>& regions) {
  return copyImageToBuffer(src.vk, src.currentLayout, dst.vk, regions);
}

int CommandBuffer::blitImage(memory::Image& src, memory::Image& dst,
                             const std::vector<VkImageBlit>& regions,
                             VkFilter filter /*= VK_FILTER_LINEAR*/) {
  return blitImage(src.vk, src.currentLayout, dst.vk, dst.currentLayout,
                   regions, filter);
}

int CommandBuffer::resolveImage(memory::Image& src, memory::Image& dst,
                                const std::vector<VkImageResolve>& regions) {
  return resolveImage(src.vk, src.currentLayout, dst.vk, dst.currentLayout,
                      regions);
}

}  // namespace command

namespace language {

void Device::setFrameNumber(uint32_t frameNumber) {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  (void)frameNumber;  // silence the "unused parameter" warning.
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  if (vmaAllocator) {
    vmaSetCurrentFrameIndex(vmaAllocator, frameNumber);
  }
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
}

}  // namespace language
