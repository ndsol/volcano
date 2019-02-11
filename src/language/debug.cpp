/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file implements VK_EXT_debug_utils (the old debug extension).
 * See old_debug.cpp for VK_EXT_debug_report and choose.cpp.
 */
#include <src/core/VkInit.h>

#include "../command/command.h"
#include "language.h"

namespace language {

Instance::~Instance() {
  if (pDestroyDebugReportCallbackEXT) {
    pDestroyDebugReportCallbackEXT(vk, debugReport, pAllocator);
    pDestroyDebugReportCallbackEXT = nullptr;
  }
  if (pDestroyDebugUtilsMessengerEXT) {
    pDestroyDebugUtilsMessengerEXT(vk, messenger, pAllocator);
    pDestroyDebugUtilsMessengerEXT = nullptr;
  }
}

static int startsWith(const std::string& s, const char* prefix) {
  int len = strlen(prefix);
  return !strncmp(s.c_str(), prefix, len);
}

static int contains(const std::string& s, const char* key) {
  return strstr(s.c_str(), key) != 0;
}

static void filter(std::string& s) {
  static int filterInit = 0;
  static std::string prefix;
  if (!filterInit) {
    filterInit = 1;
    getSelfPath(prefix);
    auto lastSlash = prefix.rfind(OS_SEPARATOR);
    if (lastSlash != std::string::npos) {
      prefix.erase(lastSlash + 1);
    }
  }

  auto pos = s.find(prefix);
  if (pos != std::string::npos) {
    s.erase(pos, prefix.size());
  }
}

static void wrapLogVolcano(char level, const char* fmt, ...)
    VOLCANO_PRINTF(2, 3);

static void wrapLogVolcano(char level, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano(level, fmt, ap);
  va_end(ap);
}

void Instance::debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                     VkDebugUtilsMessageTypeFlagsEXT types,
                     const VkDebugUtilsMessengerCallbackDataEXT* data) {
  std::string sev;
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    sev += 'E';
  }
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    sev += 'W';
  }
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    sev += 'I';
  }
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    sev += 'V';
  }

  std::string msgType;
  if (0) {  // Do not display message type.
    if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
      msgType += 'G';
    }
    if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
      msgType += 'V';
    }
    if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
      msgType += 'P';
    }
  }
  if (!msgType.empty()) {
    msgType += ' ';
  }

  std::string msg = data->pMessage;
  std::string msgId;
  if (data->pMessageIdName) {
    msgId = data->pMessageIdName;
  }

  // Filter out some messages below a certain level of "interestingness".
  if (sev == "V") {
    if ((msgId == "VUID_Undefined" && startsWith(msg, "Added callback")) ||
        (msgId == "Layer Internal Message" &&
         startsWith(msg, "Added messenger"))) {
      return;
    }
  } else if (sev == "I" && msgId == "Loader Message") {
    if (startsWith(
            msg,
            "Encountered meta-layer VK_LAYER_LUNARG_standard_validation") ||
        startsWith(msg, "Found manifest file") ||
        startsWith(msg, "Instance Extension: VK_") ||
        startsWith(msg, "Device Extension: VK_") ||
        startsWith(msg, "ReadDataFilesInSearchPaths: Searching the followi") ||
        startsWith(msg, "Searching for ICD drivers named ") ||
        startsWith(msg, "Found ICD manifest file") ||
        startsWith(msg, "Build ICD instance extension list") ||
        startsWith(msg, "Loading layer library ") ||
        startsWith(msg, "Unloading layer library ")) {
      return;
    }
    if (insideVkCreateDevice &&
        (startsWith(msg, "Insert instance layer VK_") ||
         startsWith(msg, "Inserted device layer VK_"))) {
      return;
    }
#ifdef _MSC_VER
    if (startsWith(msg, "loaderGetDeviceRegistryFiles: opening device ") ||
        startsWith(msg, "loaderGetDeviceRegistryFiles: Opening child devi") ||
        startsWith(msg, "Located json file \"C:\\")) {
      return;
    }
    if (startsWith(msg, "loaderGetDeviceRegistryFiles: GUID") &&
        contains(msg, "is not SoftwareComponent skipping")) {
      return;
    }
    if (startsWith(msg, "loaderGetDeviceRegistryEntry: Device ID(") &&
        contains(msg, ") Does not contain a value for \"")) {
      return;
    }
#else
    if (contains(msg, "sing the loader legacy path.  This is not an error.")) {
      return;
    }
#endif
    if (startsWith(msg, "Meta-layer VK_LAYER_LUNARG_standard_validation ")) {
      if (contains(msg, " adding instance extension VK_") ||
          contains(msg, " adding device extension VK_") ||
          contains(msg, "component layers appear to be valid.")) {
        return;
      }
    }
  } else if (sev == "I" && msgId == "UNASSIGNED-ObjectTracker-Info") {
    if (startsWith(msg, "OBJ_STAT Destroy") ||
        (startsWith(msg, "OBJ[0x") && contains(msg, "] : CREATE "))) {
      return;
    }
  } else if (sev == "W") {
    // This is because Vulkan is built from source. The Vulkan SDK has not been
    // installed. Registry keys are not needed - this is not an error.
    if (contains(msg, "Registry lookup failed to get layer manifest files")) {
      return;
    }
  }

  if (sev.empty()) {
    logF("No bits set, should not have this log line: %s %s", msgType.c_str(),
         data->pMessage);
  }
  sev += msgType;
  filter(msg);
  wrapLogVolcano(sev.at(0), "%s%s\n", sev.substr(1).c_str(), msg.c_str());
}

VKAPI_ATTR VkBool32 VKAPI_CALL Instance::debugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* pUserData) {
  Instance* self = static_cast<Instance*>(pUserData);
  self->debug(severity, types, data);

  // Vulkan docs specify that VK_FALSE must always be returned:
  // "The VK_TRUE value is reserved for use in layer development."
  return VK_FALSE;
}

uint32_t Device::apiVersionInUse() const {
  if (!inst) {
    logF("%s: Device not constructed by an instance?\n", "apiVersionInUse");
  }
  return inst->apiVersionInUse();
}

void Device::apiUsage(int v1, int v2, int v3, bool pred, const char* fmt, ...) {
  uint32_t v = apiVersionInUse();
  if (VK_MAKE_VERSION(uint32_t(v1), uint32_t(v2), uint32_t(v3)) <= v || !pred) {
    return;
  }

  logW("Vulkan %u.%u.%u found, but %d.%d.%d wanted for:\n", VK_VERSION_MAJOR(v),
       VK_VERSION_MINOR(v), VK_VERSION_PATCH(v), v1, v2, v3);
  va_list ap;
  va_start(ap, fmt);
  logVolcano('W', fmt, ap);
  va_end(ap);
}

void Device::extensionUsage(const char* name, bool pred, const char* fmt, ...) {
  if (!inst) {
    logF("%s: Device not constructed by an instance?\n", "extensionUsage");
  }
  if (!pred || isExtensionLoaded(name)) {
    return;
  }

  logW("Extension \"%s\" needed for\n", name);
  va_list ap;
  va_start(ap, fmt);
  logVolcano('W', fmt, ap);
  va_end(ap);
}

int Device::isExtensionLoaded(const char* name) {
  if (!inst) {
    logF("%s: Device not constructed by an instance?\n", "isExtensionLoaded");
  }
  for (auto req : requiredExtensions) {
    if (!strcmp(name, req.c_str())) {
      return 1;
    }
  }
  for (auto req : inst->requiredExtensions) {
    if (req == name) {
      return 1;
    }
  }
  return 0;
}

}  // namespace language
