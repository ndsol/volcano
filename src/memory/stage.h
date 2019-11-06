/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/memory is the 4th-level bindings for the Vulkan graphics library.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY! Just #include <src/memory/memory.h>
 */

// Comparator for VkDescriptorPoolSize enables std::map DescriptorPoolSizes
// below.
constexpr bool operator<(const VkDescriptorPoolSize& a,
                         const VkDescriptorPoolSize& b) {
  return (a.type < b.type) ||
         (a.type == b.type && a.descriptorCount < b.descriptorCount);
}

namespace memory {

// Forward declaration of Stage for Flight.
struct Stage;

// Flight represents an in-flight transfer. This does not include any
// command::Fence or other sync primitive - see Stage below for how to sync.
//
// Flight in a subclass of CommandBuffer. This allows your app to add commands
// just like a normal CommandBuffer before calling Stage::flush() or
// Stage::flushButNotSubmit().
struct Flight : public command::CommandBuffer {
  explicit Flight(Stage& stage);

  // ~Flight calls stage.release()
  virtual ~Flight();

  // canSubmit() returns whether your app must call Stage::flush() or
  // Stage::flushButNotSubmit().
  bool canSubmit() const { return canSubmit_; }

  // isImage() returns whether the target is an Image.
  bool isImage() const { return img_ != nullptr; }

  // mmap() returns a pointer to CPU-visible memory to read or write data.
  void* mmap() const { return hostMap_ ? mmap_ : nullptr; }

  VkDeviceSize offset() const { return offset_; }
  VkDeviceSize size() const { return size_; }

  // copies is ignored unless this is a copy-to-Image transfer
  // (isImage() == true). Your app adds elements to copies to copy the bytes
  // from buf to Image* img.
  std::vector<VkBufferImageCopy> copies;

