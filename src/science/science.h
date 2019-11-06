/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/science is the 5th-level bindings for the Vulkan graphics library.
 * src/science is part of github.com/ndsol/volcano.
 * This library is called "science" as a homage to Star Trek First Contact.
 * Like the Vulcan Science Academy, this library builds on the fundamentals to
 * create several convenient, high-level classes and functions:
 *
 * * copyImage*() functions
 * * Sampler class builds on Image, ImageView
 * * CommandPoolContainer class is composed of CommandPool, RenderPass
 * * SmartCommandBuffer class adds convenient methods for CommandBuffers
 * * PipeBuilder class builds Pipeline objects and Pipeline derivatives
 * * ShaderLibrary and DescriptorLibrary do shader reflection
 */

#include <src/command/command.h>
#include <src/language/language.h>
#include <src/memory/memory.h>
#include <string.h>

#include <limits>
#include <set>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "science-glfw.h"

#pragma once

namespace science {

// copyImage1to1 is a straight 1:1 copy of src to dst.
// It will add a transition to get src and dst in the right layout (or, if
// src.currentLayout and dst.currentLayout are correct, it will do nothing).
// Then it adds a copyImage command to buffer.
int copyImage1to1(command::CommandBuffer& buffer, memory::Image& src,
                  memory::Image& dst);

// copyImageMipLevel copies a single mip level from src to dst.
// NOTE: This does NOT add a layout transition. memory::Image does not track
//       the layout of each mip level. You must transition the mip level or
//       the whole image first before calling copyImageMipLevel().
int copyImageMipLevel(command::CommandBuffer& buffer, memory::Image& src,
                      uint32_t srcMipLevel, memory::Image& dst,
                      uint32_t dstMipLevel);

// copyImageToMipmap reads img at mip level 0 and creates all the other mip
// levels by calling buffer.blitImage(). img is transitioned to the right
// layout.
int copyImageToMipmap(command::CommandBuffer& buffer, memory::Image& img);

// Sampler contains an Image, the ImageView, and the VkSampler, and has
// convenience methods for passing the VkSampler to descriptor sets and shaders.
typedef struct Sampler {
  // Construct a Sampler with info set to defaults (set to NEAREST mode,
  // which looks very blocky / pixellated).
  Sampler(language::Device& dev) : imageView{dev}, vk{dev, vkDestroySampler} {
    vk.allocator = dev.dev.allocator;
    memset(&info, 0, sizeof(info));
    info.sType = autoSType(info);
    // info.magFilter = VK_FILTER_NEAREST;
    // info.minFilter = VK_FILTER_NEAREST;
    // info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minLod = 0.0f;
    info.maxLod = 0.25f;  // 0.25 suggested in VkSamplerCreateInfo doc.
    if (dev.enabledFeatures.features.samplerAnisotropy == VK_TRUE) {
      info.anisotropyEnable = VK_TRUE;
      info.maxAnisotropy = dev.physProp.properties.limits.maxSamplerAnisotropy;
    } else {
      info.anisotropyEnable = VK_FALSE;
      info.maxAnisotropy = 1.0f;
    }
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    image = std::make_shared<memory::Image>(dev);
  }

  // ctorError() constructs vk, the Image, and ImageView.
  //
  // Your app should populate Sampler::image->info and Sampler::imageView.info
  // before calling ctorError.
  //
  // NOTE: After ctorError() succeeds, your app *must* initialize the contents
  //       of Image and use a barrier to transition image to
  //       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
  WARN_UNUSED_RESULT int ctorError();

  // ctorErrorNoImageViewInit destroys and recreates the VkSampler, and is
  // useful if your app changes any members of VkSamplerCreateInfo info.
  //
  // ctorErrorNoImageViewInit may also be useful if your app already created
  // image and imageView.
  WARN_UNUSED_RESULT int ctorErrorNoImageViewInit();

  // toDescriptor is a convenience method to add this Sampler to a descriptor
  // set.
  void toDescriptor(VkDescriptorImageInfo* imageInfo) {
    if (!image || !vk) {
      imageInfo->imageView = VK_NULL_HANDLE;
      imageInfo->sampler = VK_NULL_HANDLE;
      return;
    }
    imageInfo->imageLayout = image->currentLayout;
    imageInfo->imageView = imageView.vk;
    imageInfo->sampler = vk;
  }

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  std::shared_ptr<memory::Image> image;
  language::ImageView imageView;
  VkSamplerCreateInfo info;
  VkDebugPtr<VkSampler> vk;
} Sampler;

// CommandPoolContainer implements onResized() and automatically handles
// recreating the swapChain.
// NOTE: This assumes you use a single logical language::Device.
//       Even multiple GPUs likely only use a single language::Device
//       (see VkDeviceGroupSubmitInfoKHX).
// NOTE: Defaults the CommandPool::queueFamily to language::GRAPHICS. Your app
//       can change CommandPoolContainer::cpool.queueFamily before calling
//       CommandPoolContainer::cpool.ctorError().
struct CommandPoolContainer {
  CommandPoolContainer() = delete;  // You *must* call with a Device& argument.
  CommandPoolContainer(language::Device& dev) : cpool{dev}, pass{dev} {
    cpool.queueFamily = language::GRAPHICS;
  }
  virtual ~CommandPoolContainer();

