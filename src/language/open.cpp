/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * This is Instance::open(), though it is broken into a few different methods.
 */
#include <src/core/VkEnum.h>

#include <map>

#include "language.h"

namespace language {
using namespace VkEnum;

struct ProcAddrReference {
  ProcAddrReference(const std::string& ext, const char* name,
                    PFN_vkVoidFunction* ptr)
      : ext(ext), name(name), ptr(ptr) {}
  std::string ext;
  const char* name;
  PFN_vkVoidFunction* ptr;
};

// Load function pointers if an extension was loaded. If this does not load all
// required function pointers, remove the extension. This keeps application
// logic simple.
// NOTE: if Vulkan fails to load an extension, vkCreateDevice will fail (and
//       open() returns an error). But if Vulkan fails to find the function
//       pointer, this logs a warning and *removes* the extension.
void DeviceFunctionPointers::load(Device& dev) {
  std::vector<ProcAddrReference> addr;
  size_t i;
  // Collect function pointers
  for (i = 0; i < dev.requiredExtensions.size(); i++) {
    auto& ext = dev.requiredExtensions.at(i);
    if (ext == VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCreateRenderPass2KHR",
          reinterpret_cast<PFN_vkVoidFunction*>(&createRenderPass2));
      addr.emplace_back(
          ext, "vkCmdBeginRenderPass2KHR",
          reinterpret_cast<PFN_vkVoidFunction*>(&beginRenderPass2));
      addr.emplace_back(ext, "vkCmdNextSubpass2KHR",
                        reinterpret_cast<PFN_vkVoidFunction*>(&nextSubpass2));
      addr.emplace_back(ext, "vkCmdEndRenderPass2KHR",
                        reinterpret_cast<PFN_vkVoidFunction*>(&endRenderPass2));
    } else if (ext == VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCmdPushDescriptorSetKHR",
          reinterpret_cast<PFN_vkVoidFunction*>(&pushDescriptorSet));
      addr.emplace_back(ext, "vkCmdPushDescriptorSetWithTemplateKHR",
                        reinterpret_cast<PFN_vkVoidFunction*>(
                            &pushDescriptorSetWithTemplate));
    } else if (ext == VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCmdDrawIndirectCountKHR",
          reinterpret_cast<PFN_vkVoidFunction*>(&drawIndirectCount));
      addr.emplace_back(
          ext, "vkCmdDrawIndexedIndirectCountKHR",
          reinterpret_cast<PFN_vkVoidFunction*>(&drawIndexedIndirectCount));
    } else if (ext == VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCmdBindTransformFeedbackBuffersEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&bindTransformFeedbackBuffers));
      addr.emplace_back(
          ext, "vkCmdBeginTransformFeedbackEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&beginTransformFeedback));
      addr.emplace_back(
          ext, "vkCmdEndTransformFeedbackEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&endTransformFeedback));
      addr.emplace_back(
          ext, "vkCmdBeginQueryIndexedEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&beginQueryIndexed));
      addr.emplace_back(
          ext, "vkCmdEndQueryIndexedEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&endQueryIndexed));
      addr.emplace_back(
          ext, "vkCmdDrawIndirectByteCountEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&drawIndirectByteCount));
    } else if (ext == VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCmdBeginConditionalRenderingEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&beginConditionalRendering));
      addr.emplace_back(
          ext, "vkCmdEndConditionalRenderingEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&endConditionalRendering));
    } else if (ext == VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCmdSetDiscardRectangleEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&setDiscardRectangle));
    } else if (ext == VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME) {
      addr.emplace_back(
          ext, "vkCmdSetSampleLocationsEXT",
          reinterpret_cast<PFN_vkVoidFunction*>(&setSampleLocations));
      addr.emplace_back(ext, "vkGetPhysicalDeviceMultisamplePropertiesEXT",
                        reinterpret_cast<PFN_vkVoidFunction*>(
                            &getPhysicalDeviceMultisampleProperties));
    }
  }

  // Call getInstanceProcAddr and evaluate the results.
  size_t start = 0;
  for (i = 0; i < addr.size(); i++) {
    start = (i > 0 && addr.at(i).ext != addr.at(start).ext) ? i : start;
    *addr.at(i).ptr = dev.getInstanceProcAddr(addr.at(i).name);
    if (*addr.at(i).ptr) {
      continue;
    }
    auto& ext = addr.at(i).ext;
    logW("%s: %s not found\n", ext.c_str(), addr.at(i).name);

    for (auto p = dev.requiredExtensions.begin();
         p != dev.requiredExtensions.end(); p++) {
      if (*p == ext) {
        dev.requiredExtensions.erase(p);
        break;
      }
    }

    // Set all pointers from this extension to null.
    // NOTE: To log all failed functions implies this can run multiple times.
    for (size_t j = start; j < addr.size() && addr.at(j).ext == ext; j++) {
      *addr.at(j).ptr = nullptr;
    }
  }
}

