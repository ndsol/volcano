/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int RenderPass::getSubpassDeps(size_t subpass_i,
                               std::vector<VkSubpassDependency>& subpassdeps) {
  // Link this subpass to the previous one.
  VkSubpassDependency VkInit(fromprev);
  fromprev.srcSubpass =
      (subpass_i == 0) ? VK_SUBPASS_EXTERNAL : (subpass_i - 1);
  fromprev.dstSubpass = subpass_i;
  fromprev.dependencyFlags = 0;
  if (subpass_i == 0) {
    fromprev.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    fromprev.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  } else {
    // See
    // https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkSubpassDependency.html
    fprintf(stderr, "TODO: getSubpassDep(src) needs VK_PIPELINE_STAGE_...\n");
    return 1;
  }
  if (subpass_i == pipelines.size() - 1) {
    fromprev.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    fromprev.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  } else {
    fprintf(stderr, "TODO: getSubpassDep(dst) needs VK_PIPELINE_STAGE_...\n");
    return 1;
  }
  subpassdeps.push_back(fromprev);
  return 0;
}

int RenderPass::ctorError(language::Device& dev) {
  if (shaders.empty()) {
    fprintf(stderr, "RenderPass::init: 0 shaders\n");
    return 1;
  }
  if (pipelines.empty()) {
    fprintf(stderr, "RenderPass::init: 0 pipelines\n");
    return 1;
  }

  size_t attachmentCount = 0, attachmentUse = 0;
  for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
    auto& pci = pipelines.at(subpass_i).info;
    attachmentCount += pci.attach.size();
  }
  std::vector<VkAttachmentDescription> attachmentVk(attachmentCount);
  std::vector<VkAttachmentReference> attachmentRefVk(attachmentCount);
  std::vector<VkSubpassDescription> subpassVk;
  std::vector<VkSubpassDependency> depVk;
  for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
    auto& pci = pipelines.at(subpass_i).info;
    if (pci.stages.empty()) {
      fprintf(stderr,
              "RenderPass::init: pcis[%zu].stages "
              "vector cannot be empty\n",
              subpass_i);
      return 1;
    }

    // Bookmark where the pci.attach data will be saved in attachmentRefs.
    size_t pciAttachmentStart = attachmentUse;

    VkAttachmentReference depthRef;
    int depthRefIndex = -1;
    for (size_t attach_i = 0; attach_i < pci.attach.size(); attach_i++) {
      auto& a = pci.attach.at(attach_i);

      // Save each a.refvk into attachmentRefVk, each a.vk into attachmentVk.
      // Up to one depth buffer is added to attachmentRefVk much later.
      if (a.refvk.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        if (depthRefIndex != -1) {
          fprintf(stderr,
                  "PipelineCreateInfo[%zu].attach[%zu] and "
                  "attach[%d] are both DEPTH. Can only have one!\n",
                  subpass_i, attach_i, depthRefIndex);
          return 1;
        }
        depthRefIndex = attach_i;
        depthRef = a.refvk;
      } else {
        a.refvk.attachment = attachmentUse;
        attachmentRefVk.at(attachmentUse) = a.refvk;
        attachmentVk.at(attachmentUse) = a.vk;
        attachmentUse++;
      }
    }

    // Write attachmentRefVk[pciAttachmentStart:] to pci.subpassDesc.
    pci.subpassDesc.colorAttachmentCount = attachmentUse - pciAttachmentStart;
    pci.subpassDesc.pColorAttachments =
        attachmentRefVk.data() + pciAttachmentStart;

    // Write depthRef last.
    if (depthRefIndex != -1) {
      depthRef.attachment = attachmentUse;
      pci.attach.at(depthRefIndex).refvk.attachment = attachmentUse;

      attachmentRefVk.at(attachmentUse) = depthRef;
      attachmentVk.at(attachmentUse) = pci.attach.at(depthRefIndex).vk;

      pci.subpassDesc.pDepthStencilAttachment =
          &attachmentRefVk.at(attachmentUse);

      attachmentUse++;
    }

    // Save pci.subpassDesc into only_vk_subpasses.
    subpassVk.push_back(pci.subpassDesc);
    if (getSubpassDeps(subpass_i, depVk)) {
      return 1;
    }
  }
  rpci.attachmentCount = attachmentVk.size();
  rpci.pAttachments = attachmentVk.data();
  rpci.subpassCount = subpassVk.size();
  rpci.pSubpasses = subpassVk.data();

  rpci.dependencyCount = depVk.size();
  rpci.pDependencies = depVk.data();

  VkResult v = vkCreateRenderPass(dev.dev, &rpci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "%s failed: %d (%s)\n", "vkCreateRenderPass", v,
            string_VkResult(v));
    return 1;
  }

  for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
    auto& pipeline = pipelines.at(subpass_i);
    if (pipeline.init(dev, *this, subpass_i)) {
      fprintf(stderr, "RenderPass::init pipeline[%zu] init failed\n",
              subpass_i);
      return 1;
    }
  }

  passBeginInfo.renderPass = vk;
  // passBeginInfo.framebuffer is left unset. You MUST update it for each frame!
  passBeginInfo.renderArea.offset = {0, 0};
  // passBeginInfo.renderArea.extent is set as a convenience to you. But if you
  // use CommandBuilder::beginRenderPass, beginPrimaryPass, or
  // beginSecondaryPass it is overwritten by beginRenderPass. If your
  // application needs a custom renderArea.extent it must call the right
  // CommandBuilder::beginRenderPass which takes a custom VkRenderPassBeginInfo.
  passBeginInfo.renderArea.extent = dev.swapChainInfo.imageExtent;

  passBeginInfo.clearValueCount = passBeginClearColors.size();
  passBeginInfo.pClearValues = passBeginClearColors.data();
  return 0;
}

}  // namespace command
