/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file implements Instance::ctorError(), though it calls down into a few
 * different methods. This file is the logic around vkCreateInstance.
 */
#include "language.h"

#include <src/core/VkEnum.h>

namespace language {

namespace {  // an anonymous namespace hides its contents outside this file

int initInstance(Instance& inst, std::vector<std::string>& enabledExtensions,
                 std::vector<VkLayerProperties>& availLayerProp) {
  std::set<std::string> avail;
  for (const auto& prop : availLayerProp) {
    auto r = avail.emplace(prop.layerName);
    if (!r.second) {
      logW("Instance::ctorError: VkLayerProperties \"%s\" is dup\n",
           prop.layerName);
    }
  }

  // Remove any inst.enabledLayers not in avail, and build a vector of char*
  std::vector<const char*> enabledLayersChar;
  for (auto i = inst.enabledLayers.begin(); i != inst.enabledLayers.end();) {
    if (!avail.count(*i)) {
      i = inst.enabledLayers.erase(i);
    } else {
      enabledLayersChar.push_back(i->c_str());
      i++;
    }
  }

  VkInstanceCreateInfo iinfo;
  memset(&iinfo, 0, sizeof(iinfo));
  iinfo.sType = autoSType(iinfo);
  iinfo.pApplicationInfo = &inst.applicationInfo;
  std::vector<const char*> extPointers;
  inst.requiredExtensions.swap(enabledExtensions);
  if (inst.requiredExtensions.size()) {
    iinfo.enabledExtensionCount = inst.requiredExtensions.size();
    for (size_t i = 0; i < inst.requiredExtensions.size(); i++) {
      extPointers.emplace_back(inst.requiredExtensions.at(i).c_str());
    }
    iinfo.ppEnabledExtensionNames = extPointers.data();
  }
  iinfo.enabledLayerCount = enabledLayersChar.size();
  iinfo.ppEnabledLayerNames = enabledLayersChar.data();
  const void* saveDebugUtilsNext = inst.debugUtils.pNext;
  iinfo.pNext = &inst.debugUtils;
  inst.debugUtils.pNext = NULL;

  VkResult v;
  for (;;) {
    v = vkCreateInstance(&iinfo, inst.pAllocator, &inst.vk);
#ifdef __ANDROID__
    // Android will not use the loader we built, so vkCreateInstance may fail
    // because it doesn't support applicationInfo.apiVersion. Try downgrading.
    if (v == VK_ERROR_INCOMPATIBLE_DRIVER &&
        inst.applicationInfo.apiVersion > VK_API_VERSION_1_0) {
      inst.applicationInfo.apiVersion = VK_API_VERSION_1_0;
      continue;
    }
#endif
    break;
  }
  inst.debugUtils.pNext = saveDebugUtilsNext;
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateInstance", v);
  }

  // Check what Vulkan API version is available.
  // NOTE: This requires an already-constructed instance.
  PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddr(inst.vk, "vkEnumerateInstanceVersion"));
  if (!EnumerateInstanceVersion) {
    logW("vkEnumerateInstanceVersion not found, falling %s\n",
         "back to Vulkan 1.0.");
    inst.applicationInfo.apiVersion = VK_API_VERSION_1_0;
  } else {
    VkResult v = EnumerateInstanceVersion(&inst.applicationInfo.apiVersion);
    if (v != VK_SUCCESS) {
      explainVkResult("vkEnumerateInstanceVersion", v);
      logW("Falling %s\n", "back to Vulkan 1.0.");
      inst.applicationInfo.apiVersion = VK_API_VERSION_1_0;
    }
    if (inst.applicationInfo.apiVersion > VK_MAKE_VERSION(1, 1, 0)) {
      // Allow apiVersion to be "upgraded" but only as far as Vulkan 1.1
      inst.applicationInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);
    }
  }
  return 0;
}

}  // anonymous namespace

Instance::Instance() {
  applicationName = "TODO: " __FILE__ ": customize applicationName";
  engineName = "github.com/ndsol/volcano";

  memset(&applicationInfo, 0, sizeof(applicationInfo));
  applicationInfo.sType = autoSType(applicationInfo);
  // Vulkan 1.1 is the highest API Volcano supports right now.
  applicationInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);
  applicationInfo.pApplicationName = applicationName.c_str();
  applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  applicationInfo.pEngineName = engineName.c_str();

  memset(&debugUtils, 0, sizeof(debugUtils));
  debugUtils.sType = autoSType(debugUtils);
  debugUtils.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugUtils.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugUtils.pfnUserCallback = debugUtilsCallback;
  debugUtils.pUserData = this;

#ifdef __ANDROID__
  // Android 8 has only one reliable way to request validation layers: hard
  // coding them at vkCreateInstance. Your app can of course delete any of
  // these before calling ctorError:
  enabledLayers.insert("VK_LAYER_GOOGLE_threading");
  enabledLayers.insert("VK_LAYER_LUNARG_parameter_validation");
  enabledLayers.insert("VK_LAYER_LUNARG_object_tracker");
  enabledLayers.insert("VK_LAYER_LUNARG_core_validation");
  enabledLayers.insert("VK_LAYER_GOOGLE_unique_objects");
#endif
}

int Instance::ctorError(CreateWindowSurfaceFn createWindowSurface,
                        void* window) {
  InstanceExtensionChooser instanceExtensions(*this);
  if (instanceExtensions.choose()) {
    return 1;
  }

  // Vulkan 1.1 is the highest API Volcano supports right now.
  if (applicationInfo.apiVersion > VK_MAKE_VERSION(1, 1, 0)) {
    applicationInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);
  }

  if (initInstance(*this, instanceExtensions.chosen,
                   instanceExtensions.instanceLayers)) {
    return 1;
  }

  if (initDebugUtilsOrDebugReport(
          instanceExtensions.is_EXT_debug_utils_available)) {
    return 1;
  }

  // surface.inst == VK_NULL_HANDLE, and needs to be reset to use vk.
  surface.reset(vk);
  VkResult v = createWindowSurface(*this, window);
  if (v != VK_SUCCESS) {
    return explainVkResult("createWindowSurface (the user-provided fn)", v);
  }
  surface.allocator = pAllocator;

  std::vector<VkPhysicalDevice> physDevs;
  return VkEnum::Vk::getDevices(vk, physDevs) || createDevices(physDevs);
}

VkSurfaceKHR Device::getSurface() {
  if (!inst->surface) {
    return VK_NULL_HANDLE;
  }
  return inst->surface;
}

}  // namespace language
