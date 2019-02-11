/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */

#include <src/language/language.h>

#include <vector>

#include "VkInit.h"

namespace language {

reflect::Pointer* VolcanoReflectionMap::getField(const char* fieldName) {
  auto i = find(fieldName);
  return (i == end()) ? nullptr : &i->second;
}

// get specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::get(const char* fieldName, VkBool32& value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->getVkBool32(fieldName, value);
  }
  logE("%s(%s): field not found\n", "get", fieldName);
  return 1;
}

// set specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::set(const char* fieldName, VkBool32 value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->setVkBool32(fieldName, value);
  }
  logE("%s(%s): field not found\n", "set", fieldName);
  return 1;
}

// addField specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::addField(const char* fieldName, VkBool32* field) {
  auto pointer = getField(fieldName);
  if (pointer) {
    logW("addField(%s): already exists, type %s\n", fieldName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](fieldName);
  pointer->addFieldVkBool32(field);
  return 0;
}

// addArrayField specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::addArrayField(const char* arrayName, VkBool32* field,
                                        size_t len) {
  auto pointer = getField(arrayName);
  if (pointer) {
    logW("addArrayField(%s): already exists, type %s\n", arrayName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](arrayName);
  pointer->addArrayFieldVkBool32(field, len);
  return 0;
}

#if SIZE_MAX != UINT32_MAX
// get specialized because size_t is a typedef of unsigned long
// (skip this specialization if size_t is the same size as uint32_t)
template <>
int VolcanoReflectionMap::get(const char* fieldName, size_t& value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->getsize_t(fieldName, value);
  }
  logE("%s(%s): field not found\n", "get", fieldName);
  return 1;
}

// set specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::set(const char* fieldName, size_t value) {
  auto pointer = getField(fieldName);
  if (pointer) {
    return pointer->setsize_t(fieldName, value);
  }
  logE("%s(%s): field not found\n", "set", fieldName);
  return 1;
}

// addField specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::addField(const char* fieldName, size_t* field) {
  auto pointer = getField(fieldName);
  if (pointer) {
    logW("addField(%s): already exists, type %s\n", fieldName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](fieldName);
  pointer->addFieldsize_t(field);
  return 0;
}

// addArrayField specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::addArrayField(const char* arrayName, size_t* field,
                                        size_t len) {
  auto pointer = getField(arrayName);
  if (pointer) {
    logW("addArrayField(%s): already exists, type %s\n", arrayName,
         pointer->desc.toString().c_str());
    return 1;
  }
  pointer = &operator[](arrayName);
  pointer->addArrayFieldsize_t(field, len);
  return 0;
}
#endif /*SIZE_MAX != UINT32_MAX*/

// get specialized because const_string is read-only
template <>
int VolcanoReflectionMap::get(const char* fieldName, std::string& value) {
  auto pointer = getField(fieldName);
  if (!pointer) {
    logE("%s(%s): field not found\n", "get", fieldName);
    return 1;
  }
  if (pointer->checkType("get", fieldName,
                         reflect::PointerType::value_const_string,
                         reflect::PointerAttr::NONE)) {
    return 1;
  }
  value = pointer->const_string;
  return 0;
}

void DeviceFeatures::reset() {
  VkOverwrite(*this);
  VkOverwrite(variablePointer);
  VkOverwrite(multiview);
  VkOverwrite(drm);
  VkOverwrite(shaderDraw);
  VkOverwrite(storage16Bit);
  VkOverwrite(blendOpAdvanced);
  VkOverwrite(descriptorIndexing);
  VkOverwrite(memoryModel);
}

int DeviceFeatures::getFeatures(Device& dev) {
  reset();
#define ifExtension(extname, membername)   \
  if (dev.isExtensionAvailable(extname)) { \
    *ppNext = &membername;                 \
    ppNext = &membername.pNext;            \
  }
#if VK_HEADER_VERSION != 96
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceFeatures(dev.phys, &features);
    return 0;
#ifndef __ANDROID__
  }

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &variablePointer;
  variablePointer.pNext = &multiview;
  multiview.pNext = &drm;
  drm.pNext = &shaderDraw;
  shaderDraw.pNext = &storage16Bit;
  auto ppNext = &storage16Bit.pNext;

#ifdef VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME
  ifExtension(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME, blendOpAdvanced);
#endif /* VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME */
#ifdef VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
  ifExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, descriptorIndexing);
#endif /* VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME */
#ifdef VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME
  ifExtension(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, memoryModel);
#endif /* VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME */

  vkGetPhysicalDeviceFeatures2(dev.phys, this);
  return 0;
#endif /* __ANDROID__ */
}

void PhysicalDeviceProperties::reset() {
  VkOverwrite(*this);
  VkOverwrite(id);
  VkOverwrite(maint3);
  VkOverwrite(multiview);
  VkOverwrite(pointClipping);
  VkOverwrite(drm);
  VkOverwrite(subgroup);
  VkOverwrite(blendOpAdvanced);
  VkOverwrite(conservativeRasterize);
  VkOverwrite(descriptorIndexing);
  VkOverwrite(discardRectangle);
  VkOverwrite(externalMemoryHost);
  VkOverwrite(sampleLocations);
  VkOverwrite(samplerFilterMinmax);
  VkOverwrite(vertexAttributeDivisor);
  VkOverwrite(pushDescriptor);
  VkOverwrite(nvMultiviewPerViewAttr);
  VkOverwrite(amdShaderCore);
}

