/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <algorithm>

#include "science.h"

namespace science {

int copyImage1to1(command::CommandBuffer& buffer, memory::Image& src,
                  memory::Image& dst) {
  if (dst.info.extent.width != src.info.extent.width ||
      dst.info.extent.height != src.info.extent.height ||
      dst.info.extent.depth != src.info.extent.depth) {
    logE("copyImage1to1: src.extent != dst.extent\n");
    return 1;
  }

  // Make sure src and dst are in the correct layout.
  if (src.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    if (buffer.barrier(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)) {
      logE("copyImage1to1: buffer.barrier(%s) failed\n", "src, SRC_OPT");
      return 1;
    }
  }
  if (dst.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    if (buffer.barrier(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
      logE("copyImage1to1: buffer.barrier(%s) failed\n", "dst, DST_OPT");
      return 1;
    }
  }

  // Copy as many mip levels to dst as possible.
  uint32_t minLevels = src.info.mipLevels;
  if (minLevels > dst.info.mipLevels) {
    minLevels = dst.info.mipLevels;
  }
  for (uint32_t mipLevel = 0; mipLevel < minLevels; mipLevel++) {
    if (copyImageMipLevel(buffer, src, mipLevel, dst, mipLevel)) {
      logE("copyImage1to1: copyImageMipLevel(%u) failed\n", mipLevel);
      return 1;
    }
  }
  return 0;
}

int copyImageMipLevel(command::CommandBuffer& buffer, memory::Image& src,
                      uint32_t srcMipLevel, memory::Image& dst,
                      uint32_t dstMipLevel) {
  auto& de = dst.info.extent;
  auto& se = src.info.extent;
  if ((de.width >> dstMipLevel) != (se.width >> srcMipLevel) ||
      (de.height >> dstMipLevel) != (se.height >> srcMipLevel) ||
      (de.depth >> dstMipLevel) != (se.depth >> srcMipLevel)) {
    logE("copyImageMipLevel: src.extent != dst.extent\n");
    return 1;
  }

  // This does NOT verify the mip level of src and dst are in the right layout.
  VkImageCopy region;
  region.srcSubresource = src.getSubresourceLayers(srcMipLevel);
  region.dstSubresource = dst.getSubresourceLayers(dstMipLevel);
  region.srcSubresource.aspectMask &= region.dstSubresource.aspectMask;
  region.dstSubresource.aspectMask &= region.srcSubresource.aspectMask;

  region.srcOffset = {0, 0, 0};
  region.dstOffset = {0, 0, 0};

  region.extent = se;
  region.extent.width = std::max(1u, se.width >> srcMipLevel);
  region.extent.height = std::max(1u, se.height >> srcMipLevel);
  return buffer.copyImage(src, dst, {region});
}

int copyImageToMipmap(command::CommandBuffer& buffer, memory::Image& img) {
  if (img.info.mipLevels < 2) {
    logW("copyImageToMipmap on image %p with mipLevels = %llu\n",
         img.vk.printf(), (unsigned long long)img.info.mipLevels);
    if (img.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
      if (buffer.barrier(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)) {
        logE("copyImageToMipmap: buffer.barrier(%s) failed\n", "img, SRC_OPT");
        return 1;
      }
    }
    return 0;
  }

  // Transition img (all mip levels, entire image array) to DST_OPTIMAL.
  // img is both SRC and DST, but start everything in DST_OPTIMAL here.
  if (img.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    if (buffer.barrier(img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
      logE("copyImageToMipmap: buffer.barrier(%s) failed\n", "img, DST_OPT");
      return 1;
    }
  }

  auto e = img.info.extent;  // Make a copy of extent. e gets mutated below.
  VkImageSubresourceRange sub = img.getSubresourceRange();
  sub.levelCount = 1;

  for (uint32_t mip = 0; mip < img.info.mipLevels - 1; mip++) {
    sub.baseMipLevel = mip;
    if (buffer.barrier(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sub)) {
      logE("barrier(TRANSFER_SRC, %u) failed\n", mip);
      return 1;
    }
    // Now mip level 'mip' is in SRC_OPTIMAL. Levels mip + 1, mip + 2, ...
    // are still in DST_OPTIMAL.

    // Create the next mip level by scaling down 2x.
    VkImageBlit b;
    memset(&b, 0, sizeof(b));
    b.srcSubresource = img.getSubresourceLayers(mip);
    b.srcOffsets[0] = {0, 0, 0};
    b.srcOffsets[1] = {int32_t(e.width), int32_t(e.height), int32_t(e.depth)};
    e.width = std::max(1u, e.width >> 1);  // Progressively cut e in half.
    e.height = std::max(1u, e.height >> 1);
    b.dstSubresource = img.getSubresourceLayers(mip + 1);
    b.dstOffsets[0] = {0, 0, 0};
    b.dstOffsets[1] = {int32_t(e.width), int32_t(e.height), int32_t(e.depth)};
    if (buffer.blitImage(img.vk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img.vk,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         std::vector<VkImageBlit>{b})) {
      logE("blitImage(%u of %u) failed\n", mip, img.info.mipLevels - 1);
      return 1;
    }
  }

  // Transition last mip level so whole img is in one consistent layout.
  sub.baseMipLevel = img.info.mipLevels - 1;
  if (buffer.barrier(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sub)) {
    logE("barrier(TRANSFER_SRC, %u) failed\n", img.info.mipLevels - 1);
    return 1;
  }

  // Unlike buffer.barrier(img, DST_OPT), the buffer.barrier() calls above were
  // buffer.barrier(img, SRC_OPT, range) and do NOT update img.currentLayout.
  // That's because different mip levels had different layouts - there wasn't
  // a single consistent layout. At this point all the levels have changed,
  // but currentLayout has to be set manually.
  img.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  return 0;
}

}  // namespace science
