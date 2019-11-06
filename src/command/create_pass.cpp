/* Copyright (c) 2018 the Volcano Authors. Licensed under the GPLv3.
 * This contains the code to initialize a VkRenderPassCreateInfo
 * (or VkRenderPassCreateInfo2KHR)
 */
#include "command.h"

namespace command {

int RenderPass::getSubpassDeps(size_t subpass_i,
                               VkSubpassDependency2KHR& dep) const {
  // Link this subpass to the previous one.
  memset(&dep, 0, sizeof(dep));
  dep.sType = autoSType(dep);
  dep.srcSubpass = (subpass_i == 0) ? VK_SUBPASS_EXTERNAL : (subpass_i - 1);
  dep.dstSubpass = subpass_i;
  dep.dependencyFlags = 0;
  if (subpass_i == 0) {
    dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dep.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  } else {
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if (subpass_i == pipelines.size() - 1) {
    // TODO: This can be COLOR_ATTACHMENT_OUTPUT if the previous pass rendered
    // to the framebuffer.
    dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dep.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  } else {
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  return 0;
}

int RenderPass::getVkRenderPassCreateInfo2KHR(
    VkRenderPassCreateInfo2KHR& rpci2,
    std::vector<VkAttachmentDescription2KHR>& attachments,
    std::vector<VkAttachmentReference2KHR>& refs,
    std::vector<VkSubpassDescription2KHR>& subpasses,
    std::vector<VkSubpassDependency2KHR>& subpassdeps) const {
  // Check all pipelines have an identical array of .attach[].vk. All pipelines
  // in a RenderPass must work from the same set of VkAttachmentDescription2KHR.
  //
  // .attach[].refvk can differ. If .refvk.attachment != VK_ATTACHMENT_UNUSED,
  // substitute N when pipeline.attach[N].refvk is copied to pci.subpassDesc[N].
  //
  // depthRef is handled separately: pci.subpassDesc.pDepthStencilAttachment is
  // separate from pci.subpassDesc.pColorAttachments. (But the depth image
  // attachment must still be in .attach[].vk.)
  auto& pci0a = pipelines.at(0)->info.attach;
  constexpr size_t colorAttachStart = 0;

  for (size_t i = 1; i < pipelines.size(); i++) {
    auto& pci1a = pipelines.at(i)->info.attach;
    if (pci1a.size() == pci0a.size()) {
      bool same = true;
      for (size_t j = 0; j < pci1a.size(); j++) {
        if (memcmp(&pci1a.at(j).vk, &pci0a.at(j).vk, sizeof(pci1a[0].vk))) {
          logE("pipelines[%zu].info.attach[%zu] must match pipelines[0].\n", i,
               j);
          same = false;
        }
      }
      if (same) {
        continue;
      }
    } else {
      logE("pipelines[%zu].info.attach, size %zu, is not %zu\n", i,
           pci1a.size(), pci0a.size());
    }
    logE("All RenderPass pipelines must have matching attach state.\n");
    return 1;
  }

  // Scan all pipelines for a depth image.
  size_t depthAttachStart = pci0a.size();
  int depthRefIndex = -1;
  size_t depthRefPipeline = 0;
  for (size_t i = 0; i < pipelines.size(); i++) {
    auto& pci1a = pipelines.at(i)->info.attach;
    for (size_t j = 0; j < pci1a.size(); j++) {
      if (pci1a.at(j).refvk.attachment != VK_ATTACHMENT_UNUSED &&
          pci1a.at(j).refvk.layout ==
              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        if (depthRefIndex != -1 && depthRefIndex != (int)j) {
          logE("PipelineCreateInfo[%zu].attach[%zu] and\n", i, j);
          logE("[%zu].attach[%d] are both DEPTH, but do not match!\n",
               depthRefPipeline, depthRefIndex);
          return 1;
        }
        depthAttachStart = pci0a.size() - 1;
        depthRefIndex = j;
        depthRefPipeline = i;
      }
    }
  }

  // Get attachments, including one for a depth image (if one exists).
  attachments.clear();
  attachments.resize(pci0a.size());
  refs.clear();
  refs.resize(pci0a.size());

  for (size_t dst = 0, src = 0; dst < depthAttachStart; dst++) {
    if ((int)src == depthRefIndex) {
      continue;
    }
    attachments.at(dst) = pci0a.at(src).vk;

    // Copy the layout and aspectMask from
    // struct PipelineAttachment
    refs.at(dst) = pci0a.at(src).refvk;

    src++;
    if (refs.at(dst).attachment != VK_ATTACHMENT_UNUSED) {
      refs.at(dst).attachment = dst;
    }
  }

  if (depthRefIndex != -1) {
    auto& d = pipelines.at(depthRefPipeline)->info.attach.at(depthRefIndex);
    attachments.at(depthAttachStart) = d.vk;
    refs.at(depthAttachStart) = d.refvk;
    refs.at(depthAttachStart).attachment = depthAttachStart;
  }

  // Convert pipelines into subpasses / subpassdeps.
  subpasses.clear();
  subpassdeps.clear();
  for (size_t i = 0; i < pipelines.size(); i++) {
    auto& s = pipelines.at(i)->info.subpassDesc;
    s.colorAttachmentCount = depthAttachStart;
    s.pColorAttachments = refs.data() + colorAttachStart;
    if (depthRefIndex != -1) {
      s.pDepthStencilAttachment = &refs.at(depthAttachStart);
    }
    subpasses.push_back(s);
    subpassdeps.emplace_back();
    if (getSubpassDeps(i, subpassdeps.back())) {
      subpassdeps.pop_back();
      return 1;
    }
  }

  // Fill in rpci2 with subpasses.
  rpci2.attachmentCount = attachments.size();
  rpci2.pAttachments = attachments.data();
  rpci2.subpassCount = subpasses.size();
  rpci2.pSubpasses = subpasses.data();
  rpci2.dependencyCount = subpassdeps.size();
  rpci2.pDependencies = subpassdeps.data();

  return 0;
}

// find() is a helper function to verify that a list of
// VkAttachmentReference2KHR objects exist in the overall refs2 vector.
// Return 1 if any problem is encountered. Otherwise return 0 and set base.
static int find(std::vector<VkAttachmentReference2KHR>& refs2, uint32_t count,
                const VkAttachmentReference2KHR* list, size_t& base) {
  if (!list || !count) {
    base = 0;
    return 0;
  }
  // Find list[0].
  for (size_t i = 0; i < refs2.size() - count + 1; i++) {
    if (!memcmp(&refs2.at(i), &list[0], sizeof(list[0]))) {
      // Check for the entire list.
      size_t j;
      for (j = 1; j < count; j++) {
        if (memcmp(&refs2.at(i + j), &list[j], sizeof(list[0]))) {
          break;
        }
      }
      if (j >= count) {
        base = i;
        return 0;
      }
    }
  }
  return 1;
}

int RenderPass::getVkRenderPassCreateInfo(
    VkRenderPassCreateInfo& rpci,
    std::vector<VkAttachmentDescription>& attachments,
    std::vector<VkAttachmentReference>& refs,
    std::vector<VkSubpassDescription>& subpasses,
    std::vector<VkSubpassDependency>& subpassdeps) const {
  // First run getVkRenderPassCreateInfo2KHR(). No point in duplicating it.
  VkRenderPassCreateInfo2KHR rpci2;
  std::vector<VkAttachmentDescription2KHR> attachments2;
  std::vector<VkAttachmentReference2KHR> refs2;
  std::vector<VkSubpassDescription2KHR> subpasses2;
  std::vector<VkSubpassDependency2KHR> subpassdeps2;
  if (getVkRenderPassCreateInfo2KHR(rpci2, attachments2, refs2, subpasses2,
                                    subpassdeps2)) {
    logE("getVkRenderPassCreateInfo: getVkRenderPassCreateInfo2KHR failed\n");
    return 1;
  }

  // Get attachments, including one for a depth image (if one exists).
  attachments.clear();
  attachments.reserve(attachments2.size());
  for (size_t i = 0; i < attachments2.size(); i++) {
    VkAttachmentDescription ad;
    memset(&ad, 0, sizeof(ad));
    ad.flags = attachments2.at(i).flags;
    ad.format = attachments2.at(i).format;
    ad.samples = attachments2.at(i).samples;
    ad.loadOp = attachments2.at(i).loadOp;
    ad.storeOp = attachments2.at(i).storeOp;
    ad.stencilLoadOp = attachments2.at(i).stencilLoadOp;
    ad.stencilStoreOp = attachments2.at(i).stencilStoreOp;
    ad.initialLayout = attachments2.at(i).initialLayout;
    ad.finalLayout = attachments2.at(i).finalLayout;
    attachments.emplace_back(ad);
  }

  refs.clear();
  refs.reserve(refs2.size());
  for (size_t i = 0; i < refs2.size(); i++) {
    refs.emplace_back();
    // ignore refs2.at(i).aspectMask
    refs.back().attachment = refs2.at(i).attachment;
    refs.back().layout = refs2.at(i).layout;
  }

  // Convert pipelines into subpasses / subpassdeps.
  subpasses.clear();
  subpassdeps.clear();
  for (size_t i = 0; i < pipelines.size(); i++) {
    auto& s = pipelines.at(i)->info.subpassDesc;

    // s.pInputAttachments[0..count] must map to a fixed offset in refs2
    // so that it can be converted to refs.
    size_t inputOfs = 0, colorOfs = 0, resolOfs = 0, dsOfs = 0;
    if (find(refs2, s.inputAttachmentCount, s.pInputAttachments, inputOfs)) {
      logE("inputAttachments not in refs2\n");
      return 1;
    }
    if (find(refs2, s.colorAttachmentCount, s.pColorAttachments, colorOfs)) {
      logE("BUG: colorAttachments not in refs2. Should be at 0!\n");
      return 1;
    }
    if (colorOfs != 0) {
      logE("BUG: colorAttachments in refs2 at %zu. Should be at 0!\n",
           colorOfs);
      return 1;
    }
    if (find(refs2, s.colorAttachmentCount, s.pResolveAttachments, resolOfs)) {
      logE("resolveAttachments not in refs2\n");
      return 1;
    }
    if (find(refs2, 1, s.pDepthStencilAttachment, dsOfs)) {
      logE("depthStencilAttachment not in refs2\n");
      return 1;
    }

    // Down-convert VkSubpassDescription2KHR to VkSubpassDescription.
    if (s.viewMask) {
      logE("pipelines[%zu]->info.subpassDesc.viewMask=%u\n", i, s.viewMask);
      logE("pipelines[%zu] cannot be used in a Vulkan 1.0 %s.\n", i,
           "VkRenderPassCreateInfo");
      return 1;
    }
    VkSubpassDescription sd;
    memset(&sd, 0, sizeof(sd));
    sd.flags = s.flags;
    sd.pipelineBindPoint = s.pipelineBindPoint;
    sd.inputAttachmentCount = s.inputAttachmentCount;
    sd.pInputAttachments = &refs.at(inputOfs);
    sd.colorAttachmentCount = s.colorAttachmentCount;
    sd.pColorAttachments = &refs.at(colorOfs);
    if (s.pResolveAttachments) {
      sd.pResolveAttachments = &refs.at(resolOfs);
    }
    if (s.pDepthStencilAttachment) {
      sd.pDepthStencilAttachment = &refs.at(dsOfs);
    }
    sd.preserveAttachmentCount = s.preserveAttachmentCount;
    sd.pPreserveAttachments = s.pPreserveAttachments;
    subpasses.push_back(sd);

    VkSubpassDependency2KHR& dep2 = subpassdeps2.at(i);
    if (dep2.viewOffset) {
      logE("pipelines[%zu] subpass dependency.viewOffset=%u\n", i,
           dep2.viewOffset);
      logE("pipelines[%zu] cannot be used in a Vulkan 1.0 %s.\n", i,
           "VkRenderPassCreateInfo");
      return 1;
    }
    subpassdeps.emplace_back();
    auto& dep = subpassdeps.back();
    memset(&dep, 0, sizeof(dep));
    dep.srcSubpass = dep2.srcSubpass;
    dep.dstSubpass = dep2.dstSubpass;
    dep.srcStageMask = dep2.srcStageMask;
    dep.dstStageMask = dep2.dstStageMask;
    dep.dependencyFlags = dep2.dependencyFlags;
  }

  // Fill in rpci with subpasses.
  rpci.attachmentCount = attachments.size();
  rpci.pAttachments = attachments.data();
  rpci.subpassCount = subpasses.size();
  rpci.pSubpasses = subpasses.data();
  rpci.dependencyCount = subpassdeps.size();
  rpci.pDependencies = subpassdeps.data();

  return 0;
}

}  // namespace command
