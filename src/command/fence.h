/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/command is the 3rd-level bindings for the Vulkan graphics library.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY! Just #include <src/command/command.h>
 */

namespace command {

// Semaphore represents a GPU-only synchronization operation vs. Fence, below.
// Semaphores can be waited on in any queue vs. Events which must be waited on
// within a single queue.
typedef struct Semaphore {
  Semaphore(language::Device& dev) : vk{dev, vkDestroySemaphore} {
    vk.allocator = dev.dev.allocator;
  }
  // Two-stage constructor: call ctorError() to build Semaphore.
  WARN_UNUSED_RESULT int ctorError();

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  // vk is the raw VkSemaphore.
  VkDebugPtr<VkSemaphore> vk;
} Semaphore;

// Fence represents a GPU-to-CPU synchronization. Fences are the only sync
// primitive which the CPU can wait on.
typedef struct Fence {
  Fence(language::Device& dev) : vk{dev, vkDestroyFence} {
    vk.allocator = dev.dev.allocator;
  }

  // Two-stage constructor: call ctorError() to build Fence.
  WARN_UNUSED_RESULT int ctorError();

  // reset resets the state of the fence to unsignaled.
  WARN_UNUSED_RESULT int reset();

  // waitNs waits for the state of the fence to become signaled by the Device.
  // The result MUST be checked for multiple possible success states.
  WARN_UNUSED_RESULT VkResult waitNs(uint64_t timeoutNanos);

  // waitMs waits for the state of the fence to become signaled by the Device.
  // The result MUST be checked for multiple possible success states.
  WARN_UNUSED_RESULT VkResult waitMs(uint64_t timeoutMillis) {
    return waitNs(timeoutMillis * 1000000llu);
  }

  // getStatus returns the status of the fence using vkGetFenceStatus.
  // The result MUST be checked for multiple possible success states.
  WARN_UNUSED_RESULT VkResult getStatus();

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  // vk is the raw VkFence.
  VkDebugPtr<VkFence> vk;
} Fence;

// Event represents a GPU-only synchronization operation, and must be waited on
// and set (signalled) within a single queue. Events can also be set (signalled)
// from the CPU.
typedef struct Event {
  Event(language::Device& dev) : vk{dev, vkDestroyEvent} {
    vk.allocator = dev.dev.allocator;
  }
  // Two-stage constructor: call ctorError() to build Event.
  WARN_UNUSED_RESULT int ctorError();

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  // vk is the raw VkEvent.
  VkDebugPtr<VkEvent> vk;
} Event;

// SemaphoreStageMaskPair helps SubmitInfo below be a little clearer - this is
// like a std::pair<VkSemaphore, VkPipelineStageFlags> but a little easier to
// read.
typedef struct SemaphoreStageMaskPair {
  explicit SemaphoreStageMaskPair(Semaphore& sem, VkPipelineStageFlags stage)
      : sem(sem.vk), dstStage(stage) {}
  explicit SemaphoreStageMaskPair(VkSemaphore sem, VkPipelineStageFlags stage)
      : sem(sem), dstStage(stage) {}

  VkSemaphore sem;
  VkPipelineStageFlags dstStage;
} SemaphoreStageMaskPair;

}  // namespace command
