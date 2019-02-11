/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file contains RenderPass::ctorError which calls vkCreateRenderPass
 * (or vkCreateRenderPass2KHR).
 */
#include <src/memory/memory.h>

extern char** environ;

namespace command {
int RenderPass::ctorError() {
  if (shaders.empty()) {
    logE("%s: 0 shaders\n", "RenderPass::ctorError");
    return 1;
  }
  if (pipelines.empty()) {
    logE("%s: 0 pipelines\n", "RenderPass::ctorError");
    return 1;
  }

  // NVidia Nsight Graphics does not yet support vkCreateRenderPass2.
  bool detectNVidiaNsightGraphics = false;
  for (char** env = environ; *env; env++) {
    char key[256];
    char* split = strchr(*env, '=');
    if (split) {
      strncpy(key, *env, sizeof(key));
      key[split - *env] = 0;
      key[sizeof(key) - 1] = 0;
      if (!strcmp(key, "ENABLE_VK_LAYER_NV_nomad") ||
          !strncmp(key, "NOMAD_", 6) || !strncmp(key, "NSIGHT_", 7)) {
        if (!detectNVidiaNsightGraphics) {
          logW("environment has \"%s\" - disabling vkCreateRenderPass2\n",
               *env);
        }
        detectNVidiaNsightGraphics = true;
      }
    }
  }
  // vk.dev.getSurface(): work around bug: if there is no surface (i.e. this is
  // a headless app) loader exports vkCreateRenderPass2KHR but driver does not,
  // causing a null pointer dereference.
  if (!detectNVidiaNsightGraphics && vk.dev.getSurface() &&
      vk.dev.fp.createRenderPass2) {
    VkRenderPassCreateInfo2KHR VkInit(rpci2);
    std::vector<VkAttachmentDescription2KHR> attachments;
    std::vector<VkAttachmentReference2KHR> refs;
    std::vector<VkSubpassDescription2KHR> subpasses;
    std::vector<VkSubpassDependency2KHR> subpassdeps;
    if (getVkRenderPassCreateInfo2KHR(rpci2, attachments, refs, subpasses,
                                      subpassdeps)) {
      logE("%s failed\n", "getVkRenderPassCreateInfo2KHR");
      return 1;
    }

    VkResult v = vk.dev.fp.createRenderPass2(vk.dev.dev, &rpci2,
                                             vk.dev.dev.allocator, &vk);
    if (v != VK_SUCCESS) {
      return explainVkResult("vkCreateRenderPass2KHR", v);
    }
  } else {
    // If createRenderPass2 is not going to be used, set all related function
    // pointers to nullptr, so other code does not treat the VkRenderPass as if
    // it supported createRenderPass2.
    vk.dev.fp.createRenderPass2 = nullptr;
    vk.dev.fp.beginRenderPass2 = nullptr;
    vk.dev.fp.nextSubpass2 = nullptr;
    vk.dev.fp.endRenderPass2 = nullptr;
    VkRenderPassCreateInfo VkInit(rpci);
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> refs;
    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDependency> subpassdeps;
    if (getVkRenderPassCreateInfo(rpci, attachments, refs, subpasses,
                                  subpassdeps)) {
      logE("%s failed\n", "getVkRenderPassCreateInfo");
      return 1;
    }

    VkResult v =
        vkCreateRenderPass(vk.dev.dev, &rpci, vk.dev.dev.allocator, &vk);
    if (v != VK_SUCCESS) {
      return explainVkResult("vkCreateRenderPass", v);
    }
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();

  for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
    auto& pipeline = pipelines.at(subpass_i);
    if (pipeline->ctorError(*this, subpass_i)) {
      logE("%s: pipeline[%zu].ctorError failed\n", "RenderPass::ctorError",
           subpass_i);
      return 1;
    }
  }

  // If non-default target image:
  if (image) {
    if (!image->vk) {
      logE("%s: target image ctorError must be called first.\n",
           "RenderPass::ctorError");
      return 1;
    }

    // Build and finalize image and imageFramebuf using RenderPass::vk.
    imageFramebuf->image.clear();
    imageFramebuf->image.emplace_back(image->vk);
    imageFramebuf->attachments.clear();
    imageFramebuf->attachments.emplace_back(vk.dev);
    auto& attach = imageFramebuf->attachments.back();
    if (attach.ctorError(image->vk, image->info.format)) {
      logE("%s: framebuf.attachment.ctorError failed\n",
           "RenderPass::ctorError");
      return 1;
    }

    if (imageFramebuf->ctorError(vk, image->info.extent.width,
                                 image->info.extent.height)) {
      logE("imageFramebuf->ctorError failed\n");
      return 1;
    }
  }
  return 0;
}

}  // namespace command
