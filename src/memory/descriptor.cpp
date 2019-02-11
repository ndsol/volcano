/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

namespace memory {

int DescriptorPool::ctorError(
    uint32_t sets, const std::map<VkDescriptorType, uint32_t>& descriptors,
    uint32_t multiplier) {
  VkDescriptorPoolCreateInfo VkInit(info);
  info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

  // Vulkan Spec says: "If multiple VkDescriptorPoolSize structures appear in
  // the pPoolSizes array then the pool will be created with enough storage
  // for the total number of descriptors of each type."
  //
  // Vulkan drivers may need the types grouped though.
  //
  // Multiply by maxSets, i.e. the worst-case is that all slots for the
  // VkDescriptorType are used on each and every DescriptorSet.
  std::vector<VkDescriptorPoolSize> poolSizes;
  poolSizes.reserve(descriptors.size());
  for (auto i : descriptors) {
    poolSizes.emplace_back();
    auto& poolSize = poolSizes.back();
    VkOverwrite(poolSize);
    poolSize.type = i.first;
    poolSize.descriptorCount = i.second * multiplier;
  }

  info.poolSizeCount = poolSizes.size();
  info.pPoolSizes = poolSizes.data();
  info.maxSets = sets * multiplier;

  vk.reset();
  VkResult v =
      vkCreateDescriptorPool(vk.dev.dev, &info, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateDescriptorPool", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int DescriptorSetLayout::ctorError(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
  // Save the descriptor types that make up this layout.
  types.clear();
  types.reserve(bindings.size());
  for (auto& binding : bindings) {
    types.emplace_back(binding.descriptorType);
  }

  VkDescriptorSetLayoutCreateInfo VkInit(info);
  info.bindingCount = bindings.size();
  info.pBindings = bindings.data();

#if VK_HEADER_VERSION != 96
/* Fix the excessive #ifndef __ANDROID__ below to just use the Android Loader
 * once KhronosGroup lands support. */
#error KhronosGroup update detected, splits Vulkan-LoaderAndValidationLayers
#endif
#ifndef __ANDROID__
  if (vk.dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    // Check vkGetDescriptorSetLayoutSupport first, and spit out its hints.
    // TODO: VkDescriptorSetVariableDescriptorCountLayoutSupportEXT
    VkDescriptorSetLayoutSupport VkInit(support);
    vkGetDescriptorSetLayoutSupport(vk.dev.dev, &info, &support);
    if (!support.supported) {
      logW("%s may fail: see https://www.khronos.org Vulkan API docs about\n",
           "vkCreateDescriptorSetLayout");
      logW("    vkGetDescriptorSetLayoutSupport: a VkDescriptorSetLayout\n");
      logW("    has exceeded maxPerSetDescriptors = %llu, and also exceeded\n",
           (unsigned long long)vk.dev.physProp.maint3.maxPerSetDescriptors);
      logW("    an implementation-specific limit as well.\n");
    }
  }
#endif /* __ANDROID__ */

  vk.reset();
  VkResult v =
      vkCreateDescriptorSetLayout(vk.dev.dev, &info, vk.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateDescriptorSetLayout", v);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

DescriptorSet::~DescriptorSet() {
  if (vk) {
    VkResult v = vkFreeDescriptorSets(dev.dev, poolVk, 1, &vk);
    if (v != VK_SUCCESS) {
      explainVkResult("vkFreeDescriptorSets", v);
      logF("vkFreeDescriptorSets failed in ~DescriptorSet\n");
      exit(1);
    }
  }
}

int DescriptorSet::ctorError(const DescriptorSetLayout& layout) {
  types = layout.types;
  const VkDescriptorSetLayout& setLayout = layout.vk;

  VkDescriptorSetAllocateInfo VkInit(info);
  info.descriptorPool = poolVk;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &setLayout;

  VkResult v = vkAllocateDescriptorSets(dev.dev, &info, &vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkAllocateDescriptorSets", v);
  }
  if (setName(name)) {  // This is similar to what vk.onCreate does.
    logE("DescriptorSet::ctorError: failed to update name with setName\n");
    return 1;
  }
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorImageInfo> imageInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    logE("DescriptorSet::write(%u, %s): binding=%u with only %zu bindings\n",
         binding, "imageInfo", binding, types.size());
    return 1;
  }
  if (!vk) {
    logE("DescriptorSet::write(%u, %s): before ctorError\n", binding,
         "imageInfo");
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      break;
    default:
      logE("DescriptorSet::write(%u, %s): binding=%u has type %s\n", binding,
           "imageInfo", binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = imageInfo.size();
  w.pImageInfo = imageInfo.data();
  vkUpdateDescriptorSets(dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorBufferInfo> bufferInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    logE("DescriptorSet::write(%u, %s): binding=%u with only %zu bindings\n",
         binding, "bufferInfo", binding, types.size());
    return 1;
  }
  if (!vk) {
    logE("DescriptorSet::write(%u, %s): before ctorError\n", binding,
         "bufferInfo");
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      break;
    default:
      logE("DescriptorSet::write(%u, %s): binding=%u has type %s\n", binding,
           "bufferInfo", binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = bufferInfo.size();
  w.pBufferInfo = bufferInfo.data();
  vkUpdateDescriptorSets(dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkBufferView> texelBufferViewInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    logE("DescriptorSet::write(%u, %s): binding=%u with only %zu bindings\n",
         binding, "VkBufferView", binding, types.size());
    return 1;
  }
  if (!vk) {
    logE("DescriptorSet::write(%u, %s): before ctorError\n", binding,
         "VkBufferView");
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      break;
    default:
      logE("DescriptorSet::write(%u, %s): binding=%u has type %s\n", binding,
           "VkBufferView", binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = texelBufferViewInfo.size();
  w.pTexelBufferView = texelBufferViewInfo.data();
  vkUpdateDescriptorSets(dev.dev, 1, &w, 0, nullptr);
  return 0;
}

}  // namespace memory
