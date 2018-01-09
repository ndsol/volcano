/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "VkEnum.h"
#include "VkInit.h"
#include "language.h"

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
    auto* surfaceFormats = Vk::getSurfaceFormats(phys, swapChainInfo.surface);
    if (!surfaceFormats) {
      return 1;
    }
    this->surfaceFormats = *surfaceFormats;
    delete surfaceFormats;
    surfaceFormats = nullptr;

    auto* presentModes = Vk::getPresentModes(phys, swapChainInfo.surface);
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
  if (swapChainInfo.presentMode == VK_PRESENT_MODE_MAILBOX_KHR &&
      physProp.vendorID == 0x10de /* NVIDIA PCI device ID */) {
    logW("WARNING: PRESENT_MODE_MAILBOX chosen, %s\n",
         "NVidia fullscreen has bad tearing!");
  }
#endif
  return 0;
}

Device::Device(VkSurfaceKHR surface) {
  VkOverwrite(enabledFeatures);

  VkOverwrite(swapChainInfo);
  swapChainInfo.surface = surface;
  swapChainInfo.imageArrayLayers = 1;  // e.g. 2 is for stereo displays.
  swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapChainInfo.clipped = VK_TRUE;
}

}  // namespace language
