/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <limits.h>            // for CHAR_BIT
#include <src/core/utf8dec.h>  // __builtin_clz across all platforms.

#include "memory.h"
#include "vulkan/vk_format_utils.h"

namespace memory {

int Image::setMipLevelsFromExtent() {
  if (!info.extent.width || !info.extent.height) {
    logE("setMipLevelsFromExtent: zero-size image\n");
    return 1;
  }
  uint32_t dim = info.extent.width;
  dim = dim < info.extent.height ? dim : info.extent.height;
  info.mipLevels = sizeof(uint32_t) * CHAR_BIT - 1 - __builtin_clz(dim);
  return 0;
}

int Image::validateImageCreateInfo() {
  if (!info.extent.width || !info.extent.height || !info.extent.depth ||
      !info.format || !info.usage || !info.mipLevels || !info.arrayLayers) {
    logE("Image::ctorError found uninitialized fields\n");
    return 1;
  }
  mem.dev.apiUsage(
      1, 1, 0,
      info.flags & (VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT |
                    VK_IMAGE_CREATE_EXTENDED_USAGE_BIT),
      "Image::info flags=%x\n", info.flags);
  return 0;
}

int Image::getSubresourceLayouts() {
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
        vkGetImageSubresourceLayout(mem.dev.dev, vk, &s, &colorMem.back());
      }
    }
  }
  if (FormatHasDepth(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        depthMem.emplace_back();
        vkGetImageSubresourceLayout(mem.dev.dev, vk, &s, &depthMem.back());
      }
    }
  }
  if (FormatHasStencil(info.format)) {
    s.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    for (s.arrayLayer = 0; s.arrayLayer < info.arrayLayers; s.arrayLayer++) {
      for (s.mipLevel = 0; s.mipLevel < info.mipLevels; s.mipLevel++) {
        stencilMem.emplace_back();
        vkGetImageSubresourceLayout(mem.dev.dev, vk, &s, &stencilMem.back());
      }
    }
  }
  return 0;
}

int Image::ctorError(VkMemoryPropertyFlags props) {
  if (validateImageCreateInfo()) {
    return 1;
  }

  language::ImageFormatProperties formatProps;
  VkResult v = formatProps.getProperties(mem.dev, info);
  if (v != VK_SUCCESS) {
    // If the device doesn't support this image format in this usage, say so.
    if (v == VK_ERROR_FORMAT_NOT_SUPPORTED) {
      logE("ImageFormatProperties for tiling %s format %s usage %x t %s\n",
           string_VkImageTiling(info.tiling), string_VkFormat(info.format),
           info.usage, string_VkImageType(info.imageType));
    }
    return explainVkResult("Image::ctorError: ImageFormatProperties", v);
  }

  // Silently clip info.mipLevels to device's max. The device will still do
  // as much mipmapping as it can.
  if (info.mipLevels > formatProps.imageFormatProperties.maxMipLevels) {
    info.mipLevels = formatProps.imageFormatProperties.maxMipLevels;
    if (formatProps.imageFormatProperties.maxMipLevels < 2) {
      logW("This device only supports %llu mip level\n",
           (unsigned long long)formatProps.imageFormatProperties.maxMipLevels);
    }
  }

  // Even if no validation layers, give specifics on why image creation failed.
  // (This is mostly since ImageFormatProperties has already been loaded.)
  if (info.extent.width > formatProps.imageFormatProperties.maxExtent.width ||
      info.extent.height > formatProps.imageFormatProperties.maxExtent.height ||
      info.extent.depth > formatProps.imageFormatProperties.maxExtent.depth) {
    logE("ImageFormatProperties max w=%llu h=%llu d=%llu\n",
         (unsigned long long)formatProps.imageFormatProperties.maxExtent.width,
         (unsigned long long)formatProps.imageFormatProperties.maxExtent.height,
         (unsigned long long)formatProps.imageFormatProperties.maxExtent.depth);
    return 1;
  }
  if (info.arrayLayers > formatProps.imageFormatProperties.maxArrayLayers) {
    logE("ImageFormatProperties maxArrayLayers=%llu\n",
         (unsigned long long)formatProps.imageFormatProperties.maxArrayLayers);
    return 1;
  }

  // mem.reset() frees the memory. vk.reset() destroys the VkImage handle.
  mem.reset();
  vk.reset();
  v = vkCreateImage(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateImage", v);
  }
  vk.allocator = mem.dev.dev.allocator;
  vk.onCreate();
  currentLayout = info.initialLayout;

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  MemoryRequirements req(mem.dev, *this);
  mem.vmaAlloc.requiredProps = props;
#else
  MemoryRequirements req(mem.dev, *this, VMA_MEMORY_USAGE_UNKNOWN);
  req.info.requiredFlags = props;
#endif
  if (mem.alloc(req)) {
    return 1;
  }

  return getSubresourceLayouts();
}

#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
int Image::ctorError(VmaMemoryUsage usage) {
  if (validateImageCreateInfo()) {
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateImage(mem.dev.dev, &info, mem.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateImage", v);
  }
  vk.allocator = mem.dev.dev.allocator;
  vk.onCreate();
  currentLayout = info.initialLayout;
  return mem.alloc({mem.dev, *this, usage}) || getSubresourceLayouts();
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

int Image::bindMemory(
#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
    VkDeviceSize offset /*= 0*/
#endif                  /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
) {
  VkResult v;
  const char* functionName;
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  functionName = "vmaBindImageMemory";
  v = vmaBindImageMemory(mem.dev.vmaAllocator, mem.vmaAlloc, vk);
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
#if VK_HEADER_VERSION != 96
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (mem.dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    functionName = "vkBindImageMemory";
    v = vkBindImageMemory(mem.dev.dev, vk, mem.vmaAlloc.vk, offset);
#ifndef __ANDROID__
  } else {
    // Use Vulkan 1.1 features if supported.
    functionName = "vkBindImageMemory2";
    VkBindImageMemoryInfo infos[1];
    VkOverwrite(infos[0]);
    infos[0].image = vk;
    infos[0].memory = mem.vmaAlloc.vk;
    infos[0].memoryOffset = offset;
    v = vkBindImageMemory2(mem.dev.dev, sizeof(infos) / sizeof(infos[0]),
                           infos);
  }
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  if (v != VK_SUCCESS) {
    return explainVkResult(functionName, v);
  }
  return 0;
}

int Image::reset() {
  mem.reset();
  vk.reset();
  return 0;
}

VkImageAspectFlags Image::getAllAspects() const {
  VkImageAspectFlags aspects = 0;
  if (FormatIsColor(info.format)) {
    aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (FormatHasDepth(info.format)) {
    aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (FormatHasStencil(info.format)) {
    aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  if (info.flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                    VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
                    VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)) {
    aspects |= VK_IMAGE_ASPECT_METADATA_BIT;
  }
  if (FormatPlaneCount(info.format) > 1u) {
    aspects |= VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }
  if (FormatPlaneCount(info.format) > 2u) {
    aspects |= VK_IMAGE_ASPECT_PLANE_2_BIT;
  }
  return aspects;
}

}  // namespace memory