 protected:
  friend struct Stage;
  Stage& stage;
  // buf_ is non-NULL if this is a copy-to-buffer operation.
  Buffer* buf_{nullptr};
  // img_ is non-NULL if this is a copy-to-image operation.
  Image* img_{nullptr};
  // mmap_ is the CPU-visible memory-mapped pointer.
  void* mmap_{nullptr};
  // source_ is the index into Stage::sources to flush this Flight.
  size_t source_{0};
  // offset_ is the value passed to mmap to create this Flight.
  VkDeviceSize offset_{0};
  // size_ is the value passed to mmap to create this Flight.
  VkDeviceSize size_{0};
  // canSubmit_ is true if flushButNotSubmit() must be called.
  bool canSubmit_{false};
  // hostMap_ tracks whether the void* mmap_ is valid or not.
  bool hostMap_{false};
  // deviceMap_ tracks whether this Flight can be recycled yet.
  bool deviceMap_{false};
};

// Stage manages transferring bytes to and from host-visible memory.
// Stage is for:
// * Creating command buffers and staging buffers to ping-pong data to the GPU
// * Scheduling transfers to/from the GPU
// * Determining the optimal memory type for a staging buffer
// * TODO: Direct to host-visible GPU device-local memory.
//
// Stage has a ctorError() method but it is protected; first-time setup is done
// lazily and your app does not need to call it. If your app knows it will not
// need Stage any more and wants to free all the resources, use a shared_ptr or
// similar method of calling delete on the Stage object.
typedef struct Stage {
  Stage(command::CommandPool& pool, size_t poolQindex)
      : pool(pool),
        poolQindex(poolQindex),
        mmapMaxSize(2 * 1024 * 1024),
        dummyFlight(std::make_shared<Flight>(*this)),
        dummyImgFlight(std::make_shared<Flight>(*this)) {
    // Set default number of sources.
    while (sources.size() < 2) {
      sources.emplace_back(pool);
    }
  }

  ~Stage() {
    // only needed for debugging
    bool bug = false;
    for (size_t i = 0; i < sources.size(); i++) {
      if (sources.at(i).isUsed) {
        logE("Stage: sources[%zu].isUsed = true in ~Stage.\n", i);
        bug = true;
      }
    }
    if (bug) {
      // Always call f.reset() for any std::shared_ptr<Flight> f!
      // Or this may be your destructor order is wrong.
      logF("Stage: BUG: destroying Stage which Flight still references\n");
    }
  }

  // pool has both the CommandPool and the VkQueue to use.
  command::CommandPool& pool;
  // poolQindex identifies which VkQueue to use, namely, pool.q(poolQindex).
  const size_t poolQindex;

  // FlightSource is the resources used by a Flight.
  struct FlightSource {
    explicit FlightSource(command::CommandPool& pool) : buf{pool.vk.dev} {}
    Buffer buf;
    VkCommandBuffer vk{VK_NULL_HANDLE};
    void* mmap{nullptr};
    // isUsed is set to true when allocated.
    bool isUsed{false};
  };

  // sources holds the resources Stage has for allocating a Flight. By default
  // there are only 2 buffers for Flights. Your app may add more before calling
  // ctorError().
  std::vector<FlightSource> sources;

  // getTotalSize reports this object's Vulkan memory usage.
  size_t getTotalSize() const { return mmapMaxSize * sources.size(); }

  size_t mmapMax() const { return mmapMaxSize; }

  Buffer& getRaw(Flight& flight) {
    if (flight.source_ >= sources.size()) {
      logF("Stage::getRaw: %zu sources, flight.source_=%zu\n", sources.size(),
           flight.source_);
    }
    return sources.at(flight.source_).buf;
  }

  // mmap maps a buffer for your app to write data to. 'dst' and 'offset' must
  // be specified in case mmap decides it can map 'dst' directly.
  //
  // If mmap decides to mmap 'dst' directly, the returned Flight has
  // Flight::canSubmit = false. Flight::mmap points to the CPU-visible mapping
  // of 'dst' where your app can write data. Your app does not need to call
  // flush() - though there is no harm if flush() is called.
  //
  // If mmap decides it cannot map 'dst' directly, the returned Flight has
  // Flight::canSubmit = true. Flight::mmap points to a CPU-visible staging
  // buffer where your app can write. Your app *must* call flush() to copy the
  // data from Flight::mmap to 'dst' at 'offset' (or flushButNotSubmit).
  // Your app can mutate the Flight just like any CommandBuffer before calling
  // flush / flushButNotSubmit.
  //
  // NOTE: 'bytes' is not allowed to be greater than mmapMax(). Split up large
  //       copies into smaller chunks.
  //
  // If successful, mmap returns 0. f is the resulting Flight object.
  // f->mmap() is where your app can write 'bytes'.
  WARN_UNUSED_RESULT int mmap(Buffer& dst, VkDeviceSize offset,
                              VkDeviceSize bytes, std::shared_ptr<Flight>& f);

  // mmap maps a buffer for your app to write bytes to. 'img' specifies the
  // Image the bytes will be copied to later. The return Flight contains a
  // 'copies' vector for your app to fill in the VkBufferImageCopy operations.
  // flush() does the buffer-to-image copy to 'img' using the 'copies' structs.
  //
  // Flight::canSubmit = false means your app does not need to call flush().
  //
  // NOTE: 'bytes' is not allowed to be greater than mmapMax(). Split up large
  //       copies into smaller chunks.
  //
  // If successful, mmap returns 0. f is the resulting Flight object.
  // f->mmap() is where your app can write 'bytes'.
  WARN_UNUSED_RESULT int mmap(Image& img, VkDeviceSize bytes,
                              std::shared_ptr<Flight>& f);

  // read sets up a Flight to read exactly 'bytes' from 'src' at 'offset'.
  //
  // If read decides to mmap 'src' directly, the returned Flight has
  // Flight::canSubmit = false. Flight::mmap points to the CPU-visible mapping
  // of 'src' where your app can read data. Your app does not need to call
  // flush() - though there is no harm if flush() is called.
  //
  // If read decides it cannot map 'src' directly, the returned Flight has
  // Flight::canSubmit = true. Your app can mutate it like any CommandBuffer
  // (Flight is a subclass of CommandBuffer). Your app *must* call flush() to
  // copy the data into Flight::mmap and execute any other commands (or
  // flushButNotSubmit). Flight::mmap is NULL until flush().
  //
  // After flush / flushButNotSubmit the data is CPU-visible at the address
  // Flight::mmap points to.
  //
  // NOTE: 'bytes' is not allowed to be greater than mmapMax(). Split up large
  //       copies into smaller chunks.
  WARN_UNUSED_RESULT int read(Buffer& src, VkDeviceSize offset,
                              VkDeviceSize bytes, std::shared_ptr<Flight>& f);

  // read sets up a Flight for your app to read from. 'src' specifies the
  // Image the bytes will be read from later when flush() is called.
  //
  // If read decides to mmap 'src' directly, the returned Flight has
  // Flight::canSubmit = false. Flight::mmap points to the CPU-visible mapping
  // of 'src' where your app can read data. Your app does not need to call
  // flush() - though there is no harm if flush() is called.
  //
  // If read decides it cannot map 'src' directly, the returned Flight has
  // Flight::canSubmit = true. Your app can mutate it like any CommandBuffer
  // (Flight is a subclass of CommandBuffer). Your app *must* call flush() to
  // copy the data into Flight::mmap and execute any other commands (or
  // flushButNotSubmit). Flight::mmap is NULL until flush().
  //
  // After flush / flushButNotSubmit the data is CPU-visible at the address
  // Flight::mmap points to.
  //
  // NOTE: 'bytes' is not allowed to be greater than mmapMax(). Split up large
  //       copies into smaller chunks.
  WARN_UNUSED_RESULT int read(Image& src, VkDeviceSize bytes,
                              std::shared_ptr<Flight>& f);

  // flush submits the copy to the GPU. 'f' is a Flight started with with mmap()
  // or read(). flush does not wait for the copy to complete.
  //
  // If flush sets waitForFence to false, 'fence' was not used.
  //
  // If flush setes waitForFence to true, 'fence' will be signalled when it
  // completes and your app *must* keep f valid until 'fence' is signalled.
  //
  // Your app *must* destroy f (call f.reset()) when everything has finished
  // with the Flight, or Stage will eventually run out of available Flights.
  //
  // If mmap() or read() set Flight::canSubmit = false, flush() is a no-op,
  // only setting waitForFence to false.
  //
  // NOTE: If flush is called for an mmap(), your app must *not* retain the
  //       Flight::mmap() pointer after the call to flush. Behind the scenes,
  //       Stage keeps a set of mmaps. As soon as flush is called, Stage may
  //       reuse the address of Flight::mmap.
  //
  // NOTE: If read() returned with Flight::canSubmit = true, mmap() is NULL
  //       until your app calls flush().
  WARN_UNUSED_RESULT int flush(std::shared_ptr<Flight> f, command::Fence& fence,
                               bool& waitForFence);

  // flushButNotSubmit only prepares the Flight to be submitted, but does
  // not submit it.
  //
  // Regardless of Flight::canSubmit, your app *must* destroy f (call
  // f.reset()) when everything has finished with the Flight, or Stage will
  // eventually run out of available Flights.
  //
  // If Flight::canSubmit = false, flushButNotSubmit() is a no-op.
  //
  // If Flight::canSubmit = true, flushButNotSubmit() skips some steps to give
  // your app more flexibility. These steps are done by flush() and your app
  // *must* do these:
  //  * Call f->CommandBuffer::end()
  //  * (Optional) Call pool.submit(*f) and wait for its fence to be signalled.
  //    (If pool.submit() is skipped, the Flight has no effect and is
  //     aborted. This abort is only possible if canSubmit = true.)
  //
  // NOTE: If flushButNotSubmit is called for an mmap(), your app must *not*
  //       retain the Flight::mmap pointer after the call to flushButNotSubmit.
  //       Behind the scenes, Stage keeps a set of mmaps. As soon as
  //       flushButNotSubmit is called, Stage may reuse the mmap.
  //
  // NOTE: If flushButNotSubmit is called for a read(), the Flight::mmap()
  //       pointer is only valid after flushButNotSubmit() and before release()
  WARN_UNUSED_RESULT int flushButNotSubmit(std::shared_ptr<Flight> f);

  // flushAndWait submits the Flight to the GPU and blocks the CPU until it
  // completes.
  WARN_UNUSED_RESULT int flushAndWait(std::shared_ptr<Flight> f) {
    std::shared_ptr<command::Fence> fence = pool.borrowFence();
    if (!fence) {
      logE("flushAndWait: pool.borrowFence failed\n");
      return 1;
    }
    bool waitForFence;
    if (flush(f, *fence, waitForFence)) {
      logE("flushAndWait: flush failed\n");
      (void)pool.unborrowFence(fence);
      return 1;
    }
    if (waitForFence) {
      VkResult v = fence->waitMs(1000);
      if (v != VK_SUCCESS) {
        (void)pool.unborrowFence(fence);
        return explainVkResult("flushAndWait: fence.waitMs", v);
      }
    }
    if (pool.unborrowFence(fence)) {
      logE("flushAndWait: unborrowFence failed\n");
      return 1;
    }
    return 0;
  }

  // copy transfers data from CPU to GPU. This is a convenience method for
  // vector-like classes (if it has .data(), .size(), and operator[]() like
  // std::vector then this method will work).
  template <typename T>
  WARN_UNUSED_RESULT int copy(Buffer& dst, VkDeviceSize offset, const T& vec) {
    std::shared_ptr<Flight> f;
    if (mmap(dst, offset, sizeof(vec[0]) * vec.size(), f)) {
      logE("Stage::copy: mmap failed\n");
      return 1;
    }
    memcpy(f->mmap(), vec.data(), sizeof(vec[0]) * vec.size());
    if (flushAndWait(f)) {
      logE("Stage::copy: flushAndWait failed\n");
      return 1;
    }
    return 0;
  }

 protected:
  friend struct Flight;
  // ctorError is intentionally protected. It is lazily done when needed.
  int ctorError();

  // alloc must be called with pool.lockmutex held. alloc returns the index of
  // a free FlightSource in sources and sets its isUsed. If Stage is out of
  // available Flights, alloc returns -1.
  int alloc();

  // release is called by ~Flight to reclaim the Flight. Your app releases the
  // Flight by calling f.reset() or if f goes out of scope. If 'waitForFence'
  // was true, the GPU must have finished and signalled the fence already.
  void release(Flight& f);

  // nextSource is the index into sources of the next available FlightSource.
  size_t nextSource{0};

  // mmapMaxSize is the size of one buffer.
  const size_t mmapMaxSize;

  // dummyFlight is used for certain requests.
  std::shared_ptr<Flight> dummyFlight;

  // dummyImgFlight is used for certain requests.
  std::shared_ptr<Flight> dummyImgFlight;
} Stage;

typedef std::map<VkDescriptorType, VkDescriptorPoolSize> DescriptorPoolSizes;

// DescriptorSetLayout holds all the VkDescriptorSetLayoutBinding objects
// used in a single DescriptorSet. In VkDescriptorSetLayoutBinding, the shader
// specifies the type of each input or output that will be bound before running
// the shader.
//
// A quick ascii art drawing may help:
//
// Inputs -------> Shader -------------> Outputs
// -------------                         -------
// DescriptorSet:                        DescriptorSet
// layout(layoutI = 0, set = 0)          layout(layoutI = 0, set = 2)
//  * layout(layoutI = 0, set = 0,        * layout(layoutI = 0, set = 2,
//           binding = 0)                          binding = 0)
//    Uniform Buffer                        Storage Buffer
//      - Material Properties                 - Shadow map
//  * layout(layoutI = 0, set = 0,
//           binding = 1)
//    Sampler
//      - Material Texture
//
// DescriptorSet
// layout(layoutI = 0, set = 1)
//  * layout(layoutI = 0, set = 1,
//           binding = 0)
//    Uniform Buffer
//      - Model and View matrices
//      - Shader settings
//
// Then add a completely different shader that has nothing in common with
// the above shader. It has just as many layouts. But they are all at:
//    layout(layoutI = 1)
//
// Now each of those shaders can be run with the first set of inputs and
// outputs, but later the same shader can be run with a different set of
// inputs. Your app can use many, many DescriptorSets, and as long as the
// DescriptorSet matches the shader's DescriptorSetLayout you are good to go.
//
// Just be aware of the limit the device imposes: you can only have a small
// number of DescriptorSets bound at the same time. The bare minimum required
// by any Vulkan device is 4, although this is rare. You can have *more* than
// that loaded and ready to use, but the device can only run with that many at
// *one* *time*.
//
// DescriptorPool prevents any fragmentation by allocating each DescriptorSet
// with the correct quantity of each type (i.e. its DescriptorSetLayout) from
// the pool. This way the pool always allocates the same size object every
// time and only stores a single number: the count of how much capacity
// remains.
//
// As a side effect, your app will need one DescriptorPool for each
// DescriptorSetLayout, i.e. more than one DescriptorPool.
//
// It may be simpler to use science::ShaderLibrary.
typedef struct DescriptorSetLayout {
  DescriptorSetLayout(language::Device& dev)
      : vk{dev, vkDestroyDescriptorSetLayout} {
    vk.allocator = dev.dev.allocator;
  }
  DescriptorSetLayout(DescriptorSetLayout&&) = default;
  DescriptorSetLayout(const DescriptorSetLayout&) = delete;

  // ctorError calls vkCreateDescriptorSetLayout.
  WARN_UNUSED_RESULT int ctorError(
      const std::vector<VkDescriptorSetLayoutBinding>& bindings);

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  DescriptorPoolSizes sizes;
  std::vector<VkDescriptorType> args;
  VkDebugPtr<VkDescriptorSetLayout> vk;
} DescriptorSetLayout;

// DescriptorPoolAllocator tracks a single VkDescriptorPool.
typedef struct DescriptorPoolAllocator {
  DescriptorPoolAllocator(language::Device& dev, size_t maxSets,
                          VkDescriptorPoolCreateFlags flags)
      : maxSets{maxSets}, flags{flags}, vk{dev, vkDestroyDescriptorPool} {
    vk.allocator = dev.dev.allocator;
  }

  // maxSets is the number of VkDescriptorSet objects that this can hold.
  const size_t maxSets;
  // flags is the flags used to create this VkDescriptorPool.
  const VkDescriptorPoolCreateFlags flags;
  // sets contains all VkDescriptorSet objects already allocated.
  std::set<void*> sets;
  // preallocated may contain additional VkDescriptorSet objects ready for use.
  std::vector<VkDescriptorSet> preallocated;

  VkDebugPtr<VkDescriptorPool> vk;
} DescriptorPoolAllocator;

// DescriptorPool is the allocator that creates DescriptorSet objects for your
// app.
//
// NOTE: Each DescriptorPool only provides one type of DescriptorSetLayout.
// Your shader may need multiple DescriptorPool objects to allocate all the
// DescriptorSet objects you need.
//
// It may be simpler to use a science::ShaderLibrary.
typedef struct DescriptorPool {
  // DescriptorPool constructor is easiest to use after you have a
  // DescriptorLayout. Pass in DescriptorLayout::sizes.
  //
  // The DescriptorPool is then compatible with any DescriptorLayout which
  // has identical sizes.
  DescriptorPool(language::Device& dev, const DescriptorPoolSizes& sizes)
      : dev{dev}, sizes{sizes} {}
  DescriptorPool(DescriptorPool&&) = default;
  DescriptorPool(const DescriptorPool&) = delete;

  // ctorError calls vkCreateDescriptorPool. If you want to increase the
  // initial allocation, modify maxSets before calling ctorError().
  //
  // If VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT is not set (in the
  // flags argument), all VkDescriptorSet objects are allocated immediately in
  // ctorError, but your app retrieves them one at a time by calling alloc(),
  // Your app *must* use reset() and track down the DescriptorSet::vk members
  // as documented for the reset() method.
  //
  // TODO: Allocate VkDescriptorSet objects one at a time or in blocks whether
  // the flag is set or not.
  WARN_UNUSED_RESULT int ctorError(
      VkDescriptorPoolCreateFlags flags =
          VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

  // reset can free the entire pool at once. One of the strengths of a pool
  // allocator is that freeing the entire pool is an O(1) operation.
  //
  // WARNING: This destroys the VkDescriptorSet objects without cleaning up
  // any DescriptorSet objects your app still holds. Your app must set the
  // DescriptorSet::vk member to VK_NULL_HANDLE in each object.
  WARN_UNUSED_RESULT int reset() {
    for (size_t i = 0; i < vk.size(); i++) {
      VkResult v =
          vkResetDescriptorPool(dev.dev, vk.at(i).vk, 0 /*flags is reserved*/);
      if (v != VK_SUCCESS) {
        return explainVkResult("vkResetDescriptorPool", v);
      }
      vk.at(i).sets.clear();
    }
    return 0;
  }

  // alloc creates a single VkDescriptorSet.
  WARN_UNUSED_RESULT int alloc(VkDescriptorSet& out,
                               VkDescriptorSetLayout layout);

  // alloc creates a single VkDescriptorSet.
  WARN_UNUSED_RESULT int alloc(VkDescriptorSet& out,
                               DescriptorSetLayout& layout) {
    return alloc(out, layout.vk);
  }

  // free frees a single VkDescriptorSet.
  void free(VkDescriptorSet ds);

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    if (vk.empty()) {
      logE("DescriptorPool::setName before ctorError is invalid\n");
      return 1;
    }
    for (size_t i = 0; i < vk.size(); i++) {
      if (vk.at(i).vk.setName(name)) {
        logE("DescriptorPool::setName: vk[%zu].setName failed\n", i);
        return 1;
      }
    }
    return 0;
  }

  // getName forwards the getName call to vk.
  const std::string& getName() const;

  static constexpr size_t INITIAL_MAXSETS = 8;

  // maxSets is the capacity in vk.back().
  // When vk.back() fills up, vk adds another VkDescriptorPool.
  // Your app can increase the initial allocation before calling ctorError().
  size_t maxSets{DescriptorPool::INITIAL_MAXSETS};

  language::Device& dev;
  const DescriptorPoolSizes sizes;
  std::vector<DescriptorPoolAllocator> vk;
} DescriptorPool;

// DescriptorSet represents a set of bindings (which represent inputs or
// outputs containing an image, buffer, etc.).
//
// The host application must bind (provide) all inputs and outputs the shader
// expects. If the DescriptorSet does not match the layout defined in the
// shader, Vulkan will report an error (and/or crash) - the
// DescriptorSetLayout passed to ctorError() gives the layout. (See
// VkDescriptorType for the enumeration of all types that can be specified in a
// VkDescriptorSetLayout.)
//
// Notes:
// 1. Even after a DescriptorSet is allocated with ctorError(), it does not
//    contain a valid type or buffer! Use DescriptorSet::write to populate the
//    DescriptorSet with its type and write something to its buffer.
// 2. Create DescriptorSetLayout objects during Pipeline initialization to
//    assemble a valid pipeline (see PipelineCreateInfo).
// 3. Bind a DescriptorSet to the pipeline (specifically to the shader) during
//    a RenderPass to pass in inputs and receive outputs of the shader.
//    Note: the 'uint32_t binding' is confusingly named: it is just the
//    argument number of the input or output.
typedef struct DescriptorSet {
  DescriptorSet(language::Device& dev, DescriptorPool& parent,
                DescriptorSetLayout& layout, VkDescriptorSet vk)
      : dev(dev), parent(parent), args(layout.args), vk(vk) {}
  ~DescriptorSet();

  // write populates the DescriptorSet with type and image.
  WARN_UNUSED_RESULT int write(
      uint32_t binding, const std::vector<VkDescriptorImageInfo> imageInfo,
      uint32_t arrayI = 0);
  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(
      uint32_t binding, const std::vector<VkDescriptorBufferInfo> bufferInfo,
      uint32_t arrayI = 0);
  // write populates the DescriptorSet with type and texelBuffer.
  WARN_UNUSED_RESULT int write(
      uint32_t binding, const std::vector<VkBufferView> texelBufferViewInfo,
      uint32_t arrayI = 0);

  // write is a generic method that accepts any class that implements a
  // toDescriptor method. One example is the science::Sampler class.
  //
  // There is no similar method for buffers. Use the above write() method
  // that takes a VkDescriptorBufferInfo.
  template <typename T>
  WARN_UNUSED_RESULT int write(uint32_t binding,
                               const std::vector<T*> imageResource,
                               uint32_t arrayI = 0) {
    std::vector<VkDescriptorImageInfo> imageInfo;
    imageInfo.resize(imageResource.size());
    for (size_t i = 0; i < imageResource.size(); i++) {
      imageResource.at(i)->toDescriptor(&imageInfo.at(i));
    }
    return write(binding, imageInfo, arrayI);
  }

  // setName calls setObjectName for the DescriptorSet.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    this->name = name;
    if (!vk) {
      return 0;
    }
    return language::setObjectName(dev, VOLCANO_CAST_UINTPTR(vk),
                                   language::getObjectType(vk), name.c_str());
  }
  // getName returns the object name.
  const std::string& getName() const { return name; }

  // dev holds a reference to the device where vk is stored.
  language::Device& dev;
  // parent is notified when this object is deleted. If your app calls
  // DescriptorPool::reset(), then you must prevent this from being deleted
  // later (double free bug) by overwriting vk = VK_NULL_HANDLE before
  // deleting this object.
  DescriptorPool& parent;
  // args has the VkDescriptorType at each binding, in the right order.
  std::vector<VkDescriptorType> args;
  // vk is the raw VkDescriptorSet handle, no VkPtr<> or VkDebugPtr<>,
  // because it does not have a destroy_fn that matches the VkPtr<> template.
  VkDescriptorSet vk;

 protected:
  // name is automatically set using VkDebugUtilsObjectNameInfoEXT
  // (or fallback to vkDebugMarkerSetObjectNameEXT())
  std::string name;
} DescriptorSet;

// TODO: VkDescriptorUpdateTemplate

}  // namespace memory