  command::CommandPool cpool;
  command::RenderPass pass;
  // Your application can inspect prevSize in its resizeFramebufListeners.
  VkExtent2D prevSize;

  // onResized is called when cpool.dev.framebufs need to be resized.
  // * Register in resizeFramebufListeners to have CommandPoolContainer
  //   automatically handle per-framebuf initialization. (It is necessary to
  //   re-initialize each one any time there is a resize event.)
  //
  //   NOTE: You still must call CommandPoolContainer::onResized just before
  //   starting the main polling loop; this calls RenderPass::ctorError and
  //   builds the framebuffers and command buffers.
  //
  // * If CommandPoolContainer::onResized is useful but your app needs to
  //   customize the logic further, you probably want to override onResized,
  //   then call CommandPoolContainer::onResized first thing in your onResized.
  //
  //   NOTE: resetSwapChain *modifies* newSize. Your application *must not*
  //   assume newSize as passed in is the same after this onResized is done.
  //   Get the updated value like this:
  //     newSize = cpool.dev.swapChainInfo.imageExtent;
  WARN_UNUSED_RESULT virtual int onResized(VkExtent2D newSize,
                                           size_t poolQindex);

  typedef int (*resizeFramebufCallback)(void* self,
                                        language::Framebuf& framebuf,
                                        size_t framebuf_i, size_t poolQindex);
  // resizeFramebufListeners get called for each framebuf that needs to be
  // rebuilt. Use this to rebuild the command buffers (since they are bound to
  // framebuf).
  std::vector<std::pair<resizeFramebufCallback, void*>> resizeFramebufListeners;
};

// SmartCommandBuffer builds on top of CommandBuffer with convenience method
// AutoSubmit()
typedef struct SmartCommandBuffer : public command::CommandBuffer {
  SmartCommandBuffer(command::CommandPool& cpool_, size_t poolQindex_)
      : CommandBuffer{cpool_}, poolQindex{poolQindex_} {}
  // Move constructor.
  SmartCommandBuffer(SmartCommandBuffer&& other)
      : CommandBuffer(std::move(other)),
        poolQindex(other.poolQindex),
        ctorErrorSuccess(other.ctorErrorSuccess),
        wantAutoSubmit(other.wantAutoSubmit) {}
  // The copy constructor is not allowed. The VkCommandBuffer cannot be copied.
  SmartCommandBuffer(const CommandBuffer& other) = delete;

  virtual ~SmartCommandBuffer();
  SmartCommandBuffer& operator=(SmartCommandBuffer&&) = delete;

  // ctorError sets up SmartCommandBuffer for one time use by borowing
  // a pre-allocated command buffer from cpool. This is useful for init
  // commands because the command buffer is managed by the cpool.
  //
  // CommandPool::updateBuffersAndPass can automatically set up a vector of
  // SmartCommandBuffer -- in that case do not call ctorError().
  WARN_UNUSED_RESULT int ctorError();

  // autoSubmit() will set a flag so that ~SmartCommandBuffer() will
  // "auto-submit" the buffer by calling submit(). This is convenient for app
  // init.
  WARN_UNUSED_RESULT int autoSubmit();

  // submit() will clear the auto-submit flag by submitting any commands in the
  // buffer, then waiting for them to complete.
  WARN_UNUSED_RESULT int submit();

  const size_t poolQindex{0};

 protected:
  bool ctorErrorSuccess{false};
  bool wantAutoSubmit{false};
} SmartCommandBuffer;

// PipeBuilder is a builder for command::Pipeline.
// PipeBuilder immediately installs a new command::Pipeline in the
// command::RenderPass it gets in its constructor, so instantiating a
// PipeBuilder is an immediate commitment to completing the Pipeline before
// calling RenderPass:ctorError(). Or use the deriveFrom() method to copy the
// pipeline state of another PipeBuilder.
//
// Compute shaders do not need or want a RenderPass - do not use PipeBuilder
// with a compute shader. See ShaderLibrary::add() for compute shaders, below.
typedef struct PipeBuilder {
  PipeBuilder(command::RenderPass& pass) : pass(pass) {}
  PipeBuilder(PipeBuilder&&) = default;
  PipeBuilder(const PipeBuilder& other) = delete;

  command::RenderPass& pass;
  std::shared_ptr<command::Pipeline> pipe;

  // addPipelineOnce is automatically called by the other methods in PipeBuilder
  // to initialize PipeBuilder::pipe from PipeBuilder::pass.addPipeline().
  //
  // If you prefer to use deriveFrom(), it must be called before this gets
  // called because deriveFrom populates pipe a different way (which turns this
  // into a no-op).
  void addPipelineOnce() {
    if (!pipe) {
      pipe = pass.addPipeline();
    }
  }

  // deriveFrom makes this PipeBuilder an exact match for the other PipeBuilder
  // including sharing the same RenderPass.
  //
  // If you want to run the same PipeBuilder on *different* RenderPasses,
  // use the C++ copy operator to do PipeBuilder1.info() = PipeBuilder2.info().
  //
  // deriveFrom() is for creating a pipe which you can later pass to
  // swap(other) without recreating the RenderPass.
  //
  // NOTE: deriveFrom() cannot be used after addPipelineOnce() has been called.
  int deriveFrom(PipeBuilder& other);

  // swap removes other from RenderPass pass, adding this in its place.
  // If this has not been finalized (such as via RenderPass::ctorError(), this
  // will finalize it first.
  int swap(PipeBuilder& other);

  // info() returns the PipelineCreateInfo as if this were a command::Pipeline.
  command::PipelineCreateInfo& info() {
    addPipelineOnce();
    return pipe->info;
  }

  // addDepthImage calls the same method in memory::Pipeline.
  WARN_UNUSED_RESULT int addDepthImage(
      const std::vector<VkFormat>& formatChoices) {
    addPipelineOnce();
    return pipe->addDepthImage(formatChoices);
  }

  // alphaBlendWith() updates this PipeBuilder to make it compatible with the
  // dev.framebufs and subpass given in prevPipeInfo.
  //
  // This can be used two ways:
  // 1. boundary == VK_OBJECT_TYPE_RENDER_PASS implies this pipe starts a new
  //    render pass.
  // 2. boundary == VK_OBJECT_TYPE_PIPELINE implies this pipe is a subpass in
  //    the same render pass as prevPipeInfo.
  WARN_UNUSED_RESULT int alphaBlendWith(
      const command::PipelineCreateInfo& prevPipeInfo, VkObjectType boundary);

  // setName forwards the setName call to pipe.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    addPipelineOnce();
    return pipe->setName(name);
  }
  // getName forwards the getName call to pipe.
  const std::string& getName() const { return pipe->getName(); }

