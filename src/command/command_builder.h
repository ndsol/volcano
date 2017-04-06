/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/command is the 3rd-level bindings for the Vulkan graphics library.
 *
 * Because class CommandBuilder is such a large class, it is defined in its
 * own header file. DO NOT INCLUDE THIS FILE DIRECTLY.
 *
 * You should #include "src/command/command.h"
 */

#pragma once

namespace command {

// CommandBuilder holds a vector of VkCommandBuffer, designed to simplify
// recording, executing, and reusing a VkCommandBuffer. A vector of
// VkCommandBuffers is more useful because one buffer may be executing while
// your application is recording into another (or similar designs). The
// CommandBuilder::use() method selects which VkCommandBuffer gets recorded
// or "built."
// https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#commandbuffers-lifecycle
//
// Static command buffers can also be built using this class; your application
// may still need the vector of many VkCommandBuffers because each one will
// bind to the renderPass (passBeginInfo contains a reference to a framebuffer,
// which must be set up for each framebuffer in the swapChain).
//
// TODO: describe how to use a secondary command buffer for static draw calls.
class CommandBuilder {
 protected:
  CommandPool& cpool;
  bool isAllocated = false;
  size_t bufInUse = 0;
  VkCommandBuffer buf = VK_NULL_HANDLE;

  int alloc() {
    if (cpool.alloc(bufs)) {
      return 1;
    }
    isAllocated = true;
    use(bufInUse);
    return 0;
  }

 public:
  CommandBuilder(CommandPool& cpool_, size_t initialSize = 1)
      : cpool(cpool_), bufs(initialSize) {}
  ~CommandBuilder();

  std::vector<VkCommandBuffer> bufs;
  size_t getUsed() const { return bufInUse; }

  // resize updates the vector size and reallocates the VkCommandBuffers.
  WARN_UNUSED_RESULT int resize(size_t bufsSize) {
    if (isAllocated) {
      // free any VkCommandBuffer in buf. Command Buffers are automatically
      // freed when the CommandPool is destroyed, so free() is really only
      // needed when dynamically replacing an existing set of CommandBuffers.
      cpool.free(bufs);
    }
    bufs.resize(bufsSize);
    return alloc();
  }

  // use selects which index in the vector bufs gets recorded or "built."
  // The first VkCommandBuffer is selected by default, to simplify cases where
  // only one CommandBuffer is needed.
  void use(size_t i) {
    bufInUse = i;
    buf = bufs.at(i);
  }

