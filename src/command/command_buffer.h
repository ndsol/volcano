/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/command is the 3rd-level bindings for the Vulkan graphics library.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY! Just #include <src/command/command.h>
 */

namespace memory {
// Forward declaration of Buffer for CommandBuffer.
typedef struct Buffer Buffer;
}  // namespace memory

namespace command {

// SubmitInfo holds a copy of all the info needed for a VkSubmitInfo.
// Your app calls CommandBuffer::enqueue() to add commands to this object, then
// CommandPool::submit() to submit it to the device.
typedef struct SubmitInfo {
  // waitFor contains a vector of semaphores to wait on before the batch
  // will start.
  std::vector<SemaphoreStageMaskPair> waitFor;
  // cmdBuffers contains a vector of VkCommandBuffer that will be executed in
  // order. This is the "batch."
  std::vector<VkCommandBuffer> cmdBuffers;
  // toSignal contains a vector of semaphores that will be signalled
  // when the batch completes. (Semaphores can release other GPU work to run.)
  std::vector<VkSemaphore> toSignal;
} SubmitInfo;

// Forward declaration of CommandBuffer for CommandPool.
class CommandBuffer;

// CommandPool holds a reference to the VkCommandPool from which commands are
// allocated. Create a CommandPool instance in each thread that submits
// commands to CommandPool::queueFamily.
//
// A VkCommandPool must be "externally synchronized," and so the optimal usage
// of a VkCommandPool is to create one per thread. The CommandPool::lockmutex
// member is used for synchronization.
//
// VkCommandBuffer is not allocated one-at-a-time. Rather, VkCommandPool is
// created and with a fixed number of VkCommandBuffer objects. For simple use
// cases, reallocCmdBufs() will create just enough to match the number
// of framebuffers in dev.framebufs. More complex use cases still should plan
// the number of VkCommandBuffer in advance, and can follow the same steps as
// reallocCmdBufs() which calls alloc().
class CommandPool {
 protected:
  language::QueueFamilyProperties* qf_ = nullptr;
  VkCommandBuffer toBorrow{VK_NULL_HANDLE};
  int borrowCount{0};
  friend class CommandBuffer;
  std::vector<std::shared_ptr<Fence>> freeFences;

 public:
  CommandPool(language::Device& dev)
      : fp(dev.fp), vk{dev, vkDestroyCommandPool} {
    vk.allocator = dev.dev.allocator;
  }
  CommandPool(CommandPool&& other)
      : fp(other.fp), queueFamily(other.queueFamily), vk(std::move(other.vk)) {}
  CommandPool(const CommandPool&) = delete;
  virtual ~CommandPool();

  // fp has function pointers for extensions as a shortcut for vk.dev.fp.
  language::DeviceFunctionPointers& fp;

  // lockmutex is used to synchronize access to command buffers in this pool.
  std::recursive_mutex lockmutex;

  // lock_guard_t: like c++17's constructor type inference, but we are in c++11
  // Instead of: std::lock_guard<std::recursive_mutex> lock(cpool.lockmutex);
  // Do this:    CommandPool::lock_guard_t             lock(cpool.lockmutex);
  // (Then uses of lock_guard_t do not assume lockmutex is a recursive_mutex.)
  typedef std::lock_guard<std::recursive_mutex> lock_guard_t;
  // unique_lock_t: like c++17's constructor type inference, but in c++11
  // Instead of: std::unique_lock<std::recursive_mutex> lock(cpool.lockmutex);
  // Do this:    CommandPool::unique_lock_t             lock(cpool.lockmutex);
  // (Then uses of unique_lock_t do not assume lockmutex is a recursive_mutex.)
  typedef std::unique_lock<std::recursive_mutex> unique_lock_t;