  // addVertexInput initializes a vertex *type* as an input to shaders. The
  // type variable is passed to addVertexInput at compile time to define the
  // vertex structure.
  //
  // Define the structure of your vertex shader inputs, and also define a
  // static method getAttributes() returning
  // std::vector<VkVertexInputAttributeDescription> to tell pipeBuilder the
  // structure layout:
  //   // In your vertex shader
  //   layout(location = 0) in vec3 inPos;
  //
  //   // In your C++ code
  //   struct MyVertex {
  //     glm::vec3 pos;
  //     std::vector<VkVertexInputAttributeDescription> getAttributes() {
  //       return std::vector<VkVertexInputAttributeDescription>{ ... };
  //     }
  //   };
  //   ...
  //   // addVertexInput calls getAttributes.
  //   if (pipeBuilder.addVertexInput<MyVertex>()) { ... }
  //
  // NOTE: You can use glslangVulkanToHeader() in a BUILD.gn file to
  // do this for you automatically. Then just #include the header it generates.
  template <typename T>
  WARN_UNUSED_RESULT int addVertexInput(uint32_t binding = 0) {
    return addVertexInputBySize(binding, sizeof(T), T::getAttributes());
  }

  // addVertexInputBySize is the non-template version of addVertexInput().
  WARN_UNUSED_RESULT int addVertexInputBySize(
      uint32_t binding, size_t nBytes,
      const std::vector<VkVertexInputAttributeDescription> typeAttributes) {
    vertexInputs.emplace_back();
    VkVertexInputBindingDescription& bindingDescription = vertexInputs.back();
    bindingDescription.binding = binding;
    bindingDescription.stride = nBytes;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    command::PipelineCreateInfo& pinfo(info());
    pinfo.vertsci.vertexBindingDescriptionCount = vertexInputs.size();
    pinfo.vertsci.pVertexBindingDescriptions = vertexInputs.data();

    attributeInputs.insert(attributeInputs.end(), typeAttributes.begin(),
                           typeAttributes.end());
    pinfo.vertsci.vertexAttributeDescriptionCount = attributeInputs.size();
    pinfo.vertsci.pVertexAttributeDescriptions = attributeInputs.data();
    return 0;
  }

