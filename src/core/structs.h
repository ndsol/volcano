/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * This header defines structures which gather related sub-structs as a single
 * struct which can add a VolcanoReflectionMap. All of this makes finding the
 * Vulkan field a little easier.
 *
 * E.g. VkPhysicalDeviceFeatures2 has a sub-struct VkPhysicalDeviceFeatures.
 * Accessing it explicitly looks like this:
 *   dev.enabledFeatures.features.samplerAnisotropy
 * It can be re-written as (using reflection to simplify the expression):
 *   dev.enabledFeatures.get("samplerAnisotropy", value)
 *
 * This file enumerates the sub-structures which are gathered into one.
 * "reflectionmap.h" is the C++ reflection implementation.
 *
 * "structs.h" #includes "reflectionmap.h" which #includes "VkPtr.h". Typically
 * your app only need #include <src/language/language.h> which will pull in the
 * correct headers.
 */

#include <vector>

#include "reflectionmap.h"

#pragma once

namespace language {

#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(COMPILER_MSVC)
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

// Forward declaration of Device defined in language.h.
struct Device;

// DeviceFeatures gathers all the structures that are supported by Volcano
// for VkPhysicalDeviceFeatures2.
//
// See Device::availableFeatures and Device::enabledFeatures. availableFeatures
// is filled by Instance::ctorError(). VK_EXT_* structs are queried if the
// extension is available, but your app must add the extension name to
// Device::requiredExtensions before Instance::open for it to be enabled in
// Device::enabledFeatures.
struct DeviceFeatures : VkPhysicalDeviceFeatures2 {
  DeviceFeatures() { setupReflect(); }
  DeviceFeatures(const VkPhysicalDeviceFeatures2& f)
      : VkPhysicalDeviceFeatures2(f) {
    setupReflect();
  }

  // reset clears all structures to their default (zero) values.
  void reset();

  // getFeatures is a wrapper around vkGetPhysicalDeviceFeatures (and
  // vkGetPhysicalDeviceFeatures2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getFeatures(Device& dev);

  // get returns the named field regardless of which sub-structure it is in.
  // Reading a field from an extension not loaded gives meaningless data.
  int get(const char* fieldName, VkBool32& result) {
    return reflect.get(fieldName, result);
  }

  // set sets the named field regardless of which sub-structure it is in.
  // Writing a field from an extension not loaded is meaningless.
  WARN_UNUSED_RESULT int set(const char* fieldName, VkBool32 value) {
    return reflect.set(fieldName, value);
  }

  VkPhysicalDeviceVariablePointerFeatures variablePointer;
  VkPhysicalDeviceMultiviewFeatures multiview;
  VkPhysicalDeviceProtectedMemoryFeatures drm;
  VkPhysicalDeviceShaderDrawParameterFeatures shaderDraw;
  VkPhysicalDevice16BitStorageFeatures storage16Bit;

  // Used if VK_EXT_blend_operation_advanced:
  VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT blendOpAdvanced;

  // Used if VK_EXT_descriptor_indexing:
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing;

  // Used if VK_KHR_vulkan_memory_model
  VkPhysicalDeviceVulkanMemoryModelFeaturesKHR memoryModel;

  VolcanoReflectionMap reflect;

