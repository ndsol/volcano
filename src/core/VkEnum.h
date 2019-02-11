/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <vector>

#include "VkPtr.h"

#pragma once

namespace language {
namespace VkEnum {
namespace Vk {

std::vector<VkExtensionProperties>* getExtensions();
std::vector<VkLayerProperties>* getLayers();
std::vector<VkPhysicalDevice>* getDevices(VkInstance instance);

std::vector<VkExtensionProperties>* getDeviceExtensions(VkPhysicalDevice dev);

std::vector<VkSurfaceFormatKHR>* getSurfaceFormats(VkPhysicalDevice dev,
                                                   VkSurfaceKHR surface);
std::vector<VkPresentModeKHR>* getPresentModes(VkPhysicalDevice dev,
                                               VkSurfaceKHR surface);
std::vector<VkImage>* getSwapchainImages(VkDevice dev,
                                         VkSwapchainKHR swapchain);

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
