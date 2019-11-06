/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "language.h"

namespace language {

// ImageView::ImageView() defined here to have full Device definition.
ImageView::ImageView(Device& dev) : vk{dev, vkDestroyImageView} {
  vk.allocator = dev.dev.allocator;
  memset(&info, 0, sizeof(info));
  info.sType = autoSType(info);
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A,
  };
  // info.subresourceRange could be set up using range1MipAndColor() in
  // <src/science/science.h> -- but it would be a circular dependency.
  info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.subresourceRange.baseMipLevel = 0;  // Mipmap level offset (none).
  info.subresourceRange.levelCount = 1;    // There is 1 mipmap (no mipmapping).
  info.subresourceRange.baseArrayLayer = 0;  // Offset in layerCount layers.
  info.subresourceRange.layerCount = 1;      // Might be 2 for stereo displays.
};

int ImageView::ctorError(VkImage image, VkFormat format) {
  info.image = image;
  info.format = format;
  vk.reset();
  VkResult v = vkCreateImageView(vk.dev.dev, &info, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateImageView", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

// Framebuf::Framebuf() defined here to have full Device definition.
Framebuf::Framebuf(Device& dev) : vk{dev, vkDestroyFramebuffer} {
  vk.allocator = dev.dev.allocator;
}

int Framebuf::ctorError(VkRenderPass renderPass, uint32_t width,
                        uint32_t height) {
  if (!attachments.size()) {
    // Better to print this than segfault in the vulkan driver.
    logE("Framebuf::ctorError with attachments.size == 0\n");
    return 1;
  }
  std::vector<VkImageView> imageViews;
  for (const auto& attachment : attachments) {
    imageViews.emplace_back(attachment.vk);
  }
  VkFramebufferCreateInfo fbci;
  memset(&fbci, 0, sizeof(fbci));
  fbci.sType = autoSType(fbci);
  fbci.renderPass = renderPass;
  fbci.attachmentCount = imageViews.size();
  fbci.pAttachments = imageViews.data();
  fbci.width = width;
  fbci.height = height;
  fbci.layers = attachments.at(0).info.subresourceRange.layerCount;

  vk.reset();
  VkResult v =
      vkCreateFramebuffer(vk.dev.dev, &fbci, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateFramebuffer", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

VkPtr<VkDevice>& getVkPtrVkDevice(Device& dev) { return dev.dev; }

}  // namespace language