  std::vector<VkVertexInputBindingDescription> vertexInputs;
  std::vector<VkVertexInputAttributeDescription> attributeInputs;
} PipeBuilder;

// DescriptorLibrary is the DescriptorSet objects and DescriptorPool they are
// allocated from. The DescriptorSetLayouts are computed by ShaderLibrary and
// are treated as immutable here.
typedef struct DescriptorLibrary {
 public:
  DescriptorLibrary(language::Device& dev) : dev(dev) {}

  // dev holds a reference to the device where the DescriptorSets are stored.
  language::Device& dev;

  // makeSet creates a new DescriptorSet from layouts[layoutI] and set[setI].
  // Use the setI the shader declared with "layout(set = setI)".
  //
  // Example usage:
  //   DescriptorLibrary library;
  //   // You MUST call finalizeDescriptorLibrary to init library.
  //   shaderLibrary.finalizeDescriptorLibrary(library);
  //   size_t layoutI = 0;
  //   size_t setI = 0;
  //   std::unique_ptr<memory::DescriptorSet> myDescriptorSet =
  //       library.makeSet(setI, layoutI);
  //   if (!myDescriptorSet) { ... handle errors ... }
  //
  // Further on, you call DescriptorSet::write() to populate it with data, and
  // CommandBuffer::bindGraphicsPipelineAndDescriptors while building the
  // command buffer.
  std::unique_ptr<memory::DescriptorSet> makeSet(size_t setI,
                                                 size_t layoutI = 0);

  // isFinalized tells you whether your app has called
  // ShaderLibrary::finalizeDescriptorLibrary() yet.
  bool isFinalized() const { return !pool.empty(); }

  // setName forwards the setName call to the underlying memory::DescriptorPool
  WARN_UNUSED_RESULT int setName(const std::string& name);
  // getName forwards the getName call to the underlying memory::DescriptorPool
  const std::string& getName() const;

  // layouts are the number and type of descriptor object needed to make a
  // DescriptorSet, filled by ShaderLibrary and treated as immutable here.
  //
  // layouts.at(0) corresponds to all shaders your app loads into layoutI = 0.
  // layouts.at(1) is all shaders your app loads into layoutI = 1. etc.
  //
  // Your app tracks which shaders have which layout, so layoutI is whatever
  // number your app uses for shaders that it intends to run together.
  //
  // Each layout has 1 or more sets ("layout(set = 1)" or 2, ...), and each
  // set has one or more bindings, each with a VkDescriptorType.
  std::vector<std::vector<memory::DescriptorSetLayout>> layouts;

  // pool manages allocating DescriptorSet objects by matching their layout to
  // the right DescriptorPool.
  //
  // The difference between this map and layouts above is that the layouts can
  // contain duplicates - it represents the order defined by your shaders.
  //
  // pool can then dedup layouts, especially useful if you have layoutI > 0.
  //
  // NOTE: DescriptorPoolSizes is a typedef of a std::map. Its value is the
  // memory::DescriptorPool::sizes member.
  std::map<memory::DescriptorPoolSizes, memory::DescriptorPool> pool;
} DescriptorLibrary;

struct ShaderLibraryInternal;

// ShaderLibrary uses //vendor/spirv_cross to determine the number of
// descriptors in each shader's descriptor set.
//
// Best practice with Vulkan is to have a single DescriptorSet which is used by
// all active shaders. Since not all shaders need all descriptors, it is
// expected there will be "unused variables" in some or all shaders if the
// shaders share the DescriptorSet in this manner.
//
// ShaderLibrary will print a warning if a Shader uses a different layout
// (requiring a different DescriptorSet) but will still work:
// "Shader does not match other Shader's layouts. Performance penalty."
class ShaderLibrary {
 public:
  ShaderLibrary(language::Device& dev) : dev(dev), _i(nullptr) {}
  virtual ~ShaderLibrary();

  // dev holds a reference to the device where the shaders are stored.
  language::Device& dev;