int PhysicalDeviceProperties::getProperties(Device& dev) {
  reset();
#if VK_HEADER_VERSION != 96
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceProperties(dev.phys, &properties);
    return 0;
#ifndef __ANDROID__
  }

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &id;
  id.pNext = &maint3;
  maint3.pNext = &multiview;
  multiview.pNext = &pointClipping;
  pointClipping.pNext = &drm;
  drm.pNext = &subgroup;
  auto ppNext = &subgroup.pNext;

  ifExtension("VK_EXT_blend_operation_advanced", blendOpAdvanced);
  ifExtension("VK_EXT_conservative_rasterization", conservativeRasterize);
  ifExtension("VK_EXT_descriptor_indexing", descriptorIndexing);
  ifExtension("VK_EXT_discard_rectangles", discardRectangle);
  ifExtension("VK_EXT_external_memory_host", externalMemoryHost);
  ifExtension("VK_EXT_sample_locations", sampleLocations);
  ifExtension("VK_EXT_sampler_filter_minmax", samplerFilterMinmax);
  ifExtension("VK_EXT_vertex_attribute_divisor", vertexAttributeDivisor);
  ifExtension("VK_KHR_push_descriptor", pushDescriptor);
  ifExtension("VK_NVX_multiview_per_view_attributes", nvMultiviewPerViewAttr);
  ifExtension("VK_AMD_shader_core_properties", amdShaderCore);
#if VK_HEADER_VERSION > 90
  ifExtension("VK_KHR_driver_properties", driverProperties);
#endif /* VK_HEADER_VERSION */

  vkGetPhysicalDeviceProperties2(dev.phys, this);
  return 0;
#endif /* __ANDROID__ */
}

void FormatProperties::reset() { VkOverwrite(*this); }

int FormatProperties::getProperties(Device& dev) {
  reset();
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceFormatProperties(dev.phys, format, &formatProperties);
    return 0;
#ifndef __ANDROID__
  }

  vkGetPhysicalDeviceFormatProperties2(dev.phys, format, this);
  return 0;
#endif /* __ANDROID__ */
}

void DeviceMemoryProperties::reset() { VkOverwrite(*this); }

int DeviceMemoryProperties::getProperties(Device& dev) {
  reset();
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetPhysicalDeviceMemoryProperties(dev.phys, &memoryProperties);
    return 0;
#ifndef __ANDROID__
  }

  vkGetPhysicalDeviceMemoryProperties2(dev.phys, this);
  return 0;
#endif /* __ANDROID__ */
}

void ImageFormatProperties::reset() {
  VkOverwrite(*this);
  VkOverwrite(externalImage);
  VkOverwrite(ycbcrConversion);
#ifdef VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_SPEC_VERSION
  VkOverwrite(androidHardware);
#endif
  VkOverwrite(amdLODGather);
}

VkResult ImageFormatProperties::getProperties(
    Device& dev, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags /*= 0*/) {
  reset();
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#else  /*__ANDROID__*/
  (void)optionalExternalMemoryFlags;
#endif /*__ANDROID__*/
    if (!usage) {
      // This time, the Vulkan 1.0 code is getting called. But warn about
      // Vulkan 1.1 issues that will arise soon enough.
      logW("%s needs usage != 0, please fix\n",
           "vkGetPhysicalDeviceImageFormatProperties2");
    }
    return vkGetPhysicalDeviceImageFormatProperties(
        dev.phys, format, type, tiling, usage, flags, &imageFormatProperties);
#ifndef __ANDROID__
  }

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &ycbcrConversion;
  auto ppNext = &ycbcrConversion.pNext;
  if (optionalExternalMemoryFlags) {
    *ppNext = &externalImage;
    ppNext = &externalImage.pNext;
  }

#ifdef VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_SPEC_VERSION
  ifExtension("VK_ANDROID_external_memory_android_hardware_buffer",
              androidHardware);
#endif
  ifExtension("VK_AMD_texture_gather_bias_lod", amdLODGather);

  VkPhysicalDeviceImageFormatInfo2 VkInit(ifi);
  VkPhysicalDeviceExternalImageFormatInfo VkInit(externalIfi);
  ifi.format = format;
  ifi.type = type;
  ifi.tiling = tiling;
  ifi.usage = usage;
  ifi.flags = flags;
  if (optionalExternalMemoryFlags) {
    ifi.pNext = &externalIfi;
    externalIfi.handleType = optionalExternalMemoryFlags;
  }

  auto r = vkGetPhysicalDeviceImageFormatProperties2(dev.phys, &ifi, this);

  // if r is an error, return it.
  // if VkImageFormatProperties2::imageFormatProperties is *not* filled with
  // zeros, it indicates success.
  if (r != VK_SUCCESS || imageFormatProperties.maxExtent.width ||
      imageFormatProperties.maxExtent.height ||
      imageFormatProperties.maxExtent.depth ||
      imageFormatProperties.maxMipLevels ||
      imageFormatProperties.maxArrayLayers ||
      imageFormatProperties.sampleCounts ||
      imageFormatProperties.maxResourceSize) {
    return r;
  }

  // if VkImageFormatProperties2::imageFormatProperties is filled with zeros
  logE("VkImageFormatProperties filled with zeros, but VK_SUCCESS returned\n");
  return VK_ERROR_VALIDATION_FAILED_EXT;
#endif /*__ANDROID__*/
}

}  // namespace language
