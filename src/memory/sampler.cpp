/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <src/science/science.h>
#include "memory.h"

namespace memory {

int Sampler::ctorExisting(language::Device& dev) {
  vk.reset();
  VkResult v = vkCreateSampler(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSampler", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int Sampler::ctorError(command::CommandPool& cpool, Image& src) {
  science::SmartCommandBuffer setup{cpool, ASSUME_POOL_QINDEX};
  return setup.ctorError() || setup.autoSubmit() ||
         ctorError(cpool.dev, setup, src);
}

int Sampler::ctorError(language::Device& dev, command::CommandBuffer& buffer,
                       Image& src) {
  if (ctorExisting(dev)) {
    return 1;
  }

  // Construct image as a USAGE_SAMPLED | TRANSFER_DST, then use
  // CommandBuffer::copyImage() to transfer its contents into it.
  image.info.extent = src.info.extent;
  image.info.format = src.info.format;
  image.info.mipLevels = src.info.mipLevels;
  image.info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image.info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageView.info.subresourceRange.levelCount = src.info.mipLevels;

  if (image.ctorDeviceLocal(dev) || image.bindMemory(dev) ||
      imageView.ctorError(dev, image.vk, image.info.format)) {
    return 1;
  }

  command::CommandBuffer::BarrierSet bsetSrc;
  if (src.currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    bsetSrc.img.push_back(
        src.makeTransition(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
    science::SubresUpdate<VkImageSubresourceRange>(
        bsetSrc.img.back().subresourceRange)
        .setMips(0, src.info.mipLevels);
    src.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }
  bsetSrc.img.push_back(
      image.makeTransition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
  science::SubresUpdate<VkImageSubresourceRange>(
      bsetSrc.img.back().subresourceRange)
      .setMips(0, image.info.mipLevels);
  if (buffer.barrier(bsetSrc)) {
    return 1;
  }
  image.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  if (buffer.copyImage(src.vk, src.currentLayout, image.vk, image.currentLayout,
                       science::ImageCopies(src))) {
    return 1;
  }

  command::CommandBuffer::BarrierSet bsetShader;
  bsetShader.img.push_back(
      image.makeTransition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
  science::SubresUpdate<VkImageSubresourceRange>(
      bsetShader.img.back().subresourceRange)
      .setMips(0, image.info.mipLevels);
  if (buffer.barrier(bsetShader)) {
    return 1;
  }
  image.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  return 0;
}

int Sampler::ctorError(command::CommandPool& cpool, Buffer& src,
                       const std::vector<VkBufferImageCopy>& regions) {
  science::SmartCommandBuffer setup{cpool, ASSUME_POOL_QINDEX};
  return setup.ctorError() || setup.autoSubmit() ||
         ctorError(cpool.dev, setup, src, regions);
}

int Sampler::ctorError(language::Device& dev, command::CommandBuffer& buffer,
                       Buffer& src,
                       const std::vector<VkBufferImageCopy>& regions) {
  if (!image.info.extent.width || !image.info.extent.height ||
      !image.info.extent.depth || !image.info.format || !image.info.mipLevels ||
      !image.info.arrayLayers) {
    logE("Sampler::ctorError found uninitialized fields in image.info\n");
    return 1;
  }
  if (imageView.info.subresourceRange.levelCount != image.info.mipLevels) {
    logE("Sampler::ctorError: image.info.mipLevels=%u\n", image.info.mipLevels);
    logE("but imageView.info.subresourceRange.levelCount=%u\n",
         imageView.info.subresourceRange.levelCount);
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateSampler(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateSampler", v, string_VkResult(v));
    return 1;
  }

  // Construct image as a USAGE_SAMPLED | TRANSFER_DST, then use
  // CommandBuffer::copyBufferToImage() to copy the bytes into it.
  image.info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image.info.usage |=
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (image.ctorDeviceLocal(dev) || image.bindMemory(dev) ||
      imageView.ctorError(dev, image.vk, image.info.format)) {
    return 1;
  }

  command::CommandBuffer::BarrierSet bset1;
  bset1.img.push_back(
      image.makeTransition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
  science::SubresUpdate<VkImageSubresourceRange>(
      bset1.img.back().subresourceRange)
      .setMips(0, image.info.mipLevels);
  image.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  command::CommandBuffer::BarrierSet bset2;
  bset2.img.push_back(
      image.makeTransition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
  science::SubresUpdate<VkImageSubresourceRange>(
      bset2.img.back().subresourceRange)
      .setMips(0, image.info.mipLevels);
  if (buffer.barrier(bset1) ||
      buffer.copyBufferToImage(src.vk, image.vk, image.currentLayout,
                               regions) ||
      buffer.barrier(bset2)) {
    return 1;
  }
  image.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  return 0;
}

}  // namespace memory
