/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <src/core/VkEnum.h>
// vk_object_types.h is shipped with vulkan in the layers/generated dir.
#include <layers/generated/vk_object_types.h>

#include "language.h"

namespace language {
using namespace VkEnum;

int InstanceExtensionChooser::choose() {
  std::vector<VkExtensionProperties> found;
  if (Vk::getExtensions(found)) {
    return 1;
  }
  for (const auto& ext : found) {
    // If "VK_EXT_debug_utils" is offered, automatically enable it.
    if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ext.extensionName)) {
      if (!is_EXT_debug_utils_available) {
        chosen.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      }
      is_EXT_debug_utils_available = true;
    }
  }
  if (!is_EXT_debug_utils_available) {
    // Attempt to use "VK_EXT_debug_report", the old Vulkan 1.0 way.
    chosen.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
  int r = 0;
  for (const auto& req : required) {
    if (req == VK_EXT_DEBUG_REPORT_EXTENSION_NAME) {
      // Do not check VK_EXT_debug_report. Rather, rely on the caller to
      // figure out whether to enable it or not.
      continue;
    }

    size_t i;
    for (i = 0; i < found.size(); i++) {
      if (req == found.at(i).extensionName) {
        chosen.emplace_back(req);
        break;
      }
    }
    if (i >= found.size()) {
      logE("requiredExtension \"%s\": no devices with this extension found.\n",
           req.c_str());
      r = 1;
    }
  }
  return r;
}

InstanceExtensionChooser::InstanceExtensionChooser(Instance& inst) {
  if (inst.requiredExtensions.size() == 0 &&
      inst.minSurfaceSupport.find(language::PRESENT) !=
          inst.minSurfaceSupport.end()) {
    // Do not immediately abort. Maybe you know what you're doing and
    // this error message is out of date? If so, please submit a bug.
    logW("Instance::ctorError: Missing requiredExtensions for surface.\n");
  }

  if (Vk::getLayers(instanceLayers)) {
    logF("VkEnumerateInstanceExtensionProperties failed\n");
  }
  required = inst.requiredExtensions;
  is_EXT_debug_utils_available = false;
}

InstanceExtensionChooser::~InstanceExtensionChooser(){};

