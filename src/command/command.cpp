/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

CommandPool::~CommandPool() {}

CommandBuffer::~CommandBuffer() {}

int CommandPool::ctorError(VkCommandPoolCreateFlags flags) {
  if (queueFamily == language::NONE) {
    logE("CommandPool::queueFamily must be set before calling ctorError\n");
    return 1;
  }

  auto qfam_i = vk.dev.getQfamI(queueFamily);
  if (qfam_i == (decltype(qfam_i)) - 1) {
    return 1;
  }

  // Cache QueueFamily, as all commands in this pool must be submitted here.
  qf_ = &vk.dev.qfams.at(qfam_i);

  VkCommandPoolCreateInfo VkInit(cpci);
  cpci.queueFamilyIndex = qfam_i;
  cpci.flags = flags;
  VkResult v =
      vkCreateCommandPool(vk.dev.dev, &cpci, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateCommandPool", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int CommandPool::alloc(std::vector<VkCommandBuffer>& buf,
                       VkCommandBufferLevel level) {
  if (buf.size() < 1) {
    logE("%s failed: buf.size is 0\n", "vkAllocateCommandBuffers");
    return 1;
  }

  lock_guard_t lock(lockmutex);
  VkCommandBufferAllocateInfo VkInit(ai);
  ai.commandPool = vk;
  ai.level = level;
  ai.commandBufferCount = (decltype(ai.commandBufferCount))buf.size();

  VkResult v = vkAllocateCommandBuffers(vk.dev.dev, &ai, buf.data());
  if (v != VK_SUCCESS) {
    return explainVkResult("vkAllocateCommandBuffers", v);
  }
  return 0;
}

int CommandPool::submit(lock_guard_t& lock, size_t poolQindex,
                        const std::vector<SubmitInfo>& info,
                        VkFence fence /*= VK_NULL_HANDLE*/) {
  (void)lock;

  std::vector<VkSubmitInfo> raw;
  std::vector<std::vector<VkSemaphore>> rawSem;
  std::vector<std::vector<VkPipelineStageFlags>> rawStage;
  raw.resize(info.size());
  rawSem.resize(info.size());
  rawStage.resize(info.size());

  for (size_t i = 0; i < info.size(); i++) {
    VkSubmitInfo& out = raw.at(i);
    for (auto& ss : info.at(i).waitFor) {
      rawSem.at(i).emplace_back(ss.sem);
      rawStage.at(i).emplace_back(ss.dstStage);
    }

    VkOverwrite(out);
    out.waitSemaphoreCount = info.at(i).waitFor.size();
    out.pWaitSemaphores = rawSem.at(i).data();
    out.pWaitDstStageMask = rawStage.at(i).data();
    out.commandBufferCount = info.at(i).cmdBuffers.size();
    out.pCommandBuffers = info.at(i).cmdBuffers.data();
    out.signalSemaphoreCount = info.at(i).toSignal.size();
    out.pSignalSemaphores = info.at(i).toSignal.data();
  }

  VkResult v = vkQueueSubmit(q(poolQindex), raw.size(), raw.data(), fence);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkQueueSubmit", v);
  }
  return 0;
}

int CommandPool::submit(lock_guard_t& lock, size_t poolQindex,
                        CommandBuffer& cmdBuffer,
                        VkFence fence /*= VK_NULL_HANDLE*/) {
  command::SubmitInfo info;
  if (cmdBuffer.enqueue(lock, info)) {
    logE("CommandPool::submit: cmdBuffer.enqueue failed\n");
    return 1;
  }
  if (submit(lock, poolQindex, {info}, fence)) {
    logE("CommandPool::submit(cmdBuffer): inner submit failed\n");
    return 1;
  }
  return 0;
}

int CommandPool::submitAndWait(size_t poolQindex, CommandBuffer& cmdBuffer) {
  std::shared_ptr<command::Fence> fence = borrowFence();
  if (!fence) {
    logE("submitAndWait: borrowFence failed\n");
    return 1;
  }
  command::CommandPool::lock_guard_t lock(lockmutex);
  if (submit(lock, poolQindex, cmdBuffer, fence->vk)) {
    (void)unborrowFence(fence);
    logE("submitAndWait: inner submit failed\n");
    return 1;
  }
  VkResult v = fence->waitMs(1000);
  if (v != VK_SUCCESS) {
    (void)unborrowFence(fence);
    return explainVkResult("submitAndWait: fence.waitMs", v);
  }
  if (unborrowFence(fence)) {
    logE("submitAndWait: unborrowFence failed\n");
    return 1;
  }
  return 0;
}