 private:
  void setupReflect();
};

// PhysicalDeviceProperties gathers all the structures that are supported by
// Volcano for VkPhysicalDeviceProperties2.
struct PhysicalDeviceProperties : VkPhysicalDeviceProperties2 {
  PhysicalDeviceProperties() { setupReflect(); }
  PhysicalDeviceProperties(const VkPhysicalDeviceProperties2& p)
      : VkPhysicalDeviceProperties2(p) {
    setupReflect();
  }

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetPhysicalDeviceProperties (and
  // vkGetPhysicalDeviceProperties2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getProperties(Device& dev);

  VkPhysicalDeviceIDProperties id;
  VkPhysicalDeviceMaintenance3Properties maint3;
  VkPhysicalDeviceMultiviewProperties multiview;
  VkPhysicalDevicePointClippingProperties pointClipping;
  VkPhysicalDeviceProtectedMemoryProperties drm;
  VkPhysicalDeviceSubgroupProperties subgroup;

  // Used if VK_EXT_blend_operation_advanced:
  VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT blendOpAdvanced;

  // Used if VK_EXT_conservative_rasterization:
  VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservativeRasterize;

  // Used if VK_EXT_descriptor_indexing:
  VkPhysicalDeviceDescriptorIndexingPropertiesEXT descriptorIndexing;

  // Used if VK_EXT_discard_rectangles:
  VkPhysicalDeviceDiscardRectanglePropertiesEXT discardRectangle;

  // Used if VK_EXT_external_memory_host:
  VkPhysicalDeviceExternalMemoryHostPropertiesEXT externalMemoryHost;

  // Used if VK_EXT_sample_locations:
  VkPhysicalDeviceSampleLocationsPropertiesEXT sampleLocations;

  // Used if VK_EXT_sampler_filter_minmax:
  VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT samplerFilterMinmax;

  // Used if VK_EXT_vertex_attribute_divisor:
  VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT vertexAttributeDivisor;

  // Used if VK_KHR_push_descriptor:
  VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptor;

  // Used if VK_NVX_multiview_per_view_attributes:
  VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX
      nvMultiviewPerViewAttr;

  // Used if VK_AMD_shader_core_properties:
  VkPhysicalDeviceShaderCorePropertiesAMD amdShaderCore;

#if VK_HEADER_VERSION > 90
  // Used if VK_KHR_driver_properties:
  VkPhysicalDeviceDriverPropertiesKHR driverProperties;
#endif /* VK_HEADER_VERSION */

  VolcanoReflectionMap reflect;

 private:
  void setupReflect();
};

// FormatProperties gathers all the structures that are supported by Volcano
// for VkFormatProperties2.
//
// Note: VkFormatProperties2 is for the VkFormat format member in this struct,
//       and any other VkFormat will have a different VkFormatProperties2.
struct FormatProperties : VkFormatProperties2 {
  FormatProperties(VkFormat format_) : format(format_) { reset(); }

  const VkFormat format;

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetFormatProperties (and
  // vkGetFormatProperties2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getProperties(Device& dev);

  // There are no sub-structures defined for VkFormatProperties2 at this time.
};

// DeviceMemoryProperties gathers all the structures that are supported by
// Volcano for VkPhysicalDeviceMemoryProperties2.
struct DeviceMemoryProperties : VkPhysicalDeviceMemoryProperties2 {
  DeviceMemoryProperties() { reset(); }
  DeviceMemoryProperties(const VkPhysicalDeviceMemoryProperties2& p)
      : VkPhysicalDeviceMemoryProperties2(p) {}

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetPhysicalDeviceMemoryProperties (and
  // vkGetPhysicalDeviceMemoryProperties2).
  // Your app does not need to call get(): this is called from
  // Instance::ctorError while setting up the Device.
  WARN_UNUSED_RESULT int getProperties(Device& dev);

  // There are no sub-structures defined for VkPhysicalDeviceMemoryProperties2
  // at this time.
};

// TODO: SparseImageFormatProperties for VkSparseImageFormatProperties2.
// retrieved via vkGetPhysicalDeviceSparseImageFormatProperties2
// which uses VkPhysicalDeviceSparseImageFormatInfo2
// (vkGetPhysicalDeviceSparseImageFormatProperties passes all as args)

// ImageFormatProperties gathers all the structures that are supported by
// Volcano for VkImageFormatProperties2.
struct ImageFormatProperties : VkImageFormatProperties2 {
  ImageFormatProperties() { reset(); }
  ImageFormatProperties(const VkImageFormatProperties2& p)
      : VkImageFormatProperties2(p) {}

  // reset clears all structures to their default (zero) values.
  void reset();

