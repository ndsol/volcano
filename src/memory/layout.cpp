/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"
#include "vulkan/vk_format_utils.h"

namespace memory {

int Image::makeTransitionAccessMasks(VkImageMemoryBarrier& imageB) {
  language::Device& dev = mem.dev;

  // When srcAccessMask includes *_READ_BIT, that means:
  // Any reads will see data as it was before the image transition began.
  //
  // When srcAccessMask includes *_WRITE_BIT, that means:
  // Any writes in progress must complete so the image transition will correctly
  // copy the data in the transition.
  bool isUnknownSrc = true;
  switch (imageB.oldLayout) {
    case VK_IMAGE_LAYOUT_GENERAL:
      imageB.srcAccessMask =
          VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
          VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_UNDEFINED:
      imageB.srcAccessMask = 0;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
      dev.apiUsage(1, 1, 0, true, "makeTransitionAccessMasks: oldLayout=%u\n",
                   imageB.oldLayout);
      // fall through
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      imageB.srcAccessMask =
          VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      imageB.srcAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
      dev.extensionUsage("VK_NV_shading_rate_image", true,
                         "makeTransitionAccessMasks: oldLayout=%u",
                         imageB.oldLayout);
      imageB.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      dev.extensionUsage("VK_EXT_fragment_density_map", true,
                         "makeTransitionAccessMasks: oldLayout=%u",
                         imageB.oldLayout);
      imageB.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      dev.extensionUsage("VK_KHR_shared_presentable_image", true,
                         "makeTransitionAccessMasks: oldLayout=%u",
                         imageB.oldLayout);
      imageB.srcAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT |
          VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT |
          VK_ACCESS_MEMORY_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_RANGE_SIZE:
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      break;
  }

  // When dstAccessMask includes *_READ_BIT, that means:
  // All the writes during the image transition must complete, so that any READ
  // will see them.
  //
  // When dstAccessMask includes *_WRITE_BIT, that means:
  // All the writes during the image transition must complete, so that any WRITE
  // will occur after them.
  bool isUnknownDst = true;
  switch (imageB.newLayout) {
    case VK_IMAGE_LAYOUT_GENERAL:
      imageB.dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
          VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
      // Reset imageB.subresourceRange, then set it to Depth.
      VkOverwrite(imageB.subresourceRange);
      imageB.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
      // Also add Stencil if requested.
      if (FormatHasStencil(info.format)) {
        imageB.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }

      imageB.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      isUnknownDst = false;
      break;
    }
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
      dev.apiUsage(1, 1, 0, true, "makeTransitionAccessMasks: newLayout=%u\n",
                   imageB.oldLayout);
      // fall through
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: {
      // Reset imageB.subresourceRange, then set it to Depth.
      VkOverwrite(imageB.subresourceRange);
      imageB.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
      // Also add Stencil if requested.
      if (FormatHasStencil(info.format)) {
        imageB.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      imageB.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      isUnknownDst = false;
      break;
    }
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      imageB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      imageB.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      imageB.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      imageB.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      imageB.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      imageB.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
      dev.extensionUsage("VK_NV_shading_rate_image", true,
                         "makeTransitionAccessMasks: oldLayout=%u",
                         imageB.oldLayout);
      imageB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      dev.extensionUsage("VK_EXT_fragment_density_map", true,
                         "makeTransitionAccessMasks: oldLayout=%u",
                         imageB.oldLayout);
      imageB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      dev.extensionUsage("VK_KHR_shared_presentable_image", true,
                         "makeTransitionAccessMasks: newLayout=%u",
                         imageB.newLayout);
      imageB.dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
          VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT |
          VK_ACCESS_HOST_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_RANGE_SIZE:
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      break;
  }

  if (isUnknownSrc || isUnknownDst) {
    logE("makeTransition(): unsupported: %s to %s%s%s\n",
         string_VkImageLayout(imageB.oldLayout),
         string_VkImageLayout(imageB.newLayout),
         isUnknownSrc ? " (invalid oldLayout)" : "",
         isUnknownDst ? " (invalid newLayout)" : "");
    return 1;
  }
  return 0;
}

VkImageMemoryBarrier Image::makeTransition(VkImageLayout newLayout) {
  VkImageMemoryBarrier VkInit(imageB);
  if (newLayout == currentLayout) {
    logE("Image::makeTransition(from %d to %d) is no change!\n", currentLayout,
         newLayout);
    // Returning without setting imageB.image lets CommandBuffer::barrier()
    // know there was an error.
    return imageB;
  }

  imageB.oldLayout = currentLayout;
  imageB.newLayout = newLayout;
  imageB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  VkOverwrite(imageB.subresourceRange);
  imageB.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
  imageB.subresourceRange.baseMipLevel = 0;
  imageB.subresourceRange.levelCount = info.mipLevels;

  if (makeTransitionAccessMasks(imageB)) {
    imageB.image = VK_NULL_HANDLE;
    return imageB;
  }

  imageB.image = vk;
  return imageB;
}

}  // namespace memory