  // add puts a shader into a pipeline using reflection to automatically get
  // the stage and entryPointName.
  //
  // The shader stage is one of {tesselation control, evaluation, geometry,
  // vertex, fragment} (see VkPipelineShaderStageCreateInfo). Your app can
  // set the shader stage simply by naming the file with the right extension
  // (see glslangValidator --help): .vert, .frag, .geom, .tese, .tesc
  WARN_UNUSED_RESULT int add(PipeBuilder& pipe,
                             std::shared_ptr<command::Shader> shader,
                             size_t layoutI = 0);

  // add puts a shader into a pipeline, also accepting the shader stage bits
  // and entryPointName for a single shader that contains multiple entry points
  WARN_UNUSED_RESULT int add(PipeBuilder& pipe,
                             std::shared_ptr<command::Shader> shader,
                             size_t layoutI, VkShaderStageFlagBits stageBits,
                             std::string entryPointName = "main");

  // add a compute shader. Checks that the pipeline was constructed using the
  // compute shader form of the constructor. (A compute shader does not need or
  // want a RenderPass or PipeBuilder.)
  //
  // Example:
  //  command::CommandPool computeCommandPool{dev};
  //  computeCommandPool.queueFamily = language::COMPUTE;
  //  auto compute = std::make_shared<command::Pipeline>(computeCommandPool,
  //                                                     shader);
  //  if (shaderLibrary.add(compute, layoutI)) { ... handle errors ... }
  //
  // The shader stage must be compute (see VkPipelineShaderStageCreateInfo).
  // Your app can set the shader stage simply by naming the file with the
  // right extension (see glslangValidator --help): .comp
  WARN_UNUSED_RESULT int add(std::shared_ptr<command::Pipeline> pipe,
                             size_t layoutI = 0);

  // addDynamic will switch
  // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER to _DYNAMIC and
  // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER to _DYNAMIC. It is an error if the
  // descriptor type at (setI, layoutI, binding) is not one of the above.
  //
  // addDynamic() must be called before finalizeDescriptorLibrary(), but after
  // all the add() calls.
  WARN_UNUSED_RESULT int addDynamic(size_t setI, size_t layoutI,
                                    uint32_t binding);

  // finalizeDescriptorLibrary inits a DescriptorLibrary using the layouts
  // this ShaderLibrary has found by doing reflection on its shaders.
  //
  // Note: you MUST call add() at least once before calling
  // finalizeDescriptorLibrary.
  int finalizeDescriptorLibrary(DescriptorLibrary& descriptorLibrary);

 protected:
  friend struct ShaderLibraryInternal;

  struct ShaderBinding {
    std::vector<VkDescriptorSetLayoutBinding> layouts;
    // allStageBits collects all the stages found as the shaders are loaded.
    // It is then written to every layout.stageFlags in layouts.
    //
    // A more efficient solution would look at the layouts on a per-stage basis
    // but that would make this more complicated.
    uint32_t allStageBits{0};
  };

  std::vector<std::vector<ShaderBinding>>& getBindings();

  struct FinalizeObserver {
    std::shared_ptr<command::Pipeline> pipe;
    size_t layoutIndex;
  };

  const std::vector<FinalizeObserver>& getObservers() const;

  ShaderLibraryInternal* _i;
};

typedef struct ComputeBlock {
  ComputeBlock(command::CommandPool& cpool)
      : i{cpool.vk.dev}, o{cpool.vk.dev}, cmdBuf{cpool}, cmdBufPost{cpool} {}

  // i is initialized to size blockInSize
  memory::Buffer i;
  // o is initialized to size blockOutSize
  memory::Buffer o;
  // ds is initialized with ds->write(i)
  // if blockOutSize != 0, ds is also initialized with ds->write(o)
  std::shared_ptr<memory::DescriptorSet> ds;
  // flight is a convenience so your app can ensure the transfer
  // remains valid until the queue submission has finished.
  // Use it if your app needs a memory::Flight to write to i.
  // NOTE: When reading from o, your app only needs to ensure
  //       ComputePipeline does not reuse this block.
  std::shared_ptr<memory::Flight> flight;
  // cmdBuf is used if flight->canSubmit() == false, so that commands can
  // be submitted before the dispatch of the compute pipeline.
  command::CommandBuffer cmdBuf;
  // cmdBufPost is always submitted after the dispatch.
  command::CommandBuffer cmdBufPost;
  // uniformIndex chooses which buffer from ComputePipeline::uniform to write
  // to ds. Call allocBlocks before ctorError, then set uniformIndex in
  // each block of freeBlocks before ctorError writes to ds.
  //
  // WARNING: to set uniformIndex differently for every enqueue operation,
  // your app must update ds itself.
  size_t uniformIndex{0};
  // work defines the number of threads this block has data for.
  // It is invalid to specify a dimension of 0 - use the defaults for any
  // dimension that you plan to ignore.
  VkDispatchIndirectCommand work{1, 1, 1};
  // userId and userData are for your application-specific data.
  unsigned userId{0};
  // userData and userId are for your application-specific data.
  void* userData{nullptr};
} ComputeBlock;

