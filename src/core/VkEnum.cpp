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

std::vector<VkExtensionProperties>* getExtensions() {
  uint32_t extensionCount = 0;
  VkResult r =
      vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkEnumerateInstanceExtensionProperties(count)", r);
    return nullptr;
  }
  auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
  r = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                             extensions->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkEnumerateInstanceExtensionProperties(all)", r);
    delete extensions;
    return nullptr;
  }
  if (extensionCount > extensions->size()) {
    // This can happen if an extension was added between the two Enumerate
    // calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkEnumerateInstanceExtensionProperties(all)", extensionCount,
         extensions->size());
    delete extensions;
    return nullptr;
  }
  return extensions;
}

std::vector<VkLayerProperties>* getLayers() {
  uint32_t layerCount = 0;
  VkResult r = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkEnumerateInstanceLayerProperties(count)", r);
    return nullptr;
  }
  auto* layers = new std::vector<VkLayerProperties>(layerCount);
  r = vkEnumerateInstanceLayerProperties(&layerCount, layers->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkEnumerateInstanceLayerProperties(all)", r);
    delete layers;
    return nullptr;
  }
  if (layerCount > layers->size()) {
    // This can happen if a layer was added between the two Enumerate calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkEnumerateInstanceLayerProperties(all)", layerCount, layers->size());
    delete layers;
    return nullptr;
  }
  return layers;
}

std::vector<VkPhysicalDevice>* getDevices(VkInstance instance) {
  uint32_t devCount = 0;
#if defined(__linux__) && !defined(__ANDROID__)
  // Check for wayland session. Wayland running an X11 app (i.e. Xwayland) does
  // not support vulkan yet.
  for (char** p = environ; *p; p++) {
    const char* line = *p;
    const char* split = strchr(line, '=');
    if (split && !strncmp(line, "XDG_SESSION_TYPE", split - line)) {
      if (!strcmp(split + 1, "wayland")) {
        logE("Xwayland does not support vulkan yet:\n");
        logE(
            "https://bugs.launchpad.net/ubuntu/+source/"
            "nvidia-graphics-drivers-390/+bug/1769857/comments/4\n");
        logE("Do something like: (1) log out (2) log in again but disable\n");
        logE("wayland. Otherwise only wayland-native vulkan apps work.\n");
        logE("... trying 'vkEnumeratePhysicalDevices' anyway ...\n");
        VkResult r = vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
        if (r != VK_SUCCESS) {
          return nullptr;
        }
      }
    }
  }
#endif /*defined(__linux__) && !defined(__ANDROID__)*/
  VkResult r = vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkEnumeratePhysicalDevices", r);
    return nullptr;
  }
  auto* devs = new std::vector<VkPhysicalDevice>(devCount);
  r = vkEnumeratePhysicalDevices(instance, &devCount, devs->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkEnumeratePhysicalDevices(all)", r);
    delete devs;
    return nullptr;
  }
  if (devCount > devs->size()) {
    // This can happen if a device was added between the two Enumerate calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkEnumeratePhysicalDevices(all)", devCount, devs->size());
    delete devs;
    return nullptr;
  }
  return devs;
}

std::vector<VkExtensionProperties>* getDeviceExtensions(VkPhysicalDevice dev) {
  uint32_t extensionCount = 0;
  VkResult r = vkEnumerateDeviceExtensionProperties(dev, nullptr,
                                                    &extensionCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkEnumerateDeviceExtensionProperties(count)", r);
    return nullptr;
  }
  auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
  r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount,
                                           extensions->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkEnumerateDeviceExtensionProperties(all)", r);
    delete extensions;
    return nullptr;
  }
  if (extensionCount > extensions->size()) {
    // This can happen if an extension was added between the two Enumerate
    // calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkEnumerateDeviceExtensionProperties(all)", extensionCount,
         extensions->size());
    delete extensions;
    return nullptr;
  }
  return extensions;
}

std::vector<VkSurfaceFormatKHR>* getSurfaceFormats(VkPhysicalDevice dev,
                                                   VkSurfaceKHR surface) {
  uint32_t formatCount = 0;
  VkResult r =
      vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkGetPhysicalDeviceSurfaceFormatsKHR(count)", r);
    return nullptr;
  }
  auto* formats = new std::vector<VkSurfaceFormatKHR>(formatCount);
  r = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount,
                                           formats->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkGetPhysicalDeviceSurfaceFormatsKHR(all)", r);
    delete formats;
    return nullptr;
  }
  if (formatCount > formats->size()) {
    // This can happen if a format was added between the two Get calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkGetPhysicalDeviceSurfaceFormatsKHR(all)", formatCount,
         formats->size());
    delete formats;
    return nullptr;
  }
  return formats;
}

std::vector<VkPresentModeKHR>* getPresentModes(VkPhysicalDevice dev,
                                               VkSurfaceKHR surface) {
  uint32_t modeCount = 0;
  VkResult r = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface,
                                                         &modeCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkGetPhysicalDeviceSurfacePresentModesKHR(count)", r);
    return nullptr;
  }
  auto* modes = new std::vector<VkPresentModeKHR>(modeCount);
  r = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount,
                                                modes->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkGetPhysicalDeviceSurfacePresentModesKHR(all)", r);
    delete modes;
    return nullptr;
  }
  if (modeCount > modes->size()) {
    // This can happen if a mode was added between the two Get calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkGetPhysicalDeviceSurfacePresentModesKHR(all)", modeCount,
         modes->size());
    delete modes;
    return nullptr;
  }
  return modes;
}

std::vector<VkImage>* getSwapchainImages(VkDevice dev,
                                         VkSwapchainKHR swapchain) {
  uint32_t imageCount = 0;
  VkResult r = vkGetSwapchainImagesKHR(dev, swapchain, &imageCount, nullptr);
  if (r != VK_SUCCESS) {
    explainVkResult("vkGetSwapchainImagesKHR(count)", r);
    return nullptr;
  }
  auto* images = new std::vector<VkImage>(imageCount);
  r = vkGetSwapchainImagesKHR(dev, swapchain, &imageCount, images->data());
  if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
    explainVkResult("vkGetSwapchainImagesKHR(all)", r);
    delete images;
    return nullptr;
  }
  if (imageCount > images->size()) {
    // This can happen if an image was added between the two Get calls.
    logE("%s returned count=%u, larger than previously (%zu)\n",
         "vkGetSwapchainImagesKHR(all)", imageCount, images->size());
    delete images;
    return nullptr;
  }
  return images;
}

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
