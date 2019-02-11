/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <map>

#include "reflect.h"
#include "science.h"
using namespace std;

namespace science {

int ShaderLibrary::finalizeDescriptorLibrary(
    DescriptorLibrary& descriptorLibrary) {
  if (!descriptorLibrary.typesAverage.empty()) {
    logE("finalizeDescriptorLibrary can only be performed once per object.\n");
    return 1;
  }
  const vector<vector<ShaderBinding>>& allBindings = getBindings();

  // Count the types used for DescriptorLibrary::ctorError
  multiset<VkDescriptorType> count;

  descriptorLibrary.layouts.clear();
  descriptorLibrary.layouts.reserve(allBindings.size());
  for (size_t layoutI = 0; layoutI < allBindings.size(); layoutI++) {
    const vector<ShaderBinding>& bindings = allBindings.at(layoutI);
    descriptorLibrary.layouts.emplace_back();
    vector<memory::DescriptorSetLayout>& lo = descriptorLibrary.layouts.back();

    for (size_t setI = 0; setI < bindings.size(); setI++) {
      auto types = bindings.at(setI);

      vector<VkDescriptorSetLayoutBinding> libBindings(types.layouts.size());
      for (size_t typeI = 0; typeI < types.layouts.size(); typeI++) {
        auto& layout = types.layouts.at(typeI);
        count.emplace(layout.descriptorType);

        // This could be more efficient if stage bits could be broken down
        // to per-stage granularity without making the app more complicated.
        layout.stageFlags =
            static_cast<VkShaderStageFlagBits>(types.allStageBits);
        libBindings.at(typeI) = layout;
      }

      lo.emplace_back(dev);
      auto& libLayout = lo.back();
      if (libLayout.ctorError(libBindings)) {
        logE("descriptorLibrary.layouts[%zu][%zu].ctorError failed\n", layoutI,
             setI);
        return 1;
      }
      char name[256];
      snprintf(name, sizeof(name), "DescriptorSetLayout[%zu] set=%zu", layoutI,
               setI);
      if (libLayout.setName(name)) {
        logE("finalizeDescriptorLibrary: setName(%s) failed\n", name);
        return 1;
      }
    }
  }

  // Add up the count and create an average.
  map<VkDescriptorType, uint32_t> average;
  for (auto t : count) {
    auto i = average.insert(make_pair(t, 1));
    if (i.second) {
      // t is already present, so increment the count.
      i.first->second++;
    }
  }
  descriptorLibrary.typesAverage.swap(average);

  // Reserve a DescriptorPool to start with.
  if (descriptorLibrary.addToPools()) {
    logE("finalizeDescriptorLibrary: addToPools failed\n");
    return 1;
  }

  // pipes in obserers also need descriptorLibrary.layouts
  for (auto& observer : getObservers()) {
    auto& setLayouts = observer.pipe->info.setLayouts;
    for (auto& layout : descriptorLibrary.layouts.at(observer.layoutIndex)) {
      setLayouts.emplace_back(layout.vk);
    }
  }
  return 0;
}

int DescriptorLibrary::addToPools() {
  if (layouts.empty() || !isFinalized()) {
    logE("addToPools called before %sfinalizeDescriptorLibrary\n",
         "ShaderLibrary::");
    return 1;
  }

  poolSize *= 2;
  pools.emplace_back(dev);
  if (pools.back().ctorError(layouts.size(), typesAverage, poolSize)) {
    pools.pop_back();
    availDescriptorSets = 0;
    typesAvail.clear();
    return 1;
  }

  // Track the number of available DescriptorSets.
  availDescriptorSets = layouts.size() * poolSize;
  // Track the number of each type available.
  typesAvail.clear();
  typesAvail.insert(typesAverage.begin(), typesAverage.end());
  for (auto& i : typesAvail) {
    i.second *= poolSize;
  }
  return 0;
}

unique_ptr<memory::DescriptorSet> DescriptorLibrary::makeSet(size_t setI,
                                                             size_t layoutI) {
  if (pools.empty()) {
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

  // Loop around if the DescriptorPool is full.
  for (int retry = 0; retry < 2; retry++) {
    // Check if the DescriptorSet will fit in the DescriptorPool.
    if (availDescriptorSets > 0) {
      // Build a map from type -> number requested of the type.
      map<VkDescriptorType, uint32_t> fit;
      for (auto t : layouts.at(layoutI).at(setI).types) {
        auto i = fit.insert(make_pair(t, 1));
        if (i.second) {
          // t is already present, so increment the count.
          i.first->second++;
        }
      }
      // Fit each type against typesAvail, and erase it if ok.
      for (auto i = fit.begin(); i != fit.end();) {
        auto f = typesAvail.find(i->first);
        if (f == typesAvail.end() || f->second < i->second) {
          // typesAvail lacks that type (i->first) or enough slots (i->second).
          break;
        }
        i = fit.erase(i);
      }
      // If any types were not erased, they do not fit.
      if (fit.empty()) {
        // TODO: when c++14 is used, use make_unique() here.
        auto* set = new memory::DescriptorSet(dev, pools.back().vk);
        if (set->ctorError(layouts.at(layoutI).at(setI))) {
          logE("%smakeSet(%zu, %zu): vkAllocateDescriptorSets failed\n",
               "DescriptorLibrary::", setI, layoutI);
          return unique_ptr<memory::DescriptorSet>();
        }
        availDescriptorSets--;
        for (auto t : layouts.at(layoutI).at(setI).types) {
          auto i = typesAvail.find(t);
          if (i == typesAvail.end() || i->second < 1) {
            logE("%smakeSet(%zu, %zu): typesAvail missing a slot %d\n",
                 "DescriptorLibrary::", setI, layoutI, (int)t);
            return unique_ptr<memory::DescriptorSet>();
          }
          i->second--;
        }
        return unique_ptr<memory::DescriptorSet>(set);
      }
    }
    // The DescriptorSet will not fit.
    if (retry > 0) {
      break;
    }
    if (addToPools()) {
      logE("%smakeSet(%zu, %zu): addToPools failed\n",
           "DescriptorLibrary::", setI, layoutI);
      return unique_ptr<memory::DescriptorSet>();
    }
  }
  // Already tried addToPools, and it did not help.
  logE("%smakeSet(%zu, %zu): addToPools: brand new pool cannot fit?\n",
       "DescriptorLibrary::", setI, layoutI);
  return unique_ptr<memory::DescriptorSet>();
}

int DescriptorLibrary::setName(const std::string& name) {
  if (pools.empty()) {
    logE("DescriptorLibrary::setName: nothing here, call me later.\n");
    return 1;
  }
  for (size_t i = 0; i < pools.size(); i++) {
    if (pools.at(i).setName(name)) {
      logE("DescriptorLibrary::setName: pools[%zu].setName failed\n", i);
      return 1;
    }
  }
  return 0;
}

static const std::string DescriptorLibraryGetNameEmptyError =
    "DescriptorLibrary::getName: nothing here, call me later.";

const std::string& DescriptorLibrary::getName() const {
  if (pools.empty()) {
    logE("%s\n", DescriptorLibraryGetNameEmptyError.c_str());
    return DescriptorLibraryGetNameEmptyError;
  }
  return pools.at(0).getName();
}

}  // namespace science
