/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file implements Instance::createDevices and some Device methods.
 */
#include <src/core/VkEnum.h>
#include <src/core/VkInit.h>

#include "language.h"
#ifdef __ANDROID__
#include <unistd.h>  // for usleep
#endif               /* __ANDROID__ */

namespace language {
using namespace VkEnum;

namespace {  // an anonymous namespace hides its contents outside this file

int initSurfaceFormat(Device& dev) {
  if (dev.surfaceFormats.size() == 0) {
    logE("BUG: should not init a device with 0 SurfaceFormats\n");
    return 1;
  }

  if (dev.surfaceFormats.size() == 1 &&
      dev.surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
    // Vulkan specifies "you get to choose" by returning VK_FORMAT_UNDEFINED.
    // Default to 32-bit color and hardware SRGB color space. Your application
    // probably wants to test dev.surfaceFormats itself and choose its own
    // imageFormat.
    dev.swapChainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    dev.swapChainInfo.imageColorSpace = dev.surfaceFormats[0].colorSpace;
    return 0;
  }

  // Default to the first surfaceFormat Vulkan indicates is acceptable.
  dev.swapChainInfo.imageFormat = dev.surfaceFormats.at(0).format;
  dev.swapChainInfo.imageColorSpace = dev.surfaceFormats.at(0).colorSpace;
  return 0;
}

int setMode(const std::set<VkPresentModeKHR>& got, VkPresentModeKHR want,
            VkPresentModeKHR& choice) {
  if (got.find(want) != got.end()) {
    choice = want;
    return 0;
  }
  return 1;
}

int choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                      VkPresentModeKHR& choice) {
  if (presentModes.size() == 0) {
    logE("BUG: should not init a device with 0 PresentModes\n");
    return 1;
  }

  std::set<VkPresentModeKHR> got;
  for (const auto& availableMode : presentModes) {
    got.insert(availableMode);
  }
  if (got.find(VK_PRESENT_MODE_FIFO_KHR) == got.end()) {
    // VK_PRESENT_MODE_FIFO_KHR is required to always be present by the spec.
    logW(
        "Warn: choosePresentMode() did not find VK_PRESENT_MODE_FIFO_KHR.\n"
        "      This is an unexpected surprise! Could you send us\n"
        "      what vendor/VulkamSamples/build/demo/vulkaninfo\n"
        "      outputs -- we would love a bug report at:\n"
        "      https://github.com/ndsol/volcano/issues/new\n");
  }

  if (
#if defined(__ANDROID__)
      // Desktop development and benchmarking may benefit from keeping an eye on
      // the FPS (RenderDoc is better though!). On android, there is no reason
      // to exceed vsync.
      // https://www.khronos.org/assets/uploads/developers/library/2017-khronos-uk-vulkanised/006-Vulkanised-Bringing-Vainglory-to-Vulkan_May17.pdf
      setMode(got, VK_PRESENT_MODE_FIFO_KHR, choice) &&
#endif
      setMode(got, VK_PRESENT_MODE_MAILBOX_KHR, choice) &&
      setMode(got, VK_PRESENT_MODE_IMMEDIATE_KHR, choice) &&
      setMode(got, VK_PRESENT_MODE_FIFO_RELAXED_KHR, choice) &&
#if defined(VK_HEADER_VERSION) && VK_HEADER_VERSION >= 49
      setMode(got, VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, choice) &&
      setMode(got, VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, choice) &&
#endif
      setMode(got, VK_PRESENT_MODE_FIFO_KHR, choice)) {
    logE("BUG: initSurfaceFormatAndPresentMode could not find any mode.\n");
    return 1;
  }
  return 0;
}

}  // anonymous namespace

int Device::initSurfaceFormatAndPresentMode() {
  {
    auto* surfaceFormats = Vk::getSurfaceFormats(phys, getSurface());
    if (!surfaceFormats) {
      return 1;
    }
    this->surfaceFormats = *surfaceFormats;
    delete surfaceFormats;
    surfaceFormats = nullptr;

    auto* presentModes = Vk::getPresentModes(phys, getSurface());
    if (!presentModes) {
      return 1;
    }
    this->presentModes = *presentModes;
    delete presentModes;
    presentModes = nullptr;
  }

  if (surfaceFormats.size() == 0 || presentModes.size() == 0) {
    return 0;
  }

  int r = initSurfaceFormat(*this);
  if (r) {
    return r;
  }
  if (choosePresentMode(presentModes, swapChainInfo.presentMode)) {
    return 1;
  }
#ifdef _WIN32
  if (physProp.properties.vendorID == 0x1002 &&
      physProp.properties.deviceID == 0x67B9) {
    logW("WARNING: AMD 295x2 cards may be buggy.\n");
    logW("https://www.reddit.com/r/vulkan/comments/8x8ry9/\n");
  } else if (swapChainInfo.presentMode == VK_PRESENT_MODE_MAILBOX_KHR &&
             physProp.properties.vendorID ==
                 0x10de /* NVIDIA PCI device ID */) {
    logW("WARNING: PRESENT_MODE_MAILBOX chosen, %s\n",
         "NVidia fullscreen has bad tearing!");
  }
#endif
#ifdef __ANDROID__
  if (apiVersionInUse() < VK_MAKE_VERSION(1, 0, 0)) {
    logF("Pre-1.0 Vulkan, cannot use. https://youtu.be/Aeo62YzofGc?t=25m48s");
  }
#endif
  return 0;
}

