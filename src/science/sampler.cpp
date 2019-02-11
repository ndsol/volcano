/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

int Sampler::ctorErrorNoImageViewInit() {
  if (!image) {
    logE("Sampler::ctorErrorNoImageViewInit: image is NULL\n");
    return 1;
  }
  auto& dev = image->mem.dev;
  vk.reset();
  VkResult v = vkCreateSampler(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateSampler", v);
  }
  vk.allocator = dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int Sampler::ctorError() {
  if (!image) {
    logE("Sampler::ctorError: image is NULL\n");
    return 1;
  }
  if (ctorErrorNoImageViewInit()) {
    logE("Sampler::ctorError: ctorErrorNoImageViewInit failed\n");
    return 1;
  }

  // Construct image as a USAGE_SAMPLED | TRANSFER_DST, then copy from src.
  image->info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image->info.usage |=
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (!image->info.extent.width || !image->info.extent.height ||
      !image->info.extent.depth || !image->info.format ||
      !image->info.mipLevels || !image->info.arrayLayers) {
    logE("Sampler::ctorError Sampler::image has uninitialized fields\n");
    return 1;
  }
  auto& sub = imageView.info.subresourceRange;
  if (!sub.aspectMask || !sub.levelCount || !sub.layerCount) {
    logE("Sampler::ctorError imageView.info has uninitialized fields\n");
    return 1;
  }

  if (image->ctorAndBindDeviceLocal() ||
      imageView.ctorError(image->vk, image->info.format)) {
    logE("image->ctorAndBindDeviceLocal or imageView.ctorError failed)\n");
    return 1;
  }
  return 0;
}

}  // namespace science