// ComputePipeline is useful if your app runs the same command::Pipeline over
// and over again. For maximum throughput mode (where runs may overlap to keep
// the GPU full), the default setting for allocBlocks() should work.
// WARNING: ComputePipeline will attempt to auto-tune nextSize() but that is
// not a guarantee your app will fully utilize the GPU.
//
// For minimum latency mode (where a run must immediately feed some other part
// of your app), call allocBlocks() with the number of frame buffers your app
// uses - just enough so each enqueue can run without waiting on a previous
// enqueue. Also call setNextSize() before each run to override the auto-tuning
// of work sizes.
// TODO: Maybe add a way to turn off auto-tuning entirely?
//
// Example:
//   ComputePipeline cp{cpool.vk.dev, blockInSize, blockOutSize,
//                      comp::bindingIndexOfUBO(), sizeof(comp::UBO)};
//   if (cp.shader->loadSPV(...)) { ... handle error ... }
//   if (cp.ctorError()) { ... handle error ... }
//   // Set up first compute block:
//   // NOTE: Working with only one block to the GPU at a time is not very
//   // efficient.
//   std::vector<std::shared_ptr<science::ComputeBlock>> b(cp.newBlocks(1));
//   if (b.empty()) { ... handle error ... }
//   // Transfer data to initialize cp.uniform.at(0)
//   if (stage.mmap(cp.uniform.at(0), 0, cp.uboSize, uboflight) { ... }
//   fillUbo(*reinterpret_cast<Ubo*>(uboflight.mmap()));
//   if (stage.flushButNotSubmit(uboflight)) { ... handle error ... }
//   // Transfer data to initialize b.at(0)->i
//   // to feed the run.
//   if (stage.mmap(b.at(0)->i, 0, cp.blockInSize, b->flight)) { ... }
//   fillData(*reinterpret_cast<BlockIn*>(b.at(0)->flight.mmap()));
//   if (stage.flushButNotSubmit(b.at(0)->flight)) { ... handle error ... }
//
//   // Build queue submission for first compute block
//   // First submit transfers to initialize data:
//   command::SubmitInfo info;
//   if (flightubo.end() || flightubo.enqueue(info)) { ... handle error ... }
//   if (b->flight.end()) { ... handle error ... }
//   if (b->flight.enqueue(info)) { ... handle error ... }
//
//   // Only enqueue the block to run after the transfers.
//   if (cp.enqueueBlocks(b, info)) { ... handle error ... }
//
//   // Maybe do other rendering too - the variable 'flight' is for your
//   // UI renderer.
//
//   if (uglue.submit(flight, info)) { ... handle error ... }
//
//   if (cpool.deviceWaitIdle()) { ... handle error ... }
//
//   if (cp.deleteBlocks(b)) { ... handle error ... }
//
// For the second compute block, if it is to run in parallel with the first,
// your app would also call fillData(). If you always initialize the input
// before each run, then your app logic is simple - always do the fillData()
// step. (Since fillData() is your own function, name it whatever you want.)
//
// Some compute shaders reuse the input data without modifying it. Then
// you would only call fillData() for any block returned from newBlocks()
// that you have never seen before. It would be up to you to keep track of
// which ones have been seen before. The ComputeBlock has userId and
// userData as conveniences for app specific data.
//
// Some compute shaders go further, and do a read-modify-write on the
// input buffer. It would be up to you to keep track of the state of the
// input buffer. Some compute shaders do not have an input buffer, e.g. if it
// is used to initialize the buffer! In this case, your app can use
// ComputeBlock::i as an output buffer and ignore the other buffer.
//
// To shuffle buffers around:
// The ComputeBlock::ds descriptor set is mutable. Call ds->write() if your app
// wants to feed the ComputeBlock with the memory from a different block.
// WARNING: If your app does a ds->write(), your app must also manage the
// lifetime of that 'different block,' by retaining the shared_ptr until your
// app does another ds->write(). Ideally to clean up, do a ds->write() back
// to the memory in its own block.
// WARNING: Your app must also ensure the 'different block' is not
// passed to ComputePipeline::freeBlock(), so that ComputePipeline does not
// set state == FREE, or if it is passed to freeBlock(), your app must do its
// own checking to protect the memory, such as by ensuring that all blocks have
// their descriptor set updated to point somewhere safe (not recommended!).
//
// ComputePipeline manages the blocks, allocating and enqueuing them.
// * command::CommandPool cpool - automatically set to device's compute queue
// * std::shared_ptr<command::Shader> shader - the compute shader goes here.
//   WARNING: This is not done automatically. Load your shader before calling
//   ctorError().
// * std::shared_ptr<command::Pipeline> pipe - automatically built.
// * std::vector<memory::Buffer> uniform - load uniform data here before
//   enqueuing a block. Your app must call emplace_back at least once if your
//   app specifies a non-zero uboSize, but then they are automatically set up.
//
// If the shader declares a uniform buffer, the ComputePipeline constructor
// must be given the binding index and sizeof() the uniform buffer. Otherwise
// leave uboSize == 0 to specify that the compute shader has no uniform buffer.
// Binding index 0 and 1 are reserved for the input and output block.
//
// If the shader declares an output target, the ComputePipeline constructor
// must be given the sizeof() the output target (size per block). Otherwise
// leave blockOutSize == 0 to specify that the compute shader has no output
// buffer.
class ComputePipeline {
 public:
  // The default blockOutSize == 0 means there is no output buffer.
  // The default uboSize == 0 means there is no uniform buffer.
  ComputePipeline(language::Device& dev, VkDeviceSize blockInSize,
                  VkDeviceSize blockOutSize = 0, unsigned uboBindingIndex = 0,
                  VkDeviceSize uboSize = 0)
      : blockInSize(blockInSize),
        blockOutSize(blockOutSize),
        uboBindingIndex(uboBindingIndex),
        uboSize(uboSize),
        cpool(dev),
        descriptorLibrary(dev) {
    cpool.queueFamily = language::COMPUTE;
    shader = std::make_shared<command::Shader>(cpool.vk.dev);
  }

