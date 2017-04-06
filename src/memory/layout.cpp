/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <src/science/science.h>
#include "memory.h"

using namespace science;

namespace memory {

int Image::makeTransitionAccessMasks(VkImageMemoryBarrier& imageB) {
  bool isUnknownSrc = true;
  switch (imageB.oldLayout) {
    case VK_IMAGE_LAYOUT_GENERAL:
      imageB.srcAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
          VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      imageB.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      imageB.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
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
      imageB.srcAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
      isUnknownSrc = false;
      break;

    case VK_IMAGE_LAYOUT_RANGE_SIZE:
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      break;
  }

  bool isUnknownDst = true;
  switch (imageB.newLayout) {
    case VK_IMAGE_LAYOUT_GENERAL:
      imageB.dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
          VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
      // Reset imageB.subresourceRange, then set it to Depth.
      SubresUpdate& u = Subres(imageB.subresourceRange).addDepth();
      // Also add Stencil if requested.
      if (hasStencil(info.format)) {
        u.addStencil();
      }

      imageB.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      isUnknownDst = false;
      break;
    }
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: {
      // Reset imageB.subresourceRange, then set it to Depth.
      SubresUpdate& u = Subres(imageB.subresourceRange).addDepth();
      // Also add Stencil if requested.
      if (hasStencil(info.format)) {
        u.addStencil();
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
      // if oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      // VK_ACCESS_MEMORY_READ_BIT is definitely the only bit allowed in
      // srcAccessMask according to the validation layers.
      //
      // But SaschaWillems suggests ORing in VK_ACCESS_TRANSFER_READ_BIT in his
      // examples with no explanation. TODO: find out why.
      if (!(imageB.srcAccessMask & VK_ACCESS_MEMORY_READ_BIT)) {
        imageB.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
      }
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

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      imageB.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      imageB.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
      imageB.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      isUnknownDst = false;
      break;

    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_RANGE_SIZE:
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      break;
  }

  if (isUnknownSrc || isUnknownDst) {
    fprintf(stderr, "makeTransition(): unsupported: %s to %s%s%s\n",
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
  imageB.oldLayout = currentLayout;
  imageB.newLayout = newLayout;
  imageB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  Subres(imageB.subresourceRange).addColor();

  if (makeTransitionAccessMasks(imageB)) {
    imageB.image = VK_NULL_HANDLE;
    return imageB;
  }

  imageB.image = vk;
  return imageB;
}

}  // namespace memory
