/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/science is the 5th-level bindings for the Vulkan graphics library.
 * src/science is part of github.com/ndsol/volcano.
 * This library is called "science" as a homage to Star Trek First Contact.
 * Like the Vulcan Science Academy, this library is a repository of knowledge
 * as a series of builder classes.
 */

#include <src/command/command.h>
#include <src/language/language.h>
#include <src/memory/memory.h>
#include <string.h>
#include <unistd.h>
#include <memory>

#pragma once

namespace science {

// SubresUpdate is a builder pattern for populating a
// VkImageSubresourceRange ... or a
// VkImageSubresourceLayers.
//
// Example usage:
//   VkImageCopy region = {};
//   // Just use Subres(), no need for SubresUpdate() in simple use cases.
//   science::Subres(region.srcSubresource).addColor();
//   science::Subres(region.dstSubresource).addColor();
//   region.srcOffset = {0, 0, 0};
//   region.dstOffset = {0, 0, 0};
//   region.extent = ...;
//
//   command::CommandBuilder builder(cpool);
//   if (builder.beginOneTimeUse() ||
//       builder.copyImage(srcImage.vk, dstImage.vk,
//           std::vector<VkImageCopy>{region})) { ... handle error ... }
//
// SubresUpdate is like Subres except it does not zero out the wrapped type,
// to allow for additional building.
class SubresUpdate {
 public:
  // Construct a SubresUpdate that modifies a VkImageSubresourceRange.
  SubresUpdate(VkImageSubresourceRange& range_) : range(&range_) {}
  // Construct a SubresUpdate that modifies a VkImageSubresourceLayers.
  SubresUpdate(VkImageSubresourceLayers& layers_) : layers(&layers_) {}

