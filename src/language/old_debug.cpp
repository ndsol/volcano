/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file implements VK_EXT_debug_report (the old debug extension).
 * See debug.cpp which implements VK_EXT_debug_utils and choose.cpp.
 */
#include "../command/command.h"
#include "language.h"

namespace language {

namespace {  // use an anonymous namespace to hide all its contents (only
             // reachable from this file)

VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkFlags msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject,
    size_t location, int32_t msgCode, const char* pLayerPrefix,
    const char* pMsg, void* pUserData) {
  (void)objType;
  (void)srcObject;
  (void)location;
  (void)pUserData;

#if !defined(_MSC_VER)
  // Suppress the most common log messages.
  if (!strcmp(pLayerPrefix, "DebugReport")) {
    if (strstr(pMsg, "Added callback")) {
      return false;
    }
  } else if (!strcmp(pLayerPrefix, "Loader Message")) {
    /**
     * To view loader messages that are produced before Instance::initDebug(),
     * set VK_LOADER_DEBUG=all or VK_LOADER_DEBUG=error,warn,debug,...,info
     * (see g_loader_log_msgs in loader/loader.c)
     */
    if (!strncmp(pMsg, "Loading layer library", 21)) {
      return false;
    }
    if (!strncmp(pMsg, "Device Extension: ", 18)) {
      return false;
    }
  } else if (!strcmp(pLayerPrefix, "ObjectTracker") ||
             !strcmp(pLayerPrefix, "Validation")) {
    if (msgFlags &
        (VK_DEBUG_REPORT_DEBUG_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT)) {
      // Suppress messages like:
      // I Validation: code0: Object: 0x2 | OBJ[0x6] : CREATE CommandPool
      // object 0x2 I ObjectTracker: code0: Object: 0x2 | OBJ_STAT Destroy
      // CommandPool obj 0x2 ...
      return false;
    }
  }
#endif /* Suppress the most common log messages. */

  if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    logD("%s: code%d: %s\n", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    logI("%s: code%d: %s\n", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    logI("PerfWarn: %s: code%d: %s\n", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    logW("%s: code%d: %s\n", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    logE("%s: code%d: %s\n", pLayerPrefix, msgCode, pMsg);
  }

  // Vulkan 1.0.64 spec clarifies that this should always return false.
  return false;
}

static int initDebugReport(language::Instance& inst) {
  // Set up VK_EXT_debug_report.
  VkDebugReportCallbackCreateInfoEXT dinfo;
  memset(&dinfo, 0, sizeof(dinfo));
  dinfo.sType = autoSType(dinfo);
  dinfo.flags =
      VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
  dinfo.pfnCallback = debugReportCallback;

  auto pCreateDebugReportCallback =
      (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
          inst.vk, "vkCreateDebugReportCallbackEXT");
  if (!pCreateDebugReportCallback) {
    logE("Failed to dlsym(vkCreateDebugReportCallbackEXT)\n");
    return 1;
  }

  inst.pDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
          inst.vk, "vkDestroyDebugReportCallbackEXT");
  if (!inst.pDestroyDebugReportCallbackEXT) {
    logE("Failed to dlsym(vkDestroyDebugReportCallbackEXT)\n");
    return 1;
  }

  VkResult v = pCreateDebugReportCallback(inst.vk, &dinfo, inst.pAllocator,
                                          &inst.debugReport);
  if (v != VK_SUCCESS) {
    return explainVkResult("pCreateDebugReportCallback", v);
  }
  return 0;
}

}  // anonymous namespace

int Instance::initDebugUtilsOrDebugReport(bool& is_EXT_debug_utils_available) {
  while (is_EXT_debug_utils_available) {
    PFN_vkCreateDebugUtilsMessengerEXT pCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(vk, "vkCreateDebugUtilsMessengerEXT"));
    if (!pCreateDebugUtilsMessengerEXT) {
      logE("pCreateDebugUtilsMessengerEXT not found, falling back to 1.0\n");
      is_EXT_debug_utils_available = false;
      pDestroyDebugUtilsMessengerEXT = nullptr;
      break;
    }
    pDestroyDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(vk, "vkDestroyDebugUtilsMessengerEXT"));
    if (!pDestroyDebugUtilsMessengerEXT) {
      logE("pDestroyDebugUtilsMessengerEXT not found, fall back to 1.0\n");
      is_EXT_debug_utils_available = false;
      break;
    }
    pSubmitDebugUtilsMessageEXT =
        reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(
            vkGetInstanceProcAddr(vk, "vkSubmitDebugUtilsMessageEXT"));
    if (!pDestroyDebugUtilsMessengerEXT) {
      logE("pSubmitDebugUtilsMessageEXT not found, fall back to 1.0\n");
      is_EXT_debug_utils_available = false;
      pDestroyDebugUtilsMessengerEXT = nullptr;
      break;
    }

    VkResult v =
        pCreateDebugUtilsMessengerEXT(vk, &debugUtils, pAllocator, &messenger);
    if (v != VK_SUCCESS) {
      return explainVkResult("pCreateDebugUtilsMessengerEXT", v);
    }
    // Successfully set up VK_EXT_debug_utils.
    break;
  }

  // Re-check is_EXT_debug_utils_available because it may have just changed,
  // even after creating the instance w/extensions.
  if (!is_EXT_debug_utils_available && initDebugReport(*this)) {
    return 1;
  }
  return 0;
}

}  // namespace language
