/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * This contains the implementation of the Stage class.
 */
#include "memory.h"

namespace memory {

Flight::Flight(Stage& stage)
    : command::CommandBuffer(stage.pool), stage(stage) {}

Flight::~Flight() {
  // Tell Stage this Flight has been destroyed.
  stage.release(*this);
}

int Stage::ctorError() {
  // If ctorError has already run, just return fast.
  if (!sources.empty() && sources.at(0).vk) {
    return 0;
  }
  if (sources.size() < 2) {
    logE("Stage::ctorError: Stage::sources starts with 2, cannot be %zu\n",
         sources.size());
    return 1;
  }

  {
    size_t wantSources = sources.size();
    sources.clear();
    // Make dummyPass only to throw it away in 2 lines.
    command::RenderPass dummyPass{pool.vk.dev};
    pool.reallocCmdBufs(sources, wantSources, dummyPass, 0 /*is_secondary*/);
  }
  for (size_t i = 0; i < sources.size(); i++) {
    auto& s = sources.at(i);
    s.buf.info.size = mmapMaxSize;
    s.buf.info.usage |=
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (s.buf.ctorAndBindHostCoherent() || s.buf.mem.mmap(&s.mmap)) {
      logE("Stage::ctorError: buf[%zu].ctorError or mmap failed\n", i);
      return 1;
    }
  }
  return 0;
}

int Stage::alloc() {
  for (size_t i = 0; i < sources.size(); i++) {
    int found = nextSource;
    // Update nextSource.
    nextSource++;
    if (nextSource >= sources.size()) {
      nextSource = 0;
    }
    if (!sources.at(found).isUsed) {
      sources.at(found).isUsed = true;
      return found;
    }
  }
  return -1;
}

int Stage::mmap(Buffer& dst, VkDeviceSize offset, VkDeviceSize bytes,
                std::shared_ptr<Flight>& f) {
  if (ctorError()) {
    logE("Stage::mmap(%p, %llu, %llu): ctorError failed\n", dst.vk.printf(),
         (unsigned long long)offset, (unsigned long long)bytes);
    return 1;
  }
  if (bytes > mmapMaxSize) {
    logE("Stage::mmap(%p, %llu, %llu): bytes too big: mmapMax() = %llu\n",
         dst.vk.printf(), (unsigned long long)offset, (unsigned long long)bytes,
         (unsigned long long)mmapMaxSize);
    return 1;
  }
  if (f) {
    logE("Stage::mmap(%p, %llu, %llu): flight left over from previous?\n",
         dst.vk.printf(), (unsigned long long)offset,
         (unsigned long long)bytes);
    return 1;
  }
  if (bytes == 0) {
    f = dummyFlight;
    return 0;
  }

  int r;
  {
    command::CommandPool::lock_guard_t lock(pool.lockmutex);
    r = alloc();
    if (r == -1) {
      logE("Stage::mmap(%p, %llu, %llu): Out of available Operations\n",
           dst.vk.printf(), (unsigned long long)offset,
           (unsigned long long)bytes);
      return 1;
    }
  }
  auto& s = sources.at(r);
  f = std::make_shared<Flight>(*this);
  f->vk = s.vk;
  f->mmap_ = s.mmap;
  f->source_ = static_cast<size_t>(r);
  f->buf_ = &dst;
  f->offset_ = offset;
  f->size_ = bytes;
  f->canSubmit_ = true;
  f->hostMap_ = true;
  f->deviceMap_ = true;

  if (f->reset() || f->beginSimultaneousUse()) {
    logE("Stage::mmap: reset or beginSimultaneousUse failed\n");
    return 1;
  }
  return 0;
}

int Stage::mmap(Image& img, VkDeviceSize bytes, std::shared_ptr<Flight>& f) {
  if (ctorError()) {
    logE("Stage::mmap(%p, %llu): ctorError failed\n", img.vk.printf(),
         (unsigned long long)bytes);
    return 1;
  }
  if (bytes > mmapMaxSize) {
    logE("Stage::mmap(%p, %llu): bytes too big: mmapMax() = %llu\n",
         img.vk.printf(), (unsigned long long)bytes,
         (unsigned long long)mmapMaxSize);
    return 1;
  }
  if (f) {
    logE("Stage::mmap(%p, %llu): flight left over from previous?\n",
         img.vk.printf(), (unsigned long long)bytes);
    return 1;
  }
  if (bytes == 0) {
    dummyImgFlight->img_ = &img;
    f = dummyImgFlight;
    return 0;
  }

  int r;
  {
    command::CommandPool::lock_guard_t lock(pool.lockmutex);
    r = alloc();
    if (r == -1) {
      logE("Stage::mmap(%p, %llu): Out of available Operations\n",
           img.vk.printf(), (unsigned long long)bytes);
      return 1;
    }
  }
  auto& s = sources.at(r);
  f = std::make_shared<Flight>(*this);
  f->vk = s.vk;
  f->img_ = &img;
  f->mmap_ = s.mmap;
  f->source_ = static_cast<size_t>(r);
  f->offset_ = 0;
  f->size_ = bytes;
  f->canSubmit_ = true;
  f->hostMap_ = true;
  f->deviceMap_ = true;

  if (f->beginSimultaneousUse()) {
    logE("Stage::mmap: beginSimultaneousUse failed\n");
    return 1;
  }
  return 0;
}

int Stage::read(Buffer& src, VkDeviceSize offset, VkDeviceSize bytes,
                std::shared_ptr<Flight>& f) {
  if (ctorError()) {
    logE("Stage::read(%p, %llu, %llu): ctorError failed\n", src.vk.printf(),
         (unsigned long long)offset, (unsigned long long)bytes);
    return 1;
  }
  if (bytes > mmapMaxSize) {
    logE("Stage::read(%p, %llu, %llu): bytes too big: mmapMax() = %llu\n",
         src.vk.printf(), (unsigned long long)offset, (unsigned long long)bytes,
         (unsigned long long)mmapMaxSize);
    return 1;
  }
  if (f) {
    logE("Stage::read(%p, %llu, %llu): flight left over from previous?\n",
         src.vk.printf(), (unsigned long long)offset,
         (unsigned long long)bytes);
    return 1;
  }
  if (bytes == 0) {
    f = dummyFlight;
    return 0;
  }

  int r;
  {
    command::CommandPool::lock_guard_t lock(pool.lockmutex);
    r = alloc();
    if (r == -1) {
      logE("Stage::read(%p, %llu, %llu): Out of available Operations\n",
           src.vk.printf(), (unsigned long long)offset,
           (unsigned long long)bytes);
      return 1;
    }
  }
  auto& s = sources.at(r);
  f = std::make_shared<Flight>(*this);
  f->vk = s.vk;
  f->mmap_ = s.mmap;
  f->source_ = static_cast<size_t>(r);
  f->buf_ = &src;
  f->offset_ = offset;
  f->size_ = bytes;
  f->canSubmit_ = true;
  f->hostMap_ = false;
  f->deviceMap_ = true;

  if (f->beginSimultaneousUse()) {
    logE("Stage::mmap: beginSimultaneousUse failed\n");
    return 1;
  }
  return 0;
}

int Stage::read(Image& src, VkDeviceSize bytes, std::shared_ptr<Flight>& f) {
  if (ctorError()) {
    logE("Stage::read(%p, %llu): ctorError failed\n", src.vk.printf(),
         (unsigned long long)bytes);
    return 1;
  }
  if (bytes > mmapMaxSize) {
    logE("Stage::read(%p, %llu): bytes too big: mmapMax() = %llu\n",
         src.vk.printf(), (unsigned long long)bytes,
         (unsigned long long)mmapMaxSize);
    return 1;
  }
  if (f) {
    logE("Stage::read(%p, %llu): flight left over from previous?\n",
         src.vk.printf(), (unsigned long long)bytes);
    return 1;
  }
  if (bytes == 0) {
    dummyImgFlight->img_ = &src;
    f = dummyImgFlight;
    return 0;
  }

  int r;
  {
    command::CommandPool::lock_guard_t lock(pool.lockmutex);
    r = alloc();
    if (r == -1) {
      logE("Stage::read(%p, %llu): Out of available Operations\n",
           src.vk.printf(), (unsigned long long)bytes);
      return 1;
    }
  }
  auto& s = sources.at(r);
  f = std::make_shared<Flight>(*this);
  f->vk = s.vk;
  f->img_ = &src;
  f->mmap_ = s.mmap;
  f->source_ = static_cast<size_t>(r);
  f->offset_ = 0;
  f->size_ = bytes;
  f->canSubmit_ = true;
  f->hostMap_ = false;
  f->deviceMap_ = true;

  if (f->beginSimultaneousUse()) {
    logE("Stage::mmap: beginSimultaneousUse failed\n");
    return 1;
  }
  return 0;
}

int Stage::flushButNotSubmit(std::shared_ptr<Flight> f) {
  if (!f->canSubmit()) {
    return 0;
  }
  if (!f->deviceMap_) {
    logE("%sBUG: already released\n", "Stage::flushButNotSubmit: ");
    return 1;
  }
  if (f->hostMap_) {
    if (!f->isImage()) {
      // Flush a write to a buffer.
      VkBufferCopy region = {};
      region.dstOffset = f->offset_;
      region.size = f->size_;
      if (f->copyBuffer(sources.at(f->source_).buf.vk, f->buf_->vk, {region})) {
        logE("%scopyBuffer(w) failed\n", "Stage::flushButNotSubmit: ");
        return 1;
      }
    } else {
      // Flush a write to an Image.
      Image& img = *f->img_;
      if (!img.vk) {
        logE("%sImage::ctorError must be called before flush\n",
             "Stage::flushButNotSubmit: ");
        return 1;
      }
      if (img.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
          f->barrier(img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
        logE("%sbarrier(TRANSFER_DST_OPTIMAL) failed\n",
             "Stage::flushButNotSubmit: ");
        return 1;
      }
      if (f->copyBufferToImage(sources.at(f->source_).buf.vk, img.vk,
                               img.currentLayout, f->copies)) {
        logE("%scopyBufferToImage(w) failed\n", "Stage::flushButNotSubmit: ");
        return 1;
      }
    }
    f->hostMap_ = false;
  } else {
    if (!f->isImage()) {
      // Flush a read from a buffer.
      VkBufferCopy region = {};
      region.dstOffset = f->offset_;
      region.size = f->size_;
      if (f->copyBuffer(f->buf_->vk, sources.at(f->source_).buf.vk, {region})) {
        logE("%scopyBuffer(r) failed\n", "Stage::flushButNotSubmit: ");
        return 1;
      }
    } else {
      // Flush a read from an Image.
      Image& img = *f->img_;
      if (!img.vk) {
        logE("%sImage::ctorError must be called before flush\n",
             "Stage::flushButNotSubmit: ");
        return 1;
      }
      if (img.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
          f->barrier(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)) {
        logE("%sbarrier(TRANSFER_DST_OPTIMAL) failed\n",
             "Stage::flushButNotSubmit: ");
        return 1;
      }
      if (f->copyImageToBuffer(img.vk, img.currentLayout,
                               sources.at(f->source_).buf.vk, f->copies)) {
        logE("%scopyImageToBuffer(r) failed\n", "Stage::flushButNotSubmit: ");
        return 1;
      }
    }
    f->hostMap_ = true;
  }
  return 0;
}

int Stage::flush(std::shared_ptr<Flight> f, command::Fence& fence,
                 bool& waitForFence) {
  if (flushButNotSubmit(f)) {
    waitForFence = false;
    logE("Stage::flush: inner call to flushButNotSubmit failed\n");
    return 1;
  }
  waitForFence = f->canSubmit_;
  if (f->canSubmit_) {
    command::CommandPool::lock_guard_t lock(pool.lockmutex);
    if (f->end() || pool.submit(lock, poolQindex, *f, fence.vk)) {
      logE("Stage::flush: end or submit failed\n");
      return 1;
    }
  }
  return 0;
}

void Stage::release(Flight& f) {
  if (&f == &*dummyFlight || &f == &*dummyImgFlight) {
    return;
  }
  if (!f.deviceMap_) {
    logE("Stage::release: BUG: not mapped?\n");
  }
  command::CommandPool::lock_guard_t lock(pool.lockmutex);
  if (sources.size() <= f.source_) {
    logE("Stage::release: BUG: flight refers to source[%zu] of %zu\n",
         f.source_, sources.size());
    return;
  }
  sources.at(f.source_).isUsed = false;
}

}  // namespace memory