  // getProperties is a wrapper around vkGetPhysicalDeviceImageFormatProperties
  // (and vkGetPhysicalDeviceImageFormatProperties2).
  //
  // This form takes as input the fields for VkPhysicalDeviceImageFormatInfo2.
  // optionalExternalMemoryFlags should only be nonzero if requesting support
  // for external memory.
  WARN_UNUSED_RESULT VkResult getProperties(
      Device& dev, VkFormat format, VkImageType type, VkImageTiling tiling,
      VkImageUsageFlags usage, VkImageCreateFlags flags,
      VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags =
          (VkExternalMemoryHandleTypeFlagBits)0);

  // getProperties is a wrapper around vkGetPhysicalDeviceImageFormatProperties
  // (and vkGetPhysicalDeviceImageFormatProperties2).
  //
  // This is a convenience method that accepts VkImageCreateInfo.
  // Example:
  //   memory::Image img;
  //   // Call getProperties before img.ctorError to check for any issues.
  //   ImageFormatProperties props;
  //   if (props.getProperties(dev, img.info)) { ... }
  WARN_UNUSED_RESULT VkResult
  getProperties(Device& dev, const VkImageCreateInfo& ici,
                VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags =
                    (VkExternalMemoryHandleTypeFlagBits)0) {
    return getProperties(dev, ici.format, ici.imageType, ici.tiling, ici.usage,
                         ici.flags, optionalExternalMemoryFlags);
  }

  VkSamplerYcbcrConversionImageFormatProperties ycbcrConversion;

  // Used if getProperties is called with optionalExternalMemoryFlags nonzero.
  // i.e. nonzero requests support for external memory.
  VkExternalImageFormatProperties externalImage;

#ifdef VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_SPEC_VERSION
  // Used if VK_ANDROID_external_memory_android_hardware_buffer:
  VkAndroidHardwareBufferUsageANDROID androidHardware;
#endif

  // Used if VK_AMD_texture_gather_bias_lod:
  VkTextureLODGatherFormatPropertiesAMD amdLODGather;
};

// SurfaceSupport encodes the result of vkGetPhysicalDeviceSurfaceSupportKHR().
// As an exception, the GRAPHICS value in QueueFamilyProperties requests a
// VkQueue with queueFlags & VK_QUEUE_GRAPHICS_BIT in Instance::requestQfams()
// and Device::getQfamI().
//
// TODO: Add COMPUTE.
// GRAPHICS and COMPUTE support are not tied to a surface, but volcano makes the
// simplifying assumption that all these bits can be lumped together here.
enum SurfaceSupport {
  UNDEFINED = 0,
  NONE = 1,
  PRESENT = 2,

  GRAPHICS = 0x1000,  // Special case. Not used in QueueFamilyProperties.
};

// QueueFamilyProperties gathers all the structures that are supported by
// Volcano for VkQueueFamilyProperties2. SurfaceSupport is also per-QueueFamily
// as a simplifying assumption.
//
// There is no getProperties for this class because the Vulkan API does not
// support querying a single VkQueueFamilyProperties2, only an array. See
// Instance::initSupportedQueues() which calls getQueueFamilies()
struct QueueFamilyProperties : VkQueueFamilyProperties2 {
  QueueFamilyProperties() { reset(); }
  QueueFamilyProperties(const VkQueueFamilyProperties2& p)
      : VkQueueFamilyProperties2(p) {
    surfaceSupport_ = NONE;
    // TODO: set pNext here if any sub-structs are added.
  }

  // surfaceSupport is what vkGetPhysicalDeviceSurfaceSupportKHR reported for
  // this QueueFamily.
  SurfaceSupport surfaceSupport() const { return surfaceSupport_; }

  // setSurfaceSupport sets surfaceSupport_.
  void setSurfaceSupport(SurfaceSupport s) { surfaceSupport_ = s; }

  inline bool isGraphics() const {
    return queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }

  // prios and queues store what VkQueues were actually created.
  // Populated only after open().
  std::vector<float> prios;

  // queues and prios store what VkQueues were actually created.
  // Populated only after open().
  std::vector<VkQueue> queues;

  // There are no sub-structures defined for VkQueueFamilyProperties2
  // at this time.