  // Two-stage constructor: set queueFamily, then call ctorError() to build
  // CommandPool. Typically a queueFamily of language::GRAPHICS is wanted.
  WARN_UNUSED_RESULT int ctorError(
      VkCommandPoolCreateFlags flags =
          VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  // q is an accessor to return a VkQueue from the queueFamily.
  VkQueue q(size_t i) {
    if (!qf_) {
      logF("CommandPool::q called before CommandPool::ctorError\n");
    }
    return qf_->queues.at(i);
  }

  // free releases any VkCommandBuffer in buf. Command Buffers are automatically
  // freed when the CommandPool is destroyed, so free() is really only needed
  // when dynamically replacing an existing set of CommandBuffers.
  void free(std::vector<VkCommandBuffer>& buf) {
    if (!buf.size()) return;
    lock_guard_t lock(lockmutex);
    vkFreeCommandBuffers(vk.dev.dev, vk, buf.size(), buf.data());
  }

  // alloc calls vkAllocateCommandBuffers to populate buf with empty buffers.
  // Specify the VkCommandBufferLevel for a secondary command buffer.
  WARN_UNUSED_RESULT int alloc(
      std::vector<VkCommandBuffer>& buf,
      VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  // reset deallocates all command buffers in the pool (very quickly).
  WARN_UNUSED_RESULT int reset(
      VkCommandPoolResetFlagBits flags =
          VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) {
    lock_guard_t lock(lockmutex);
    VkResult v;
    if ((v = vkResetCommandPool(vk.dev.dev, vk, flags)) != VK_SUCCESS) {
      return explainVkResult("vkResetCommandPool", v);
    }
    return 0;
  }

  // borrowOneTimeBuffer by default only has one VkCommandBuffer to lend out.
  // If that is too restrictive, override this method. The default keeps things
  // simple, short, and sweet.
  //
  // Note that if your app never calls this function, no "one time buffer" is
  // allocated in this CommandPool. After this is called, the "one time buffer"
  // is held for the rest of the life of this CommandPool. An empty
  // VkCommandBuffer uses very little space, though.
  VkCommandBuffer borrowOneTimeBuffer();

  // unborrowOneTimeBuffer must be called before the next borrowOneTimeBuffer
  // in the default implementation. This should help catch bugs in your init
  // code. If you really want something more complex, override this method.
  int unborrowOneTimeBuffer(VkCommandBuffer buf);

  // borrowFence returns an unsignalled Fence. If all fences are in use,
  // borrowFence allocates more.
  // TODO: potential optimization to use thread id to give fences a loose
  //       affinity to the thread that used them last.
  std::shared_ptr<Fence> borrowFence();

  // unborrowFence puts the Fence back into a pool of available fences.
  WARN_UNUSED_RESULT int unborrowFence(std::shared_ptr<Fence> fence);

  // reallocCmdBufs is a convenience method that resizes any type as long as it
  // works like std::vector (has .size(), .at(), .emplace_back()) and its
  // element type returned by .at() has a .vk member of type VkCommandBuffer.
  //
  // moreArgs is if the buffers are being expanded and new T's need to be
  // allocated: moreArgs are passed as constructor args to new T's.
  template <typename T, typename... U>
  int reallocCmdBufs(T& buffers, size_t newSize, RenderPass& pass,
                     int is_secondary, U&&... moreArgs) {
    if (buffers.size() == newSize) {
      return 0;
    }
    std::vector<VkCommandBuffer> vkBufs;
    vkBufs.resize(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
      vkBufs.at(i) = buffers.at(i).vk;  // Steal the VkCommandBuffer handles.
      buffers.at(i).vk = VK_NULL_HANDLE;
    }
    if (vkBufs.size()) {
      // Changed CommandBuffers, must also recreate RenderPass and Framebufs.
      pass.markDirty();
      for (size_t i = 0; i < vk.dev.framebufs.size(); i++) {
        vk.dev.framebufs.at(i).markDirty();
      }
    }
    this->free(vkBufs);  // Delete old VkCommandBuffer handles.
    while (buffers.size() > newSize) {
      buffers.pop_back();  // Remove buffers as needed.
    }
    while (buffers.size() < newSize) {
      // Pass moreArgs to the constructor, creating a new T in buffers.
      buffers.emplace_back(*this, std::forward<U>(moreArgs)...);
    }
    vkBufs.resize(buffers.size());
    // Allocate new VkCommandBuffer handles.
    if (alloc(vkBufs, is_secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
                                   : VK_COMMAND_BUFFER_LEVEL_PRIMARY)) {
      logE("reallocCmdBufs: alloc[%zu] failed\n", buffers.size());
      return 1;
    }
    for (size_t i = 0; i < buffers.size(); i++) {
      buffers.at(i).vk = vkBufs.at(i);  // Save each VkCommandBuffer handle.
    }
    return 0;
  }

  // submit takes a SubmitInfo, defined above. Your app first creates a
  // CommandBuffer, then calls CommandBuffer:enqueue(SubmitInfo& info) to add
  // the commands to a SubmitInfo. Collect as many SubmitInfo as possible to
  // minimize the number of calls to submit. But to send work to the GPU, call
  // submit():
  //
  // NOTE: vkQueueSubmit is a high overhead operation. Batch up as many
  //       SubmitInfo together as possible when calling this method.
  //
  // You may optionally pass in a single fence that will be signalled when all
  // the SubmitInfo have been completed.
  //
  // You must create a lock_guard_t lock(this->lockmutex) to protect
  // queue submissions. Pass it in to show you hold the lock. It is not used.
  WARN_UNUSED_RESULT int submit(lock_guard_t& lock, size_t poolQindex,
                                const std::vector<SubmitInfo>& info,
                                VkFence fence = VK_NULL_HANDLE);

  // submit is a convenience for one CommandBuffer and no other SubmitInfo. It
  // calls the full submit with just the CommandBuffer.
  WARN_UNUSED_RESULT int submit(lock_guard_t& lock, size_t poolQindex,
                                CommandBuffer& cmdBuffer,
                                VkFence fence = VK_NULL_HANDLE);

  // submitAndWait is a convenience for one CommandBuffer and no other
  // SubmitInfo. It calls the full submit with a fence obtained with
  // borrowFence and waits on the fence, only returning when the GPU is done.
  // While simple, this clearly leads to inefficient use of the GPU.
  WARN_UNUSED_RESULT int submitAndWait(size_t poolQindex,
                                       CommandBuffer& cmdBuffer);

  // deviceWaitIdle is a helper method to wait for the device to complete all
  // outstanding commands and return to the idle state. Use of this function is
  // suboptimal, since it bypasses all of the regular Vulkan synchronization
  // primitives.
  //
  // WARNING: A known bug may lead to an out-of-memory crash when using this
  // function and the standard validation layers.
  // https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1628
  // (now migrated to
  // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/24 - search
  // for "1628" in issue #24.)
  WARN_UNUSED_RESULT int deviceWaitIdle() {
    VkResult v = vkDeviceWaitIdle(vk.dev.dev);
    if (v != VK_SUCCESS) {
      return explainVkResult("vkDeviceWaitIdle", v);
    }
    return 0;
  }

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  // queueFamily is the SurfaceSupport level of this CommandPool.
  language::SurfaceSupport queueFamily{language::NONE};
  // vk is the raw VkCommandPool.
  VkDebugPtr<VkCommandPool> vk;
};

// CommandBuffer holds a VkCommandBuffer, and provides helpful utility methods
// to create commands in the buffer.
//
// NOTE: CommandBuffer does not have a ctorError() method. The member
// CommandBuffer::vk is public and something outside this class must manage
// it. CommandPool::updateBuffersAndPass() is one way to construct it.
//
// CommandBuffer::vk is not a VkPtr<> or VkDebugPtr<> because VkCommandPool and
// VkCommandBuffer are managed together. See command.cpp.
class CommandBuffer {
 protected:
  // trimSrcStage modifies access bits that are not supported by stage. It also
  // simplifies stage selection by tailoring the stage to the access bits'
  // implied operation.
  void trimSrcStage(VkAccessFlags& access, VkPipelineStageFlags& stage);

  // trimDstStage modifies access bits that are not supported by stage.
  void trimDstStage(VkAccessFlags& access, VkPipelineStageFlags& stage);

  // validateLazyBarriers is called by flushLazyBarriers.
  WARN_UNUSED_RESULT int validateLazyBarriers(CommandPool::lock_guard_t& lock);

  // flushLazyBarriers needs to be called with the lock held, hence it is passed
  // in (but never used).
  WARN_UNUSED_RESULT int flushLazyBarriers(CommandPool::lock_guard_t& lock);

 public:
  // This form of constructor creates an empty CommandBuffer.
  CommandBuffer(CommandPool& cpool_) : cpool(cpool_) {}
  // Move constructor.
  CommandBuffer(CommandBuffer&& other) : cpool(other.cpool) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    vk = other.vk;
    other.vk = VK_NULL_HANDLE;
  }
  // The copy constructor is not allowed. The VkCommandBuffer cannot be copied.
  CommandBuffer(const CommandBuffer& other) = delete;

  virtual ~CommandBuffer();
  CommandBuffer& operator=(CommandBuffer&&) = delete;

  // cpool is a reference to the CommandPool that created this CommandBuffer.
  CommandPool& cpool;
  // vk is NULL until the VkCommandBuffer is actually allocated.
  VkCommandBuffer vk{VK_NULL_HANDLE};

  // enqueue adds this CommandBuffer to info.cmdBuffers.
  //
  // You must create a lock_guard_t lock(cpool.lockmutex) to protect
  // queue submissions. Pass it in to show you hold the lock. It is not used.
  WARN_UNUSED_RESULT int enqueue(CommandPool::lock_guard_t& lock,
                                 SubmitInfo& info) {
    if (flushLazyBarriers(lock)) return 1;
    info.cmdBuffers.emplace_back(vk);
    return 0;
  }

  // reset deallocates and clears the current VkCommandBuffer. Note that in
  // most cases, begin() calls vkBeginCommandBuffer() which implicitly resets
  // the buffer and clears any old data it may have had.
  WARN_UNUSED_RESULT int reset(
      VkCommandBufferResetFlagBits flags =
          VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkResult v;
    if ((v = vkResetCommandBuffer(vk, flags)) != VK_SUCCESS) {
      return explainVkResult("vkResetCommandBuffer", v);
    }
    return 0;
  }

  WARN_UNUSED_RESULT int begin(
      VkCommandBufferUsageFlags usageFlags,
      VkCommandBufferInheritanceInfo* pInherits = nullptr) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkCommandBufferBeginInfo cbbi;
    memset(&cbbi, 0, sizeof(cbbi));
    cbbi.sType = autoSType(cbbi);
    cbbi.flags = usageFlags;
    cbbi.pInheritanceInfo = pInherits;
    VkResult v = vkBeginCommandBuffer(vk, &cbbi);
    if (v != VK_SUCCESS) {
      return explainVkResult("vkBeginCommandBuffer", v);
    }
    return 0;
  }

