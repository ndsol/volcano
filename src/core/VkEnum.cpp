/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "VkEnum.h"

#include <stdio.h>

#include <vector>

#include "VkPtr.h"

#if defined(__linux__) && !defined(__ANDROID__)
extern char** environ;
#endif /*defined(__linux__) && !defined(__ANDROID__)*/

namespace language {
namespace VkEnum {
namespace Vk {

int getExtensions(std::vector<VkExtensionProperties>& out) {
  for (;;) {
    uint32_t extensionCount = 0;
    VkResult r = vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, nullptr);
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumerateInstanceExtensionProperties(count)",
                             r);
    }
    out.resize(extensionCount);
    r = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                               out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumerateInstanceExtensionProperties(all)", r);
    }
    if (extensionCount > out.size()) {
      // This can happen if an extension was added between the two Enumerate
      // calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkEnumerateInstanceExtensionProperties(all)", extensionCount,
           out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

int getLayers(std::vector<VkLayerProperties>& out) {
  for (;;) {
    uint32_t layerCount = 0;
    VkResult r = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumerateInstanceLayerProperties(count)", r);
    }
    out.resize(layerCount);
    r = vkEnumerateInstanceLayerProperties(&layerCount, out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumerateInstanceLayerProperties(all)", r);
    }
    if (layerCount > out.size()) {
      // This can happen if a layer was added between the two Enumerate calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkEnumerateInstanceLayerProperties(all)", layerCount, out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

int getDevices(VkInstance instance, std::vector<VkPhysicalDevice>& out) {
  VkResult r = VK_SUCCESS;
  for (;;) {
    uint32_t devCount = 0;
#if defined(__linux__) && !defined(__ANDROID__)
    // Check for wayland session. Wayland running an X11 app (i.e. Xwayland)
    // does not support vulkan yet.
    for (char** p = environ; *p; p++) {
      const char* line = *p;
      const char* split = strchr(line, '=');
      if (split && !strncmp(line, "XDG_SESSION_TYPE", split - line)) {
        if (!strcmp(split + 1, "wayland")) {
          logE("Xwayland does not support vulkan yet:\n");
          logE(
              "https://bugs.launchpad.net/ubuntu/+source/"
              "nvidia-graphics-drivers-390/+bug/1769857/comments/4\n");
          logE("Try something like: (1) log out (2) disable wayland on\n");
          logE("login screen, or only wayland-native vulkan apps will work.\n");
          logE("... trying 'vkEnumeratePhysicalDevices' anyway ...\n");
          r = vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
          if (r != VK_SUCCESS) {
            return 1;
          }
        }
      }
    }
#endif /*defined(__linux__) && !defined(__ANDROID__)*/
    r = vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumeratePhysicalDevices", r);
    }
    out.resize(devCount);
    r = vkEnumeratePhysicalDevices(instance, &devCount, out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumeratePhysicalDevices(all)", r);
    }
    if (devCount > out.size()) {
      // This can happen if a device was added between the two Enumerate calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkEnumeratePhysicalDevices(all)", devCount, out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

int getDeviceExtensions(VkPhysicalDevice dev,
                        std::vector<VkExtensionProperties>& out) {
  for (;;) {
    uint32_t extensionCount = 0;
    VkResult r = vkEnumerateDeviceExtensionProperties(dev, nullptr,
                                                      &extensionCount, nullptr);
    if (r != VK_SUCCESS) {
      explainVkResult("vkEnumerateDeviceExtensionProperties(count)", r);
      return 1;
    }
    out.resize(extensionCount);
    r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount,
                                             out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkEnumerateDeviceExtensionProperties(all)", r);
    }
    if (extensionCount > out.size()) {
      // This can happen if an extension was added between the two Enumerate
      // calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkEnumerateDeviceExtensionProperties(all)", extensionCount,
           out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

int getSurfaceFormats(VkPhysicalDevice dev, VkSurfaceKHR surface,
                      std::vector<VkSurfaceFormatKHR>& out) {
  for (;;) {
    uint32_t formatCount = 0;
    VkResult r = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface,
                                                      &formatCount, nullptr);
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkGetPhysicalDeviceSurfaceFormatsKHR(count)", r);
    }
    out.resize(formatCount);
    r = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount,
                                             out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkGetPhysicalDeviceSurfaceFormatsKHR(all)", r);
    }
    if (formatCount > out.size()) {
      // This can happen if a format was added between the two Get calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkGetPhysicalDeviceSurfaceFormatsKHR(all)", formatCount,
           out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

int getPresentModes(VkPhysicalDevice dev, VkSurfaceKHR surface,
                    std::vector<VkPresentModeKHR>& out) {
  for (;;) {
    uint32_t modeCount = 0;
    VkResult r = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface,
                                                           &modeCount, nullptr);
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkGetPhysicalDeviceSurfacePresentModesKHR(count)",
                             r);
    }
    out.resize(modeCount);
    r = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount,
                                                  out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkGetPhysicalDeviceSurfacePresentModesKHR(all)",
                             r);
    }
    if (modeCount > out.size()) {
      // This can happen if a mode was added between the two Get calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkGetPhysicalDeviceSurfacePresentModesKHR(all)", modeCount,
           out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

int getSwapchainImages(VkDevice dev, VkSwapchainKHR swapchain,
                       std::vector<VkImage>& out) {
  for (;;) {
    uint32_t imageCount = 0;
    VkResult r = vkGetSwapchainImagesKHR(dev, swapchain, &imageCount, nullptr);
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkGetSwapchainImagesKHR(count)", r);
    }
    out.resize(imageCount);
    r = vkGetSwapchainImagesKHR(dev, swapchain, &imageCount, out.data());
    if (r == VK_INCOMPLETE) {
      continue;
    }
    if (r != VK_SUCCESS) {
      out.clear();
      return explainVkResult("vkGetSwapchainImagesKHR(all)", r);
    }
    if (imageCount > out.size()) {
      // This can happen if an image was added between the two Get calls.
      logE("%s returned count=%u, larger than previously (%zu)\n",
           "vkGetSwapchainImagesKHR(all)", imageCount, out.size());
      out.clear();
      return 1;
    }
    return 0;
  }
}

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