  // Specify that this Subres applied to a color attachment.
  SubresUpdate& addColor() {
    if (range) {
      range->aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (layers) {
      layers->aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    return *this;
  }
  // Specify that this Subres applied to a depth attachment.
  SubresUpdate& addDepth() {
    if (range) {
      range->aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (layers) {
      layers->aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    return *this;
  }
  // Specify that this Subres applied to a stencil attachment.
  SubresUpdate& addStencil() {
    if (range) {
      range->aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    if (layers) {
      layers->aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return *this;
  }

  // Specify mip-mapping offset and count.
  // This only applies to VkImageSubresourceRange, and will abort on a
  // VkImageSubresourceLayers.
  SubresUpdate& setMips(uint32_t offset, uint32_t count) {
    if (range) {
      range->baseMipLevel = offset;
      range->levelCount = count;
    }
    if (layers) {
      fprintf(stderr,
              "Subres: cannot setMips() on VkImageSubresourceLayers. "
              "Try setMip() instead.\n");
      exit(1);
    }
    return *this;
  }

  // Specify mipmap layer.
  // This applies to VkImageSubresourceLayers, and will abort on a
  // VkImageSubresourceRange
  SubresUpdate& setMip(uint32_t level) {
    if (range) {
      fprintf(stderr,
              "Subres: cannot setMip() on VkImageSubresourceRange. "
              "Try setMips() instead.\n");
      exit(1);
    }
    if (layers) {
      layers->mipLevel = level;
    }
    return *this;
  }

  // Specify layer offset and count. Might be used for stereo displays.
  SubresUpdate& setLayer(uint32_t offset, uint32_t count) {
    if (range) {
      range->baseArrayLayer = offset;
      range->layerCount = count;
    }
    if (layers) {
      layers->baseArrayLayer = offset;
      layers->layerCount = count;
    }
    return *this;
  }

 protected:
  VkImageSubresourceRange* range = nullptr;
  VkImageSubresourceLayers* layers = nullptr;
};

// Subres is a builder pattern for populating a
// VkImageSubresourceRange ... or a
// VkImageSubresourceLayers.
//
// Construct it wrapped around an existing VkImageSubresourceRange or
// VkImageSubresourceLayers. It will immediately zero out the wrapped type.
// Then use the mutator methods to build the type.
class Subres : public SubresUpdate {
 public:
  // Construct a Subres that modifies a VkImageSubresourceRange.
  Subres(VkImageSubresourceRange& range_) : SubresUpdate(range_) {
    memset(range, 0, sizeof(*range));
    range->baseMipLevel = 0;    // Mipmap level offset (none).
    range->levelCount = 1;      // There is 1 mipmap (no mipmapping).
    range->baseArrayLayer = 0;  // Offset in layerCount layers.
    range->layerCount = 1;      // Might be 2 for stereo displays.
  }
  // Construct a Subres that modifies a VkImageSubresourceLayers.
  Subres(VkImageSubresourceLayers& layers_) : SubresUpdate(layers_) {
    memset(layers, 0, sizeof(*layers));
    layers->mipLevel = 0;        // First mipmap level.
    layers->baseArrayLayer = 0;  // Offset in layerCount layers.
    layers->layerCount = 1;      // Might be 2 for stereo displays.
  }
};

// hasStencil() returns whether a VkFormat includes the stencil buffer or not.
// This is used for example by memory::Image::makeTransition().
inline bool hasStencil(VkFormat format) {
  switch (format) {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
    default:
      return false;
  }
};

// SwapChainResizeObserver is an interface (pure virtual class) which
// has one virtual method: onResized. Implement this class to get notified
// when the swapchain is resized.
class SwapChainResizeObserver {
 public:
  virtual ~SwapChainResizeObserver();

  // onResized is called when the Device dev's swapchain is resized.
  // The CommandBuilder builder is ready to receive setup commands related
  // to the resize (specifically, begin...() has been called on builder).
  // oldSize is what dev.swapChainInfo.imageExtent was before the resize.
  WARN_UNUSED_RESULT virtual int onResized(language::Device& dev,
                                           command::CommandBuilder& builder,
                                           VkExtent2D oldSize) = 0;
};

// ResizeDeviceWaitIdle is a convenient SwapChainResizeObserver  which
// just calls vkDeviceWaitIdle.
struct ResizeDeviceWaitIdle : public SwapChainResizeObserver {
  // onResized implements SwapChainResizeObserver::onResized.
  WARN_UNUSED_RESULT virtual int onResized(language::Device& dev,
                                           command::CommandBuilder& builder,
                                           VkExtent2D oldSize) {
    (void)builder;  // builder is not used. Silence the compiler warning.
    (void)oldSize;  // oldSize is not used. Silence the compiler warning.
    VkResult v = vkDeviceWaitIdle(dev.dev);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "%s failed: %d (%s)\n",
              "ResizeDeviceWaitIdle: vkDeviceWaitIdle", v, string_VkResult(v));
      return 1;
    }
    return 0;
  }
};

// ResizeResetSwapChain is a convenient SwapChainResizeObserver which
// just calls language::Device::resetSwapChain.
struct ResizeResetSwapChain : public SwapChainResizeObserver {
  // onResized implements SwapChainResizeObserver::onResized.
  WARN_UNUSED_RESULT virtual int onResized(language::Device& dev,
                                           command::CommandBuilder& builder,
                                           VkExtent2D oldSize) {
    (void)builder;  // builder is not used. Silence the compiler warning.
    (void)oldSize;  // oldSize is not used. Silence the compiler warning.
    return dev.resetSwapChain();
  }
};

struct SwapChainResizeList {
  SwapChainResizeList() {
    // Set up some default initial SwapChainResizeObservers.
    list.emplace_back(&resizeDeviceWaitIdle);
    list.emplace_back(&resizeResetSwapChain);
  }
  ResizeDeviceWaitIdle resizeDeviceWaitIdle;
  ResizeResetSwapChain resizeResetSwapChain;

  // list contains all SwapChainResizeObserver to be notified in onResized.
  std::vector<SwapChainResizeObserver*> list;

  // syncResize notifies all SwapChainResizeObserver in list and waits until
  // the device has completed any command buffers.
  int syncResize(command::CommandPool& pool, VkExtent2D newSize,
                 size_t poolQindex = 0) {
    command::CommandBuilder rebuilder(pool);
    if (rebuilder.beginOneTimeUse()) {
      fprintf(stderr, "SwapChainResizeList: beginOneTimeUse failed\n");
      return 1;
    }
    auto oldSize = pool.dev.swapChainInfo.imageExtent;
    pool.dev.swapChainInfo.imageExtent = newSize;
    for (size_t i = 0; i < list.size(); i++) {
      auto* observer = list.at(i);
      if (observer->onResized(pool.dev, rebuilder, oldSize)) {
        fprintf(stderr, "SwapChainResizeList: an observer failed\n");
        return 1;
      }
    }
    if (rebuilder.end() || rebuilder.submit(0)) {
      fprintf(stderr, "SwapChainResizeList: rebuilder failed\n");
      return 1;
    }
    vkQueueWaitIdle(pool.q(poolQindex));
    return 0;
  }
};

// PipeBuilder is a builder for command::Pipeline.
// PipeBuilder immediately installs a new command::Pipeline in the
// command::RenderPass it gets in its constructor, so instantiating a
// PipeBuilder is an immediate commitment to completing the Pipeline before
// calling RenderPass:ctorError().
typedef struct PipeBuilder : public SwapChainResizeObserver {
  PipeBuilder(language::Device& dev, command::RenderPass& pass)
      : pipeline{pass.addPipeline(dev)}, depthImage{dev}, depthImageView{dev} {}
  PipeBuilder(PipeBuilder&&) = default;
  PipeBuilder(const PipeBuilder& other) = delete;

  command::Pipeline& pipeline;

  // addDepthImage adds a depth buffer to the pipeline, choosing the first
  // of formatChoices that is available.
  // Because addDepthImage automatically calls onResized, you must pass in a
  // valid builder for onResized.
  //
  // Note: after calling addDepthImage(), the RenderPass holds a reference to
  // PipeBuilder::depthImage. Do not delete PipeBuilder while RenderPass still
  // exists. (If not using addDepthImage, feel free to delete PipeBuilder.)
  WARN_UNUSED_RESULT int addDepthImage(
      language::Device& dev, command::RenderPass& pass,
      command::CommandBuilder& builder,
      const std::vector<VkFormat>& formatChoices);

  // addVertexInput initializes a vertex *type* as an input to shaders. The
  // type variable is passed to addVertexInput at compile time to define the
  // vertex structure.
  //
  // You must define the structure of your vertex shader inputs,
  // and also define a static method getAttributes() returning
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
  template <typename T>
  WARN_UNUSED_RESULT int addVertexInput() {
    return addVertexInputBySize(sizeof(T), T::getAttributes());
  }

  // addVertexInputBySize is the non-template version of addVertexInput().
  WARN_UNUSED_RESULT int addVertexInputBySize(
      size_t nBytes,
      const std::vector<VkVertexInputAttributeDescription> typeAttributes) {
    vertexInputs.emplace_back();
    VkVertexInputBindingDescription& bindingDescription = vertexInputs.back();
    bindingDescription.binding = 0;
    bindingDescription.stride = nBytes;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    pipeline.info.vertsci.vertexBindingDescriptionCount = vertexInputs.size();
    pipeline.info.vertsci.pVertexBindingDescriptions = vertexInputs.data();

    attributeInputs.insert(attributeInputs.end(), typeAttributes.begin(),
                           typeAttributes.end());
    pipeline.info.vertsci.vertexAttributeDescriptionCount =
        attributeInputs.size();
    pipeline.info.vertsci.pVertexAttributeDescriptions = attributeInputs.data();
    return 0;
  }

  // onResized allows PipeBuilder to rebuild itself when the swapChain is
  // resized.
  WARN_UNUSED_RESULT virtual int onResized(language::Device& dev,
                                           command::CommandBuilder& builder,
                                           VkExtent2D oldSize);

  std::vector<VkVertexInputBindingDescription> vertexInputs;
  std::vector<VkVertexInputAttributeDescription> attributeInputs;
  memory::Image depthImage;
  language::ImageView depthImageView;
} PipeBuilder;

#ifdef USE_SPIRV_CROSS_REFLECTION

// DescriptorLibrary is the DescriptorSet objects and DescriptorPool they are
// allocated from.
class DescriptorLibrary {
 public:
  DescriptorLibrary(language::Device& dev) : pool{dev} {}

  std::vector<memory::DescriptorSetLayout> layouts;

  // makeSet creates a new DescriptorSet from layouts[layoutI].
  // A shaders that declares "layout(set = N)" for N > 0 results in
  // needing a layoutI > 0 here.
  //
  // Example usage:
  //   DescriptorLibrary library;
  //   // You MUST call makeDescriptorLibrary to init library.
  //   shaderLibrary.makeDescriptorLibrary(library);
  //   std::unique_ptr<memory::DescriptorSet> d(library.makeSet(pipeBuilder));
  //   if (!d) {
  //     handleErrors;
  //   }
  std::unique_ptr<memory::DescriptorSet> makeSet(PipeBuilder& pipe,
                                                 size_t layoutI = 0);

 protected:
  friend class ShaderLibrary;
  memory::DescriptorPool pool;
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
  ShaderLibrary(language::Device& dev) : dev{dev}, _i(nullptr) {}
  virtual ~ShaderLibrary();

  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const uint32_t* spvBegin, size_t len);
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const void* spvBegin, size_t len) {
    return load(reinterpret_cast<const uint32_t*>(spvBegin), len);
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const void* spvBegin,
                                        const void* spvEnd) {
    return load(spvBegin, reinterpret_cast<const char*>(spvEnd) -
                              reinterpret_cast<const char*>(spvBegin));
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const std::vector<char>& spv) {
    return load(&*spv.begin(), &*spv.end());
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const std::vector<uint32_t>& spv) {
    return load(&*spv.begin(), &*spv.end());
  }
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(const char* filename);
  // load creates and calls loadSPV() on a Shader. If loadSPV() fails, it
  // returns an empty shared_ptr.
  std::shared_ptr<command::Shader> load(std::string filename) {
    return load(filename.c_str());
  }

  // stage puts a shader into a pipeline at the specified stageBits.
  WARN_UNUSED_RESULT int stage(command::RenderPass& renderPass,
                               PipeBuilder& pipe,
                               VkShaderStageFlagBits stageBits,
                               std::shared_ptr<command::Shader> shader,
                               std::string entryPointName = "main");

  // makeDescriptorLibrary inits a DescriptorLibrary to the ShaderLibrary and
  // inits its layouts from the layouts in the shaders in ShaderLibrary.
  //
  // Note: you MUST call load() at least once before calling
  // makeDescriptorLibrary.
  int makeDescriptorLibrary(DescriptorLibrary& descriptorLibrary);

 protected:
  language::Device& dev;
  ShaderLibraryInternal* _i;
};

#endif /*USE_SPIRV_CROSS_REFLECTION*/

}  // namespace science
