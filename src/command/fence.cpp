/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int Semaphore::ctorError() {
  VkSemaphoreCreateInfo VkInit(sci);
  VkResult v = vkCreateSemaphore(vk.dev.dev, &sci, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateSemaphore", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int Fence::ctorError() {
  VkFenceCreateInfo VkInit(fci);
  VkResult v = vkCreateFence(vk.dev.dev, &fci, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateFence", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int Fence::reset() {
  VkFence fences[] = {vk};
  VkResult v =
      vkResetFences(vk.dev.dev, sizeof(fences) / sizeof(fences[0]), fences);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkResetFences", v);
  }
  return 0;
}

VkResult Fence::waitNs(uint64_t timeoutNanos) {
  VkFence fences[] = {vk};
  return vkWaitForFences(vk.dev.dev, sizeof(fences) / sizeof(fences[0]), fences,
                         VK_FALSE, timeoutNanos);
}

VkResult Fence::getStatus() { return vkGetFenceStatus(vk.dev.dev, vk); }

int Event::ctorError() {
  VkEventCreateInfo VkInit(eci);
  VkResult v = vkCreateEvent(vk.dev.dev, &eci, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateEvent", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int CommandBuffer::validateLazyBarriers(CommandPool::lock_guard_t&) {
  auto& b = lazyBarriers;
  bool found = false;
  VkPipelineStageFlags origSrc = b.srcStageMask;
  VkPipelineStageFlags origDst = b.dstStageMask;
  b.srcStageMask = 0;
  b.dstStageMask = 0;
  for (auto& mem : b.mem) {
    found = true;
    if (mem.sType != VK_STRUCTURE_TYPE_MEMORY_BARRIER) {
      logE("lazyBarriers::mem contains invalid VkMemoryBarrier\n");
      return 1;
    }
    VkPipelineStageFlags src = origSrc;
    trimSrcStage(mem.srcAccessMask, src);
    if (src != origSrc) {
      b.srcStageMask |= src;  // If trimmed, set b now.
    }
    VkPipelineStageFlags dst = origDst;
    trimDstStage(mem.dstAccessMask, dst);
    if (dst != origDst) {
      b.dstStageMask |= dst;  // If trimmed, set b now.
    }
  }
  for (auto& buf : b.buf) {
    found = true;
    if (buf.sType != VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER) {
      logE("lazyBarriers::buf contains invalid VkBufferMemoryBarrier\n");
      return 1;
    }
    if (!buf.buffer) {
      logE("lazyBarriers::buf contains invalid VkBuffer\n");
      return 1;
    }
    VkPipelineStageFlags src = origSrc;
    trimSrcStage(buf.srcAccessMask, src);
    if (src != origSrc) {
      b.srcStageMask |= src;  // If trimmed, set b now.
    }
    VkPipelineStageFlags dst = origDst;
    trimDstStage(buf.dstAccessMask, dst);
    if (dst != origDst) {
      b.dstStageMask |= dst;  // If trimmed, set b now.
    }
  }
  for (auto& img : b.img) {
    found = true;
    if (img.sType != VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER) {
      logE("lazyBarriers::img contains invalid VkImageMemoryBarrier\n");
      return 1;
    }
    if (!img.image) {
      logE("lazyBarriers::img contains invalid VkImage\n");
      return 1;
    }
    VkPipelineStageFlags src = origSrc;
    trimSrcStage(img.srcAccessMask, src);
    if (src != origSrc) {
      b.srcStageMask |= src;  // If trimmed, set b now.
    }
    VkPipelineStageFlags dst = origDst;
    trimDstStage(img.dstAccessMask, dst);
    if (dst != origDst) {
      b.dstStageMask |= dst;  // If trimmed, set b now.
    }
  }
  if (b.srcStageMask == 0) {  // If nobody trimmed, reset b.
    b.srcStageMask = origSrc;
  }
  if (b.dstStageMask == 0) {  // If nobody trimmed, reset b.
    b.dstStageMask = origDst;
  }
  if (!vk) {
    logE("CommandBuffer::flushLazyBarriers: not allocated\n");
    return 1;
  }
  return found ? 0 : 2;
}

int CommandBuffer::flushLazyBarriers(CommandPool::lock_guard_t& lock) {
  switch (validateLazyBarriers(lock)) {
    case 1:
      return 1;
    case 2:
      return 0;  // Optimization: no need to call vkCmdPipelineBarrier.
    default:
      break;
  }
  auto& b = lazyBarriers;
  VkDependencyFlags dependencyFlags = 0;
  vkCmdPipelineBarrier(vk, b.srcStageMask, b.dstStageMask, dependencyFlags,
                       b.mem.size(), b.mem.data(), b.buf.size(), b.buf.data(),
                       b.img.size(), b.img.data());
  b.reset();
  return 0;
}

int CommandBuffer::waitBarrier(const BarrierSet& b,
                               VkDependencyFlags dependencyFlags /*= 0*/) {
  CommandPool::lock_guard_t lock(cpool.lockmutex);
  if (flushLazyBarriers(lock)) return 1;
  vkCmdPipelineBarrier(vk, b.srcStageMask, b.dstStageMask, dependencyFlags,
                       b.mem.size(), b.mem.data(), b.buf.size(), b.buf.data(),
                       b.img.size(), b.img.data());
  return 0;
}

int CommandBuffer::waitEvents(const std::vector<VkEvent>& events) {
  CommandPool::lock_guard_t lock(cpool.lockmutex);
  // use of b followed by b.reset() is the same as flushLazyBarriers().
  switch (validateLazyBarriers(lock)) {
    case 1:
      return 1;
    case 2:
      // FALLTHROUGH: Do not optimize because events is most likely not empty.
    default:
      break;
  }
  auto& b = lazyBarriers;
  vkCmdWaitEvents(vk, events.size(), events.data(), b.srcStageMask,
                  b.dstStageMask, b.mem.size(), b.mem.data(), b.buf.size(),
                  b.buf.data(), b.img.size(), b.img.data());
  b.reset();
  return 0;
}

}  // namespace command
