/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file selects from the available queue family objects to create queues.
 */
#include <src/core/VkEnum.h>

#include "language.h"

namespace language {
using namespace VkEnum;

void QueueFamilyProperties::reset() {
  VkQueueFamilyProperties2& qfp2 =
      *static_cast<VkQueueFamilyProperties2*>(this);
  memset(&qfp2, 0, sizeof(qfp2));
  qfp2.sType = autoSType(qfp2);
  surfaceSupport_ = NONE;
}

enum templGetStages {
  BEFORE_ENUM = 0,
  AFTER_ENUM,  // tells mapFn to install the queues into dev.
};

template <typename T>
static int templGetQueueFamilies(Device& dev, const char* enumFnName,
                                 int (*mapFn)(Device&, templGetStages,
                                              std::vector<T>&)) {
  void(VKAPI_PTR * enumFn)(VkPhysicalDevice, uint32_t*, T*) =
      (decltype(enumFn))dev.getInstanceProcAddr(enumFnName);
  if (!enumFn) {
    logE("%s not found\n", enumFnName);
    return 1;
  }
  uint32_t qCount = 0;
  enumFn(dev.phys, &qCount, nullptr);
  if (!qCount) {
    logE("%s returned count=0, expected at least 1 queue\n", enumFnName);
    return 1;
  }

  std::vector<T> qfp(qCount);
  if (mapFn(dev, BEFORE_ENUM, qfp)) {
    return 1;
  }
  enumFn(dev.phys, &qCount, qfp.data());
  if (qCount > qfp.size()) {
    // This can happen if a queue family was added between the two calls.
    logF("%s returned count=%u, larger than previously (%zu)\n", enumFnName,
         qCount, qfp.size());
    return 1;
  }
  // AFTER_ENUM tells mapFn to install the queues into dev.
  return mapFn(dev, AFTER_ENUM, qfp);
}

static int mapQueueFamiliyProperties(Device& dev, templGetStages stage,
                                     std::vector<VkQueueFamilyProperties>& v) {
  if (stage == BEFORE_ENUM) {
    return 0;
  }
  dev.qfams.clear();
  dev.qfams.resize(v.size());
  for (size_t i = 0; i < v.size(); i++) {
    dev.qfams.at(i).queueFamilyProperties = v.at(i);
  }
  v.clear();
  return 0;
}

static int mapQueueFamiliyProperties2(
    Device& dev, templGetStages stage,
    std::vector<VkQueueFamilyProperties2>& v) {
  if (stage == BEFORE_ENUM) {
    for (size_t i = 0; i < v.size(); i++) {
      v.at(i) = QueueFamilyProperties();
    }
    return 0;
  }
  dev.qfams.clear();
  dev.qfams.resize(v.size());
  for (size_t i = 0; i < v.size(); i++) {
    dev.qfams.at(i) = QueueFamilyProperties(v.at(i));
  }
  v.clear();
  return 0;
}

static int getQueueFamilies(Device& dev) {
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
    return templGetQueueFamilies<VkQueueFamilyProperties>(
        dev, "vkGetPhysicalDeviceQueueFamilyProperties",
        mapQueueFamiliyProperties);
  } else {
    return templGetQueueFamilies<VkQueueFamilyProperties2>(
        dev, "vkGetPhysicalDeviceQueueFamilyProperties2",
        mapQueueFamiliyProperties2);
  }
}