VkFormat Device::chooseFormat(VkImageTiling tiling, VkFormatFeatureFlags flags,
                              VkImageType imageType,
                              const std::vector<VkFormat>& fmts) {
  apiUsage(1, 1, 0,
           flags & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                    VK_FORMAT_FEATURE_TRANSFER_DST_BIT),
           "chooseFormat(flags=%x) %s\n", flags,
           "uses VK_FORMAT_FEATURE_TRANSFER_{SRC or DST}_BIT");
  switch (tiling) {
    case VK_IMAGE_TILING_LINEAR:
    case VK_IMAGE_TILING_OPTIMAL:
      for (auto format : fmts) {
        FormatProperties props(format);
        if (props.getProperties(*this)) {
          logE("Device::chooseFormat(%s, %x):%s", string_VkImageTiling(tiling),
               flags, "FormatProperties.getProperties failed\n");
          return VK_FORMAT_UNDEFINED;
        }
        VkFormatFeatureFlags supported;
        if (tiling == VK_IMAGE_TILING_LINEAR) {
          supported = props.formatProperties.linearTilingFeatures;
        } else {
          supported = props.formatProperties.optimalTilingFeatures;
          if (flags & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                       VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                       VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                       VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
            if (!(flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
              logE("chooseFormat(flags=%x) %s\n", flags,
                   "flags omits "
                   "VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT.");
              logE("chooseFormat(flags=%x) %s\n", flags,
                   "Breaks vkCmdBlitImage and use in VkSampler");
              return VK_FORMAT_UNDEFINED;
            }
          }
        }
        if ((supported & flags) == flags) {
          // Also check ImageformatProperties. Both must pass.
          VkImageCreateInfo info;
          memset(&info, 0, sizeof(info));
          info.sType = autoSType(info);
          info.tiling = tiling;
          info.format = format;
          info.usage =
              // FIXME: These have no analog in flags. Find a better way.
              VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
          if (flags & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                       VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
            info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
          }
          if (flags & (VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                       VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
            info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
          }
          if (flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
            info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
          }
          if (flags & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                       VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
          }
          if (flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
            info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
          }
          if (flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
          }
          info.imageType = imageType;
          ImageFormatProperties iprops;
          VkResult v = iprops.getProperties(*this, info);
          if (v == VK_SUCCESS) {
            return format;
          }
        }
      }
      break;

    case VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT:
      logE("TILING_DRM_FORMAT_MODIFIER_EXT not supported\n");
      return VK_FORMAT_UNDEFINED;
    case VK_IMAGE_TILING_RANGE_SIZE:
      logE("_RANGE_SIZE are only placeholders. This should never happen.");
      break;
    case VK_IMAGE_TILING_MAX_ENUM:
      logE("_MAX_ENUM are only placeholders. This should never happen.");
      break;
  }

  return VK_FORMAT_UNDEFINED;
}

int Device::isExtensionAvailable(const char* name) {
  for (auto& extProps : availableExtensions) {
    if (!strcmp(extProps.extensionName, name)) {
      return 1;
    }
  }
  return 0;
}

int setObjectName(Device& dev, uintptr_t handle, VkObjectType objectType,
                  const char* name) {
  static int extensionWarnCount = 0;
  if (!handle) {
    logE("setObjectName: handle=NULL\n");
    return 1;
  }
  if (objectType == VK_OBJECT_TYPE_UNKNOWN) {
    logE("setObjectName(handle=%p name=\"%s\"): VK_OBJECT_TYPE_UNKNOWN %s%s\n",
         (void*)handle, name, "probably due to missing type in ",
         "src/core/structs.h");
    return 1;
  }

  if (dev.isExtensionLoaded(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    auto pfn = (PFN_vkSetDebugUtilsObjectNameEXT)dev.getInstanceProcAddr(
        "vkSetDebugUtilsObjectNameEXT");
    if (!pfn) {
      if (!extensionWarnCount) {
        logW("setObjectName(%p, %s, %s): " VK_EXT_DEBUG_UTILS_EXTENSION_NAME
             " found, but vkSetDebugUtilsObjectNameEXT was NULL.\n",
             reinterpret_cast<void*>(handle), string_VkObjectType(objectType),
             name);
      } else if (extensionWarnCount < 10) {
        logW(VK_EXT_DEBUG_UTILS_EXTENSION_NAME " found, but incomplete\n");
      }
      extensionWarnCount++;
      return 0;
    }
    VkDebugUtilsObjectNameInfoEXT nameInfo;
    memset(&nameInfo, 0, sizeof(nameInfo));
    nameInfo.sType = autoSType(nameInfo);
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = handle;
    nameInfo.pObjectName = name;

    VkResult v = pfn(dev.dev, &nameInfo);
    if (v == VK_SUCCESS) {
      return 0;
    }

    char what[256];
    snprintf(what, sizeof(what), "%s(dev=%p, name=%s)",
             "vkSetDebugUtilsObjectNameEXT",
             reinterpret_cast<void*>(static_cast<VkDevice>(dev.dev)), name);
    return explainVkResult(what, v);
  }

  // Use <layers/generated/vk_object_types.h> to get VkDebugReportObjectTypeEXT
  VkDebugReportObjectTypeEXT old =
      convertCoreObjectToDebugReportObject(objectType);
  if (old == VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT) {
    logE("setObjectName ERROR: VkDebugPtr<> is a %s%s\n",
         string_VkObjectType(objectType),
         ", something added in Vulkan 1.1. It cannot be used with the Vulkan"
         "1.0 API. The Vulkan 1.1 " VK_EXT_DEBUG_UTILS_EXTENSION_NAME
         " extension must be enabled.");
    return 1;
  }

  if (!dev.isExtensionLoaded(VK_EXT_DEBUG_REPORT_EXTENSION_NAME) ||
      !dev.isExtensionLoaded(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
    if (!name[0]) {
      return 0;
    }
    if (!extensionWarnCount) {
      logW(
          "setName(%s=%p, %s): instance "
          "extension " VK_EXT_DEBUG_UTILS_EXTENSION_NAME
          " (Vulkan 1.1) not loaded, and "
          "Vulkan 1.0 extensions " VK_EXT_DEBUG_REPORT_EXTENSION_NAME
          " + " VK_EXT_DEBUG_UTILS_EXTENSION_NAME " also not loaded.\n",
          string_VkObjectType(objectType) + strlen("VK_OBJECT_TYPE_"),
          reinterpret_cast<void*>(handle), name);
      logW("setName(%s=%p, %s): no instance extension\n",
           string_VkObjectType(objectType) + strlen("VK_OBJECT_TYPE_"),
           reinterpret_cast<void*>(handle), name);
    } else if (extensionWarnCount < 10) {
      logW("setName: no instance extension\n");
    }
    extensionWarnCount++;
    return 0;
  }

  auto pSet = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(
      dev.dev, "vkDebugMarkerSetObjectNameEXT");
  if (!pSet) {
    logE(
        "setObjectName(%p, %s, %s): "
        "extensions " VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        " + " VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        " must be loaded (pSet NULL).\n",
        reinterpret_cast<void*>(handle), string_VkObjectType(objectType), name);
    return 1;
  }

  VkDebugMarkerObjectNameInfoEXT nameInfo;
  memset(&nameInfo, 0, sizeof(nameInfo));
  nameInfo.sType = autoSType(nameInfo);
  nameInfo.objectType = old;
  nameInfo.object = handle;
  nameInfo.pObjectName = name;

  VkResult v = pSet(dev.dev, &nameInfo);
  if (v == VK_SUCCESS) {
    return 0;
  }
  char what[256];
  snprintf(what, sizeof(what), "%s(dev=%p, name=%s)",
           "vkDebugMarkerSetObjectNameEXT",
           reinterpret_cast<void*>(static_cast<VkDevice>(dev.dev)), name);
  return explainVkResult(what, v);
}

/*
FIXME: This causes a segfault on NVidia Linux.
int Device::setInstanceName(const std::string& name) {
  if (!inst) {
    return 0;
  }
  return setObjectName(*this,
                       VOLCANO_CAST_UINTPTR((VkInstance)inst->vk),
                       getObjectType((VkInstance)inst->vk), name.c_str());
}*/

int Device::setSurfaceName(const std::string& name) {
  if (!inst || !inst->surface) {
    return 0;
  }
  return setObjectName(*this, VOLCANO_CAST_UINTPTR((VkSurfaceKHR)inst->surface),
                       getObjectType((VkSurfaceKHR)inst->surface),
                       name.c_str());
}

}  // namespace language