 protected:
  // reset clears VkQueueFamilyProperties2 and any sub-structures. It also
  // resets surfaceSupport_ to NONE. This is protected and hidden because there
  // is no getProperties and thus no way for your app to use reset().
  void reset();

  SurfaceSupport surfaceSupport_;
};

template <typename T>
VkObjectType getObjectType(const T) {
  return VK_OBJECT_TYPE_UNKNOWN;
};

template <>
inline VkObjectType getObjectType(const VkInstance) {
  return VK_OBJECT_TYPE_INSTANCE;
};

template <>
inline VkObjectType getObjectType(const VkPhysicalDevice) {
  return VK_OBJECT_TYPE_PHYSICAL_DEVICE;
};

template <>
inline VkObjectType getObjectType(const VkDevice) {
  return VK_OBJECT_TYPE_DEVICE;
};

template <>
inline VkObjectType getObjectType(const VkQueue) {
  return VK_OBJECT_TYPE_QUEUE;
};

template <>
inline VkObjectType getObjectType(const VkSemaphore) {
  return VK_OBJECT_TYPE_SEMAPHORE;
};

template <>
inline VkObjectType getObjectType(const VkCommandPool) {
  return VK_OBJECT_TYPE_COMMAND_POOL;
};

template <>
inline VkObjectType getObjectType(const VkCommandBuffer) {
  return VK_OBJECT_TYPE_COMMAND_BUFFER;
};

template <>
inline VkObjectType getObjectType(const VkFence) {
  return VK_OBJECT_TYPE_FENCE;
};

template <>
inline VkObjectType getObjectType(const VkDeviceMemory) {
  return VK_OBJECT_TYPE_DEVICE_MEMORY;
};

template <>
inline VkObjectType getObjectType(const VkBuffer) {
  return VK_OBJECT_TYPE_BUFFER;
};

template <>
inline VkObjectType getObjectType(const VkImage) {
  return VK_OBJECT_TYPE_IMAGE;
};

template <>
inline VkObjectType getObjectType(const VkSampler) {
  return VK_OBJECT_TYPE_SAMPLER;
};

template <>
inline VkObjectType getObjectType(const VkFramebuffer) {
  return VK_OBJECT_TYPE_FRAMEBUFFER;
};

template <>
inline VkObjectType getObjectType(const VkSwapchainKHR) {
  return VK_OBJECT_TYPE_SWAPCHAIN_KHR;
};

template <>
inline VkObjectType getObjectType(const VkSurfaceKHR) {
  return VK_OBJECT_TYPE_SURFACE_KHR;
};

template <>
inline VkObjectType getObjectType(const VkEvent) {
  return VK_OBJECT_TYPE_EVENT;
};

template <>
inline VkObjectType getObjectType(const VkQueryPool) {
  return VK_OBJECT_TYPE_QUERY_POOL;
};

template <>
inline VkObjectType getObjectType(const VkBufferView) {
  return VK_OBJECT_TYPE_BUFFER_VIEW;
};

template <>
inline VkObjectType getObjectType(const VkImageView) {
  return VK_OBJECT_TYPE_IMAGE_VIEW;
};

template <>
inline VkObjectType getObjectType(const VkShaderModule) {
  return VK_OBJECT_TYPE_SHADER_MODULE;
};

template <>
inline VkObjectType getObjectType(const VkRenderPass) {
  return VK_OBJECT_TYPE_RENDER_PASS;
};

template <>
inline VkObjectType getObjectType(const VkPipeline) {
  return VK_OBJECT_TYPE_PIPELINE;
};

template <>
inline VkObjectType getObjectType(const VkPipelineLayout) {
  return VK_OBJECT_TYPE_PIPELINE_LAYOUT;
};

template <>
inline VkObjectType getObjectType(const VkDescriptorPool) {
  return VK_OBJECT_TYPE_DESCRIPTOR_POOL;
};

template <>
inline VkObjectType getObjectType(const VkDescriptorSetLayout) {
  return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
};

template <>
inline VkObjectType getObjectType(const VkDescriptorSet) {
  return VK_OBJECT_TYPE_DESCRIPTOR_SET;
};

// DeviceFunctionPointers contains function pointers that must be loaded after
// an extension is loaded. Your app would add the extension to
// Device::requiredExtensions before calling Instance::open().
//
// If a given extension is loaded, all the function pointers for that extension
// are guaranteed to be non-null. If the function pointers are not all found,
// the extension will be rejected.
typedef struct DeviceFunctionPointers {
  // load rejects any extension that does not load. Implemented in open.cpp.
  void load(Device& dev);

  // If VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME is loaded:
  PFN_vkCreateRenderPass2KHR createRenderPass2{nullptr};
  PFN_vkCmdBeginRenderPass2KHR beginRenderPass2{nullptr};
  PFN_vkCmdNextSubpass2KHR nextSubpass2{nullptr};
  PFN_vkCmdEndRenderPass2KHR endRenderPass2{nullptr};
  // If VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME is loaded:
  PFN_vkCmdPushDescriptorSetKHR pushDescriptorSet{nullptr};
  PFN_vkCmdPushDescriptorSetWithTemplateKHR pushDescriptorSetWithTemplate{
      nullptr};
  // If VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME is loaded:
  PFN_vkCmdDrawIndirectCountKHR drawIndirectCount{nullptr};
  PFN_vkCmdDrawIndexedIndirectCountKHR drawIndexedIndirectCount{nullptr};
  // If VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME is loaded:
  PFN_vkCmdBindTransformFeedbackBuffersEXT bindTransformFeedbackBuffers{
      nullptr};
  PFN_vkCmdBeginTransformFeedbackEXT beginTransformFeedback{nullptr};
  PFN_vkCmdEndTransformFeedbackEXT endTransformFeedback{nullptr};
  PFN_vkCmdBeginQueryIndexedEXT beginQueryIndexed{nullptr};
  PFN_vkCmdEndQueryIndexedEXT endQueryIndexed{nullptr};
  PFN_vkCmdDrawIndirectByteCountEXT drawIndirectByteCount{nullptr};
  // If VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME is loaded:
  PFN_vkCmdBeginConditionalRenderingEXT beginConditionalRendering{nullptr};
  PFN_vkCmdEndConditionalRenderingEXT endConditionalRendering{nullptr};
  // If VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME is loaded:
  PFN_vkCmdSetDiscardRectangleEXT setDiscardRectangle{nullptr};
  // If VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME is loaded:
  PFN_vkCmdSetSampleLocationsEXT setSampleLocations{nullptr};
  PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT
      getPhysicalDeviceMultisampleProperties{nullptr};
} DeviceFunctionPointers;

}  // namespace language

#ifdef _WIN32
#define OS_SEPARATOR '\\'
#else
#define OS_SEPARATOR '/'
#endif

// On non-Android platforms, search several paths for a filename.
// Note: foundPath is corrupted during the search. If findInPaths returns
// 0, foundPath is valid. Otherwise, foundPath is the last failed path tried.
//
// For Android, this is a no-op that always returns filename and 0.
int findInPaths(const char* filename, std::string& foundPath);

// Non-Android: getSelfPath() provides the path of the executable.
// Android: getSelfPath() returns the empty string.
void getSelfPath(std::string& out);

// Platform-independent mmap implementation.
typedef struct MMapFile {
  MMapFile()
      : map(nullptr),
        len(0),
        winFileHandle(nullptr),
        winMmapHandle(nullptr),
        fd(-1){};
  virtual ~MMapFile();

  // mmap opens and memory-maps the file.
  // The mapped file is accessible using the 'map' member.
  // If len_ is not specified or 0, the mapping is to the end of the file.
  // Returns 0=success, 1=failure.
  WARN_UNUSED_RESULT int mmapRead(const char* filename, int64_t offset = 0,
                                  int64_t len_ = 0);

  // TODO: mapWrite() for a writable mapping.

  // munmap or the destructor will remove the memory mapping.
  WARN_UNUSED_RESULT int munmap();

  void* map;
  int64_t len;
  void* winFileHandle;
  void* winMmapHandle;
  int fd;
} MMapFile;
