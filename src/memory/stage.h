/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/memory is the 4th-level bindings for the Vulkan graphics library.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY! Just #include <src/memory/memory.h>
 */

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
        dummyImgFlight(std::make_shared<Flight>(*this)) {}
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

  // sources holds the resources Stage has for allocating a Flight.
  std::vector<FlightSource> sources;

  // getTotalSize reports this object's Vulkan memory usage.
  size_t getTotalSize() const { return mmapMaxSize * sources.size(); }

  size_t mmapMax() const { return mmapMaxSize; }

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
  // If Flight::canSubmit = false, flushButLeaveOpen() is a no-op.
  //
  // If Flight::canSubmit = true, flushButLeaveOpen() skips some steps to give
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

// DescriptorPool represents memory reserved for a DescriptorSet (or many).
// The assumption is that your application knows in advance the max number of
// DescriptorSet instances that will exist.
//
// It is also assumed your application knows the max number of each descriptor
// (VkDescriptorType) that will make up the DescriptorSet or sets.
typedef struct DescriptorPool {
  DescriptorPool(language::Device& dev) : vk{dev, vkDestroyDescriptorPool} {
    vk.allocator = dev.dev.allocator;
  }
  DescriptorPool(DescriptorPool&&) = default;
  DescriptorPool(const DescriptorPool&) = delete;

  // ctorError calls vkCreateDescriptorPool to enable creating
  // a DescriptorSet from this pool.
  //
  // sets is how many DescriptorSet instances can be created.
  // descriptors is how many of each VkDescriptorType can be create.
  // multiplier reserves that many copies of both sets and descriptors.
  WARN_UNUSED_RESULT int ctorError(
      uint32_t sets, const std::map<VkDescriptorType, uint32_t>& descriptors,
      uint32_t multiplier);

  WARN_UNUSED_RESULT int reset() {
    VkResult v = vkResetDescriptorPool(vk.dev.dev, vk, 0 /*flags is reserved*/);
    if (v != VK_SUCCESS) {
      return explainVkResult("vkResetDescriptorPool", v);
    }
    return 0;
  }

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  VkDebugPtr<VkDescriptorPool> vk;
} DescriptorPool;

// DescriptorSetLayout represents a group of VkDescriptorSetLayoutBinding
// objects. This is useful when several groups are being assembled into a
// DescriptorSet.
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

  std::vector<VkDescriptorType> types;
  VkDebugPtr<VkDescriptorSetLayout> vk;
} DescriptorSetLayout;

// DescriptorSet represents a set of bindings (which represent blocks of memory,
// containing an image, buffer, etc.).
//
// The host application must bind (provide) all the inputs the shader expects.
// If the DescriptorSet does not match the layout defined in the shader, Vulkan
// will report an error (and/or crash) - the VkDescriptorSetLayout is sent by
// your app to specify what it is providing. (See VkDescriptorType for the
// enumeration of all types that can be specified in a VkDescriptorSetLayout.)
//
// Notes:
// 1. Even after a DescriptorSet is allocated with ctorError(), it does not
//    contain a valid type or buffer! Use DescriptorSet::write to populate the
//    DescriptorSet with its type and write something to its buffer.
// 2. Create DescriptorSetLayout objects during Pipeline initialization to
//    assemble a valid pipeline (see PipelineCreateInfo).
// 3. Bind a DescriptorSet to the pipeline (specifically to the shader) during
//    a RenderPass to pass in inputs and receive outputs of the shader.
//    Note: the 'uint32_t binding' is confusingly named: it is just an index
//    for referring to similarly-typed descriptors (e.g. the uniform buffer
//    for the vertex shader may be at binding=0 while the uniform buffer for
//    the fragment shader may be at binding=1). Your app would still have to
//    bind buffers to both the binding=0 input and the binding=1 input.
typedef struct DescriptorSet {
  DescriptorSet(language::Device& dev, VkDescriptorPool poolVk)
      : dev(dev), poolVk(poolVk) {}
  virtual ~DescriptorSet();

  // ctorError allocates the DescriptorSet by calling vkAllocateDescriptorSets.
  WARN_UNUSED_RESULT int ctorError(const DescriptorSetLayout& layout);

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

  // write populates the DescriptorSet with type and buffer.
  WARN_UNUSED_RESULT int write(uint32_t binding,
                               const std::vector<Buffer*> buffers,
                               uint32_t arrayI = 0) {
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    bufferInfos.resize(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
      auto& bufferInfo = bufferInfos.at(i);
      auto& buffer = *buffers.at(i);
      bufferInfo.buffer = buffer.vk;
      bufferInfo.offset = 0;
      bufferInfo.range = buffer.info.size;
    }
    return write(binding, bufferInfos, arrayI);
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
  // poolVk is the raw VkDescriptorPool handle used to allocate this set.
  VkDescriptorPool poolVk;
  // types contains one VkDescriptorType entry for each member of that type.
  std::vector<VkDescriptorType> types;
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
