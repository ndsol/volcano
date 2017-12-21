/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "command.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__GLIBC__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <io.h>
#endif

namespace command {

int Shader::loadSPV(const uint32_t* spvBegin, size_t len) {
  if ((len & 3) != 0) {
    logE("LoadSPV(%p, %zu): len must be a multiple of 4\n", spvBegin, len);
    return 1;
  }

  VkShaderModuleCreateInfo VkInit(smci);
  smci.codeSize = len;
  smci.pCode = spvBegin;
  VkResult r = vkCreateShaderModule(vkdev, &smci, nullptr, &vk);
  if (r != VK_SUCCESS) {
    logE("loadSPV(%p, %zu) vkCreateShaderModule failed: %d (%s)\n", spvBegin,
         len, r, string_VkResult(r));
    return 1;
  }
  return 0;
}

int Shader::loadSPV(const char* filename) {
  int infile = open(filename, O_RDONLY);
  if (infile < 0) {
    logE("loadSPV: open(%s) failed: %d %s\n", filename, errno, strerror(errno));
    return 1;
  }
  struct stat s;
  if (fstat(infile, &s) == -1) {
    logE("loadSPV: fstat(%s) failed: %d %s\n", filename, errno,
         strerror(errno));
    return 1;
  }
#if defined(__GLIBC__) || defined(__APPLE__)
  char* map =
      (char*)mmap(0, s.st_size, PROT_READ, MAP_SHARED, infile, 0 /*offset*/);
#else
  char* map = 0;
  logF("This platform does not support mmap.\n");
#endif
  if (!map) {
    logE("loadSPV: mmap(%s) failed: %d %s\n", filename, errno, strerror(errno));
    close(infile);
    return 1;
  }

  int r = loadSPV(map, map + s.st_size);
#if defined(__GLIBC__) || defined(__APPLE__)
  if (munmap(map, s.st_size) < 0) {
#else
  logF("This platform does not support mmap.\n");
  if (1) {
#endif
    logE("loadSPV: munmap(%s) failed: %d %s\n", filename, errno,
         strerror(errno));
    close(infile);
    return 1;
  }
  if (close(infile) < 0) {
    logE("loadSPV: close(%s) failed: %d %s\n", filename, errno,
         strerror(errno));
    return 1;
  }
  return r;
}

int CommandPool::ctorError(VkCommandPoolCreateFlags flags) {
  if (queueFamily == language::NONE) {
    logE("CommandPool::queueFamily must be set before calling ctorError\n");
    return 1;
  }

  auto qfam_i = dev.getQfamI(queueFamily);
  if (qfam_i == (decltype(qfam_i)) - 1) {
    return 1;
  }

  // Cache QueueFamily, as all commands in this pool must be submitted here.
  qf_ = &dev.qfams.at(qfam_i);

  VkCommandPoolCreateInfo VkInit(cpci);
  cpci.queueFamilyIndex = qfam_i;
  cpci.flags = flags;
  VkResult v = vkCreateCommandPool(dev.dev, &cpci, nullptr, &vk);
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkCreateCommandPool", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

int CommandPool::alloc(std::vector<VkCommandBuffer>& buf,
                       VkCommandBufferLevel level) {
  if (buf.size() < 1) {
    logE("%s failed: buf.size is 0\n", "vkAllocateCommandBuffers");
    return 1;
  }
  VkCommandBufferAllocateInfo VkInit(ai);
  ai.commandPool = vk;
  ai.level = level;
  ai.commandBufferCount = (decltype(ai.commandBufferCount))buf.size();

  VkResult v = vkAllocateCommandBuffers(dev.dev, &ai, buf.data());
  if (v != VK_SUCCESS) {
    logE("%s failed: %d (%s)\n", "vkAllocateCommandBuffers", v,
         string_VkResult(v));
    return 1;
  }
  return 0;
}

VkCommandBuffer CommandPool::borrowOneTimeBuffer() {
  if (!toBorrow) {
    std::vector<VkCommandBuffer> v;
    v.resize(1);
    if (alloc(v)) {
      logE("borrowOneTimeBuffer: alloc failed\n");
      return VK_NULL_HANDLE;
    }
    toBorrow = v.at(0);
    borrowCount = 0;
  }
  if (borrowCount) {
    logE("borrowOneTimeBuffer only has one VkCommandBuffer to lend out.\n");
    logE("This keeps things simple, short, and sweet. Consider whether you\n");
    logE("need two buffers during init, since it will hide bugs.\n");
    return VK_NULL_HANDLE;
  }
  borrowCount++;
  return toBorrow;
}

int CommandPool::unborrowOneTimeBuffer(VkCommandBuffer buf) {
  if (!toBorrow) {
    logE("unborrowOneTimeBuffer: borrowOneTimeBuffer was never called!\n");
    return 1;
  }
  if (!borrowCount) {
    logE("unborrowOneTimeBuffer: borrowOneTimeBuffer has been called.\n");
    logE("unborrowOneTimeBuffer: but the buffer is not currently borrowed!\n");
    return 1;
  }
  if (buf != toBorrow) {
    logE("unborrowOneTimeBuffer(%p): wanted buf=%p\n", (void*)buf,
         (void*)toBorrow);
    return 1;
  }
  borrowCount--;
  return 0;
}

CommandBuffer::~CommandBuffer() {}

// trimSrcStage modifies access bits that are not supported by stage. It also
// simplifies stage selection by tailoring the stage to the access bits'
// implied operation.
void CommandBuffer::trimSrcStage(VkAccessFlags& access,
                                 VkPipelineStageFlags& stage) {
  switch (stage) {
    case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
      // Top of pipe means the host is implicitly aware of this operation.
      // Simplify the barrier by reducing access or picking a later stage.
      // (Vulkan does not do this automatically.)
      if (access & (VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_HOST_READ_BIT)) {
        // Access mask includes HOST_WRITE: all synchronization can be dropped.
        access = 0;
      } else if (access & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) {
        stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      } else if (access & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) {
        stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      } else if (access &
                 (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
                  VK_ACCESS_SHADER_WRITE_BIT)) {
        stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
      } else if (access & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
        stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      } else if (access & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
        stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      } else if (access &
                 (VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT)) {
        stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      }
      break;
    default:
      // User called CommandBuffer::barrier() with a non-default srcStageMask.
      // Assume the user is right.
      break;
  }
}

// trimDstStage modifies access bits that are not supported by stage.
void CommandBuffer::trimDstStage(VkAccessFlags& /*access*/,
                                 VkPipelineStageFlags& /*stage*/) {}

}  // namespace command
