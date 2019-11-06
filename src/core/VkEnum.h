/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <vector>

#include "VkPtr.h"

#pragma once

namespace language {
namespace VkEnum {
namespace Vk {

// getExtensions wraps vkEnumerateInstanceExtensionProperties and replaces out
// with the results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getExtensions(std::vector<VkExtensionProperties>& out);

// getLayers wraps vkEnumerateInstanceLayerProperties and replaces out with the
// results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getLayers(std::vector<VkLayerProperties>& out);

// getDevices wraps vkEnumeratePhysicalDevices and replaces out with the
// results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getDevices(VkInstance instance,
                                  std::vector<VkPhysicalDevice>& out);

// getDeviceExtensions wraps vkEnumerateDeviceExtensionProperties and replaces
// out with the results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getDeviceExtensions(
    VkPhysicalDevice dev, std::vector<VkExtensionProperties>& out);

// getSurfaceFormats wraps vkGetPhysicalDeviceSurfaceFormatsKHR and replaces
// out with the results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getSurfaceFormats(VkPhysicalDevice dev,
                                         VkSurfaceKHR surface,
                                         std::vector<VkSurfaceFormatKHR>& out);

// getPresentModes wraps vkGetPhysicalDeviceSurfacePresentModesKHR and replaces
// out with the results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getPresentModes(VkPhysicalDevice dev,
                                       VkSurfaceKHR surface,
                                       std::vector<VkPresentModeKHR>& out);

// getSwapchainImages wraps vkGetSwapchainImagesKHR and replaces out with the
// results. Returns 0 = success or non-zero = error.
WARN_UNUSED_RESULT int getSwapchainImages(VkDevice dev,
                                          VkSwapchainKHR swapchain,
                                          std::vector<VkImage>& out);

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