  // submit calls vkQueueSubmit using commandPoolQueueI.
  // Note vkQueueSubmit is a high overhead operation; submitting multiple
  // command buffers and even multiple VkSubmitInfo batches is recommended --
  // see submitMany.
  //
  // An optional VkFence parameter can be specified to signal the VkFence when
  // the operation is complete.
  WARN_UNUSED_RESULT int submit(
      size_t commandPoolQueueI,
      const std::vector<VkSemaphore>& waitSemaphores =
          std::vector<VkSemaphore>(),
      const std::vector<VkPipelineStageFlags>& waitStages =
          std::vector<VkPipelineStageFlags>(),
      const std::vector<VkSemaphore>& signalSemaphores =
          std::vector<VkSemaphore>(),
      VkFence fence = VK_NULL_HANDLE) {
    if (waitSemaphores.size() != waitStages.size()) {
      fprintf(stderr, "submit: waitSemaphores len=%zu but waitStages len=%zu\n",
              waitSemaphores.size(), waitStages.size());
      return 1;
    }
    VkSubmitInfo VkInit(submitInfo);
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buf;
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    VkResult v =
        vkQueueSubmit(cpool.q(commandPoolQueueI), 1, &submitInfo, fence);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkQueueSubmit", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  // submitMany bypasses the typical CommandBuilder::use() and allows raw
  // access to the VkQueueSubmit() call.
  //
  // Note vkQueueSubmit is a high overhead operation. submitMany is the way to
  // submit multiple VkSubmitInfo objects at once.
  //
  // An optional VkFence parameter can be specified to signal the VkFence when
  // the operation is complete.
  WARN_UNUSED_RESULT int submitMany(size_t commandPoolQueueI,
                                    std::vector<VkSubmitInfo> info,
                                    VkFence fence = VK_NULL_HANDLE) {
    VkResult v = vkQueueSubmit(cpool.q(commandPoolQueueI), info.size(),
                               info.data(), fence);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkQueueSubmit", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  // reset deallocates and clears the current VkCommandBuffer. Note that in
  // most cases, begin() calls vkBeginCommandBuffer() which implicitly resets
  // the buffer and clears any old data it may have had.
  WARN_UNUSED_RESULT int reset(
      VkCommandBufferResetFlagBits flags =
          VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) {
    VkResult v;
    if ((v = vkResetCommandBuffer(buf, flags)) != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkResetCommandBuffer", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  WARN_UNUSED_RESULT int begin(VkCommandBufferUsageFlagBits usageFlags) {
    if (!isAllocated && alloc()) {
      return 1;
    }
    VkCommandBufferBeginInfo VkInit(cbbi);
    cbbi.flags = usageFlags;
    VkResult v = vkBeginCommandBuffer(buf, &cbbi);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkBeginCommandBuffer", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  WARN_UNUSED_RESULT int beginOneTimeUse() {
    return begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  }
  WARN_UNUSED_RESULT int beginSimultaneousUse() {
    return begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
  }

  WARN_UNUSED_RESULT int end() {
    if (!isAllocated && alloc()) {
      return 1;
    }
    VkResult v = vkEndCommandBuffer(buf);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n", "vkEndCommandBuffer", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  WARN_UNUSED_RESULT int executeCommands(uint32_t secondaryCmdsCount,
                                         VkCommandBuffer* pSecondaryCmds) {
    vkCmdExecuteCommands(buf, secondaryCmdsCount, pSecondaryCmds);
    return 0;
  }

  WARN_UNUSED_RESULT int waitEvents(
      uint32_t eventCount, const VkEvent* pEvents,
      VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
      uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
      uint32_t bufferMemoryBarrierCount,
      const VkBufferMemoryBarrier* pBufferMemoryBarriers,
      uint32_t imageMemoryBarrierCount,
      const VkImageMemoryBarrier* pImageMemoryBarriers) {
    vkCmdWaitEvents(buf, eventCount, pEvents, srcStageMask, dstStageMask,
                    memoryBarrierCount, pMemoryBarriers,
                    bufferMemoryBarrierCount, pBufferMemoryBarriers,
                    imageMemoryBarrierCount, pImageMemoryBarriers);
    return 0;
  }

  WARN_UNUSED_RESULT int setEvent(VkEvent event,
                                  VkPipelineStageFlags stageMask) {
    vkCmdSetEvent(buf, event, stageMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setEvent(Event& event,
                                  VkPipelineStageFlags stageMask) {
    return setEvent(event.vk, stageMask);
  }

  WARN_UNUSED_RESULT int resetEvent(VkEvent event,
                                    VkPipelineStageFlags stageMask) {
    vkCmdResetEvent(buf, event, stageMask);
    return 0;
  }
  WARN_UNUSED_RESULT int resetEvent(Event& event,
                                    VkPipelineStageFlags stageMask) {
    return resetEvent(event.vk, stageMask);
  }

  WARN_UNUSED_RESULT int pushConstants(Pipeline& pipe,
                                       VkShaderStageFlags stageFlags,
                                       uint32_t offset, uint32_t size,
                                       const void* pValues) {
    vkCmdPushConstants(buf, pipe.pipelineLayout, stageFlags, offset, size,
                       pValues);
    return 0;
  }

  WARN_UNUSED_RESULT int fillBuffer(VkBuffer dst, VkDeviceSize dstOffset,
                                    VkDeviceSize size, uint32_t data) {
    vkCmdFillBuffer(buf, dst, dstOffset, size, data);
    return 0;
  }

  WARN_UNUSED_RESULT int updateBuffer(VkBuffer dst, VkDeviceSize dstOffset,
                                      VkDeviceSize dataSize,
                                      const void* pData) {
    vkCmdUpdateBuffer(buf, dst, dstOffset, dataSize, pData);
    return 0;
  }

  WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst,
                                    const std::vector<VkBufferCopy>& regions) {
    if (regions.size() == 0) {
      fprintf(stderr, "copyBuffer with empty regions\n");
      return 1;
    }
    if (!isAllocated && alloc()) {
      return 1;
    }
    vkCmdCopyBuffer(buf, src, dst, regions.size(), regions.data());
    return 0;
  }
  WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst, size_t size) {
    VkBufferCopy region = {};
    region.size = size;
    std::vector<VkBufferCopy> regions{region};
    return copyBuffer(src, dst, regions);
  }

  WARN_UNUSED_RESULT int copyBufferToImage(
      VkBuffer src, VkImage dst, VkImageLayout dstLayout,
      std::vector<VkBufferImageCopy>& regions) {
    vkCmdCopyBufferToImage(buf, src, dst, dstLayout, regions.size(),
                           regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyImageToBuffer(
      VkImage src, VkImageLayout srcLayout, VkBuffer dst,
      std::vector<VkBufferImageCopy>& regions) {
    vkCmdCopyImageToBuffer(buf, src, srcLayout, dst, regions.size(),
                           regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyImage(VkImage src, VkImageLayout srcLayout,
                                   VkImage dst, VkImageLayout dstLayout,
                                   const std::vector<VkImageCopy>& regions) {
    vkCmdCopyImage(buf, src, srcLayout, dst, dstLayout, regions.size(),
                   regions.data());
    return 0;
  }
  WARN_UNUSED_RESULT int copyImage(VkImage src, VkImage dst,
                                   const std::vector<VkImageCopy>& regions) {
    return copyImage(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions);
  }

  WARN_UNUSED_RESULT int blitImage(VkImage src, VkImageLayout srcLayout,
                                   VkImage dst, VkImageLayout dstLayout,
                                   std::vector<VkImageBlit>& regions,
                                   VkFilter filter) {
    vkCmdBlitImage(buf, src, srcLayout, dst, dstLayout, regions.size(),
                   regions.data(), filter);
    return 0;
  }

  WARN_UNUSED_RESULT int resolveImage(VkImage src, VkImageLayout srcLayout,
                                      VkImage dst, VkImageLayout dstLayout,
                                      std::vector<VkImageResolve>& regions) {
    vkCmdResolveImage(buf, src, srcLayout, dst, dstLayout, regions.size(),
                      regions.data());
    return 0;
  }

  WARN_UNUSED_RESULT int copyQueryPoolResults(
      VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
      VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride,
      VkQueryResultFlags flags) {
    vkCmdCopyQueryPoolResults(buf, queryPool, firstQuery, queryCount, dstBuffer,
                              dstOffset, stride, flags);
    return 0;
  }

  WARN_UNUSED_RESULT int resetQueryPool(VkQueryPool queryPool,
                                        uint32_t firstQuery,
                                        uint32_t queryCount) {
    vkCmdResetQueryPool(buf, queryPool, firstQuery, queryCount);
    return 0;
  }

  WARN_UNUSED_RESULT int beginQuery(VkQueryPool queryPool, uint32_t query,
                                    VkQueryControlFlags flags) {
    vkCmdBeginQuery(buf, queryPool, query, flags);
    return 0;
  }

  WARN_UNUSED_RESULT int endQuery(VkQueryPool queryPool, uint32_t query) {
    vkCmdEndQuery(buf, queryPool, query);
    return 0;
  }

  WARN_UNUSED_RESULT int writeTimestamp(VkPipelineStageFlagBits stage,
                                        VkQueryPool queryPool, uint32_t query) {
    vkCmdWriteTimestamp(buf, stage, queryPool, query);
    return 0;
  }

  WARN_UNUSED_RESULT int beginRenderPass(VkRenderPassBeginInfo& passBeginInfo,
                                         VkSubpassContents contents) {
    if (!passBeginInfo.framebuffer) {
      fprintf(stderr, "beginRenderPass: framebuffer was not set\n");
      return 1;
    }
    vkCmdBeginRenderPass(buf, &passBeginInfo, contents);
    return 0;
  }
  WARN_UNUSED_RESULT int beginRenderPass(RenderPass& pass,
                                         VkSubpassContents contents) {
    pass.passBeginInfo.renderArea.extent = cpool.dev.swapChainInfo.imageExtent;
    return beginRenderPass(pass.passBeginInfo, contents);
  }

  // beginPrimaryPass starts a RenderPass using a primary command buffer.
  WARN_UNUSED_RESULT int beginPrimaryPass(RenderPass& pass) {
    return beginRenderPass(pass, VK_SUBPASS_CONTENTS_INLINE);
  }
  // beginSecondaryPass is necessary to bind a secondary command buffer to
  // its associated RenderPass even though it will be called from the correct
  // primary command buffer.
  WARN_UNUSED_RESULT int beginSecondaryPass(RenderPass& pass) {
    return beginRenderPass(pass, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }

  WARN_UNUSED_RESULT int nextSubpass(VkSubpassContents contents) {
    vkCmdNextSubpass(buf, contents);
    return 0;
  }
  // nextPrimarySubpass starts the next subpass, but must be in a primary
  // command buffer.
  WARN_UNUSED_RESULT int nextPrimarySubpass() {
    return nextSubpass(VK_SUBPASS_CONTENTS_INLINE);
  }
  // nextSecondarySubpass allows a secondary command buffer to have subpasses.
  WARN_UNUSED_RESULT int nextSecondarySubpass() {
    return nextSubpass(VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }

  WARN_UNUSED_RESULT int endRenderPass() {
    vkCmdEndRenderPass(buf);
    return 0;
  }

  WARN_UNUSED_RESULT int bindPipeline(VkPipelineBindPoint bindPoint,
                                      Pipeline& pipe) {
    vkCmdBindPipeline(buf, bindPoint, pipe.vk);
    return 0;
  }

  WARN_UNUSED_RESULT int bindDescriptorSets(
      VkPipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t firstSet,
      uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets,
      uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    vkCmdBindDescriptorSets(buf, bindPoint, layout, firstSet,
                            descriptorSetCount, pDescriptorSets,
                            dynamicOffsetCount, pDynamicOffsets);
    return 0;
  }
  WARN_UNUSED_RESULT int bindGraphicsPipelineAndDescriptors(
      Pipeline& pipe, uint32_t firstSet, uint32_t descriptorSetCount,
      const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    return bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipe) ||
           bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipe.pipelineLayout, firstSet, descriptorSetCount,
                              pDescriptorSets, dynamicOffsetCount,
                              pDynamicOffsets);
  }
  WARN_UNUSED_RESULT int bindComputePipelineAndDescriptors(
      Pipeline& pipe, uint32_t firstSet, uint32_t descriptorSetCount,
      const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0,
      const uint32_t* pDynamicOffsets = nullptr) {
    return bindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipe) ||
           bindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE,
                              pipe.pipelineLayout, firstSet, descriptorSetCount,
                              pDescriptorSets, dynamicOffsetCount,
                              pDynamicOffsets);
  }

  WARN_UNUSED_RESULT int bindVertexBuffers(uint32_t firstBinding,
                                           uint32_t bindingCount,
                                           const VkBuffer* pBuffers,
                                           const VkDeviceSize* pOffsets) {
    vkCmdBindVertexBuffers(buf, firstBinding, bindingCount, pBuffers, pOffsets);
    return 0;
  }

  WARN_UNUSED_RESULT int bindIndexBuffer(VkBuffer indexBuf, VkDeviceSize offset,
                                         VkIndexType indexType) {
    vkCmdBindIndexBuffer(buf, indexBuf, offset, indexType);
    return 0;
  }

  WARN_UNUSED_RESULT int drawIndexed(uint32_t indexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstIndex, int32_t vertexOffset,
                                     uint32_t firstInstance) {
    vkCmdDrawIndexed(buf, indexCount, instanceCount, firstIndex, vertexOffset,
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

  WARN_UNUSED_RESULT int drawIndexedIndirect(VkBuffer buffer,
                                             VkDeviceSize offset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
    vkCmdDrawIndexedIndirect(buf, buffer, offset, drawCount, stride);
    return 0;
  }

  WARN_UNUSED_RESULT int draw(uint32_t vertexCount, uint32_t instanceCount,
                              uint32_t firstVertex, uint32_t firstInstance) {
    vkCmdDraw(buf, vertexCount, instanceCount, firstVertex, firstInstance);
    return 0;
  }

  WARN_UNUSED_RESULT int drawIndirect(VkBuffer buffer, VkDeviceSize offset,
                                      uint32_t drawCount, uint32_t stride) {
    vkCmdDrawIndirect(buf, buffer, offset, drawCount, stride);
    return 0;
  }

  WARN_UNUSED_RESULT int clearAttachments(uint32_t attachmentCount,
                                          const VkClearAttachment* pAttachments,
                                          uint32_t rectCount,
                                          const VkClearRect* pRects) {
    vkCmdClearAttachments(buf, attachmentCount, pAttachments, rectCount,
                          pRects);
    return 0;
  }

  WARN_UNUSED_RESULT int clearColorImage(
      VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor,
      uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    vkCmdClearColorImage(buf, image, imageLayout, pColor, rangeCount, pRanges);
    return 0;
  }

  WARN_UNUSED_RESULT int clearDepthStencilImage(
      VkImage image, VkImageLayout imageLayout,
      const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount,
      const VkImageSubresourceRange* pRanges) {
    vkCmdClearDepthStencilImage(buf, image, imageLayout, pDepthStencil,
                                rangeCount, pRanges);
    return 0;
  }

  WARN_UNUSED_RESULT int dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                  uint32_t groupCountZ) {
    vkCmdDispatch(buf, groupCountX, groupCountY, groupCountZ);
    return 0;
  }

  WARN_UNUSED_RESULT int dispatchIndirect(VkBuffer buffer,
                                          VkDeviceSize offset) {
    vkCmdDispatchIndirect(buf, buffer, offset);
    return 0;
  }

  struct BarrierSet {
    std::vector<VkMemoryBarrier> mem;
    std::vector<VkBufferMemoryBarrier> buf;
    std::vector<VkImageMemoryBarrier> img;
  };

  WARN_UNUSED_RESULT int barrier(
      BarrierSet& bset,
      VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VkDependencyFlags dependencyFlags = 0) {
    bool found = false;
    for (auto& mem : bset.mem) {
      found = true;
      if (mem.sType != VK_STRUCTURE_TYPE_MEMORY_BARRIER) {
        fprintf(stderr, "BarrierSet::mem contains invalid VkMemoryBarrier\n");
        return 1;
      }
    }
    for (auto& buf : bset.buf) {
      found = true;
      if (!buf.buffer) {
        fprintf(stderr, "BarrierSet::buf contains invalid VkBuffer\n");
        return 1;
      }
    }
    for (auto& img : bset.img) {
      found = true;
      if (!img.image) {
        fprintf(stderr, "BarrierSet::img contains invalid VkImage\n");
        return 1;
      }
    }
    if (!found) {
      fprintf(stderr, "All {mem,buf,img} were empty in BarrierSet.\n");
      return 1;
    }
    vkCmdPipelineBarrier(buf, srcStageMask, dstStageMask, dependencyFlags,
                         bset.mem.size(), bset.mem.data(), bset.buf.size(),
                         bset.buf.data(), bset.img.size(), bset.img.data());
    return 0;
  }

  //
  // The following commands require the currently bound pipeline had
  // VK_DYNAMIC_STATE_* flags enabled first.
  //

  WARN_UNUSED_RESULT int setBlendConstants(const float blendConstants[4]) {
    vkCmdSetBlendConstants(buf, blendConstants);
    return 0;
  }
  WARN_UNUSED_RESULT int setDepthBias(float constantFactor, float clamp,
                                      float slopeFactor) {
    vkCmdSetDepthBias(buf, constantFactor, clamp, slopeFactor);
    return 0;
  }
  WARN_UNUSED_RESULT int setDepthBounds(float minBound, float maxBound) {
    vkCmdSetDepthBounds(buf, minBound, maxBound);
    return 0;
  }
  WARN_UNUSED_RESULT int setLineWidth(float lineWidth) {
    vkCmdSetLineWidth(buf, lineWidth);
    return 0;
  }
  WARN_UNUSED_RESULT int setScissor(uint32_t firstScissor,
                                    uint32_t scissorCount,
                                    const VkRect2D* pScissors) {
    vkCmdSetScissor(buf, firstScissor, scissorCount, pScissors);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilCompareMask(VkStencilFaceFlags faceMask,
                                               uint32_t compareMask) {
    vkCmdSetStencilCompareMask(buf, faceMask, compareMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilReference(VkStencilFaceFlags faceMask,
                                             uint32_t reference) {
    vkCmdSetStencilReference(buf, faceMask, reference);
    return 0;
  }
  WARN_UNUSED_RESULT int setStencilWriteMask(VkStencilFaceFlags faceMask,
                                             uint32_t writeMask) {
    vkCmdSetStencilWriteMask(buf, faceMask, writeMask);
    return 0;
  }
  WARN_UNUSED_RESULT int setViewport(uint32_t firstViewport,
                                     uint32_t viewportCount,
                                     const VkViewport* pViewports) {
    vkCmdSetViewport(buf, firstViewport, viewportCount, pViewports);
    return 0;
  }
  // setViewport is a convenience method to update all viewports in a
  // VkRenderPass from the viewports in pass.pipelines[].info.
  WARN_UNUSED_RESULT int setViewport(RenderPass& pass) {
    std::vector<VkViewport> viewports;
    for (auto& pipe : pass.pipelines) {
      viewports.insert(viewports.end(), pipe.info.viewports.begin(),
                       pipe.info.viewports.end());
    }
    return setViewport(0, viewports.size(), viewports.data());
  }
  // setScissor is a convenience method to update all scissors in a
  // VkRenderPass from the scissors in pass.pipelines[].info.
  WARN_UNUSED_RESULT int setScissor(RenderPass& pass) {
    std::vector<VkRect2D> scissors;
    for (auto& pipe : pass.pipelines) {
      scissors.insert(scissors.end(), pipe.info.scissors.begin(),
                      pipe.info.scissors.end());
    }
    return setScissor(0, scissors.size(), scissors.data());
  }

  //
  // The following command is almost entirely separate from CommandBuilder,
  // but is included for convenience because it calls use():
  //

  // acquireNextImage calls vkAcquireNextImageKHR and updates this
  // CommandBuilder by calling use() on the acquired image index.
  // Note: you must handle several different VkResult results differently.
  WARN_UNUSED_RESULT VkResult acquireNextImage(
      Semaphore& imageAvailableSemaphore, uint64_t timeout = UINT64_MAX) {
    return acquireNextImage(imageAvailableSemaphore.vk, VK_NULL_HANDLE,
                            timeout);
  }

  // acquireNextImage calls vkAcquireNextImageKHR and updates this
  // CommandBuilder by calling use() on the acquired image index.
  // Note: you must handle several different VkResult results differently.
  WARN_UNUSED_RESULT VkResult acquireNextImage(Fence& imageAvailableFence,
                                               uint64_t timeout = UINT64_MAX) {
    return acquireNextImage(VK_NULL_HANDLE, imageAvailableFence.vk, timeout);
  }

  // acquireNextImage calls vkAcquireNextImageKHR and updates this
  // CommandBuilder by calling use() on the acquired image index.
  // Note: you must handle several different VkResult results differently.
  WARN_UNUSED_RESULT VkResult
  acquireNextImage(Semaphore& imageAvailableSemaphore,
                   Fence& imageAvailableFence, uint64_t timeout = UINT64_MAX) {
    return acquireNextImage(imageAvailableSemaphore.vk, imageAvailableFence.vk,
                            timeout);
  }

  // acquireNextImage calls vkAcquireNextImageKHR and updates this
  // CommandBuilder by calling use() on the acquired image index.
  // Note: you must handle several different VkResult results differently.
  WARN_UNUSED_RESULT VkResult
  acquireNextImage(VkSemaphore imageAvailableSemaphore,
                   VkFence imageAvailableFence, uint64_t timeout = UINT64_MAX) {
    uint32_t nextImageIndex;
    VkResult v = vkAcquireNextImageKHR(cpool.dev.dev, cpool.dev.swapChain,
                                       timeout, imageAvailableSemaphore,
                                       imageAvailableFence, &nextImageIndex);
    if (v == VK_SUCCESS || v == VK_SUBOPTIMAL_KHR) {
      // nextImageIndex is valid and should be passed to use().
      if (nextImageIndex >= bufs.size()) {
        fprintf(stderr, "BUG: nextImageIndex=%u while bufs size=%zu\n",
                nextImageIndex, bufs.size());
        return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
      use(nextImageIndex);
    }
    return v;
  }
};

}  // namespace command
