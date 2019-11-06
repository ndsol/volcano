/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */

#include <src/language/language.h>

#include <vector>

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
  VkPhysicalDeviceFeatures2& pdf2 =
      *static_cast<VkPhysicalDeviceFeatures2*>(this);
  memset(&pdf2, 0, sizeof(pdf2));
  pdf2.sType = autoSType(pdf2);
  memset(&variablePointer, 0, sizeof(variablePointer));
  variablePointer.sType = autoSType(variablePointer);
  memset(&multiview, 0, sizeof(multiview));
  multiview.sType = autoSType(multiview);
  memset(&drm, 0, sizeof(drm));
  drm.sType = autoSType(drm);
  memset(&shaderDraw, 0, sizeof(shaderDraw));
  shaderDraw.sType = autoSType(shaderDraw);
  memset(&storage16Bit, 0, sizeof(storage16Bit));
  storage16Bit.sType = autoSType(storage16Bit);
  memset(&blendOpAdvanced, 0, sizeof(blendOpAdvanced));
  blendOpAdvanced.sType = autoSType(blendOpAdvanced);
  memset(&descriptorIndexing, 0, sizeof(descriptorIndexing));
  descriptorIndexing.sType = autoSType(descriptorIndexing);
  memset(&memoryModel, 0, sizeof(memoryModel));
  memoryModel.sType = autoSType(memoryModel);
}

