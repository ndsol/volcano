/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * Implements ComputePipeline.
 */

#include "science.h"

namespace science {

int ComputePipeline::allocBlocks(size_t minSize) {
  size_t startIndex = freeBlocks.size();
  {  // Hold lock to protect freeBlocks.
    lock_guard_t lock(lockmutex);

    if (minSize < freeBlocks.size()) {
      logW("TODO: allocBlocks can reduce freeBlocks size.\n");
    }
    while (freeBlocks.size() < minSize) {
      freeBlocks.emplace_back(std::make_shared<ComputeBlock>(cpool));
    }
  }  // Release lock protecting freeBlocks.
  if (!pipe) {
    return 0;  // ctorError has not been called yet.
  }
  return initBlocks(startIndex);
}

int ComputePipeline::ctorError() {
  if (!shader || shader->bytes.empty()) {
    logE("must call shader->loadSPV before ComputePipeline::ctorError\n");
    return 1;
  }
  if (pipe) {
    logE("ComputePipeline::ctorError cannot be called again\n");
    return 1;
  }
  if (cpool.ctorError()) {
    logE("cpool.ctorError failed\n");
    return 1;
  }

  // Use ShaderLibrary and DescriptorLibrary for reflection, build pipe.
  if (allocBlocks(nextSize)) {
    logE("ComputePipeline::ctorError: allocBlocks(%zu) failed\n", nextSize);
    return 1;
  }
  pipe = std::make_shared<command::Pipeline>(cpool, shader);

  ShaderLibrary shaders{cpool.vk.dev};
  if (shaders.add(pipe)) {
    return 1;
  }
  if (shaders.finalizeDescriptorLibrary(descriptorLibrary)) {
    logE("ComputePipeline::ctorError: finalizeDescriptorLibrary failed\n");
    return 1;
  }
  if (pipe->ctorError(cpool)) {
    logE("ComputePipeline::ctorError: vkCreateComputePipelines failed\n");
    return 1;
  }

  // Initialize uniform.
  if (uboSize) {
    if (uniform.empty()) {
      logE("ctorError: uniform vector is empty, but uboSize=%zu.\n",
           (size_t)uboSize);
      return 1;
    }
    for (size_t i = 0; i < uniform.size(); i++) {
      auto& ubo = uniform.at(i);
      ubo.info.size = uboSize;
      ubo.info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      if (ubo.ctorAndBindDeviceLocal()) {
        logE("uniform[%zu].ctorError failed\n", i);
        return 1;
      }
    }
  }

  // Populate empty blocks.
  if (initBlocks(0)) {
    logE("ctorError: initBlocks() failed\n");
    return 1;
  }
  return 0;
}

int ComputePipeline::initBlocks(size_t startIndex) {
  std::vector<VkCommandBuffer> cmdBufVk;
  cmdBufVk.resize((freeBlocks.size() - startIndex) * 2);
  if (!cmdBufVk.empty() && cpool.alloc(cmdBufVk)) {
    logE("initBlocks: cpool.alloc(%zu) failed\n", cmdBufVk.size());
    return 1;
  }

  lock_guard_t lock(lockmutex);  // Protect freeBlocks

  for (size_t i = startIndex; i < freeBlocks.size(); i++) {
    auto b = freeBlocks.at(i);
    if (!b) {
      logE("initBlocks: freeBlocks[%zu] null\n", i);
      return 1;
    }
    b->ds = descriptorLibrary.makeSet(0);
    if (!b->ds) {
      logE("initBlocks: makeSet[%zu] failed\n", i);
      return 1;
    }

    char suffix[128];
    snprintf(suffix, sizeof(suffix), ".freeBlocks[%zu]", i);
    std::string name = debugName + suffix;
    std::vector<VkDescriptorBufferInfo> dsBuf;
    if (blockInSize && (b->i.info.size != blockInSize || !b->i.vk)) {
      if (b->i.setName(name + ".i")) {
        logE("%s.i.setName failed\n", name.c_str());
        return 1;
      }
      b->i.info.size = blockInSize;
      b->i.info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      if (b->i.ctorAndBindDeviceLocal()) {
        logE("%s.i.ctorAndBindDeviceLocal failed\n", name.c_str());
        return 1;
      }

      dsBuf.emplace_back();
      auto& oneBuf = dsBuf.back();
      memset(&oneBuf, 0, sizeof(oneBuf));
      oneBuf.buffer = b->i.vk;
      oneBuf.offset = 0;
      oneBuf.range = blockInSize;
    }
    if (blockOutSize && (b->o.info.size != blockOutSize || !b->i.vk)) {
      if (b->o.setName(name + ".o")) {
        logE("%s.o.setName failed\n", name.c_str());
        return 1;
      }
      b->o.info.size = blockOutSize;
      b->o.info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      if (b->o.ctorAndBindDeviceLocal()) {
        logE("%s.i.ctorAndBindDeviceLocal failed\n", name.c_str());
        return 1;
      }

      dsBuf.emplace_back();
      auto& oneBuf = dsBuf.back();
      memset(&oneBuf, 0, sizeof(oneBuf));
      oneBuf.buffer = b->o.vk;
      oneBuf.offset = 0;
      oneBuf.range = blockOutSize;
    }
    if (bindingIndexOut != bindingIndexIn + 1) {
      logF("BUG: cannot write both bindingIndexIn,Out in one call\n");
      return 1;
    }
    if (dsBuf.size() && b->ds->write(bindingIndexIn, dsBuf)) {
      logE("ds.write(%s.i) failed\n", name.c_str());
      return 1;
    }

    if (uboSize) {
      if (b->uniformIndex >= uniform.size()) {
        logE("initBlocks: freeBlocks[%zu].uniformIndex = %zu out of range\n", i,
             b->uniformIndex);
        return 1;
      }
      dsBuf.resize(1);
      auto& oneBuf = dsBuf.at(0);
      memset(&oneBuf, 0, sizeof(oneBuf));
      oneBuf.buffer = uniform.at(b->uniformIndex).vk;
      oneBuf.offset = 0;
      oneBuf.range = uboSize;
      if (b->ds->write(uboBindingIndex, {oneBuf})) {
        logE("initBlocks: ds.write(%s.ubo) failed\n", name.c_str());
        return 1;
      }
    }
    b->cmdBuf.vk = cmdBufVk.back();
    cmdBufVk.pop_back();
    b->cmdBufPost.vk = cmdBufVk.back();
    cmdBufVk.pop_back();
    if (b->cmdBuf.beginSimultaneousUse() ||
        b->cmdBufPost.beginSimultaneousUse()) {
      logE("initBlocks: freeBlocks[%zu] beginSimultaneousUse failed\n", i);
      return 1;
    }
  }
  return 0;
}

ComputePipeline::BlockVec ComputePipeline::newBlocks(size_t N) {
  lock_guard_t lock(lockmutex);  // Protect freeBlocks
  if (freeBlocks.size() < N) {
    logE("newBlocks(%zu): only %zu in freeBlocks\n", N, freeBlocks.size());
    return BlockVec();
  }
  auto i = freeBlocks.begin() + N;
  prepBlocks.insert(prepBlocks.end(), freeBlocks.begin(), i);
  BlockVec r(freeBlocks.begin(), i);
  freeBlocks.erase(freeBlocks.begin(), i);
  return r;
}

// dispatch is just an internal function to write the commands.
int ComputePipeline::dispatch(command::CommandBuffer& cmd,
                              std::shared_ptr<ComputeBlock> b) {
  if (cmd.bindComputePipelineAndDescriptors(*pipe, 0, 1, &b->ds->vk)) {
    logE("dispatch: cmd.bindComputePipelineAndDescriptors failed\n");
    return 1;
  }
  if (cmd.dispatch(b->work.x, b->work.y, b->work.z)) {
    logE("dispatch: cmd.dispatch failed\n");
    return 1;
  }
  return 0;
}

int ComputePipeline::appendCmds(BlockVec& v, command::SubmitInfo& submitInfo) {
  auto ri = runBlocks.end();  // Mark where runBlocks.end() was previously.
  for (size_t j = 0; j < v.size(); j++) {
    for (auto pi = prepBlocks.begin();;) {
      if (v.at(j) != *pi) {
        pi++;
        if (pi == prepBlocks.end()) {
          logE("enqueueBlocks: block[%zu] not found in prepBlocks.\n", j);
          return 1;
        }
        continue;
      }

      // validate block
      auto& b = v.at(j);
      if (b->work.x < 1 || b->work.y < 1 || b->work.z < 1) {
        logE("enqueueBlocks: block[%zu] has invalid work: %zu, %zu, %zu\n", j,
             (size_t)b->work.x, (size_t)b->work.y, (size_t)b->work.z);
        runBlocks.erase(ri, runBlocks.end());
        return 1;
      }

      lock_guard_t lock(lockmutex);  // Protect prepBlocks and runBlocks
      if (b->flight && b->flight->canSubmit()) {
        // b->flight is now re-used to run compute pipeline too.
        if (dispatch(*b->flight, b)) {
          logE("enqueueBlocks: dispatch(b->flight) failed\n");
          runBlocks.erase(ri, runBlocks.end());
          return 1;
        }
        if (b->flight->end() || b->flight->enqueue(lock, submitInfo) ||
            b->cmdBufPost.end() || b->cmdBufPost.enqueue(lock, submitInfo)) {
          logE("enqueueBlocks[%zu]: flight end or enqueue failed\n", j);
          runBlocks.erase(ri, runBlocks.end());
          return 1;
        }
      } else {
        // Have to use b->cmdBuf because b->flight is null.
        if (dispatch(b->cmdBuf, b)) {
          logE("enqueueBlocks: dispatch(cmdBuf) failed\n");
          runBlocks.erase(ri, runBlocks.end());
          return 1;
        }
        if (b->cmdBuf.end() || b->cmdBuf.enqueue(lock, submitInfo) ||
            b->cmdBufPost.end() || b->cmdBufPost.enqueue(lock, submitInfo)) {
          logE("enqueueBlocks[%zu]: end or enqueue failed\n", j);
          runBlocks.erase(ri, runBlocks.end());
          return 1;
        }
      }

      // Move block from prepBlocks (pi points to prepBlocks) to runBlocks.
      runBlocks.insert(runBlocks.end(), *pi);
      prepBlocks.erase(pi);
      break;
    }
  }
  return 0;
}

int ComputePipeline::enqueueChain(std::vector<BlockVec*> work,
                                  command::SubmitInfo& submitInfo) {
  if (work.size() < 1) {
    logE("enqueueChain: work size %zu is invalid\n", work.size());
    return 1;
  }
  auto ri = runBlocks.end();  // Mark where runBlocks.end() was previously.
  if (appendCmds(*work.at(0), submitInfo)) {
    logE("enqueueChain: appendCmds[%zu] failed\n", (size_t)0);
    return 1;
  }
  std::shared_ptr<ComputePipeline> child = chain;
  for (size_t i = 1; i < work.size(); i++) {
    if (!child) {
      logE("enqueueChain: work size %zu but only %zu chain links\n",
           work.size(), i - 1);
      return 1;
    }
    if (child->appendCmds(*work.at(i), submitInfo)) {
      logE("enqueueChain: appendCmds[%zu] failed\n", i);
      return 1;
    }
    child = child->chain;
  }

  // All blocks passed validation. Submit them.
  lock_guard_t lock(lockmutex);
  waitList.emplace_back(cpool);
  auto& w = waitList.back();
  if (!w.fence) {
    logE("enqueueBlocks: borrowFence failed\n");
    runBlocks.erase(ri, runBlocks.end());
    return 1;
  }
  // Add BlockVec work.at(i) to child->waitList
  child = chain;
  for (size_t i = 1; i < work.size(); i++) {
    if (!child) {
      logE("enqueueChain: work size %zu but only %zu chain links - step 2\n",
           work.size(), i - 1);
      return 1;
    }
    child->waitList.emplace_back(cpool);
    auto& childW = child->waitList.back();
    childW.reset();  // unborrow childW.fence and clear it.
    childW.parentFence = w.fence;
    childW.fenceBlocks.insert(childW.fenceBlocks.end(), work.at(i)->begin(),
                              work.at(i)->end());
    child = child->chain;
  }
  if (cpool.submit(lock, poolQindex, {submitInfo}, w.fence->vk)) {
    logE("enqueueBlocks: cpool.submit failed\n");
    waitList.pop_back();
    runBlocks.erase(ri, runBlocks.end());
    return 1;
  }
  w.fenceBlocks.insert(w.fenceBlocks.end(), work.at(0)->begin(),
                       work.at(0)->end());
  w.children = work.size() - 1;
  return 0;
}

int ComputePipeline::deleteBlocks(BlockVec& v) {
  lock_guard_t lock(lockmutex);  // Protect doneBlocks and freeBlocks
  for (size_t j = v.size();;) {
    j--;
    v.at(j)->flight.reset();
    bool found = false;
    for (auto di = doneBlocks.begin(); di != doneBlocks.end(); di++) {
      if (v.at(j) == *di) {
        if ((*di)->cmdBuf.reset() || (*di)->cmdBuf.beginSimultaneousUse()) {
          logE("deleteBlocks: reset or beginSimultaneousUse failed\n");
          return 1;
        }
        if ((*di)->cmdBufPost.beginSimultaneousUse()) {
          logE("deleteBlocks: beginSimultaneousUse failed\n");
          return 1;
        }
        // Move block from doneBlocks (di points to doneBlocks) to freeBlocks.
        freeBlocks.insert(freeBlocks.end(), *di);
        doneBlocks.erase(di);
        found = true;
        break;
      }
    }
    if (!found) {
      for (auto pi = prepBlocks.begin(); pi != prepBlocks.end(); pi++) {
        if (v.at(j) == *pi) {
          // Move block from prepBlocks (pi points to prepBlocks) to freeBlocks.
          freeBlocks.insert(freeBlocks.end(), *pi);
          prepBlocks.erase(pi);
          found = true;
          break;
        }
      }
    }
    if (!found) {
      logE("deleteBlocks: block[%zu] not found in doneBlocks.\n", j);
      return 1;
    }
    if (!j) {
      break;
    }
  }
  v.clear();
  return 0;
}

int ComputePipeline::poll() {
  lock_guard_t lock(lockmutex);  // Protect doneBlocks and waitList
  for (size_t i = 0; i < waitList.size();) {
    if (!waitList.at(i).fence) {
      if (waitList.at(i).parentFence) {
        continue;
      }
      logE("%s: waitList[%zu] fence null\n", "ComputePipeline::poll", i);
      return 1;
    }
    VkResult v = vkGetFenceStatus(cpool.vk.dev.dev, waitList.at(i).fence->vk);
    switch (v) {
      case VK_SUCCESS:
        if (retire(i)) {
          logE("retire(waitList[%zu]) failed\n", i);
          return 1;
        }
        break;
      case VK_NOT_READY:
        i++;
        break;
      default:
        return explainVkResult("ComputePipeline::poll", v);
    }
  }
  return 0;
}

int ComputePipeline::wait(uint64_t nanos, bool& timeout) {
  std::vector<VkFence> fences;
  {
    lock_guard_t lock(lockmutex);  // Protect prepBlocks and runBlocks
    fences.reserve(waitList.size());
    for (size_t i = 0; i < waitList.size(); i++) {
      if (!waitList.at(i).fence) {
        if (waitList.at(i).parentFence) {
          continue;
        }
        logE("%s: waitList[%zu] fence null\n", "ComputePipeline::wait", i);
        return 1;
      }
      fences.emplace_back(waitList.at(i).fence->vk);
    }
  }
  if (fences.empty()) {
    logE("ComputePipeline::wait: no fences\n");
    return 1;
  }
  VkResult v = vkWaitForFences(cpool.vk.dev.dev, fences.size(), fences.data(),
                               VK_FALSE /*waitAll*/, nanos);
  switch (v) {
    case VK_TIMEOUT:
      timeout = true;
      break;
    case VK_SUCCESS:
      timeout = false;
      break;
    default:
      return explainVkResult("ComputePipeline::wait", v);
  }
  return 0;
}

int ComputePipeline::retire(size_t i) {
  // move waitlist.at(i).fenceBlocks from runBlocks to doneBlocks
  auto& v = waitList.at(i).fenceBlocks;
  for (size_t j = 0; j < v.size(); j++) {
    for (auto ri = runBlocks.begin();;) {
      if (v.at(j) != *ri) {
        ri++;
        if (ri == doneBlocks.end()) {
          logE("waitList[%zu].fenceBlocks[%zu] not in runBlocks.\n", i, j);
          return 1;
        }
        continue;
      }

      // Move from runBlocks (ri points to runBlocks) to doneBlocks.
      doneBlocks.insert(doneBlocks.end(), *ri);
      runBlocks.erase(ri);
      break;
    }
  }

  if (waitList.at(i).children) {
    if (!waitList.at(i).fence || waitList.at(i).parentFence) {
      logE("retire(%zu): fence=%s parentFence=%s\n", i,
           waitList.at(i).fence ? "ok" : "NULL",
           waitList.at(i).parentFence ? "ok" : "NULL");
      return 1;
    }
    // Retire child pipelines' waitList
    std::shared_ptr<ComputePipeline> child = chain;
    for (size_t depth = 1; child; depth++) {
      bool found = false;
      for (size_t j = 0; j < child->waitList.size(); j++) {
        if (child->waitList.at(j).parentFence == waitList.at(i).fence) {
          found = true;
          if (child->retire(j)) {
            logE("retire(%zu): child waitList[%zu] retire failed\n", i, j);
            return 1;
          }
        }
      }
      if (!found) {
        logE("retire: child[%zu] had no matching parentFence\n", depth);
        return 1;
      }
      child = child->chain;
      if (!child && depth != waitList.at(i).children) {
        logE("retire(%zu): child[%zu] missing child, want %zu\n", i, depth,
             waitList.at(i).children);
        return 1;
      }
    }
  }
  waitList.erase(waitList.begin() + i);
  return 0;
}

}  // namespace science