  typedef std::vector<std::shared_ptr<ComputeBlock>> BlockVec;
  const unsigned bindingIndexIn{0};
  const unsigned bindingIndexOut{1};
  const VkDeviceSize blockInSize;
  const VkDeviceSize blockOutSize;
  const unsigned uboBindingIndex;
  const VkDeviceSize uboSize;
  const size_t poolQindex{0};

  command::CommandPool cpool;
  DescriptorLibrary descriptorLibrary;
  std::shared_ptr<command::Shader> shader;
  std::shared_ptr<command::Pipeline> pipe;
  // uniform: as many uniform buffers as you need. If your app sets uboSize to
  // non-zero, call uniform.emplace_back(cpool.vk.dev) at least once.
  std::vector<memory::Buffer> uniform;
  // chain allows chained compute pipelines to be launched together by linking
  // the next pipeline in the chain as a child of this pipeline (store the
  // child in 'chain'). Then launch from any point in the chain by calling
  // enqueueChain() with the number of elements in its 'work' parameter
  // determining how many chained pipelines are submitted.
  std::shared_ptr<ComputePipeline> chain;

  // allocBlocks adds blocks to freeBlocks until it reaches minSize.
  // NOTE: ComputePipeline will auto-size the number of blocks to occupy
  // the GPU fully. You can use this to reduce the number of freeBlocks.
  // allocBlocks can be called before ctorError().
  WARN_UNUSED_RESULT int allocBlocks(size_t minSize);

  // ctorError uses shader to initialize cpool, descriptorLibrary, pipe,
  // uniform and block. The shader must be loaded before calling ctorError:
  // call ComputePipeline::shader->loadSPV() first.
  WARN_UNUSED_RESULT int ctorError();

  // newBlocks() returns N of freeBlocks and moves them to prepBlocks.
  // Returns an empty vector on error.
  BlockVec newBlocks(size_t N);

  // poll() checks the GPU and moves ComputeBlock objects from runBlocks to
  // doneBlocks.
  WARN_UNUSED_RESULT int poll();

  // wait() asks the driver to block this thread for nanos nanoseconds or
  // until at least one block has changed state. Your app must then call poll()
  // to update the state of the blocks.
  //
  // The second parameter, timeout, is set to true if no block changed state
  // but the nanos timeout expired.
  WARN_UNUSED_RESULT int wait(uint64_t nanos, bool& timeout);

  // deleteBlocks() tells ComputePipeline that your app has released all
  // resources in these blocks (such as ComputeBlock::flight - the GPU must not
  // be executing the flight any more, which your app is responsible to
  // verify). deleteBlocks will call b->flight.reset(). The flight was just
  // there for a convenience.
  //
  // The input parameter gets b.clear() called on it - it comes back empty.
  //
  // deleteBlocks will fail if any block is not in doneBlocks already. Be sure
  // to call poll() first. It is also ok if the block is in prepBlocks - your
  // app may change its mind about submitting work.
  WARN_UNUSED_RESULT int deleteBlocks(BlockVec& v);