int DeviceFeatures::getFeatures(Device& dev) {
  reset();
#define ifExtension(extname, membername)   \
  if (dev.isExtensionAvailable(extname)) { \
    *ppNext = &membername;                 \
    ppNext = &membername.pNext;            \
  }
  PFN_vkGetPhysicalDeviceFeatures2 pfn = nullptr;
  if (dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    pfn =
        (decltype(pfn))dev.getInstanceProcAddr("vkGetPhysicalDeviceFeatures2");
    if (!pfn) {
      logW("%s not found, falling back to 1.0\n",
           "vkGetPhysicalDeviceFeatures2");
    }
  }
  if (!pfn) {
    vkGetPhysicalDeviceFeatures(dev.phys, &features);
    return 0;
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

  pfn(dev.phys, this);
  return 0;
}

void PhysicalDeviceProperties::reset() {
  VkPhysicalDeviceProperties2& pdp2 =
      *static_cast<VkPhysicalDeviceProperties2*>(this);
  memset(&pdp2, 0, sizeof(pdp2));
  pdp2.sType = autoSType(pdp2);
  memset(&id, 0, sizeof(id));
  id.sType = autoSType(id);
  memset(&maint3, 0, sizeof(maint3));
  maint3.sType = autoSType(maint3);
  memset(&multiview, 0, sizeof(multiview));
  multiview.sType = autoSType(multiview);
  memset(&pointClipping, 0, sizeof(pointClipping));
  pointClipping.sType = autoSType(pointClipping);
  memset(&drm, 0, sizeof(drm));
  drm.sType = autoSType(drm);
  memset(&subgroup, 0, sizeof(subgroup));
  subgroup.sType = autoSType(subgroup);
  memset(&blendOpAdvanced, 0, sizeof(blendOpAdvanced));
  blendOpAdvanced.sType = autoSType(blendOpAdvanced);
  memset(&conservativeRasterize, 0, sizeof(conservativeRasterize));
  conservativeRasterize.sType = autoSType(conservativeRasterize);
  memset(&descriptorIndexing, 0, sizeof(descriptorIndexing));
  descriptorIndexing.sType = autoSType(descriptorIndexing);
  memset(&discardRectangle, 0, sizeof(discardRectangle));
  discardRectangle.sType = autoSType(discardRectangle);
  memset(&externalMemoryHost, 0, sizeof(externalMemoryHost));
  externalMemoryHost.sType = autoSType(externalMemoryHost);
  memset(&sampleLocations, 0, sizeof(sampleLocations));
  sampleLocations.sType = autoSType(sampleLocations);
  memset(&samplerFilterMinmax, 0, sizeof(samplerFilterMinmax));
  samplerFilterMinmax.sType = autoSType(samplerFilterMinmax);
  memset(&vertexAttributeDivisor, 0, sizeof(vertexAttributeDivisor));
  vertexAttributeDivisor.sType = autoSType(vertexAttributeDivisor);
  memset(&pushDescriptor, 0, sizeof(pushDescriptor));
  pushDescriptor.sType = autoSType(pushDescriptor);
  memset(&nvMultiviewPerViewAttr, 0, sizeof(nvMultiviewPerViewAttr));
  nvMultiviewPerViewAttr.sType = autoSType(nvMultiviewPerViewAttr);
  memset(&amdShaderCore, 0, sizeof(amdShaderCore));
  amdShaderCore.sType = autoSType(amdShaderCore);
}

int PhysicalDeviceProperties::getProperties(Device& dev) {
  reset();
  PFN_vkGetPhysicalDeviceProperties2 pfn = nullptr;
  if (dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    pfn = (decltype(pfn))dev.getInstanceProcAddr(
        "vkGetPhysicalDeviceProperties2");
    if (!pfn) {
      logW("%s not found, falling back to 1.0\n",
           "vkGetPhysicalDeviceFeatures2");
    }
  }
  if (!pfn) {
    vkGetPhysicalDeviceProperties(dev.phys, &properties);
    return 0;
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

  pfn(dev.phys, this);
  return 0;
}

void FormatProperties::reset() {
  VkFormatProperties2& fp2 = *static_cast<VkFormatProperties2*>(this);
  memset(&fp2, 0, sizeof(fp2));
  fp2.sType = autoSType(fp2);
}

int FormatProperties::getProperties(Device& dev) {
  reset();
  PFN_vkGetPhysicalDeviceFormatProperties2 pfn = nullptr;
  if (dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    pfn = (decltype(pfn))dev.getInstanceProcAddr(
        "vkGetPhysicalDeviceFormatProperties2");
    if (!pfn) {
      logW("%s not found, falling back to 1.0\n",
           "vkGetPhysicalDeviceFormatProperties2");
    }
  }
  if (!pfn) {
    vkGetPhysicalDeviceFormatProperties(dev.phys, format, &formatProperties);
    return 0;
  }

  pfn(dev.phys, format, this);
  return 0;
}

void DeviceMemoryProperties::reset() {
  VkPhysicalDeviceMemoryProperties2& dmp2 =
      *static_cast<VkPhysicalDeviceMemoryProperties2*>(this);
  memset(&dmp2, 0, sizeof(dmp2));
  dmp2.sType = autoSType(dmp2);
}

int DeviceMemoryProperties::getProperties(Device& dev) {
  reset();
  PFN_vkGetPhysicalDeviceMemoryProperties2 pfn = nullptr;
  if (dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    pfn = (decltype(pfn))dev.getInstanceProcAddr(
        "vkGetPhysicalDeviceMemoryProperties2");
    if (!pfn) {
      logW("%s not found, falling back to 1.0\n",
           "vkGetPhysicalDeviceMemoryProperties2");
    }
  }
  if (!pfn) {
    vkGetPhysicalDeviceMemoryProperties(dev.phys, &memoryProperties);
    return 0;
  }

  pfn(dev.phys, this);
  return 0;
}

void ImageFormatProperties::reset() {
  VkImageFormatProperties2& ifp2 =
      *static_cast<VkImageFormatProperties2*>(this);
  memset(&ifp2, 0, sizeof(ifp2));
  ifp2.sType = autoSType(ifp2);
  memset(&externalImage, 0, sizeof(externalImage));
  externalImage.sType = autoSType(externalImage);
  memset(&ycbcrConversion, 0, sizeof(ycbcrConversion));
  ycbcrConversion.sType = autoSType(ycbcrConversion);
#ifdef VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_SPEC_VERSION
  memset(&androidHardware, 0, sizeof(androidHardware));
  androidHardware.sType = autoSType(androidHardware);
#endif
  memset(&amdLODGather, 0, sizeof(amdLODGather));
  amdLODGather.sType = autoSType(amdLODGather);
}

VkResult ImageFormatProperties::getProperties(
    Device& dev, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkExternalMemoryHandleTypeFlagBits optionalExternalMemoryFlags /*= 0*/) {
  reset();
  PFN_vkGetPhysicalDeviceImageFormatProperties2 pfn = nullptr;
  if (dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    pfn = (decltype(pfn))dev.getInstanceProcAddr(
        "vkGetPhysicalDeviceImageFormatProperties2");
    if (!pfn) {
      logW("%s not found, falling back to 1.0\n",
           "vkGetPhysicalDeviceImageFormatProperties2");
    }
  }
  if (!pfn) {
    if (!usage) {
      // This time, the Vulkan 1.0 code is getting called. But warn about
      // Vulkan 1.1 issues that will arise soon enough.
      logW("%s needs usage != 0, please fix\n",
           "vkGetPhysicalDeviceImageFormatProperties2");
    }
    return vkGetPhysicalDeviceImageFormatProperties(
        dev.phys, format, type, tiling, usage, flags, &imageFormatProperties);
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

  VkPhysicalDeviceImageFormatInfo2 ifi;
  memset(&ifi, 0, sizeof(ifi));
  ifi.sType = autoSType(ifi);
  ifi.format = format;
  ifi.type = type;
  ifi.tiling = tiling;
  ifi.usage = usage;
  ifi.flags = flags;
  VkPhysicalDeviceExternalImageFormatInfo externalIfi;
  memset(&externalIfi, 0, sizeof(externalIfi));
  externalIfi.sType = autoSType(externalIfi);
  if (optionalExternalMemoryFlags) {
    ifi.pNext = &externalIfi;
    externalIfi.handleType = optionalExternalMemoryFlags;
  }

  auto r = pfn(dev.phys, &ifi, this);

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
}

void SurfaceCapabilities::reset() {
  VkSurfaceCapabilities2KHR& sc2 =
      *static_cast<VkSurfaceCapabilities2KHR*>(this);
  memset(&sc2, 0, sizeof(sc2));
  sc2.sType = autoSType(sc2);
  memset(&nativeHdr, 0, sizeof(nativeHdr));
  nativeHdr.sType = autoSType(nativeHdr);
  memset(&sharedPresent, 0, sizeof(sharedPresent));
  sharedPresent.sType = autoSType(sharedPresent);
#ifdef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
  memset(&fullscreenExclusive, 0, sizeof(fullscreenExclusive));
  fullscreenExclusive.sType = autoSType(fullscreenExclusive);
#endif /*VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME*/
  memset(&drm, 0, sizeof(drm));
  drm.sType = autoSType(drm);
}

VkResult SurfaceCapabilities::getProperties(Device& dev, VkSurfaceKHR s) {
  reset();
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR get2 = NULL;
  if (dev.isExtensionAvailable(
          VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)) {
    get2 = (decltype(get2))vkGetDeviceProcAddr(
        dev.dev, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
  }
  if (!get2) {
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        dev.phys, s, &surfaceCapabilities);
    if (r != VK_SUCCESS) {
      return r;
    }

#ifdef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
    // Query without having vkGetPhysicalDeviceSurfaceCapabilities2KHR
    if (dev.isExtensionAvailable(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME)) {
      PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT getModes =
          (decltype(getModes))vkGetDeviceProcAddr(
              dev.dev, "vkGetPhysicalDeviceSurfacePresentModes2EXT");
      if (getModes) {
        logE("can getModes\n");
        VkSurfaceFullScreenExclusiveInfoEXT fullscreenRequest;
        memset(&fullscreenRequest, 0, sizeof(fullscreenRequest));
        fullscreenRequest.sType = autoSType(fullscreenRequest);
        fullscreenRequest.fullScreenExclusive =
            VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;

        VkSurfaceFullScreenExclusiveWin32InfoEXT win32info;
        memset(&win32info, 0, sizeof(win32info));
        win32info.sType = autoSType(win32info);
        win32info.hmonitor = monitor;
        win32info.pNext = &fullscreenRequest;

        VkPhysicalDeviceSurfaceInfo2KHR surfInfo;
        memset(&surfInfo, 0, sizeof(surfInfo));
        surfInfo.sType = autoSType(surfInfo);
        surfInfo.surface = s;
        surfInfo.pNext = &win32info;
        uint32_t count;
        r = getModes(dev.phys, &surfInfo, &count, NULL);
        if (r != VK_SUCCESS) {
          return VK_SUCCESS;  // Driver does *not* support fullscreenExclusive.
        }
        std::vector<VkPresentModeKHR> modes(count);
        r = getModes(dev.phys, &surfInfo, &count, modes.data());
        if (r != VK_SUCCESS) {
          return VK_SUCCESS;  // Driver does *not* support fullscreenExclusive.
        }
        if (count != modes.size()) {
          logE("%s returned count=%u, larger than previously (%zu)\n",
               "vkGetPhysicalDeviceSurfacePresentModes2EXT(all)", count,
               modes.size());
          return VK_ERROR_VALIDATION_FAILED_EXT;
        }
        logE("did getModes, got %zu modes\n", modes.size());
        // FIXME: Not implement yet: report what modes are found.
        // NOTE: Despite not reporting the modes, this tests whether
        // fullscreenExclusive is supported.
        fullscreenExclusive.fullScreenExclusiveSupported = VK_TRUE;
      }
    }
#endif /*VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME*/
    return VK_SUCCESS;
  }

  VkPhysicalDeviceSurfaceInfo2KHR surfInfo;
  memset(&surfInfo, 0, sizeof(surfInfo));
  surfInfo.sType = autoSType(surfInfo);
  surfInfo.surface = s;

  // Create pNext chain for Vulkan 1.1 features.
  pNext = &nativeHdr;
  nativeHdr.pNext = &sharedPresent;
  auto ppNext = &sharedPresent.pNext;
#ifdef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
  VkSurfaceFullScreenExclusiveWin32InfoEXT win32info;
  memset(&win32info, 0, sizeof(win32info));
  win32info.sType = autoSType(win32info);
  win32info.hmonitor = monitor;
  surfInfo.pNext = &win32info;

  *ppNext = &fullscreenExclusive;
  ppNext = &fullscreenExclusive.pNext;
#endif /*VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME*/
  *ppNext = &drm;

  return get2(dev.phys, &surfInfo, this);
}

}  // namespace language