VkResult Instance::initSupportedQueues(Device& dev) {
  if (dev.physProp.properties.apiVersion < minApiVersion) {
    // Devices are excluded if they do not support minApiVersion
    return VK_INCOMPLETE;
  }

  if (dev.availableFeatures.getFeatures(dev)) {
    logE("initSupportedQueues: availableFeatures.getFeatures failed.\n");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  // Attempt to enable, if supported, features supported by all the below.
  // NOTE: textureCompression features are not supported by all devices, but
  // some are supported by any one device; the app must choose a supported one:
  // * Adreno 330 driver 26.24.512, android 7.0.
  // * Radeon R7 200 driver 1.4.0, ubuntu 16.04 x86_64.
  // * GeForce 840M driver 378.13.0.0, arch x86_64.
  for (auto name : std::vector<const char*>{
           "inheritedQueries",
           "robustBufferAccess",
           "samplerAnisotropy",
           "occlusionQueryPrecise",
           "textureCompressionETC2",
           "textureCompressionASTC_LDR",
           "textureCompressionBC",
       }) {
    if (dev.enabledFeatures.set(name, VK_TRUE)) {
      logE("initSupportedQueues: failed to set widely-supported features\n");
      return VK_ERROR_INITIALIZATION_FAILED;
    }
  }

  if (getQueueFamilies(dev)) {
    logE("Instance::ctorError: getQueueFamilies failed\n");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  bool hasEXTcreateRenderpass2 = false;
  bool oneQueueWithPresentSupported = false;
  for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
    auto& queueFlags = dev.qfams.at(q_i).queueFamilyProperties.queueFlags;
    if (queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
      // Per the vulkan spec for VkQueueFlagBits, GRAPHICS or COMPUTE always
      // imply TRANSFER and TRANSFER does not even have to be reported in that
      // case.
      //
      // In order to make life simple, set TRANSFER if TRANSFER is supported.
      queueFlags |= VK_QUEUE_TRANSFER_BIT;
    }

    VkBool32 isPresentSupported = VK_FALSE;
    dev.qfams.at(q_i).setSurfaceSupport(NONE);
    if (dev.getSurface()) {
      // Probe VkPhysicalDevice for surface support.
      VkResult v = vkGetPhysicalDeviceSurfaceSupportKHR(
          dev.phys, q_i, dev.getSurface(), &isPresentSupported);
      if (v != VK_SUCCESS) {
        char what[256];
        snprintf(what, sizeof(what),
                 "qfam %zu: vkGetPhysicalDeviceSurfaceSupportKHR", q_i);
        explainVkResult(what, v);
        return VK_ERROR_INITIALIZATION_FAILED;
      }
      if (isPresentSupported) {
        oneQueueWithPresentSupported = true;
        dev.qfams.at(q_i).setSurfaceSupport(PRESENT);
      }
    }
  }

  if (Vk::getDeviceExtensions(dev.phys, dev.availableExtensions)) {
    logE("Instance::ctorError: getDeviceExtensions failed\n");
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  if (!oneQueueWithPresentSupported) {
    // Add dev, but do not init any surface or present mode.
    return VK_SUCCESS;
  }

  // A device with a queue with PRESENT support should have all of
  // deviceWithPresentRequiredExts.
  std::vector<std::string> deviceWithPresentRequiredExts{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  bool is_debug_report_enabled = false;
  for (size_t i = 0; i < requiredExtensions.size(); i++) {
    if (requiredExtensions.at(i) == VK_EXT_DEBUG_REPORT_EXTENSION_NAME) {
      is_debug_report_enabled = true;
    }
  }
  bool hasEXTgetScap2 = false;
#ifdef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
  bool hasEXTfullscreenExclusive = false;
#endif /*VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME*/
  bool is_EXT_debug_marker_available = false;
  for (size_t i = 0; i < dev.availableExtensions.size(); i++) {
    const char* extensionName = dev.availableExtensions.at(i).extensionName;
    if (!strcmp(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, extensionName)) {
      // Automatically put VK_EXT_DEBUG_MARKER_EXTENSION_NAME in the chosen list
      // if VK_EXT_DEBUG_REPORT_EXTENSION_NAME is enabled on the instance.
      if (!is_EXT_debug_marker_available && is_debug_report_enabled) {
        dev.requiredExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
      }
      is_EXT_debug_marker_available = true;
      continue;
    }
    if (!hasEXTcreateRenderpass2 &&
        !strcmp(extensionName, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) {
      hasEXTcreateRenderpass2 = true;
      continue;
    }
    if (!strcmp(extensionName,
                VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)) {
      hasEXTgetScap2 = true;
      continue;
    }
#ifdef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
    if (!strcmp(extensionName, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME)) {
      hasEXTfullscreenExclusive = true;
      continue;
    }
#endif /*VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME*/
    for (auto j = deviceWithPresentRequiredExts.begin();
         j != deviceWithPresentRequiredExts.end(); j++) {
      if (!strcmp(extensionName, j->c_str())) {
        dev.requiredExtensions.push_back(*j);
        deviceWithPresentRequiredExts.erase(j);
        break;
      }
    }
  }

  // Automatically add VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME if supported.
  if (hasEXTcreateRenderpass2) {
    dev.requiredExtensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
  }
#if !defined(__APPLE__) || VK_HEADER_VERSION != 119
  // Automatically add VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME.
  // macOS has it but warns it is not supported (see kDeviceExtensionNames), so
  // tie this check to header version 119 and re-test on the next update.
  if (hasEXTgetScap2) {
    dev.requiredExtensions.push_back(
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
  }
#endif
#ifdef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
  // Automatically add VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME.
  if (hasEXTfullscreenExclusive) {
    dev.requiredExtensions.push_back(
        VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
  }
#endif /*VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME*/

  if (deviceWithPresentRequiredExts.size()) {
    // Do not add dev: it claims oneQueueWithPresentSupported but it lacks
    // required extensions. (If it does not do PRESENT at all, it is
    // assumed the device would not be used in the swap chain anyway, so it
    // is not removed.)
    return VK_ERROR_DEVICE_LOST;
  }

  // Init dev.surfaceFormats and dev.presentModes early. Your app can inspect
  // and modify them and then call open().
  int r = dev.initSurfaceFormatAndPresentMode();
  if (r) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  if (dev.surfaceFormats.size() == 0 || dev.presentModes.size() == 0) {
    // Do not add dev: it claims oneQueueWithPresentSupported but it has no
    // surfaceFormats -- or no presentModes.
    return VK_ERROR_DEVICE_LOST;
  }
  return VK_SUCCESS;
}

}  // namespace language