  WARN_UNUSED_RESULT int beginOneTimeUse(
      VkCommandBufferInheritanceInfo* pInherits = nullptr) {
    return begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, pInherits);
  }
  WARN_UNUSED_RESULT int beginSimultaneousUse(
      VkCommandBufferInheritanceInfo* pInherits = nullptr) {
    return begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, pInherits);
  }
  WARN_UNUSED_RESULT int beginSimultaneousUseInRenderPass(
      VkCommandBufferInheritanceInfo* pInherits = nullptr) {
    return begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT |
                     VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                 pInherits);
  }

  WARN_UNUSED_RESULT int end() {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    VkResult v = vkEndCommandBuffer(vk);
    if (v != VK_SUCCESS) {
      return explainVkResult("vkEndCommandBuffer", v);
    }
    return 0;
  }

  WARN_UNUSED_RESULT int executeCommands(
      const std::vector<VkCommandBuffer>& cmds) {
    return executeCommands(cmds.size(), cmds.data());
  }

  WARN_UNUSED_RESULT int executeCommands(
      uint32_t secondaryCmdsCount, const VkCommandBuffer* pSecondaryCmds) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdExecuteCommands(vk, secondaryCmdsCount, pSecondaryCmds);
    return 0;
  }

  WARN_UNUSED_RESULT int pushConstants(Pipeline& pipe,
                                       VkShaderStageFlags stageFlags,
                                       uint32_t offset, uint32_t size,
                                       const void* pValues) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdPushConstants(vk, pipe.pipelineLayout, stageFlags, offset, size,
                       pValues);
    return 0;
  }

  template <typename T>
  WARN_UNUSED_RESULT int pushConstants(Pipeline& pipe,
                                       VkShaderStageFlags stageFlags,
                                       const T& value, uint32_t offset = 0) {
    return pushConstants(pipe, stageFlags, offset, sizeof(T), &value);
  }

  WARN_UNUSED_RESULT int fillBuffer(VkBuffer dst, VkDeviceSize dstOffset,
                                    VkDeviceSize size, uint32_t data) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdFillBuffer(vk, dst, dstOffset, size, data);
    return 0;
  }

  WARN_UNUSED_RESULT int updateBuffer(VkBuffer dst, VkDeviceSize dstOffset,
                                      VkDeviceSize dataSize,
                                      const void* pData) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdUpdateBuffer(vk, dst, dstOffset, dataSize, pData);
    return 0;
  }

  WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst,
                                    const std::vector<VkBufferCopy>& regions) {
    if (regions.size() == 0) {
      logE("copyBuffer with empty regions\n");
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyBuffer(vk, src, dst, regions.size(), regions.data());
    return 0;
  }
  WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst, size_t size) {
    VkBufferCopy region = {};
    region.size = size;
    return copyBuffer(src, dst, std::vector<VkBufferCopy>{region});
  }

  WARN_UNUSED_RESULT int copyBufferToImage(
      VkBuffer src, VkImage dst, VkImageLayout dstLayout,
      const std::vector<VkBufferImageCopy>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyBufferToImage(vk, src, dst, dstLayout, regions.size(),
                           regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyImageToBuffer(
      VkImage src, VkImageLayout srcLayout, VkBuffer dst,
      const std::vector<VkBufferImageCopy>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyImageToBuffer(vk, src, srcLayout, dst, regions.size(),
                           regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyImage(VkImage src, VkImageLayout srcLayout,
                                   VkImage dst, VkImageLayout dstLayout,
                                   const std::vector<VkImageCopy>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyImage(vk, src, srcLayout, dst, dstLayout, regions.size(),
                   regions.data());
    return 0;
  }

  // copyImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int copyImage(memory::Image& src, memory::Image& dst,
                                   const std::vector<VkImageCopy>& regions);

  // copyImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int copyImage(
      memory::Buffer& src, memory::Image& dst,
      const std::vector<VkBufferImageCopy>& regions);

  // copyImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int copyImage(
      memory::Image& src, memory::Buffer& dst,
      const std::vector<VkBufferImageCopy>& regions);

  WARN_UNUSED_RESULT int blitImage(VkImage src, VkImageLayout srcLayout,
                                   VkImage dst, VkImageLayout dstLayout,
                                   const std::vector<VkImageBlit>& regions,
                                   VkFilter filter = VK_FILTER_LINEAR) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBlitImage(vk, src, srcLayout, dst, dstLayout, regions.size(),
                   regions.data(), filter);
    return 0;
  }

  // blitImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int blitImage(memory::Image& src, memory::Image& dst,
                                   const std::vector<VkImageBlit>& regions,
                                   VkFilter filter = VK_FILTER_LINEAR);

  WARN_UNUSED_RESULT int resolveImage(
      VkImage src, VkImageLayout srcLayout, VkImage dst,
      VkImageLayout dstLayout, const std::vector<VkImageResolve>& regions) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdResolveImage(vk, src, srcLayout, dst, dstLayout, regions.size(),
                      regions.data());
    return 0;
  }

  // resolveImage is a convenience method to get the layout from memory::Image.
  WARN_UNUSED_RESULT int resolveImage(
      memory::Image& src, memory::Image& dst,
      const std::vector<VkImageResolve>& regions);

  WARN_UNUSED_RESULT int copyQueryPoolResults(
      VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
      VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride,
      VkQueryResultFlags flags) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdCopyQueryPoolResults(vk, queryPool, firstQuery, queryCount, dstBuffer,
                              dstOffset, stride, flags);
    return 0;
  }

  WARN_UNUSED_RESULT int resetQueryPool(VkQueryPool queryPool,
                                        uint32_t firstQuery,
                                        uint32_t queryCount) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdResetQueryPool(vk, queryPool, firstQuery, queryCount);
    return 0;
  }

  WARN_UNUSED_RESULT int beginQuery(VkQueryPool queryPool, uint32_t query,
                                    VkQueryControlFlags flags) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBeginQuery(vk, queryPool, query, flags);
    return 0;
  }

  WARN_UNUSED_RESULT int endQuery(VkQueryPool queryPool, uint32_t query) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdEndQuery(vk, queryPool, query);
    return 0;
  }

  WARN_UNUSED_RESULT int writeTimestamp(VkPipelineStageFlagBits stage,
                                        VkQueryPool queryPool, uint32_t query) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdWriteTimestamp(vk, stage, queryPool, query);
    return 0;
  }

  WARN_UNUSED_RESULT int beginRenderPass(VkRenderPassBeginInfo& passBeginInfo,
                                         VkSubpassContents contents) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    if (!passBeginInfo.framebuffer) {
      logE("CommandBuffer::beginRenderPass: framebuffer was not set\n");
      return 1;
    }
    if (cpool.fp.beginRenderPass2) {
      VkSubpassBeginInfoKHR sbi;
      memset(&sbi, 0, sizeof(sbi));
      sbi.sType = autoSType(sbi);
      sbi.contents = contents;
      cpool.fp.beginRenderPass2(vk, &passBeginInfo, &sbi);
    } else {
      vkCmdBeginRenderPass(vk, &passBeginInfo, contents);
    }
    return 0;
  }

  WARN_UNUSED_RESULT int nextSubpass(VkSubpassContents contents) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    if (cpool.fp.nextSubpass2) {
      VkSubpassBeginInfoKHR sbi;
      memset(&sbi, 0, sizeof(sbi));
      sbi.sType = autoSType(sbi);
      sbi.contents = contents;
      VkSubpassEndInfoKHR sei;
      memset(&sei, 0, sizeof(sei));
      sei.sType = autoSType(sei);
      cpool.fp.nextSubpass2(vk, &sbi, &sei);
    } else {
      vkCmdNextSubpass(vk, contents);
    }
    return 0;
  }

  // beginSubpass is a convenience that calls vkCmdBeginRenderPass
  // using VkSubpassContents from pass.pipelines.at(subpass)->commandBufferType
  WARN_UNUSED_RESULT int beginSubpass(RenderPass& pass,
                                      language::Framebuf& framebuf,
                                      uint32_t subpass) {
    if (subpass >= pass.pipelines.size()) {
      logE("beginSubpass(subpass = %u) out of range\n", subpass);
      return 1;
    }
    if (!pass.pipelines.at(subpass)) {
      logE("beginSubpass(subpass = %u) but Pipeline is null\n", subpass);
      return 1;
    }
    if (pass.isDirty()) {
      logW("beginRenderPass: dirty RenderPass should be rebuilt first!\n");
    }
    if (framebuf.dirty) {
      logW("beginRenderPass: dirty Framebuf should be rebuilt first!\n");
    }

    Pipeline& pipe = *pass.pipelines.at(subpass);
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (subpass != 0) {
      return nextSubpass(pipe.commandBufferType);
    }
    VkRenderPassBeginInfo passBeginInfo;
    memset(&passBeginInfo, 0, sizeof(passBeginInfo));
    passBeginInfo.sType = autoSType(passBeginInfo);
    passBeginInfo.renderPass = pass.vk;
    passBeginInfo.framebuffer = framebuf.vk;
    passBeginInfo.renderArea.offset = {0, 0};
    VkExtent3D extent = pass.getTargetExtent();
    passBeginInfo.renderArea.extent = {extent.width, extent.height};
    passBeginInfo.clearValueCount = pipe.clearColors.size();
    passBeginInfo.pClearValues = pipe.clearColors.data();
    return beginRenderPass(passBeginInfo, pipe.commandBufferType);
  }

  WARN_UNUSED_RESULT int endRenderPass() {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    if (cpool.fp.endRenderPass2) {
      VkSubpassEndInfoKHR sei;
      memset(&sei, 0, sizeof(sei));
      sei.sType = autoSType(sei);
      cpool.fp.endRenderPass2(vk, &sei);
    } else {
      vkCmdEndRenderPass(vk);
    }
    return 0;
  }

  WARN_UNUSED_RESULT int bindPipeline(VkPipelineBindPoint bindPoint,
                                      Pipeline& pipe) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindPipeline(vk, bindPoint, pipe.vk);
    return 0;
  }

  WARN_UNUSED_RESULT int bindDescriptorSets(
      VkPipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t firstSet,
      uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets,
      uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindDescriptorSets(vk, bindPoint, layout, firstSet, descriptorSetCount,
                            pDescriptorSets, dynamicOffsetCount,
                            pDynamicOffsets);
    return 0;
  }
  WARN_UNUSED_RESULT int bindGraphicsPipelineAndDescriptors(
      Pipeline& pipe, uint32_t firstSet, uint32_t descriptorSetCount,
      const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    if (bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipe)) {
      return 1;
    }
    if (!descriptorSetCount) {
      return 0;
    }
    return bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipe.pipelineLayout, firstSet, descriptorSetCount,
                              pDescriptorSets, dynamicOffsetCount,
                              pDynamicOffsets);
  }
  WARN_UNUSED_RESULT int bindComputePipelineAndDescriptors(
      Pipeline& pipe, uint32_t firstSet, uint32_t descriptorSetCount,
      const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    if (bindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipe)) {
      return 1;
    }
    if (!descriptorSetCount) {
      return 0;
    }
    return bindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,
                              pipe.pipelineLayout, firstSet, descriptorSetCount,
                              pDescriptorSets, dynamicOffsetCount,
                              pDynamicOffsets);
  }

  WARN_UNUSED_RESULT int bindVertexBuffers(uint32_t firstBinding,
                                           uint32_t bindingCount,
                                           const VkBuffer* pBuffers,
                                           const VkDeviceSize* pOffsets) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindVertexBuffers(vk, firstBinding, bindingCount, pBuffers, pOffsets);
    return 0;
  }

  WARN_UNUSED_RESULT int bindIndexBuffer(VkBuffer indexBuf, VkDeviceSize offset,
                                         VkIndexType indexType) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdBindIndexBuffer(vk, indexBuf, offset, indexType);
    return 0;
  }

  WARN_UNUSED_RESULT int drawIndexed(uint32_t indexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstIndex, int32_t vertexOffset,
                                     uint32_t firstInstance) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDrawIndexed(vk, indexCount, instanceCount, firstIndex, vertexOffset,
                     firstInstance);
    return 0;
  }
  WARN_UNUSED_RESULT int bindAndDraw(const std::vector<uint16_t>& indices,
                                     VkBuffer indexBuf, VkDeviceSize offset,
                                     uint32_t instanceCount = 1,
                                     uint32_t firstIndex = 0,
                                     int32_t vertexOffset = 0,
                                     uint32_t firstInstance = 0) {
    return bindIndexBuffer(indexBuf, offset, VK_INDEX_TYPE_UINT16) ||
           drawIndexed(indices.size(), instanceCount, firstIndex, vertexOffset,
                       firstInstance);
  }
  WARN_UNUSED_RESULT int bindAndDraw(const std::vector<uint32_t>& indices,
                                     VkBuffer indexBuf,
                                     VkDeviceSize indexBufOffset,
                                     uint32_t instanceCount = 1,
                                     uint32_t firstIndex = 0,
                                     int32_t vertexOffset = 0,
                                     uint32_t firstInstance = 0) {
    return bindIndexBuffer(indexBuf, indexBufOffset, VK_INDEX_TYPE_UINT32) ||
           drawIndexed(indices.size(), instanceCount, firstIndex, vertexOffset,
                       firstInstance);
  }

  WARN_UNUSED_RESULT int drawIndexedIndirect(
      VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
      uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) {
    if (stride % 4) {
      // Check stride, especially since it has a default argument.
      // Offset must also be a multiple of 4.
      logE("drawIndexedIndirect: stride %u not multiple of 4\n", stride);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDrawIndexedIndirect(vk, buffer, offset, drawCount, stride);
    return 0;
  }

  WARN_UNUSED_RESULT int draw(uint32_t vertexCount, uint32_t instanceCount,
                              uint32_t firstVertex, uint32_t firstInstance) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDraw(vk, vertexCount, instanceCount, firstVertex, firstInstance);
    return 0;
  }

  WARN_UNUSED_RESULT int drawIndirect(VkBuffer buffer, VkDeviceSize offset,
                                      uint32_t drawCount, uint32_t stride) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDrawIndirect(vk, buffer, offset, drawCount, stride);
    return 0;
  }

  WARN_UNUSED_RESULT int clearAttachments(uint32_t attachmentCount,
                                          const VkClearAttachment* pAttachments,
                                          uint32_t rectCount,
                                          const VkClearRect* pRects) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdClearAttachments(vk, attachmentCount, pAttachments, rectCount, pRects);
    return 0;
  }

  WARN_UNUSED_RESULT int clearColorImage(
      VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor,
      uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdClearColorImage(vk, image, imageLayout, pColor, rangeCount, pRanges);
    return 0;
  }

  WARN_UNUSED_RESULT int clearDepthStencilImage(
      VkImage image, VkImageLayout imageLayout,
      const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount,
      const VkImageSubresourceRange* pRanges) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdClearDepthStencilImage(vk, image, imageLayout, pDepthStencil,
                                rangeCount, pRanges);
    return 0;
  }

  WARN_UNUSED_RESULT int dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                  uint32_t groupCountZ) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDispatch(vk, groupCountX, groupCountY, groupCountZ);
    return 0;
  }

  WARN_UNUSED_RESULT int dispatchIndirect(VkBuffer buffer,
                                          VkDeviceSize offset) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDispatchIndirect(vk, buffer, offset);
    return 0;
  }
