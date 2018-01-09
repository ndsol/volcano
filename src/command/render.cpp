/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int RenderPass::ctorError() {
  if (shaders.empty()) {
    logE("RenderPass::init: 0 shaders\n");
    return 1;
  }
  if (pipelines.empty()) {
    logE("RenderPass::init: 0 pipelines\n");
    return 1;
  }
  language::Device& dev = pipelines.at(0)->dev;

  // Check all pipelines have an identical array of .attach[].vk. All pipelines
  // in a RenderPass must work from the same set of VkAttachmentDescription.
  //
  // .attach[].refvk can differ. If .refvk.attachment != VK_ATTACHMENT_UNUSED,
  // substitute N when pipeline.attach[N].refvk is copied to pci.subpassDesc[N].
  //
  // depthRef is handled separately: pci.subpassDesc.pDepthStencilAttachment is
  // separate from pci.subpassDesc.pColorAttachments. (But the depth image
  // attachment must still be in .attach[].vk.)
  std::vector<VkSubpassDescription> subpassVk;
  std::vector<VkSubpassDependency> depVk;
  auto& pci0a = pipelines.at(0)->info.attach;

  constexpr size_t colorAttachStart = 0;

  for (size_t i = 1; i < pipelines.size(); i++) {
    auto& pci1a = pipelines.at(i)->info.attach;
    if (pci1a.size() == pci0a.size()) {
      for (size_t j = 0; j < pci1a.size(); j++) {
        if (memcmp(&pci1a.at(j).vk, &pci0a.at(j).vk, sizeof(pci1a[0].vk))) {
          goto differentAttach;
        }
      }
      continue;
    }
  differentAttach:
    logE("pipelines[%zu].info.attach does not match pipelines[0].\n", i);
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

  std::vector<VkAttachmentDescription> descs(pci0a.size());
  std::vector<VkAttachmentReference> refs(pci0a.size());
  // Get descs, including one for a depth image (if one exists).
  for (size_t dst = 0, src = 0; dst < depthAttachStart; dst++) {
    if ((int)src == depthRefIndex) {
      continue;
    }
    descs.at(dst) = pci0a.at(src).vk;
    refs.at(dst) = pci0a.at(src).refvk;
    src++;
    if (refs.at(dst).attachment != VK_ATTACHMENT_UNUSED) {
      refs.at(dst).attachment = dst;
    }
  }

  if (depthRefIndex != -1) {
    auto& d = pipelines.at(depthRefPipeline)->info.attach.at(depthRefIndex);
    descs.at(depthAttachStart) = d.vk;
    refs.at(depthAttachStart) = d.refvk;
    refs.at(depthAttachStart).attachment = depthAttachStart;
  }

  for (size_t i = 0; i < pipelines.size(); i++) {
    auto& s = pipelines.at(i)->info.subpassDesc;
    s.colorAttachmentCount = depthAttachStart;
    s.pColorAttachments = refs.data() + colorAttachStart;
    if (depthRefIndex != -1) {
      s.pDepthStencilAttachment = &refs.at(depthAttachStart);
    }
    subpassVk.push_back(s);
    if (getSubpassDeps(i, depVk)) {
      return 1;
    }
  }
  rpci.attachmentCount = descs.size();
  rpci.pAttachments = descs.data();
  rpci.subpassCount = subpassVk.size();
  rpci.pSubpasses = subpassVk.data();
  rpci.dependencyCount = depVk.size();
  rpci.pDependencies = depVk.data();

  VkResult v = vkCreateRenderPass(dev.dev, &rpci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateRenderPass", v, string_VkResult(v));
    return 1;
  }

  for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
    auto& pipeline = pipelines.at(subpass_i);
    if (pipeline->init(*this, subpass_i)) {
      logE("RenderPass::init pipeline[%zu] init failed\n", subpass_i);
      return 1;
    }
  }
  return 0;
}

}  // namespace command
