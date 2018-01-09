/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

#include <vulkan/vk_format_utils.h>

namespace memory {

int Image::ctorError(language::Device& dev, VkMemoryPropertyFlags props) {
  if (!info.extent.width || !info.extent.height || !info.extent.depth ||
      !info.format || !info.usage || !info.mipLevels || !info.arrayLayers) {
    logE("Image::ctorError found uninitialized fields\n");
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateImage(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateImage", v, string_VkResult(v));
    return 1;
  }
  currentLayout = info.initialLayout;

  if (mem.alloc({dev, *this}, props)) {
    return 1;
  }
  // NOTE: vkGetImageSubresourceLayout is only valid for an image with
  // LINEAR tiling.
  //
  // Vulkan Spec: "The layout of a subresource (mipLevel/arrayLayer) of an
  // image created with linear tiling is queried by calling
  // vkGetImageSubresourceLayout".
  if (info.tiling != VK_IMAGE_TILING_LINEAR) {
    return 0;
  }
  VkImageSubresource s = {};
  if (FormatIsColor(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        colorMem.emplace_back();
        vkGetImageSubresourceLayout(dev.dev, vk, &s, &colorMem.back());
      }
    }
  }
  if (FormatHasDepth(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        depthMem.emplace_back();
        vkGetImageSubresourceLayout(dev.dev, vk, &s, &depthMem.back());
      }
    }
  }
  if (FormatHasStencil(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        stencilMem.emplace_back();
        vkGetImageSubresourceLayout(dev.dev, vk, &s, &stencilMem.back());
      }
    }
  }
  return 0;
}

int Image::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindImageMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkBindImageMemory", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Image::reset() {
  mem.allocSize = 0;
  mem.vk.reset();
  vk.reset();
  return 0;
}

}  // namespace memory