Device::Device(language::Instance& inst, VkPhysicalDevice phys)
    : phys{phys}, inst{&inst} {
  VkOverwrite(swapChainInfo);
  swapChainInfo.imageArrayLayers = 1;  // e.g. 2 is for stereo displays.
  swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapChainInfo.clipped = VK_TRUE;
}

PFN_vkVoidFunction Device::getInstanceProcAddr(const char* funcName) {
  if (!inst) {
    logF("%s: Device not constructed by an instance?\n", "getInstanceProcAddr");
  }
  return vkGetInstanceProcAddr(inst->vk, funcName);
}

int Instance::createDevices(std::vector<VkPhysicalDevice>& physDevs) {
  // Check all devices for lowest API version supported before creating any dev
  // If user-set minApiVersion is higher, use that.
  // minApiVersion of 0 will use the autodetected apiVersion.
  if (minApiVersion > 0 && minApiVersion < applicationInfo.apiVersion) {
    logE("Instance supports apiVersion %x, you set minApiVersion=%x.\n",
         applicationInfo.apiVersion, minApiVersion);
    logE("Driver does not support the requested minApiVersion.\n");
    return 1;
  }

  detectedApiVersionInUse = applicationInfo.apiVersion;
  for (const auto& phys : physDevs) {
    // Just use Vulkan 1.0.x API to get apiVersion.
    VkPhysicalDeviceProperties VkInit(physProp);
    vkGetPhysicalDeviceProperties(phys, &physProp);
    if (physProp.apiVersion < detectedApiVersionInUse &&
        physProp.apiVersion >= minApiVersion) {
      logW("%s limits api to %u.%u.%u\n", physProp.deviceName,
           VK_VERSION_MAJOR(physProp.apiVersion),
           VK_VERSION_MINOR(physProp.apiVersion),
           VK_VERSION_PATCH(physProp.apiVersion));
      detectedApiVersionInUse = physProp.apiVersion;
    }
  }
#ifdef __ANDROID__
  // On android, there can only ever be one device.
  if (physDevs.size() != 1) {
    for (int zzz = 0; zzz < 100; zzz++) {
      logW("Android encountered %zu physical devices!\n", physDevs.size());
      logW("Android should never have anything but 1 physical device!\n");
      usleep(20000);
    }
  }
#endif

  uint32_t highestRejected = 0;
  for (const auto& phys : physDevs) {
    // Construct a new dev.
    //
    // Be careful to also call pop_back() unless initSupportedQueues()
    // succeeded.
    //
    // FIXME: if there are several devices, then surface comes from a window
    //        which comes from an unknown WSI-level-device where the app is
    //        running; just handing out the surface to all devices won't work.
    //        Need to wait until open() and possibly make a new device-group
    //        then plug in surface.
    devs.emplace_back(std::make_shared<Device>(*this, phys));
    Device& dev = *devs.back();
    if (dev.physProp.getProperties(dev)) {
      logE("Instance::ctorError: physProp.getProperties failed\n");
      return 1;
    }
    if (dev.memProps.getProperties(dev)) {
      logE("Instance::ctorError: memProp.getProperties failed\n");
      return 1;
    }

    // Vulkan 1.1 has deviceUUID, so it is possible to detect duplicates.
    if (dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
      bool duplicate = false;
      for (size_t j = 0; j < devs.size() - 1; j++) {
        if (!memcmp(devs.at(j)->physProp.id.deviceUUID,
                    dev.physProp.id.deviceUUID,
                    sizeof(dev.physProp.id.deviceUUID))) {
          // Found a duplicate
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        logW("Dup Device: \"%s\"\n", dev.physProp.properties.deviceName);
        logW("It may be the loader is finding duplicate json files.\n");
        logW(
            "See "
            "https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/"
            "issues/2331\n");
        devs.pop_back();
        continue;
      }
    }

    VkResult v = initSupportedQueues(dev);
    if (v != VK_SUCCESS) {
      devs.pop_back();
    }
    switch (v) {
      case VK_SUCCESS:
      case VK_ERROR_DEVICE_LOST:
        break;

      case VK_INCOMPLETE:
        // This error will never be sent by a Vulkan API.
        // It just means minApiVersion blocked this device.
        if (highestRejected < dev.physProp.properties.apiVersion) {
          highestRejected = dev.physProp.properties.apiVersion;
        }
        break;

      default:
        return explainVkResult("initSupportedQueues unexpectedly", v);
    }
  }

  // If at least one device was added to devs, ctorError is successful.
  if (devs.size() != 0) {
    return 0;
  }

  logE("No Vulkan-capable devices found on your system.\n");
  if (highestRejected > 0) {
    logE("Volcano Instance.minApiVersion=%x > any device: %x supported\n",
         minApiVersion, highestRejected);
  } else {
    logE("Try running vulkaninfo to troubleshoot.\n");
  }
  return 1;
}

}  // namespace language