#if defined(VK_VERSION_1_1) && !defined(__ANDROID__)
  // dispatchBase comes from vkCmdDispatchBaseKHR in VK_KHR_device_group but
  // was promoted to core in Vulkan 1.1.
  WARN_UNUSED_RESULT int dispatchBase(uint32_t baseGroupX, uint32_t baseGroupY,
                                      uint32_t baseGroupZ, uint32_t groupCountX,
                                      uint32_t groupCountY,
                                      uint32_t groupCountZ) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdDispatchBase(vk, baseGroupX, baseGroupY, baseGroupZ, groupCountX,
                      groupCountY, groupCountZ);
    return 0;
  }
#endif

  // References for understanding memory synchronization primitives:
  // https://www.khronos.org/assets/uploads/developers/library/2017-khronos-uk-vulkanised/004-Synchronization-Keeping%20Your%20Device%20Fed_May17.pdf
  //
  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
  struct BarrierSet {
    BarrierSet() { reset(); }

    void reset() {
      mem.clear();
      buf.clear();
      img.clear();
      srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    std::vector<VkMemoryBarrier> mem;
    std::vector<VkBufferMemoryBarrier> buf;
    std::vector<VkImageMemoryBarrier> img;

    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
  };
  BarrierSet lazyBarriers;

  // waitBarrier calls vkCmdPipelineBarrier. This will flush previous barrier()
  // calls if they were used, but gives direct access to vkCmdPipelineBarrier.
  // The other forms of barrier() below lazily construct a BarrierSet which is
  // automatically flushed before the command buffer is used again.
  WARN_UNUSED_RESULT int waitBarrier(const BarrierSet& b,
                                     VkDependencyFlags dependencyFlags = 0);

  // waitEvents calls vkCmdWaitEvents. Since vkCmdWaitEvents also accepts
  // all the barrier structs, this flushes all lazy barriers.
  // Please see setEvent() and resetEvent() for using VkEvent.
  WARN_UNUSED_RESULT int waitEvents(const std::vector<VkEvent>& events);

  // waitEvents calls vkCmdWaitEvents. But it accepts all the barrier structs
  // as well, except not VkDependencyFlags. In addition, any VkEvents are
  // waited on. Please see setEvent() and resetEvent() for using VkEvent.
  WARN_UNUSED_RESULT int waitEvents(const std::vector<VkEvent>& events,
                                    BarrierSet& b) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdWaitEvents(vk, events.size(), events.data(), b.srcStageMask,
                    b.dstStageMask, b.mem.size(), b.mem.data(), b.buf.size(),
                    b.buf.data(), b.img.size(), b.img.data());
    return 0;
  }

  // barrier(Image, VkImageLayout) adds a barrier that transitions Image to
  // a new layout.
  //
  // As a convenience this changes img.currentLayout to newLayout.
  // WARNING: the other barrier() does NOT auto-update img.currentLayout!
  WARN_UNUSED_RESULT int barrier(memory::Image& img, VkImageLayout newLayout);

  // barrier(Image, VkImageLayout, VkImageSubresourceRange) adds a barrier that
  // transitions the part of Image given by 'range' to a new layout.
  //
  // WARNING: this does NOT auto-update img.currentLayout, since only a range
  // within the image is being transitioned. The other barrier() method does
  // auto-update img.currentLayout. Potential confusion alert!
  WARN_UNUSED_RESULT int barrier(memory::Image& img, VkImageLayout newLayout,
                                 const VkImageSubresourceRange& range);

  // barrier(VkBufferMemoryBarrier) can be used to transition a buffer to a new
  // queue.
  WARN_UNUSED_RESULT int barrier(const VkBufferMemoryBarrier& b) {
    if (b.sType != VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER) {
      logE("barrier(%s): invalid %s.sType\n", "VkBufferMemoryBarrier",
           "VkBufferMemoryBarrier");
      return 1;
    }
    if (!b.buffer) {
      logE("CommandBuffer::barrier(VkBufferMemoryBarrier): invalid VkBuffer\n");
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    lazyBarriers.buf.emplace_back(b);
    return 0;
  }

  // barrier(VkMemoryBarrier) can be used to enforce a device-wide barrier.
  WARN_UNUSED_RESULT int barrier(const VkMemoryBarrier& b) {
    if (b.sType != VK_STRUCTURE_TYPE_MEMORY_BARRIER) {
      logE("barrier(%s): invalid %s.sType\n", "VkMemoryBarrier",
           "VkMemoryBarrier");
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    lazyBarriers.mem.emplace_back(b);
    return 0;
  }

  WARN_UNUSED_RESULT int setEvent(VkEvent event,
                                  VkPipelineStageFlags stageMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetEvent(vk, event, stageMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setEvent(Event& event,
                                  VkPipelineStageFlags stageMask) {
    return setEvent(event.vk, stageMask);
  }

  WARN_UNUSED_RESULT int resetEvent(VkEvent event,
                                    VkPipelineStageFlags stageMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdResetEvent(vk, event, stageMask);
    return 0;
  }
  WARN_UNUSED_RESULT int resetEvent(Event& event,
                                    VkPipelineStageFlags stageMask) {
    return resetEvent(event.vk, stageMask);
  }

  //
  // The following commands require the currently bound pipeline had
  // VK_DYNAMIC_STATE_* flags enabled first.
  //

  WARN_UNUSED_RESULT int setBlendConstants(const float blendConstants[4]) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetBlendConstants(vk, blendConstants);
    return 0;
  }
  WARN_UNUSED_RESULT int setDepthBias(float constantFactor, float clamp,
                                      float slopeFactor) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetDepthBias(vk, constantFactor, clamp, slopeFactor);
    return 0;
  }
  WARN_UNUSED_RESULT int setDepthBounds(float minBound, float maxBound) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetDepthBounds(vk, minBound, maxBound);
    return 0;
  }
  WARN_UNUSED_RESULT int setLineWidth(float lineWidth) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetLineWidth(vk, lineWidth);
    return 0;
  }
  WARN_UNUSED_RESULT int setScissor(uint32_t firstScissor,
                                    uint32_t scissorCount,
                                    const VkRect2D* pScissors) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetScissor(vk, firstScissor, scissorCount, pScissors);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilCompareMask(VkStencilFaceFlags faceMask,
                                               uint32_t compareMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetStencilCompareMask(vk, faceMask, compareMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilReference(VkStencilFaceFlags faceMask,
                                             uint32_t reference) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetStencilReference(vk, faceMask, reference);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilWriteMask(VkStencilFaceFlags faceMask,
                                             uint32_t writeMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetStencilWriteMask(vk, faceMask, writeMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setViewport(uint32_t firstViewport,
                                     uint32_t viewportCount,
                                     const VkViewport* pViewports) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetViewport(vk, firstViewport, viewportCount, pViewports);
    return 0;
  }
#if defined(VK_VERSION_1_1) && !defined(__ANDROID__)
  // setDeviceMask comes from vkCmdSetDeviceMaskKHR in VK_KHR_device_group but
  // was promoted to core in Vulkan 1.1.
  WARN_UNUSED_RESULT int setDeviceMask(uint32_t deviceMask) {
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    vkCmdSetDeviceMask(vk, deviceMask);
    return 0;
  }
#endif
  WARN_UNUSED_RESULT int pushDescriptorSet(
      VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
      uint32_t set, uint32_t descriptorWriteCount,
      const VkWriteDescriptorSet* pDescriptorWrites) {
    if (!cpool.fp.pushDescriptorSet) {
      logE("cpool.%s is NULL. Please load %s and retry.\n", "pushDescriptorSet",
           VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.pushDescriptorSet(vk, pipelineBindPoint, layout, set,
                               descriptorWriteCount, pDescriptorWrites);
    return 0;
  }
  WARN_UNUSED_RESULT int pushDescriptorSetWithTemplate(
      VkDescriptorUpdateTemplate descriptorUpdateTemplate,
      VkPipelineLayout layout, uint32_t set, const void* pData) {
    if (!cpool.fp.pushDescriptorSetWithTemplate) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "pushDescriptorSetWithTemplate",
           VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.pushDescriptorSetWithTemplate(vk, descriptorUpdateTemplate, layout,
                                           set, pData);
    return 0;
  }
  WARN_UNUSED_RESULT int drawIndirectCount(VkBuffer buffer, VkDeviceSize offset,
                                           VkBuffer countBuffer,
                                           VkDeviceSize countBufferOffset,
                                           uint32_t maxDrawCount,
                                           uint32_t stride) {
    if (!cpool.fp.drawIndirectCount) {
      logE("cpool.%s is NULL. Please load %s and retry.\n", "drawIndirectCount",
           VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.drawIndirectCount(vk, buffer, offset, countBuffer,
                               countBufferOffset, maxDrawCount, stride);
    return 0;
  }
  WARN_UNUSED_RESULT int drawIndexedIndirectCount(
      VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
      VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    if (!cpool.fp.drawIndexedIndirectCount) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "drawIndexedIndirectCount",
           VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.drawIndexedIndirectCount(vk, buffer, offset, countBuffer,
                                      countBufferOffset, maxDrawCount, stride);
    return 0;
  }
  WARN_UNUSED_RESULT int bindTransformFeedbackBuffers(
      uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers,
      const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    if (!cpool.fp.bindTransformFeedbackBuffers) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "bindTransformFeedbackBuffers",
           VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.bindTransformFeedbackBuffers(vk, firstBinding, bindingCount,
                                          pBuffers, pOffsets, pSizes);
    return 0;
  }
  WARN_UNUSED_RESULT int beginTransformFeedback(
      uint32_t firstCounterBuffer, uint32_t counterBufferCount,
      const VkBuffer* pCounterBuffers,
      const VkDeviceSize* pCounterBufferOffsets) {
    if (!cpool.fp.beginTransformFeedback) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "beginTransformFeedback", VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.beginTransformFeedback(vk, firstCounterBuffer, counterBufferCount,
                                    pCounterBuffers, pCounterBufferOffsets);
    return 0;
  }
  WARN_UNUSED_RESULT int endTransformFeedback(
      uint32_t firstCounterBuffer, uint32_t counterBufferCount,
      const VkBuffer* pCounterBuffers,
      const VkDeviceSize* pCounterBufferOffsets) {
    if (!cpool.fp.endTransformFeedback) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "endTransformFeedback", VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.endTransformFeedback(vk, firstCounterBuffer, counterBufferCount,
                                  pCounterBuffers, pCounterBufferOffsets);
    return 0;
  }
  WARN_UNUSED_RESULT int beginQueryIndexed(VkQueryPool queryPool,
                                           uint32_t query,
                                           VkQueryControlFlags flags,
                                           uint32_t index) {
    if (!cpool.fp.beginQueryIndexed) {
      logE("cpool.%s is NULL. Please load %s and retry.\n", "beginQueryIndexed",
           VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.beginQueryIndexed(vk, queryPool, query, flags, index);
    return 0;
  }
  WARN_UNUSED_RESULT int endQueryIndexed(VkQueryPool queryPool, uint32_t query,
                                         uint32_t index) {
    if (!cpool.fp.endQueryIndexed) {
      logE("cpool.%s is NULL. Please load %s and retry.\n", "endQueryIndexed",
           VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.endQueryIndexed(vk, queryPool, query, index);
    return 0;
  }
  WARN_UNUSED_RESULT int drawIndirectByteCount(uint32_t instanceCount,
                                               uint32_t firstInstance,
                                               VkBuffer counterBuffer,
                                               VkDeviceSize counterBufferOffset,
                                               uint32_t counterOffset,
                                               uint32_t vertexStride) {
    if (!cpool.fp.drawIndirectByteCount) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "drawIndirectByteCount", VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.drawIndirectByteCount(vk, instanceCount, firstInstance,
                                   counterBuffer, counterBufferOffset,
                                   counterOffset, vertexStride);
    return 0;
  }
  WARN_UNUSED_RESULT int beginConditionalRendering(
      const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    if (!cpool.fp.beginConditionalRendering) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "beginConditionalRendering",
           VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.beginConditionalRendering(vk, pConditionalRenderingBegin);
    return 0;
  }
  WARN_UNUSED_RESULT int endConditionalRendering() {
    if (!cpool.fp.endConditionalRendering) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "endConditionalRendering",
           VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.endConditionalRendering(vk);
    return 0;
  }
  WARN_UNUSED_RESULT int setDiscardRectangle(
      uint32_t firstDiscardRectangle, uint32_t discardRectangleCount,
      const VkRect2D* pDiscardRectangles) {
    if (!cpool.fp.setDiscardRectangle) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "setDiscardRectangle", VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.setDiscardRectangle(vk, firstDiscardRectangle,
                                 discardRectangleCount, pDiscardRectangles);
    return 0;
  }
  WARN_UNUSED_RESULT int setSampleLocations(
      const VkSampleLocationsInfoEXT* pSampleLocationsInfo) {
    if (!cpool.fp.setSampleLocations) {
      logE("cpool.%s is NULL. Please load %s and retry.\n",
           "setSampleLocations", VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME);
      return 1;
    }
    CommandPool::lock_guard_t lock(cpool.lockmutex);
    if (flushLazyBarriers(lock)) return 1;
    cpool.fp.setSampleLocations(vk, pSampleLocationsInfo);
    return 0;
  }
};

}  // namespace command
