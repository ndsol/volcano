/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "memory.h"

namespace memory {

int DescriptorPool::ctorError(
    VkDescriptorPoolCreateFlags flags
    /*= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT*/) {
  if (!vk.empty()) {
    auto& last = vk.back();
    if (last.sets.size() < last.maxSets) {
      return 0;
    }
  }

  vk.emplace_back(dev, maxSets, flags);
  auto& last = vk.back();

  VkDescriptorPoolCreateInfo info;
  memset(&info, 0, sizeof(info));
  info.sType = autoSType(info);
  info.flags = flags;

  std::vector<VkDescriptorPoolSize> sizeOfAll;
  sizeOfAll.reserve(sizes.size());
  for (auto i = sizes.cbegin(); i != sizes.cend(); i++) {
    sizeOfAll.emplace_back(i->second);
    sizeOfAll.back().descriptorCount *= last.maxSets;
  }
  info.poolSizeCount = sizeOfAll.size();
  info.pPoolSizes = sizeOfAll.data();
  info.maxSets = last.maxSets;

  last.vk.reset();
  VkResult v =
      vkCreateDescriptorPool(dev.dev, &info, dev.dev.allocator, &last.vk);
  if (v != VK_SUCCESS) {
    return explainVkResult("vkCreateDescriptorPool", v);
  }
  last.vk.allocator = dev.dev.allocator;
  last.vk.onCreate();
  return 0;
}

int DescriptorPool::alloc(VkDescriptorSet& out, VkDescriptorSetLayout layout) {
  if (vk.empty()) {
    // Call ctorError to make a pool.
    if (ctorError()) {
      logE("DescriptorPool::alloc: vk.empty but ctorError failed\n");
      return 1;
    }
    if (vk.empty()) {
      logE("DescriptorPool::alloc: BUG: vk is still empty\n");
      return 1;
    }
  }

  for (size_t i = 0;; i++) {
    if (i >= vk.size()) {
      // Another larger pool is needed. Reuse ctorError with the same flags.
      maxSets *= 2;
      if (ctorError(vk.back().flags)) {
        logE("DescriptorPool::alloc: need another pool, failed to create\n");
        return 1;
      }
      if (i >= vk.size()) {
        logE("DescriptorPool::alloc: BUG: need a pool, stuck at %zu\n",
             vk.size());
        return 1;
      }
    }
    auto& pool = vk.at(i);
    if (pool.sets.size() >= pool.maxSets) {
      continue;  // Find the first pool that is not 100% full.
    }

    // preallocated should not be empty if the pool is not full.
    if (pool.preallocated.empty()) {
      VkDescriptorSetAllocateInfo info;
      memset(&info, 0, sizeof(info));
      info.sType = autoSType(info);
      info.descriptorPool = pool.vk;

      if (pool.sets.empty() &&
          !(pool.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)) {
        // Since pool.flags wants it, preallocate VkDescriptorSet objects now.
        pool.preallocated.resize(pool.maxSets);
      } else {
        // pool.flags does not want to preallocate. Get 1 VkDescriptorSet.
        // last.preallocated only has the VkDescriptorSet for a short time.
        pool.preallocated.resize(1);
      }
      info.descriptorSetCount = pool.preallocated.size();
      std::vector<VkDescriptorSetLayout> setLayouts(pool.preallocated.size());
      for (size_t j = 0; j < pool.preallocated.size(); j++) {
        setLayouts.at(j) = layout;
      }
      info.pSetLayouts = setLayouts.data();
      VkResult v =
          vkAllocateDescriptorSets(dev.dev, &info, pool.preallocated.data());
      if (v != VK_SUCCESS) {
        return explainVkResult("vkAllocateDescriptorSets", v);
      }
    }
    // Consume one preallocated VkDescriptorSet.
    out = pool.preallocated.back();
    pool.preallocated.pop_back();

    void* dsPtr = static_cast<void*>(out);
    auto r = pool.sets.insert(dsPtr);
    if (!r.second) {
      logE("DescriptorPool::alloc: BUG: vk.back().sets already has %p in it\n",
           dsPtr);
      return 1;
    }
    break;
  }
  return 0;
}

void DescriptorPool::free(VkDescriptorSet ds) {
  void* dsPtr = static_cast<void*>(ds);
  for (size_t i = vk.size(); i;) {
    i--;
    if (!vk.at(i).vk) {  // DescriptorPoolAllocator already freed?
      logE("DescriptorPool::free(%p): ~DescriptorPool already ran\n", dsPtr);
      logE("    Hint: declare DescriptorLibrary first, *before* any\n");
      logE("          DescriptorSet in your class\n");
      return;
    }
    auto& sets = vk.at(i).sets;
    auto f = sets.find(dsPtr);
    if (f != sets.end()) {
      VkResult v = vkFreeDescriptorSets(dev.dev, vk.at(i).vk, 1, &ds);
      if (v != VK_SUCCESS) {
        explainVkResult("vkFreeDescriptorSets", v);
        logF("vkFreeDescriptorSets failed in DescriptorPool::free\n");
        exit(1);
      }
      sets.erase(f);
      return;
    }
  }
  logF("BUG: DescriptorPool::free(%p) not found\n", dsPtr);
  exit(1);
}

static const std::string DescriptorPoolGetNameEmptyError =
    "DescriptorPool::getName before ctorError is invalid";

const std::string& DescriptorPool::getName() const {
  if (vk.empty()) {
    logE("%s\n", DescriptorPoolGetNameEmptyError.c_str());
  }
  return vk.at(0).vk.getName();
}

int DescriptorSetLayout::ctorError(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
  // Save the descriptor types that make up this layout.
  sizes.clear();
  VkDescriptorPoolSize zeroSize;
  memset(&zeroSize, 0, sizeof(zeroSize));
  for (auto& binding : bindings) {
    args.emplace_back(binding.descriptorType);
    zeroSize.type = binding.descriptorType;
    if (zeroSize.descriptorCount != 0) {
      logE("BUG: DescriptorSetLayout::ctorError arg %zu, zero is not %zu\n",
           args.size(), (size_t)zeroSize.descriptorCount);
      return 1;
    }
    auto i = sizes.emplace(std::make_pair(binding.descriptorType, zeroSize));
    i.first->second.descriptorCount += binding.descriptorCount;
  }

  VkDescriptorSetLayoutCreateInfo info;
  memset(&info, 0, sizeof(info));
  info.sType = autoSType(info);
  info.bindingCount = bindings.size();
  info.pBindings = bindings.data();

  if (vk.dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    // Check vkGetDescriptorSetLayoutSupport first, and spit out its hints.
    // TODO: VkDescriptorSetVariableDescriptorCountLayoutSupportEXT
    VkDescriptorSetLayoutSupport support;
    memset(&support, 0, sizeof(support));
    support.sType = autoSType(support);

    PFN_vkGetDescriptorSetLayoutSupport getSupport =
        (decltype(getSupport))vkGetDeviceProcAddr(
            vk.dev.dev, "vkGetDescriptorSetLayoutSupport");
    if (getSupport) {
      getSupport(vk.dev.dev, &info, &support);
      if (!support.supported) {
        logW("%s may fail: see https://www.khronos.org Vulkan API docs about\n",
             "vkCreateDescriptorSetLayout");
        logW("    vkGetDescriptorSetLayoutSupport: a VkDescriptorSetLayout\n");
        logW(
            "    has exceeded maxPerSetDescriptors = %llu, and also exceeded\n",
            (unsigned long long)vk.dev.physProp.maint3.maxPerSetDescriptors);
        logW("    an implementation-specific limit as well.\n");
      }
    }
  }

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
    parent.free(vk);
    vk = VK_NULL_HANDLE;
  }
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorImageInfo> imageInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > args.size()) {
    logE("DescriptorSet::write(%u, %s): binding=%u with only %zu bindings\n",
         binding, "imageInfo", binding, args.size());
    return 1;
  }
  if (!vk) {
    logE("DescriptorSet::write(%u, %s): before ctorError\n", binding,
         "imageInfo");
    return 1;
  }
  switch (args.at(binding)) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      break;
    default:
      logE("DescriptorSet::write(%u, %s): binding=%u has type %s\n", binding,
           "imageInfo", binding, string_VkDescriptorType(args.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet w;
  memset(&w, 0, sizeof(w));
  w.sType = autoSType(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = args.at(binding);
  w.descriptorCount = imageInfo.size();
  w.pImageInfo = imageInfo.data();
  vkUpdateDescriptorSets(dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorBufferInfo> bufferInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > args.size()) {
    logE("DescriptorSet::write(%u, %s): binding=%u with only %zu bindings\n",
         binding, "bufferInfo", binding, args.size());
    return 1;
  }
  if (!vk) {
    logE("DescriptorSet::write(%u, %s): before ctorError\n", binding,
         "bufferInfo");
    return 1;
  }
  switch (args.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      // Check size limit, even though validation layers also catch this.
      // Because write() can be deceptive, leading you to believe you can use
      // a larger buffer and just shove it into the descriptor.
      for (size_t i = 0; i < bufferInfo.size(); i++) {
        if (bufferInfo.at(i).range > (VkDeviceSize)dev.physProp.properties
                                         .limits.maxUniformBufferRange) {
          logE("write(%s) bufferInfo[%zu] is a buffer of size %zu\n",
               string_VkDescriptorType(args.at(binding)), i,
               (size_t)bufferInfo.at(i).range);
          logE("maxUniformBufferRange = %zu, try dynamic uniform buffers?\n",
               (size_t)dev.physProp.properties.limits.maxUniformBufferRange);
          return 1;
        }
      }
      break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      break;
    default:
      logE("DescriptorSet::write(%u, %s): binding=%u has type %s\n", binding,
           "bufferInfo", binding, string_VkDescriptorType(args.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet w;
  memset(&w, 0, sizeof(w));
  w.sType = autoSType(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = args.at(binding);
  w.descriptorCount = bufferInfo.size();
  w.pBufferInfo = bufferInfo.data();
  vkUpdateDescriptorSets(dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkBufferView> texelBufferViewInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > args.size()) {
    logE("DescriptorSet::write(%u, %s): binding=%u with only %zu bindings\n",
         binding, "VkBufferView", binding, args.size());
    return 1;
  }
  if (!vk) {
    logE("DescriptorSet::write(%u, %s): before ctorError\n", binding,
         "VkBufferView");
    return 1;
  }
  switch (args.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      break;
    default:
      logE("DescriptorSet::write(%u, %s): binding=%u has type %s\n", binding,
           "VkBufferView", binding, string_VkDescriptorType(args.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet w;
  memset(&w, 0, sizeof(w));
  w.sType = autoSType(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = args.at(binding);
  w.descriptorCount = texelBufferViewInfo.size();
  w.pTexelBufferView = texelBufferViewInfo.data();
  vkUpdateDescriptorSets(dev.dev, 1, &w, 0, nullptr);
  return 0;
}

}  // namespace memory