  // setNextSize replaces the computed nextSize with one chosen by your app.
  void setNextSize(size_t s) { nextSize = s; }

  // enqueueChain submits multiple pipelines at once, one for each element in
  // 'work.' There must be that many links by following chain->chain->chain
  // defined above.
  //
  // Only the fence in this ComputePipeline is signalled at the end: you must
  // call poll() on this ComputePipeline and the entire chain will move their
  // blocks to doneBlocks at the same time.
  //
  // NOTE: A barrier is needed between chained work, and this does not
  // automatically add one. In for example the BlockVec at work[1], write a
  // barrier to the command buffer - the BlockVec command buffer is run before
  // the compute pipeline is dispatched.
  WARN_UNUSED_RESULT int enqueueChain(std::vector<BlockVec*> work,
                                      command::SubmitInfo& submitInfo);

  // enqueueBlocks adds the blocks to submitInfo.
  // When submitInfo is submitted, the blocks will then be scheduled on the GPU
  // The blocks are moved to runBlocks, and poll() will eventually move them
  // to doneBlocks.
  //
  // WARNING: if your app uses a memory::Stage to populate b->flight, your app
  // must have already called memory::Stage::flushButNotSubmit on b->flight.
  WARN_UNUSED_RESULT int enqueueBlocks(BlockVec& v,
                                       command::SubmitInfo& submitInfo) {
    return enqueueChain({&v}, submitInfo);
  }

  // setName sets a base name. Objects held by ComputePipeline then have names
  // derived from this name.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    debugName = name;
    return cpool.setName(name + ".cpool");
  }

  const std::string& getName() const { return debugName; }

  // lockmutex is protects all members below this point.
  std::recursive_mutex lockmutex;
  typedef std::lock_guard<std::recursive_mutex> lock_guard_t;
  typedef std::unique_lock<std::recursive_mutex> unique_lock_t;

  BlockVec freeBlocks;  // Ready for app to call newBlocks.
  BlockVec prepBlocks;  // newBlocks gave them to app to initialize them.
  BlockVec runBlocks;   // App passed them to enqueueBlocks, will run on GPU.
  BlockVec doneBlocks;  // GPU done, app gets a last look before deleteBlocks

 protected:
  size_t nextSize{3};
  std::string debugName{"ComputePipeline"};

  // ComputeFence records which blocks are the target of the given fence.
  typedef struct ComputeFence {
    ComputeFence(command::CommandPool& cpool) : cpool{cpool} {}
    ComputeFence(ComputeFence&& other)
        : cpool{other.cpool},
          fence{std::move(other.fence)},
          fenceBlocks{std::move(other.fenceBlocks)} {}
    ComputeFence& operator=(const ComputeFence& other) {
      fence = std::move(other.fence);
      fenceBlocks = std::move(other.fenceBlocks);
      return *this;
    }

    int reset() {
      if (fence && cpool.unborrowFence(fence)) {
        logE("ComputeFence::reset: unborrowFence failed\n");
        return 1;
      }
      fence.reset();
      return 0;
    }

    command::CommandPool& cpool;
    std::shared_ptr<command::Fence> fence{cpool.borrowFence()};
    std::shared_ptr<command::Fence> parentFence;
    BlockVec fenceBlocks;
    size_t children{0};
  } ComputeFence;

  std::vector<ComputeFence> waitList;

  // initBlocks() populates blocks if they are in state == INVALID.
  WARN_UNUSED_RESULT int initBlocks(size_t startIndex);

  // dispatch writes commands to run pipe.
  int dispatch(command::CommandBuffer& cmd, std::shared_ptr<ComputeBlock> b);

  // appendCmds adds to submitInfo
  WARN_UNUSED_RESULT int appendCmds(BlockVec& v, command::SubmitInfo& info);

  // retire moves the blocks from runBlocks to doneBlocks.
  WARN_UNUSED_RESULT int retire(size_t i);
};

// InstanceBuf holds an indirect draw command and the instance buffer handle
// it uses. Because a single instance buffer is probably used for multiple
// different indirect draw commands, the instance buffer handle is only a
// reference to the actual memory::Buffer.
//
// This is only a convenience if your app uses instanced drawing.
typedef struct InstanceBuf {
  InstanceBuf() { memset(&cmd, 0, sizeof(cmd)); }

  VkDrawIndexedIndirectCommand cmd;
  VkBuffer vk{VK_NULL_HANDLE};
  VkDeviceSize ofs{0};
} InstanceBuf;

}  // namespace science
