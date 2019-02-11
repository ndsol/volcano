/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
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

inline void _VkInit(VkDebugUtilsMessengerCreateInfoEXT& dumc) {
  memset(&dumc, 0, sizeof(dumc));
  dumc.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
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

inline void _VkInit(VkPhysicalDeviceFeatures2& df) {
  memset(&df, 0, sizeof(df));
  df.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
}

inline void _VkInit(VkPhysicalDeviceProperties& pp) {
  memset(&pp, 0, sizeof(pp));
  // VkPhysicalDeviceProperties has no 'sType'.
}

inline void _VkInit(VkPhysicalDeviceProperties2& pp) {
  memset(&pp, 0, sizeof(pp));
  pp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
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

inline void _VkInit(VkSubpassDependency& spd) {
  memset(&spd, 0, sizeof(spd));
  // VkSubpassDependency has no 'sType'.
}

inline void _VkInit(VkAttachmentDescription2KHR& ad2) {
  memset(&ad2, 0, sizeof(ad2));
  ad2.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
}

inline void _VkInit(VkAttachmentReference2KHR& ar2) {
  memset(&ar2, 0, sizeof(ar2));
  ar2.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
}

inline void _VkInit(VkSubpassDescription2KHR& sd2) {
  memset(&sd2, 0, sizeof(sd2));
  sd2.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR;
}

inline void _VkInit(VkSubpassDependency2KHR& spd2) {
  memset(&spd2, 0, sizeof(spd2));
  spd2.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR;
}

inline void _VkInit(VkRenderPassCreateInfo& rpci) {
  memset(&rpci, 0, sizeof(rpci));
  rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

inline void _VkInit(VkRenderPassCreateInfo2KHR& rpc2) {
  memset(&rpc2, 0, sizeof(rpc2));
  rpc2.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
}

inline void _VkInit(VkSubpassBeginInfoKHR& sbi) {
  memset(&sbi, 0, sizeof(sbi));
  sbi.sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR;
}

inline void _VkInit(VkSubpassEndInfoKHR& sei) {
  memset(&sei, 0, sizeof(sei));
  sei.sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO_KHR;
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

inline void _VkInit(VkCommandBufferInheritanceInfo& cbii) {
  memset(&cbii, 0, sizeof(cbii));
  cbii.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
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

inline void _VkInit(VkImageSubresourceRange& srr) {
  // VkImageSubresourceRange has no sType.
  memset(&srr, 0, sizeof(srr));
}

inline void _VkInit(VkImageSubresourceLayers& srl) {
  // VkImageSubresourceLayers has no sType.
  memset(&srl, 0, sizeof(srl));
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

inline void _VkInit(VkPhysicalDeviceVariablePointerFeatures& vpf) {
  memset(&vpf, 0, sizeof(vpf));
  vpf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES;
}

inline void _VkInit(VkPhysicalDeviceMultiviewFeatures& mvf) {
  memset(&mvf, 0, sizeof(mvf));
  mvf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
}

inline void _VkInit(VkPhysicalDeviceProtectedMemoryFeatures& drm) {
  memset(&drm, 0, sizeof(drm));
  drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
}

inline void _VkInit(VkPhysicalDeviceShaderDrawParameterFeatures& sdp) {
  memset(&sdp, 0, sizeof(sdp));
  sdp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES;
}

inline void _VkInit(VkPhysicalDevice16BitStorageFeatures& s16) {
  memset(&s16, 0, sizeof(s16));
  s16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
}

inline void _VkInit(VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT& boa) {
  memset(&boa, 0, sizeof(boa));
  boa.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT;
}

inline void _VkInit(VkPhysicalDeviceDescriptorIndexingFeaturesEXT& dif) {
  memset(&dif, 0, sizeof(dif));
  dif.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
}

inline void _VkInit(VkPhysicalDeviceVulkanMemoryModelFeaturesKHR& vmm) {
  memset(&vmm, 0, sizeof(vmm));
  vmm.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR;
}

inline void _VkInit(VkPhysicalDeviceIDProperties& idp) {
  memset(&idp, 0, sizeof(idp));
  idp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
}

inline void _VkInit(VkPhysicalDeviceMaintenance3Properties& m3p) {
  memset(&m3p, 0, sizeof(m3p));
  m3p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
}

inline void _VkInit(VkPhysicalDeviceMultiviewProperties& mv) {
  memset(&mv, 0, sizeof(mv));
  mv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
}

inline void _VkInit(VkPhysicalDevicePointClippingProperties& pc) {
  memset(&pc, 0, sizeof(pc));
  pc.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
}

inline void _VkInit(VkPhysicalDeviceProtectedMemoryProperties& drm) {
  memset(&drm, 0, sizeof(drm));
  drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;
}

inline void _VkInit(VkPhysicalDeviceSubgroupProperties& sgp) {
  memset(&sgp, 0, sizeof(sgp));
  sgp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
}

inline void _VkInit(VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT& bop) {
  memset(&bop, 0, sizeof(bop));
  bop.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT;
}

inline void _VkInit(
    VkPhysicalDeviceConservativeRasterizationPropertiesEXT& crp) {
  memset(&crp, 0, sizeof(crp));
  crp.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDeviceDescriptorIndexingPropertiesEXT& dip) {
  memset(&dip, 0, sizeof(dip));
  dip.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDeviceDiscardRectanglePropertiesEXT& drp) {
  memset(&drp, 0, sizeof(drp));
  drp.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDeviceExternalMemoryHostPropertiesEXT& emh) {
  memset(&emh, 0, sizeof(emh));
  emh.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDeviceSampleLocationsPropertiesEXT& slp) {
  memset(&slp, 0, sizeof(slp));
  slp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT& sfp) {
  memset(&sfp, 0, sizeof(sfp));
  sfp.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT& vad) {
  memset(&vad, 0, sizeof(vad));
  vad.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
}

inline void _VkInit(VkPhysicalDevicePushDescriptorPropertiesKHR& pdp) {
  memset(&pdp, 0, sizeof(pdp));
  pdp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
}

inline void _VkInit(
    VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX& nvp) {
  memset(&nvp, 0, sizeof(nvp));
  nvp.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX;
}

inline void _VkInit(VkPhysicalDeviceShaderCorePropertiesAMD& amd) {
  memset(&amd, 0, sizeof(amd));
  amd.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD;
}

inline void _VkInit(VkFormatProperties2& fp) {
  memset(&fp, 0, sizeof(fp));
  fp.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
}

inline void _VkInit(VkPhysicalDeviceMemoryProperties2& dmp) {
  memset(&dmp, 0, sizeof(dmp));
  dmp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
}

inline void _VkInit(VkImageFormatProperties2& ifp) {
  memset(&ifp, 0, sizeof(ifp));
  ifp.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
}

inline void _VkInit(VkExternalImageFormatProperties& efp) {
  memset(&efp, 0, sizeof(efp));
  efp.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
}

inline void _VkInit(VkSamplerYcbcrConversionImageFormatProperties& ycb) {
  memset(&ycb, 0, sizeof(ycb));
  ycb.sType =
      VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
}

#ifdef VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_SPEC_VERSION
inline void _VkInit(VkAndroidHardwareBufferUsageANDROID& ahw) {
  memset(&ahw, 0, sizeof(ahw));
  ahw.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID;
}
#endif

inline void _VkInit(VkTextureLODGatherFormatPropertiesAMD& lg) {
  memset(&lg, 0, sizeof(lg));
  lg.sType = VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD;
}

inline void _VkInit(VkPhysicalDeviceImageFormatInfo2& ifi) {
  memset(&ifi, 0, sizeof(ifi));
  ifi.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
}

inline void _VkInit(VkPhysicalDeviceExternalImageFormatInfo& ei) {
  memset(&ei, 0, sizeof(ei));
  ei.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
}

inline void _VkInit(VkQueueFamilyProperties2& qfp) {
  memset(&qfp, 0, sizeof(qfp));
  qfp.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
}

inline void _VkInit(VkDescriptorSetLayoutSupport& dsl) {
  memset(&dsl, 0, sizeof(dsl));
  dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT;
}

inline void _VkInit(VkBindBufferMemoryInfo& bmi) {
  memset(&bmi, 0, sizeof(bmi));
  bmi.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
}

inline void _VkInit(VkBindImageMemoryInfo& imi) {
  memset(&imi, 0, sizeof(imi));
  imi.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
}

inline void _VkInit(VkMemoryRequirements2& mr2) {
  memset(&mr2, 0, sizeof(mr2));
  mr2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
}

inline void _VkInit(VkMemoryDedicatedRequirements& ded) {
  memset(&ded, 0, sizeof(ded));
  ded.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
}

inline void _VkInit(VkBufferMemoryRequirementsInfo2& mr2) {
  memset(&mr2, 0, sizeof(mr2));
  mr2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
}

inline void _VkInit(VkImageMemoryRequirementsInfo2& mr2) {
  memset(&mr2, 0, sizeof(mr2));
  mr2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
}

inline void _VkInit(VkImagePlaneMemoryRequirementsInfo& pri) {
  memset(&pri, 0, sizeof(pri));
  pri.sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
}

inline void _VkInit(VkDebugMarkerObjectNameInfoEXT& doni) {
  memset(&doni, 0, sizeof(doni));
  doni.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
}

inline void _VkInit(VkDebugUtilsObjectNameInfoEXT& doni) {
  memset(&doni, 0, sizeof(doni));
  doni.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
}

}  // namespace internal
}  // namespace language
