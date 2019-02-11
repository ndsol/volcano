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
 * * PresentSemaphore class implements a special case of Semaphore
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
    VkOverwrite(info);
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

#if defined(_WIN32) && defined(max)
#undef max
#endif
  // acquireNextImage wraps vkAcquireNextImageKHR and handles the various cases
  // that can arise: Success returns 0 and sets next_image_i.
  //                 Failure returns 1 signalling to destroy the device.
  // Restartable errors like VK_ERROR_OUT_OF_DATE_KHR are also handled, but set
  // next_image_i = (uint32_t) -1, signalling that the app must immediately
  // jump to the top of its main loop.
  //
  // Pass in frameNumber for convenience; dev.setFrameNumber is called for you.
  //
  // Note: Use PresentSemaphore::present() to handle all the same edge cases as
  //       they can arise from the result of vkQueuePresentKHR.
  WARN_UNUSED_RESULT int acquireNextImage(
      uint32_t frameNumber, uint32_t* next_image_i,
      command::Semaphore& imageAvailableSemaphore,
      // A timeout of uint64_t::max means infinity.
      uint64_t timeout = std::numeric_limits<uint64_t>::max(),
      // An optional fence can signal the CPU when
      // imageAvailableSemaphore is signalled on the GPU.
      VkFence fence = VK_NULL_HANDLE,
      // poolQindex may be needed to call onResized.
      size_t poolQindex = memory::ASSUME_POOL_QINDEX);
};

// PresentSemaphore is a special Semaphore that adds the present() method.
class PresentSemaphore : public command::Semaphore {
 public:
  CommandPoolContainer& parent;
  VkQueue q;

 public:
  PresentSemaphore(CommandPoolContainer& parent)
      : Semaphore(parent.cpool.vk.dev), parent(parent) {}
  PresentSemaphore(PresentSemaphore&&) = default;
  PresentSemaphore(const PresentSemaphore&) = delete;

  // Two-stage constructor: call ctorError() to build PresentSemaphore.
  WARN_UNUSED_RESULT int ctorError();

  // present submits the given swapChain image_i to Device dev's screen using
  // the correct language::PRESENT queue and synchronization.
  // * Success returns 0.
  // * Failure returns 1 signalling to destroy the device.
  // * Restartable errors like VK_ERROR_OUT_OF_DATE_KHR are also handled, but
  //   set image_i = (uint32_t) -1, signalling that the app must immediately
  //   jump to the top of its main loop.
  //
  // Note: CommandPoolContainer::acquireNextImage() handles all the same edge
  //       cases as they can arise from the result of vkAcquireNextImageKHR.
  WARN_UNUSED_RESULT int present(
      uint32_t* image_i,
      // poolQindex may be needed to call onResized.
      size_t poolQindex = memory::ASSUME_POOL_QINDEX);

  // waitIdle is used to prevent validation layers from leaking memory, and
  // will slightly reduce the overall application's performance. Removing calls
  // to waitIdle may or may not be worth the trouble.
  WARN_UNUSED_RESULT int waitIdle();

  language::SurfaceSupport queueFamily{language::PRESENT};
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

// DescriptorLibrary is the DescriptorSet objects and DescriptorPool (or pools)
// they are allocated from. The DescriptorSetLayouts are computed by
// ShaderLibrary and are treated as immutable.
class DescriptorLibrary {
 public:
  DescriptorLibrary(language::Device& dev) : dev(dev) {}

  // dev holds a reference to the device where the DescriptorSets are stored.
  language::Device& dev;

  // layouts are the number and type of descriptor object needed to make a
  // DescriptorSet, filled by ShaderLibrary and treated as immutable here.
  //
  // layouts.at(0) corresponds to all shaders add with layoutI = 0.
  // layouts.at(1) is all shaders with layoutI = 1. etc.
  //
  // Each layout has 1 or more descriptor sets ("layout(set = 1)" or 2, ...)
  std::vector<std::vector<memory::DescriptorSetLayout>> layouts;

  // makeSet creates a new DescriptorSet from layouts[layoutI] and set[setI].
  // Use the setI the shader declared with "layout(set = setI)".
  //
  // Example usage:
  //   DescriptorLibrary library;
  //   // You MUST call finalizeDescriptorLibrary to init library.
  //   shaderLibrary.finalizeDescriptorLibrary(library);
  //   size_t layoutI = 0;
  //   size_t setI = 0;
  //   std::unique_ptr<memory::DescriptorSet> d =
  //       library.makeSet(setI, layoutI);
  //   if (!d) { ... }
  //   pipe.info.setLayouts.emplace_back(library.layouts.at(layoutI).vk);
  //
  // Call DescriptorSet::write() to populate it with data, and
  // CommandBuffer::bindGraphicsPipelineAndDescriptors to bind it for use.
  // For example, if binding number 0 were a uniform buffer:
  //
  //   if (d->write(0 /*binding number*/, {&uniform})) { ... }
  //   if (cmdBuffer.bindGraphicsPipelineAndDescriptors(pipe.pipe, 0, 1,
  //                                                    d->vk)) { ... }
  std::unique_ptr<memory::DescriptorSet> makeSet(size_t setI,
                                                 size_t layoutI = 0);

  bool isFinalized() const { return !typesAverage.empty(); }

  // setName forwards the setName call to the underlying VkCommandPool.
  WARN_UNUSED_RESULT int setName(const std::string& name);
  // getName forwards the getName call to the underlying VkCommandPool.
  const std::string& getName() const;

 protected:
  friend class ShaderLibrary;

  // poolSize is how many "average DescriptorSets" in a pool, increased each
  // time a pool is exhausted and a new one must be added.
  uint32_t poolSize{4};

  // typesAverage is how many of the given type are needed to make
  // "average DescriptorSets" for one of each layout in layouts.
  std::map<VkDescriptorType, uint32_t> typesAverage;

  // availDescriptorSets is how many DescriptorSets the pool has remaining.
  uint32_t availDescriptorSets{0};

  // typesAvail tracks how many of the given type are available in the pool.
  // This is to avoid failing a vkAllocateDescriptorSets call.
  std::map<VkDescriptorType, uint32_t> typesAvail;

  // pools is a vector, resized as more DescriptorPools are created.
  // Only the last DescriptorPool is allocated from.
  std::vector<memory::DescriptorPool> pools;

  // addToPools adds a new pool to pools and resets typesAvail.
  WARN_UNUSED_RESULT int addToPools();
};

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
  // vertex, fragment, compute} (see VkPipelineShaderStageCreateInfo). Your app
  // can set the shader stage simply by naming the file with the right
  // extension (see glslangValidator --help):
  // .vert, .frag, .geom, .tese, .tesc, .comp
  WARN_UNUSED_RESULT int add(PipeBuilder& pipe,
                             std::shared_ptr<command::Shader> shader,
                             size_t layoutI = 0);

  // add puts a shader into a pipeline, also accepting the shader stage bits
  // and entryPointName for a single shader that contains multiple entry points
  WARN_UNUSED_RESULT int add(PipeBuilder& pipe,
                             std::shared_ptr<command::Shader> shader,
                             size_t layoutI, VkShaderStageFlagBits stageBits,
                             std::string entryPointName = "main");

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
    uint32_t allStageBits{0};
  };

  const std::vector<std::vector<ShaderBinding>>& getBindings() const;

  struct FinalizeObserver {
    std::shared_ptr<command::Pipeline> pipe;
    size_t layoutIndex;
  };

  const std::vector<FinalizeObserver>& getObservers() const;

  ShaderLibraryInternal* _i;
};

}  // namespace science
