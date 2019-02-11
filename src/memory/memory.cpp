/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This contains the implementation of the MemoryRequirements class.
 */
#include "memory.h"

#include <vulkan/vk_format_utils.h>

namespace memory {

int MemoryRequirements::get(VkImage img,
                            VkImageAspectFlagBits optionalAspect /*= 0*/) {
  reset();
#if VK_HEADER_VERSION != 96
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  vkbuf = VK_NULL_HANDLE;
  vkimg = img;
  memset(&info, 0, sizeof(info));
  (void)dev;
  if (optionalAspect != (VkImageAspectFlagBits)0) {
    logE("VkImageAspectFlagBits is not supported by VulkanMemoryAllocator!\n");
    return 1;
  }
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  isImage = 1;
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#else
  (void)optionalAspect;
#endif
    vkGetImageMemoryRequirements(dev.dev, img, &vk.memoryRequirements);
    return 0;
#ifndef __ANDROID__
  }

  // Use Vulkan 1.1 features if supported.
  VkImageMemoryRequirementsInfo2 VkInit(info);
  VkImagePlaneMemoryRequirementsInfo VkInit(planeInfo);
  if (optionalAspect != (VkImageAspectFlagBits)0) {
    info.pNext = &planeInfo;
    planeInfo.planeAspect = optionalAspect;
  }
  info.image = img;
  vkGetImageMemoryRequirements2(dev.dev, &info, &vk);
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  return 0;
}

int MemoryRequirements::get(Image& img,
                            VkImageAspectFlagBits optionalAspect /*= 0*/) {
  if (optionalAspect != (VkImageAspectFlagBits)0) {
    if (!FormatIsMultiplane(img.info.format) ||
        !(img.info.flags & VK_IMAGE_CREATE_DISJOINT_BIT)) {
      logW("MemoryRequirements::get(Image): optionalAspect ignored, need\n");
      logW("multiplane format and VK_IMAGE_CREATE_DISJOINT_BIT set\n");
      optionalAspect = (VkImageAspectFlagBits)0;
    }
  }
  return get(img.vk, optionalAspect);
}

int MemoryRequirements::get(VkBuffer buf) {
  reset();
#ifndef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
  vkbuf = buf;
  vkimg = VK_NULL_HANDLE;
  memset(&info, 0, sizeof(info));
  (void)dev;
#else /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  isImage = 0;
#ifndef __ANDROID__
  if (dev.apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
#endif
    vkGetBufferMemoryRequirements(dev.dev, buf, &vk.memoryRequirements);
    return 0;
#ifndef __ANDROID__
  }

  // Use Vulkan 1.1 features if supported.
  VkBufferMemoryRequirementsInfo2 VkInit(info);
  // No pNext structures defined at this time.
  info.buffer = buf;
  vkGetBufferMemoryRequirements2(dev.dev, &info, &vk);
#endif /* __ANDROID__ */
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/
  return 0;
}

#ifdef VOLCANO_DISABLE_VULKANMEMORYALLOCATOR
int MemoryRequirements::indexOf(VkMemoryPropertyFlags props) const {
  auto& memProps = dev.memProps.memoryProperties;
  for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
    if ((vk.memoryRequirements.memoryTypeBits & (1 << i)) &&
        (memProps.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  return -1;
}

int MemoryRequirements::findVkalloc(VkMemoryPropertyFlags props) {
  // TODO: optionally accept another props with fewer bits (i.e. first props are
  // the most optimal, second props are the bare minimum).
  int i = indexOf(props);
  if (i == -1) {
    logE("MemoryRequirements::indexOf(%x): not found in %x\n", props,
         vk.memoryRequirements.memoryTypeBits);
    return 1;
  }

  vkalloc.memoryTypeIndex = i;
  vkalloc.allocationSize = vk.memoryRequirements.size;

  return 0;
}
#endif /*VOLCANO_DISABLE_VULKANMEMORYALLOCATOR*/

}  // namespace memory
