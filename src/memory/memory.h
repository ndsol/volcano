/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/memory is the 4th-level bindings for the Vulkan graphics library.
 * src/memory is part of github.com/ndsol/volcano.
 * This library is called "memory" as a homage to Star Trek First Contact.
 * Like a Vulcan's Memory, this library remembers everything.
 *
 * This library can be broken into distinct categories:
 *
 * 1. DeviceMemory and MemoryRequirements
 *    These handle all memory allocations. Wraps VulkanMemoryAllocator.
 *
 * 2. Image and Buffer
 *
 * 3. Stage, DescriptorPool, DescriptorSetLayout and DescriptorSet
 *    Slightly more advanced use cases that build on Image and Buffer.
 *    (These are in stage.h to break things up a little - DO NOT #include THAT
 *    FILE, just #include <src/memory/memory.h> to get all of this.)
 */

#include <src/command/command.h>
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
#ifdef __ANDROID__
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DEDICATED_ALLOCATION 0
#endif /*__ANDROID__*/
#include <vendor/vulkanmemoryallocator/vk_mem_alloc.h>
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

#pragma once

namespace memory {

// ASSUME_POOL_QINDEX is used when the CommandPool queue must be assumed.
// (There are many use cases where a non-zero queue index is so uncommon,
// it is not supported. The use of this constant documents the assumption.)
constexpr size_t ASSUME_POOL_QINDEX = 0;
// ASSUME_PRESENT_QINDEX is used in science::PresentSemaphore to assume the
// queue index is zero. The use of this constant documents the assumption.
constexpr size_t ASSUME_PRESENT_QINDEX = 0;

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
// Define a simple substitute for struct VmaAllocation.
// This is only used if your app defines VOLCANO_DISABLE_VULKANMEMORYALLOCATOR.
typedef struct VmaAllocation {
  VmaAllocation(language::Device& dev) : vk{dev, vkFreeMemory} {}

  VkMemoryPropertyFlags requiredProps;
  VkDeviceSize allocSize{0};
  void* mapped{0};
  VkDebugPtr<VkDeviceMemory> vk;
} VmaAllocation;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

struct MemoryRequirements;

// DeviceMemory represents a raw chunk of bytes that can be accessed by the
// device. Because GPUs are in everything now, the memory may not be physically
// "on the device," but all that is hidden by the device driver to make it
// seem like it is.
//
// DeviceMemory is not very useful on its own. But alloc() can be fed a
// MemoryRequirements object with an in-place constructor, like this:
//   DeviceMemory mem(dev);
//   mem.alloc({dev, img});  // MemoryRequirements constructed from an Image.
//
//   DeviceMemory mem(dev);
//   mem.alloc({dev, buf});  // MemoryRequirements constructed from a Buffer.
//
// By using the overloaded constructors in MemoryRequirements,
// DeviceMemory::alloc() is kept simple.
typedef struct DeviceMemory {
  DeviceMemory(language::Device& dev)
      : dev(dev),
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
        vmaAlloc(0)
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
        vmaAlloc(dev)  // The simple substitute struct uses Device directly.
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  {
  }

  // Explicit move constructor because of lockmutex:
  DeviceMemory(DeviceMemory&& other)
      : dev(other.dev), vmaAlloc(std::move(other.vmaAlloc)) {
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (other.lockmutex.try_lock()) {
      other.vmaAlloc = 0;
      other.lockmutex.unlock();
    } else {
      logE("DeviceMemory(DeviceMemory&&): other.lockmutex is locked!");
      logE("DeviceMemory(DeviceMemory&&): thread collision!\n");
    }
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  }

  virtual ~DeviceMemory();

  // reset() calls vmaFreeMemory() or if not using vulkanmemoryallocator,
  // vkFreeMemory().
  void reset();

  // setName forwards the setName call.
  WARN_UNUSED_RESULT int setName(const std::string& name);
  // getName forwards the getName call.
  const std::string& getName();

  // alloc() calls vmaAllocateMemoryFor{Buffer,Image}()
  // (or vkAllocateMemory() if not using vulkanmemoryallocator).
  // returns non-zero on error.
  // NOTE: if you use Image, Buffer, etc. below, alloc() is automatically called
  // for you by Image::ctorError(), Buffer::ctorError(), etc.
  //
  // NOTE: if not using vulkanmemoryallocator, vmaAlloc.requiredProps *must* be
  // set before alloc() is called. This is automatically done for you by
  // Image::ctorError(), Buffer::ctorError(), etc.
  WARN_UNUSED_RESULT int alloc(MemoryRequirements req);

  // mmap() calls vkMapMemory() and returns non-zero on error.
  // NOTE: The vkMapMemory spec currently says "flags is reserved for future
  // use." You probably can ignore the flags parameter.
  //
  // NOTE: Vulkan only permits one mmap from a DeviceMemory instance. If you
  // call mmap() again without calling munmap() first, it will fail. See
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#vkMapMemory
  WARN_UNUSED_RESULT int mmap(void** pData, VkDeviceSize offset = 0,
                              VkDeviceSize size = VK_WHOLE_SIZE,
                              VkMemoryMapFlags flags = 0);

  // makeRange overwrites range and constructs it to point to this DeviceMemory
  // block, with the given offset and size.
  void makeRange(VkMappedMemoryRange& range,
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
                 VkDeviceSize offset, VkDeviceSize size
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
                 const VmaAllocationInfo& info
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  );

  // flush tells the Vulkan driver to flush any CPU writes that may still be
  // pending in CPU caches; flush makes all CPU writes visible to the device.
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT generally means that calling flush
  // is not neeed.
  // NOTE: input VkMappedMemoryRange's get their .memory updated to this->vk.
  WARN_UNUSED_RESULT int flush(
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
      std::vector<VkMappedMemoryRange> mem
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  );

  // invalidate tells the Vulkan driver to flush any device writes so that they
  // are visible to the CPU.
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT generally means that calling
  // invalidate is not neeed.
  WARN_UNUSED_RESULT int invalidate(
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
      const std::vector<VkMappedMemoryRange>& mem
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  );

  // munmap() calls vkUnmapMemory().
  void munmap();

  // dev holds a reference to the device where this memory is located.
  language::Device& dev;
  // vmaAlloc is an internal struct if VOLCANO_DISABLE_VULKANMEMORYALLOCATOR:
  VmaAllocation vmaAlloc;
  // isImage is non-zero when the allocation is for an image.
  int isImage{0};

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // getAllocInfo is a convenient wrapper around vmaGetAllocationInfo
  // WARNING: calling vmaGetAllocationInfo directly must be synchronized
  // (consider synchronizing on lockmutex below like getAllocInfo() does).
  WARN_UNUSED_RESULT int getAllocInfo(VmaAllocationInfo& info);

  // lock_guard_t: like c++17's constructor type inference, but we are in c++11
  // Instead of: std::lock_guard<std::recursive_mutex> lock(mem.lockmutex);
  // Do this:    DeviceMemory::lock_guard_t            lock(mem.lockmutex);
  // (Then uses of lock_guard_t do not assume lockmutex is a recursive_mutex.)
  typedef std::lock_guard<std::recursive_mutex> lock_guard_t;
  // unique_lock_t: like c++17's constructor type inference, but in c++11
  // Instead of: std::unique_lock<std::recursive_mutex> lock(mem.lockmutex);
  // Do this:    DeviceMemory::unique_lock_t            lock(mem.lockmutex);
  // (Then uses of unique_lock_t do not assume lockmutex is a recursive_mutex.)
  typedef std::unique_lock<std::recursive_mutex> unique_lock_t;

  // lockmutex is used internally when vulkanmemoryallocator requires a mutex.
  std::recursive_mutex lockmutex;

 protected:
  // name is used to store the name until alloc(), after which the name is
  // copied to vmaAlloc using VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT.
  std::string name;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
} DeviceMemory;

// Image represents a VkImage.
typedef struct Image {
  Image(language::Device& dev) : vk{dev, vkDestroyImage}, mem(dev) {
    vk.allocator = dev.dev.allocator;
    VkOverwrite(info);
    info.imageType = VK_IMAGE_TYPE_2D;
    // You must set info.extent.width, info.extent.height, and
    // info.extent.depth. For a 2D image, set depth = 1. For a 1D image,
    // set height = 1 and depth = 1.
    info.mipLevels = 1;
    info.arrayLayers = 1;
    // You must set info.format.
    // You probably want tiling = VK_IMAGE_TILING_OPTIMAL most of the time:
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    // You must set info.usage.
    // You must set vmaUsage.
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // ctorError() sets currentLayout = info.initialLayout.
    currentLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  }
  Image(Image&&) = default;
  Image(const Image&) = delete;

  // ctorError must be called after filling in this->info to construct the
  // Image. Note that bindMemory() should be called after ctorError().
  //
  // The Vulkan spec says vkGetImageSubresourceLayout is only valid for
  // info.tiling == VK_IMAGE_TILING_LINEAR, and is invariant after ctorError().
  // It is automatically queried in the `colorMem` vector and associates.
  //
  // Your application may not need to call ctorError directly.
  // Sampler::ctorError(), below, automatically sets up an image for a shader
  // sampler, and science::Pipeline() automatically sets up an image for a depth
  // buffer.
  WARN_UNUSED_RESULT int ctorError(VkMemoryPropertyFlags props);

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // ctorError must be called after filling in this->info to construct the
  // Image. Note that bindMemory() should be alled after ctorError(). This is
  // a more convenient form of ctorError that uses VmaMemoryUsage.
  WARN_UNUSED_RESULT int ctorError(VmaMemoryUsage usage);
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  // bindMemory() calls vmaBindImageMemory which binds this->mem
  // NOTE: do not call bindMemory() until after calling ctorError().
  //
  // If VOLCANO_DISABLE_VULKANMEMORYALLOCATOR is defined:
  // bindMemory() calls vkBindImageMemory which binds this->mem
  // or automatically upgrades to vkBindImageMemory2 if supported.
  // offset is an optional parameter to sub-allocate within an existing block.
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
#define bindMemoryArgs VkDeviceSize offset = 0
#define addBindMemoryArgs , VkDeviceSize offset = 0
#define passBindMemoryArgs offset
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
#define bindMemoryArgs
#define addBindMemoryArgs
#define passBindMemoryArgs
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  WARN_UNUSED_RESULT int bindMemory(bindMemoryArgs);

  // ctorAndBindDeviceLocal() calls ctorError() to set up device local memory
  // and then immediate calls bindMemory().
  WARN_UNUSED_RESULT int ctorAndBindDeviceLocal(bindMemoryArgs) {
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (vmaUsage == VMA_MEMORY_USAGE_UNKNOWN) {
      vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    }
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
    return ctorError(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
           bindMemory(passBindMemoryArgs);
  }

  // ctorAndBindHostVisible() is for linear, host visible images.
  //
  // WARNING: Linear, host visible images have significant limits:
  //  * Only 1 mip level (no mipmaps)
  //  * Only 1 array layer (no image arrays)
  //  * Very minimal supported formats (no compressed formats, minimal texture
  //    formats supported)
  //  * macOS moltenVK cannot actually mmap the image via Metal. Instead it
  //    hides a copy operation in the munmap() call to emulate it.
  //    That munmap() may also munmap random other buffers in your app!
  //    https://github.com/KhronosGroup/MoltenVK/issues/175
  //
  // A Buffer really does work just like a linear, host visible image. Buffers
  // are linear by definition.
  WARN_UNUSED_RESULT int ctorAndBindHostVisible(bindMemoryArgs) {
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage |=
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.usage &= ~(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (vmaUsage == VMA_MEMORY_USAGE_UNKNOWN) {
      vmaUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ||
           bindMemory(passBindMemoryArgs);
  }

  // ctorAndBindHostCoherent() is for linear, host coherent images.
  //
  // NOTE: You probably should look at using buffers instead, which
  // support loading compressed, tiled image formats directly.
  WARN_UNUSED_RESULT int ctorAndBindHostCoherent(bindMemoryArgs) {
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage |=
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.usage &= ~(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (vmaUsage == VMA_MEMORY_USAGE_UNKNOWN) {
      vmaUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    }
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
           bindMemory(passBindMemoryArgs);
  }

  // reset() releases this and this->mem. This will try to munmap(), but it is
  // still preferred that your app munmap anything first.
  WARN_UNUSED_RESULT int reset();

  // setMipLevelsFromExtent is a convenience method that calculates
  // info.mipLevels from info.extent.width and info.extent.height.
  WARN_UNUSED_RESULT int setMipLevelsFromExtent();

  // makeTransition() makes a VkImageMemoryBarrier for commandBuffer::barrier()
  // Your app can use commandBuffer::barrier(), which will call this for you.
  VkImageMemoryBarrier makeTransition(VkImageLayout newLayout);

  // getAllAspects computes VkImageAspectFlags purely as a function of
  // info.format.
  VkImageAspectFlags getAllAspects() const;

  // getSubresource is a convenience to get a VkImageSubresource.
  // NOTE: getSubresource is purely a function of info.format, so set it first.
  VkImageSubresource getSubresource(uint32_t mipLevel,
                                    uint32_t arrayLayer = 0) const {
    VkImageSubresource r;
    r.aspectMask = getAllAspects();
    r.mipLevel = mipLevel;
    r.arrayLayer = arrayLayer;
    return r;
  }

  // getSubresourceRange is a convenience to get a VkImageSubresourceRange.
  // NOTE: getSubresourceRange is purely a function of info.format,
  // info.mipLevels, and info.arrayLayers. They must have valid values first.
  VkImageSubresourceRange getSubresourceRange() const {
    VkImageSubresourceRange r;
    r.aspectMask = getAllAspects();
    r.baseMipLevel = 0;
    r.levelCount = info.mipLevels;
    r.baseArrayLayer = 0;
    r.layerCount = info.arrayLayers;
    return r;
  }

  // getSubresourceLayers is a convenience to get a VkImageSubresourceLayers.
  // NOTE: getSubresourceLayers is purely a function of info.format and
  // info.arrayLayers. They must have valid values first.
  VkImageSubresourceLayers getSubresourceLayers(uint32_t mipLevel) const {
    VkImageSubresourceLayers r;
    r.aspectMask = getAllAspects();
    r.mipLevel = mipLevel;
    r.baseArrayLayer = 0;
    r.layerCount = info.arrayLayers;
    return r;
  }

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  VkImageCreateInfo info;
  VkImageLayout currentLayout;
  VkDebugPtr<VkImage> vk;  // populated after ctorError().
  DeviceMemory mem;        // ctorError() calls mem.alloc() for you.
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  VmaMemoryUsage vmaUsage{VMA_MEMORY_USAGE_UNKNOWN};
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */

  // colorMem is populated by ctorError() if layout is LINEAR and the format
  // includes color channels.
  std::vector<VkSubresourceLayout> colorMem;
  // depthMem is populated by ctorError() if layout is LINEAR and the format
  // includes a depth channel.
  std::vector<VkSubresourceLayout> depthMem;
  // stencilMem is populated by ctorError() if layout is LINEAR and the format
  // includes a stencil channel.
  std::vector<VkSubresourceLayout> stencilMem;

 protected:
  int makeTransitionAccessMasks(VkImageMemoryBarrier& imageB);
  int validateImageCreateInfo();
  int getSubresourceLayouts();
} Image;

// Buffer represents a VkBuffer.
typedef struct Buffer {
  Buffer(language::Device& dev) : vk{dev, vkDestroyBuffer}, mem(dev) {
    vk.allocator = dev.dev.allocator;
    VkOverwrite(info);
    // You must set info.size.
    // You must set info.usage.
    // You must set vmaUsage.
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  Buffer(Buffer&&) = default;
  Buffer(const Buffer&) = delete;

  // ctorError() must be called after filling in this->info to construct the
  // Buffer. Note that bindMemory() should be called after ctorError().
  //
  // Some aliases of ctorError() are defined below, which may make your
  // application less verbose. These are not all the possible combinations.
  //
  // If queueFams is empty, info.sharingMode can be anything. But if queueFams
  // is set, it holds the queue families that share this buffer and
  // info.sharingMode is overwritten with VK_SHARING_MODE_CONCURRENT.
  WARN_UNUSED_RESULT int ctorError(
      VkMemoryPropertyFlags props,
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>());

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // ctorError must be called after filling in this->info to construct the
  // Buffer. Note that bindMemory() should be alled after ctorError(). This is
  // a more convenient form of ctorError that uses VmaMemoryUsage.
  WARN_UNUSED_RESULT int ctorError(
      VmaMemoryUsage usage,
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>());
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  // bindMemory() calls vkBindBufferMemory which binds this->mem.
  // NOTE: do not call bindMemory() until a point after ctorError().
  WARN_UNUSED_RESULT int bindMemory(bindMemoryArgs);

  // ctorAndBindDeviceLocal() calls ctorError() to set up device local memory
  // and then immediate calls bindMemory().
  //
  // ctorAndBindDeviceLocal() adds TRANSFER_DST to usage, but you should set
  // its primary uses (for example, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
  // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  // or all three).
  WARN_UNUSED_RESULT int ctorAndBindDeviceLocal(
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>()
          addBindMemoryArgs) {
    info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (vmaUsage == VMA_MEMORY_USAGE_UNKNOWN) {
      vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    }
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
    return ctorError(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, queueFams) ||
           bindMemory(passBindMemoryArgs);
  }

  // ctorAndBindHostVisible() calls ctorError to set up host visible memory
  // and then immediate calls bindMemory().
  WARN_UNUSED_RESULT int ctorAndBindHostVisible(
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>()
          addBindMemoryArgs) {
    info.usage |=
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (vmaUsage == VMA_MEMORY_USAGE_UNKNOWN) {
      vmaUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    }
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, queueFams) ||
           bindMemory(passBindMemoryArgs);
  }

  // ctorAndBindHostCoherent() calls ctorError to set up host coherent memory
  // and then immediate calls bindMemory().
  WARN_UNUSED_RESULT int ctorAndBindHostCoherent(
      const std::vector<uint32_t>& queueFams = std::vector<uint32_t>()
          addBindMemoryArgs) {
    info.usage |=
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    if (vmaUsage == VMA_MEMORY_USAGE_UNKNOWN) {
      vmaUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    }
#endif /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
    return ctorError(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     queueFams) ||
           bindMemory(passBindMemoryArgs);
  }
#undef bindMemoryArgs
#undef addBindMemoryArgs
#undef passBindMemoryArgs

  // reset() releases this and this->mem. This will try to munmap(), but it is
  // still preferred that your app munmap anything first.
  WARN_UNUSED_RESULT int reset();

  // copy copies all the contents of Buffer src using cmdBuffer, and does not
  // wait for the copy to complete. For even more precise control, see
  // command::CommandBuffer::copyBuffer().
  WARN_UNUSED_RESULT int copy(command::CommandBuffer& cmdBuffer, Buffer& src,
                              VkDeviceSize dstOffset = 0) {
    if (dstOffset + src.info.size > info.size) {
      logE("Buffer::copy(dstOffset=0x%llx,src.info.size=0x%llx): size=0x%llx\n",
           (unsigned long long)dstOffset, (unsigned long long)src.info.size,
           (unsigned long long)info.size);
      return 1;
    }

    VkBufferCopy region = {};
    region.dstOffset = dstOffset;
    region.size = src.info.size;
    return cmdBuffer.copyBuffer(src.vk, vk, std::vector<VkBufferCopy>{region});
  }

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  VkBufferCreateInfo info;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  VmaMemoryUsage vmaUsage{VMA_MEMORY_USAGE_UNKNOWN};
#endif                      /* VOLCANO_DISABLE_VULKANMEMORYALLOCATOR */
  VkDebugPtr<VkBuffer> vk;  // populated after ctorError().
  DeviceMemory mem;         // ctorError() calls mem.alloc() for you.

 protected:
  int validateBufferCreateInfo(const std::vector<uint32_t>& queueFams);
} Buffer;

// MemoryRequirements automatically gets the VkMemoryRequirements from
// the Device, and has helper methods for finding the VkMemoryAllocateInfo.
typedef struct MemoryRequirements {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  // Automatically get MemoryRequirements of a VkImage.
  MemoryRequirements(language::Device& dev, VkImage img) : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(VkImage) failed\n");
    }
  }
  // Automatically get MemoryRequirements of an Image.
  MemoryRequirements(language::Device& dev, Image& img) : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(Image) failed\n");
    }
  }
  // Automatically get MemoryRequirements of a VkBuffer.
  MemoryRequirements(language::Device& dev, VkBuffer buf) : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(VkBuffer) failed\n");
    }
  }
  // Automatically get MemoryRequirements of a Buffer.
  MemoryRequirements(language::Device& dev, Buffer& buf) : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(Buffer) failed\n");
    }
  }
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  // Automatically get MemoryRequirements of a VkImage.
  MemoryRequirements(language::Device& dev, VkImage img, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(VkImage) failed\n");
    }
    info.usage = usage;
  }
  // Automatically get MemoryRequirements of an Image.
  MemoryRequirements(language::Device& dev, Image& img, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(img)) {
      logF("MemoryRequirements ctor: get(Image) failed\n");
    }
    info.usage = usage;
  }
  // Automatically get MemoryRequirements of a VkBuffer.
  MemoryRequirements(language::Device& dev, VkBuffer buf, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(VkBuffer) failed\n");
    }
    info.usage = usage;
  }
  // Automatically get MemoryRequirements of a Buffer.
  MemoryRequirements(language::Device& dev, Buffer& buf, VmaMemoryUsage usage)
      : dev(dev) {
    if (get(buf)) {
      logF("MemoryRequirements ctor: get(Buffer) failed\n");
    }
    info.usage = usage;
  }
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  // reset clears any previous requirements.
  void reset() {
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    VkOverwrite(vk);
    VkOverwrite(dedicated);
    VkOverwrite(vkalloc);
    vk.pNext = &dedicated;
#else  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
    vkbuf = VK_NULL_HANDLE;
    vkimg = VK_NULL_HANDLE;
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  };

  // get populates VkMemoryRequirements2 vk from VkImage img.
  // If aspect is not 0 (0 is an invalid aspect), then img *must* be a planar
  // format and *must* have been created with VK_IMAGE_CREATE_DISJOINT_BIT.
  WARN_UNUSED_RESULT int get(VkImage img, VkImageAspectFlagBits optionalAspect =
                                              (VkImageAspectFlagBits)0);

  // get populates VkMemoryRequirements2 vk from memory::Image img.
  WARN_UNUSED_RESULT int get(Image& img, VkImageAspectFlagBits optionalAspect =
                                             (VkImageAspectFlagBits)0);

  // get populates VkMemoryRequirements2 vk from VkBuffer buf.
  WARN_UNUSED_RESULT int get(VkBuffer buf);

  // get populates VkMemoryRequirements2 vk from memory::Buffer img.
  WARN_UNUSED_RESULT int get(Buffer& buf) { return get(buf.vk); }

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
 protected:
  friend struct DeviceMemory;

  // findVkalloc() populates vkalloc using the requirements in vk and the flags
  // specified in props. If an error occurs, findVkalloc returns 1.
  int findVkalloc(VkMemoryPropertyFlags props);

 private:
  // indexOf() returns -1 if the props cannot be found.
  int indexOf(VkMemoryPropertyFlags props) const;

 public:
  VkMemoryRequirements2 vk;
  VkMemoryDedicatedRequirements dedicated;
  VkMemoryAllocateInfo vkalloc;
  int isImage{0};
  // TODO: VkMemoryDedicatedAllocateInfo in vkalloc.pNext.
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  // vkbuf and vkimg cannot both be non-NULL.
  VkBuffer vkbuf;
  // vkimg and vkbuf cannot both be non-NULL.
  VkImage vkimg;
  // info is initialized after get, and your app should then fill in
  // info.usage and optionally info.flags. Or, leave info.usage unset and
  // set info.requiredFlags.
  VmaAllocationCreateInfo info;

#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

  // dev holds a reference to the device where the memory would be located.
  language::Device& dev;
} MemoryRequirements;

}  // namespace memory

// Some classes are split out of memory.h just to break things up a little:
#include "stage.h"