int Instance::initQueues(std::vector<QueueRequest>& request) {
  // Search for a single device that support minSurfaceSupport.
  bool foundQueue = false;
  for (size_t dev_i = 0; dev_i < devs.size(); dev_i++) {
    auto selectedQfams = requestQfams(dev_i, minSurfaceSupport);
    foundQueue |= selectedQfams.size() > 0;
    if (foundQueue) {
      request.insert(request.end(), selectedQfams.begin(), selectedQfams.end());
      return 0;
    }
  }
  if (!foundQueue) {
    logE("Error: no device has minSurfaceSupport.\n");
    return 1;
  }
  return 0;
}

int Instance::open(VkExtent2D surfaceSizeRequest) {
  std::vector<QueueRequest> request;
  int r = initQueues(request);
  if (r != 0) {
    return r;
  }

  // Split up QueueRequest by device index. This has the side effect of
  // ignoring any device for which there is no QueueRequest.
  std::map<size_t, std::vector<QueueRequest>> requested_devs;
  for (auto& req : request) {
    auto it = requested_devs.find(req.dev_index);
    if (it == requested_devs.end()) {
      auto ret =
          requested_devs.emplace(req.dev_index, std::vector<QueueRequest>());
      it = ret.first;
    }
    it->second.push_back(req);
  }

  // For each device that has one or more queues requested, call vkCreateDevice
  // i.e. dispatch each queue request's dev_index to vkCreateDevice now.
  for (const auto& kv : requested_devs) {
    auto& dev = *devs.at(kv.first);
    std::vector<VkDeviceQueueCreateInfo> allQci;

    // Vulkan wants the queues grouped by queue family (and also grouped by
    // device). kv.second (a vector<QueueRequest>) has an unordered list of
    // dev_qfam_index. Assemble QueueRequests by brute force looking for each
    // dev_qfam_index.
    for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
      auto& qfam = dev.qfams.at(q_i);
      for (const auto& qr : kv.second) {
        if (qr.dev_qfam_index == q_i) {
          // prios vector size() is the number of requests for this qfam
          qfam.prios.push_back(qr.priority);
        }
      }

      if (qfam.prios.size() < 1) {
        continue;  // This qfam is not being requested on this dev.
      } else if (qfam.prios.size() > qfam.queueFamilyProperties.queueCount) {
        logE("Cannot request %zu of dev_i=%zu, qFam[%zu] (max %zu allowed)\n",
             qfam.prios.size(), (size_t)kv.first, q_i,
             (size_t)qfam.queueFamilyProperties.queueCount);
        return 1;
      }

      VkDeviceQueueCreateInfo dqci;
      memset(&dqci, 0, sizeof(dqci));
      dqci.sType = autoSType(dqci);
      dqci.queueFamilyIndex = q_i;
      dqci.queueCount = qfam.prios.size();
      dqci.pQueuePriorities = qfam.prios.data();
      allQci.push_back(dqci);
    }

    // Check dev.enableFeatures against dev.availableFeatures.
    for (const auto kv : dev.enabledFeatures.reflect) {
      const auto& name = kv.first.c_str();
      VkBool32 enabled, avail;
      if (dev.enabledFeatures.get(name, enabled) ||
          dev.availableFeatures.get(name, avail)) {
        return 1;
      }
      if (enabled && !avail && dev.enabledFeatures.set(name, VK_FALSE)) {
        logE("enabledFeatures.set(%s, false) failed\n", name);
        return 1;
      }
    }

    VkDeviceCreateInfo dCreateInfo;
    memset(&dCreateInfo, 0, sizeof(dCreateInfo));
    dCreateInfo.sType = autoSType(dCreateInfo);
    dCreateInfo.queueCreateInfoCount = allQci.size();
    dCreateInfo.pQueueCreateInfos = allQci.data();
    if (apiVersionInUse() < VK_MAKE_VERSION(1, 1, 0)) {
      dCreateInfo.pEnabledFeatures = &dev.enabledFeatures.features;
    } else {
      dCreateInfo.pEnabledFeatures = NULL;
      dCreateInfo.pNext = &dev.enabledFeatures;
    }
    std::vector<const char*> reqPtrs;
    if (dev.requiredExtensions.size()) {
      dCreateInfo.enabledExtensionCount = dev.requiredExtensions.size();
      for (auto& e : dev.requiredExtensions) {
        reqPtrs.push_back(e.c_str());
      }
      dCreateInfo.ppEnabledExtensionNames = reqPtrs.data();
    }
    // As of Vulkan 1.0.33, device-only layers are now deprecated.
    dCreateInfo.enabledLayerCount = 0;
    dCreateInfo.ppEnabledLayerNames = 0;

    insideVkCreateDevice = true;
    VkResult v = vkCreateDevice(dev.phys, &dCreateInfo, pAllocator, &dev.dev);
    insideVkCreateDevice = false;
    if (v != VK_SUCCESS) {
      char who[256];
      snprintf(who, sizeof(who), "dev_i=%zu vkCreateDevice", (size_t)kv.first);
      return explainVkResult(who, v);
    }
    dev.dev.allocator = pAllocator;
    dev.swapChainInfo.imageExtent = surfaceSizeRequest;
    // load() may remove an extesion from requiredExtensions if it fails.
    dev.fp.load(dev);
    if (!dev.getName().empty() && dev.setName(dev.getName())) {
      logE("vkCreateDevice then dev.setName failed\n");
      return 1;
    }
  }

  // vkGetDeviceQueue returns the created queues - fill in dev.qfams.
  size_t swap_chain_count = 0;
  for (const auto& kv : requested_devs) {
    auto& dev = *devs.at(kv.first);
    size_t q_count = 0;
    for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
      auto& qfam = dev.qfams.at(q_i);
      // Copy the newly minted VkQueue objects into dev.qfam.queues.
      for (size_t i = 0; i < qfam.prios.size(); i++) {
        qfam.queues.emplace_back();
        vkGetDeviceQueue(dev.dev, q_i, i, &(*(qfam.queues.end() - 1)));
        q_count++;
      }
    }
    if (!q_count) {
      dev.presentModes.clear();
    } else if (dev.presentModes.size()) {
      if (swap_chain_count == 1) {
        logW("Warn: Using two GPUs at once is unsupported.\n");
        logW("Warn: Here be dragons.\n");
        logW("https://lunarg.com/faqs/vulkan-multiple-gpus-acceleration/\n");
      }
      swap_chain_count++;
    }
  }

  // Move successfully opened devices to the front of the list.
  size_t keepers = 0;
  for (size_t i = 0; i < devs.size(); i++) {
    if (devs.at(i)->dev) {
      if (i != keepers) {
        // Insert devs.at(i) where devs.at(keepers) is now.
        auto keep = devs.at(i);
        devs.erase(devs.begin() + i);
        devs.insert(devs.begin() + keepers, keep);
      }
      keepers++;
    }
  }
  return 0;
}

}  // namespace language
