/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <string.h>
#include "VkPtr.h"

#pragma once

// VkInit simplifies vulkan object initialization idioms by automatically
// initializing the Vulkan object as soon as it is instantiated.
#define VkInit(x) \
  x;              \
  ::language::internal::_VkInit(x)

#define VkOverwrite(x) language::internal::_VkInit(x)

namespace language {
namespace internal {

inline void _VkInit(VkApplicationInfo& app) {
  memset(&app, 0, sizeof(app));
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
};

inline void _VkInit(VkInstanceCreateInfo& ici) {
  memset(&ici, 0, sizeof(ici));
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
};

inline void _VkInit(VkDebugReportCallbackCreateInfoEXT& drcb) {
  memset(&drcb, 0, sizeof(drcb));
  drcb.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
};

inline void _VkInit(VkDeviceCreateInfo& dci) {
  memset(&dci, 0, sizeof(dci));
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
}

inline void _VkInit(VkDeviceQueueCreateInfo& qinfo) {
  memset(&qinfo, 0, sizeof(qinfo));
  qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
}

inline void _VkInit(VkPhysicalDeviceFeatures& df) {
  memset(&df, 0, sizeof(df));
  // VkPhysicalDeviceFeatures has no 'sType'.
}

inline void _VkInit(VkSwapchainCreateInfoKHR& scci) {
  memset(&scci, 0, sizeof(scci));
  scci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
}

inline void _VkInit(VkImageViewCreateInfo& ivci) {
  memset(&ivci, 0, sizeof(ivci));
  ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
}

inline void _VkInit(VkShaderModuleCreateInfo& smci) {
  memset(&smci, 0, sizeof(smci));
  smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
}

inline void _VkInit(VkPipelineShaderStageCreateInfo& ssci) {
  memset(&ssci, 0, sizeof(ssci));
  ssci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
}

inline void _VkInit(VkPipelineVertexInputStateCreateInfo& vsci) {
  memset(&vsci, 0, sizeof(vsci));
  vsci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineInputAssemblyStateCreateInfo& asci) {
  memset(&asci, 0, sizeof(asci));
  asci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineViewportStateCreateInfo& vsci) {
  memset(&vsci, 0, sizeof(vsci));
  vsci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineRasterizationStateCreateInfo& rsci) {
  memset(&rsci, 0, sizeof(rsci));
  rsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineMultisampleStateCreateInfo& msci) {
  memset(&msci, 0, sizeof(msci));
  msci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineDepthStencilStateCreateInfo& dsci) {
  memset(&dsci, 0, sizeof(dsci));
  dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineColorBlendAttachmentState& cbas) {
  memset(&cbas, 0, sizeof(cbas));
  // VkPipelineColorBlendAttachmentState has no 'sType'.
}

inline void _VkInit(VkPipelineColorBlendStateCreateInfo& cbsci) {
  memset(&cbsci, 0, sizeof(cbsci));
  cbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineDynamicStateCreateInfo& pdsci) {
  memset(&pdsci, 0, sizeof(pdsci));
  pdsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineLayoutCreateInfo& plci) {
  memset(&plci, 0, sizeof(plci));
  plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
}

inline void _VkInit(VkAttachmentDescription& ad) {
  memset(&ad, 0, sizeof(ad));
  // VkAttachmentDescription has no 'sType'.
}

inline void _VkInit(VkAttachmentReference& ar) {
  memset(&ar, 0, sizeof(ar));
  // VkAttachmentReference has no 'sType'.
}

inline void _VkInit(VkSubpassDescription& sd) {
  memset(&sd, 0, sizeof(sd));
  // VkSubpassDescription has no 'sType'.
}

inline void _VkInit(VkRenderPassCreateInfo& rpci) {
  memset(&rpci, 0, sizeof(rpci));
  rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

inline void _VkInit(VkSubpassDependency& spd) {
  memset(&spd, 0, sizeof(spd));
  // VkSubpassDependency has no 'sType'.
}

inline void _VkInit(VkGraphicsPipelineCreateInfo& gpci) {
  memset(&gpci, 0, sizeof(gpci));
  gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
}

inline void _VkInit(VkFramebufferCreateInfo& fbci) {
  memset(&fbci, 0, sizeof(fbci));
  fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
}

inline void _VkInit(VkSemaphoreCreateInfo& sci) {
  memset(&sci, 0, sizeof(sci));
  sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
}

inline void _VkInit(VkFenceCreateInfo& fci) {
  memset(&fci, 0, sizeof(fci));
  fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
}

inline void _VkInit(VkEventCreateInfo& eci) {
  memset(&eci, 0, sizeof(eci));
  eci.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
}

inline void _VkInit(VkCommandPoolCreateInfo& cpci) {
  memset(&cpci, 0, sizeof(cpci));
  cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
}

inline void _VkInit(VkRenderPassBeginInfo& rpbi) {
  memset(&rpbi, 0, sizeof(rpbi));
  rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
}

inline void _VkInit(VkPresentInfoKHR& pik) {
  memset(&pik, 0, sizeof(pik));
  pik.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
}

#ifdef VK_KHR_XCB_SURFACE_EXTENSION_NAME
inline void _VkInit(VkXcbSurfaceCreateInfoKHR& xcb) {
  memset(&xcb, 0, sizeof(xcb));
  xcb.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
}
#endif

inline void _VkInit(VkSubmitInfo& si) {
  memset(&si, 0, sizeof(si));
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
}

inline void _VkInit(VkCommandBufferAllocateInfo& ai) {
  memset(&ai, 0, sizeof(ai));
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
}

inline void _VkInit(VkCommandBufferBeginInfo& cbbi) {
  memset(&cbbi, 0, sizeof(cbbi));
  cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
}

inline void _VkInit(VkMemoryAllocateInfo& mai) {
  memset(&mai, 0, sizeof(mai));
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
}

inline void _VkInit(VkBufferCreateInfo& bci) {
  memset(&bci, 0, sizeof(bci));
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
}

inline void _VkInit(VkImageCreateInfo& ici) {
  memset(&ici, 0, sizeof(ici));
  ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
}

inline void _VkInit(VkImageMemoryBarrier& imb) {
  memset(&imb, 0, sizeof(imb));
  imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
}

inline void _VkInit(VkSamplerCreateInfo& sci) {
  memset(&sci, 0, sizeof(sci));
  sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
}

inline void _VkInit(VkDescriptorPoolCreateInfo& dpci) {
  memset(&dpci, 0, sizeof(dpci));
  dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
}

inline void _VkInit(VkDescriptorPoolSize& dps) { memset(&dps, 0, sizeof(dps)); }

inline void _VkInit(VkDescriptorSetLayoutCreateInfo& dsli) {
  memset(&dsli, 0, sizeof(dsli));
  dsli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
}

inline void _VkInit(VkDescriptorSetLayoutBinding& dslb) {
  memset(&dslb, 0, sizeof(dslb));
}

inline void _VkInit(VkDescriptorSetAllocateInfo& dsai) {
  memset(&dsai, 0, sizeof(dsai));
  dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
}

inline void _VkInit(VkWriteDescriptorSet& wds) {
  memset(&wds, 0, sizeof(wds));
  wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
}

inline void _VkInit(VkMappedMemoryRange& mmr) {
  memset(&mmr, 0, sizeof(mmr));
  mmr.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
}

inline void _VkInit(VkPushConstantRange& pcr) { memset(&pcr, 0, sizeof(pcr)); }

}  // namespace internal
}  // namespace language