VkCommandBuffer CommandPool::borrowOneTimeBuffer() {
  std::vector<VkCommandBuffer> v;
  for (;;) {
    if (v.size()) {
      // Clear out v for a second attempt. (It's already empty the first time.)
      free(v);
      v.clear();
    }
    // Read toBorrow without a lock. Write-after-read safe because toBorrow
    // has only transition in its lifetime, from NULL to populated, which is
    // checked below after acquiring the lock.
    if (!toBorrow) {
      v.resize(1);
      // Call alloc() without holding lock.
      if (alloc(v)) {
        logE("borrowOneTimeBuffer: alloc failed\n");
        return VK_NULL_HANDLE;
      }
    }
    // Now transfer v to toBorrow while holding lock.
    lock_guard_t lock(lockmutex);
    if (!toBorrow) {
      toBorrow = v.at(0);
      borrowCount = 0;
    } else if (v.size()) {
      // A race occured: toBorrow was updated. Release lock and try again.
      continue;
    }
    if (borrowCount) {
      logE("borrowOneTimeBuffer only has one VkCommandBuffer to lend out.\n");
      logE("This keeps it simple, short, and sweet. Consider whether you\n");
      logE("need two buffers during init, since it will hide bugs.\n");
      return VK_NULL_HANDLE;
    }
    borrowCount++;
    return toBorrow;
  }
}

int CommandPool::unborrowOneTimeBuffer(VkCommandBuffer buf) {
  lock_guard_t lock(lockmutex);
  if (!toBorrow) {
    logE("unborrowOneTimeBuffer: borrowOneTimeBuffer was never called!\n");
    return 1;
  }
  if (!borrowCount) {
    logE("unborrowOneTimeBuffer: borrowOneTimeBuffer has been called.\n");
    logE("unborrowOneTimeBuffer: but the buffer is not currently borrowed!\n");
    return 1;
  }
  if (buf != toBorrow) {
    logE("unborrowOneTimeBuffer(%p): wanted buf=%p\n", (void*)buf,
         (void*)toBorrow);
    return 1;
  }
  borrowCount--;
  return 0;
}

std::shared_ptr<Fence> CommandPool::borrowFence() {
  if (freeFences.empty()) {
    static constexpr size_t borrowFenceChunkSize = 2;
    while (freeFences.size() < borrowFenceChunkSize) {
      auto f = std::make_shared<Fence>(vk.dev);
      if (f->ctorError()) {
        logE("CommandPool::borrowFence: fence[%zu].ctorError failed\n",
             freeFences.size());
        return std::shared_ptr<Fence>();
      }
      freeFences.emplace_back(f);
    }
  }
  auto f = freeFences.back();
  freeFences.pop_back();
  return f;
}

int CommandPool::unborrowFence(std::shared_ptr<Fence> fence) {
  if (!fence) {
    logE("unborrowFence: shared_ptr is NULL\n");
    return 1;
  }
  if (fence->reset()) {
    logE("unborrowFence: fence.reset failed\n");
    return 1;
  }
  freeFences.push_back(fence);
  return 0;
}

// trimSrcStage modifies access bits that are not supported by stage. It also
// simplifies stage selection by tailoring the stage to the access bits'
// implied operation.
void CommandBuffer::trimSrcStage(VkAccessFlags& access,
                                 VkPipelineStageFlags& stage) {
  switch (stage) {
    case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
      // Top of pipe means the host is implicitly aware of this operation.
      // Simplify the barrier by reducing access or picking a later stage.
      // (Vulkan does not do this automatically.)
      stage = 0;
      if (access & (VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_HOST_READ_BIT)) {
        // Access mask includes HOST_WRITE or HOST_READ: use STAGE_HOST.
        stage |= VK_PIPELINE_STAGE_HOST_BIT;
      }
      if (access & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) {
        stage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      }
      if (access & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) {
        stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      }
      if (access & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
                    VK_ACCESS_SHADER_WRITE_BIT)) {
        stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
      }
      if (access & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
        stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      }
      if (access & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
        stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      }
      if (access &
          (VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT)) {
        stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
      }
      if (!stage) {  // No access flags matched.
        stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      }
      break;
    default:
      // User called CommandBuffer::barrier() with a non-default srcStageMask.
      // Assume the user is right.
      break;
  }
}

// trimDstStage modifies access bits that are not supported by stage.
void CommandBuffer::trimDstStage(VkAccessFlags& /*access*/,
                                 VkPipelineStageFlags& /*stage*/) {}

}  // namespace command
