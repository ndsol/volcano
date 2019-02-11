/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "command.h"
#if defined(__GLIBC__) || defined(__ANDROID__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <io.h>
#else
#error Unsupported platform
#endif

namespace command {

int Shader::loadSPV(const uint32_t* spvBegin, size_t len) {
  if ((len & 3) != 0) {
    logE("LoadSPV(%p, %zu): len must be a multiple of 4\n", spvBegin, len);
    return 1;
  }

  {
    std::vector<uint32_t> spv{spvBegin, spvBegin + len / sizeof(*spvBegin)};
    bytes.swap(spv);
  }

  VkShaderModuleCreateInfo VkInit(smci);
  smci.codeSize = len;
  smci.pCode = spvBegin;
  VkResult r =
      vkCreateShaderModule(vk.dev.dev, &smci, vk.dev.dev.allocator, &vk);
  if (r != VK_SUCCESS) {
    char what[256];
    snprintf(what, sizeof(what), "loadSPV(%p, %zu) vkCreateShaderModule",
             spvBegin, len);
    return explainVkResult(what, r);
  }
  vk.allocator = vk.dev.dev.allocator;
  vk.onCreate();
  return 0;
}

int Shader::loadSPV(const char* filename) {
  // MMapFile is defined in core/structs.h.
  MMapFile infile;
  if (infile.mmapRead(filename)) {
    logE("%sload: mmapRead(%s) failed\n", "ShaderLibrary::", filename);
    return 1;
  }
  const uint32_t* map = reinterpret_cast<const uint32_t*>(infile.map);
  return loadSPV(map, infile.len);
}

}  // namespace command
