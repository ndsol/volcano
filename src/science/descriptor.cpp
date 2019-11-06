/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <map>

#include "reflect.h"
#include "science.h"
using namespace std;

namespace science {

int ShaderLibrary::addDynamic(size_t setI, size_t layoutI, uint32_t binding) {
  if (!_i) {
    logE("ShaderLibrary::addDynamic() before add()\n");
    return 1;
  }

  auto& bindings = getBindings();
  if (layoutI >= bindings.size()) {
    logE("ShaderLibrary::addDynamic(%zu, %zu, %zu): layoutI=%zu >= %zu\n", setI,
         layoutI, (size_t)binding, layoutI, bindings.size());
    return 1;
  }
  auto& bindSet = bindings.at(layoutI);
  if (setI >= bindSet.size()) {
    logE("ShaderLibrary::addDynamic(%zu, %zu, %zu): setI=%zu >= %zu\n", setI,
         layoutI, (size_t)binding, setI, bindSet.size());
    return 1;
  }
  auto& layouts = bindSet.at(setI).layouts;
  if ((size_t)binding >= layouts.size()) {
    logE("ShaderLibrary::addDynamic(%zu, %zu, %zu): binding=%zu >= %zu\n", setI,
         layoutI, (size_t)binding, (size_t)binding, layouts.size());
    return 1;
  }
  VkDescriptorSetLayoutBinding& layout = layouts.at(binding);
  switch (layout.descriptorType) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      layout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      layout.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
      break;
    default:
      logE("ShaderLibrary::addDynamic(%zu, %zu, %zu): %s(%zu) not supported\n",
           setI, layoutI, (size_t)binding,
           string_VkDescriptorType(layout.descriptorType),
           (size_t)layout.descriptorType);
      return 1;
  }
  return 0;
}

int ShaderLibrary::finalizeDescriptorLibrary(
    DescriptorLibrary& descriptorLibrary) {
  if (!descriptorLibrary.pool.empty()) {
    logE("finalizeDescriptorLibrary can only be performed once per object.\n");
    return 1;
  }
  const vector<vector<ShaderBinding>>& allSrcBindings = getBindings();

  descriptorLibrary.layouts.clear();
  descriptorLibrary.layouts.reserve(allSrcBindings.size());
  for (size_t layoutI = 0; layoutI < allSrcBindings.size(); layoutI++) {
    vector<memory::DescriptorSetLayout> dstLayouts;
    const vector<ShaderBinding>& srcBindings = allSrcBindings.at(layoutI);
    for (size_t setI = 0; setI < srcBindings.size(); setI++) {
      // Copy srcBindings.at(setI) to dstBindings and set all stageFlags.
      auto srcBinding = srcBindings.at(setI);
      vector<VkDescriptorSetLayoutBinding> dstBindings(srcBinding.layouts);
      for (size_t typeI = 0; typeI < dstBindings.size(); typeI++) {
        dstBindings.at(typeI).stageFlags =
            static_cast<VkShaderStageFlagBits>(srcBinding.allStageBits);
      }

      dstLayouts.emplace_back(dev);
      auto& dstLayout = dstLayouts.back();
      if (dstLayout.ctorError(dstBindings)) {
        logE("descriptorLibrary.layouts[%zu][%zu].ctorError failed\n", layoutI,
             setI);
        return 1;
      }
      char name[256];
      snprintf(name, sizeof(name), "descriptorLibrary.layouts[%zu] set=%zu",
               layoutI, setI);
      if (dstLayout.setName(name)) {
        logE("finalizeDescriptorLibrary: setName(%s) failed\n", name);
        return 1;
      }
      auto r = descriptorLibrary.pool.emplace(std::make_pair(
          dstLayout.sizes, memory::DescriptorPool(dev, dstLayout.sizes)));
      if (!r.second) {  // Enlarge maxSets if the pool feeds multiple layouts.
        r.first->second.maxSets += memory::DescriptorPool::INITIAL_MAXSETS;
      }
    }
    descriptorLibrary.layouts.emplace_back(std::move(dstLayouts));
  }

  // For each pool call DescriptorPool::ctorError()
  for (auto i = descriptorLibrary.pool.begin();
       i != descriptorLibrary.pool.end(); i++) {
    if (i->second.ctorError()) {
      logE("finalizeDescriptorLibrary: DescriptorPool::ctorError failed\n");
      return 1;
    }
  }

  // Update obserers (pipe objects) with descriptorLibrary.layouts.
  for (auto& observer : getObservers()) {
    auto& setLayouts = observer.pipe->info.setLayouts;
    for (auto& layout : descriptorLibrary.layouts.at(observer.layoutIndex)) {
      setLayouts.emplace_back(layout.vk);
    }
  }
  return 0;
}

unique_ptr<memory::DescriptorSet> DescriptorLibrary::makeSet(size_t setI,
                                                             size_t layoutI) {
  if (pool.empty()) {
    logE("BUG: %smakeSet(%zu, %zu) before %sfinalizeDescriptorLibrary\n",
         "DescriptorLibrary::", setI, layoutI, "ShaderLibrary::");
    return unique_ptr<memory::DescriptorSet>();
  }
  if (layoutI > layouts.size()) {
    logE("BUG: %smakeSet(%zu, %zu) with %zu layouts\n",
         "DescriptorLibrary::", setI, layoutI, layouts.size());
    return unique_ptr<memory::DescriptorSet>();
  }
  if (setI > layouts.at(layoutI).size()) {
    logE("BUG: %smakeSet(%zu, %zu) layoutI=%zu only has %zu sets\n",
         "DescriptorLibrary::", setI, layoutI, layoutI,
         layouts.at(layoutI).size());
    return unique_ptr<memory::DescriptorSet>();
  }
  auto& layout = layouts.at(layoutI).at(setI);

  auto matchingPool = pool.find(layout.sizes);
  if (matchingPool == pool.end()) {
    logE("BUG: %smakeSet(%zu, %zu): layout has no matching pool\n",
         "DescriptorLibrary::", setI, layoutI);
    return unique_ptr<memory::DescriptorSet>();
  }

  VkDescriptorSet ds;
  if (matchingPool->second.alloc(ds, layout)) {
    logE("%smakeSet(%zu, %zu): pool.alloc failed\n",
         "DescriptorLibrary::", setI, layoutI);
    return unique_ptr<memory::DescriptorSet>();
  }
  // TODO: when c++14 is used, use make_unique() here.
  return unique_ptr<memory::DescriptorSet>(
      new memory::DescriptorSet(dev, matchingPool->second, layout, ds));
}

int DescriptorLibrary::setName(const std::string& name) {
  if (pool.empty()) {
    logE("%ssetName: nothing here, call me later.\n", "DescriptorLibrary::");
    return 1;
  }
  for (auto i = pool.begin(); i != pool.end(); i++) {
    if (i->second.setName(name)) {
      logE("%ssetName: pool.setName failed\n", "DescriptorLibrary::");
      return 1;
    }
  }
  return 0;
}

static const std::string DescriptorLibraryGetNameEmptyError =
    "DescriptorLibrary::getName: nothing here, call me later.";

const std::string& DescriptorLibrary::getName() const {
  if (pool.empty()) {
    logE("%s\n", DescriptorLibraryGetNameEmptyError.c_str());
    return DescriptorLibraryGetNameEmptyError;
  }
  return pool.begin()->second.getName();
}

}  // namespace science
