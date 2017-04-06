/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "command.h"

namespace command {

int Shader::loadSPV(const uint32_t* spvBegin, size_t len) {
  if ((len & 3) != 0) {
    fprintf(stderr, "LoadSPV(%p, %zu): len must be a multiple of 4\n", spvBegin,
            len);
    return 1;
  }

  VkShaderModuleCreateInfo VkInit(smci);
  smci.codeSize = len;
  smci.pCode = spvBegin;
  VkResult r = vkCreateShaderModule(vkdev, &smci, nullptr, &vk);
  if (r != VK_SUCCESS) {
    fprintf(stderr, "loadSPV(%p, %zu) vkCreateShaderModule failed: %d (%s)\n",
            spvBegin, len, r, string_VkResult(r));
    return 1;
  }
  return 0;
}

int Shader::loadSPV(const char* filename) {
  int infile = open(filename, O_RDONLY);
  if (infile < 0) {
    fprintf(stderr, "loadSPV: open(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  struct stat s;
  if (fstat(infile, &s) == -1) {
    fprintf(stderr, "loadSPV: fstat(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  char* map =
      (char*)mmap(0, s.st_size, PROT_READ, MAP_SHARED, infile, 0 /*offset*/);
  if (!map) {
    fprintf(stderr, "loadSPV: mmap(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    close(infile);
    return 1;
  }

  int r = loadSPV(map, map + s.st_size);
  if (munmap(map, s.st_size) < 0) {
    fprintf(stderr, "loadSPV: munmap(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    close(infile);
    return 1;
  }
  if (close(infile) < 0) {
    fprintf(stderr, "loadSPV: close(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  return r;
}

int PipelineCreateInfo::addShader(std::shared_ptr<Shader> shader,
                                  RenderPass& renderPass,
                                  VkShaderStageFlagBits stageBits,
                                  std::string entryPointName /*= "main"*/) {
  stages.emplace_back();
  auto& pipelinestage = stages.back();
  pipelinestage.info.stage = stageBits;
  pipelinestage.entryPointName = entryPointName;

  // renderPass.shaders is a set, so shader will not be duplicated in it.
  auto result = renderPass.shaders.insert(shader);
  // result.first is a std::iterator<std::shared_ptr<Shader>> pointing to
  // the Shader in the set.
  // result.second is (bool) false if the shader was a duplicate -- this
  // is not an error: shaders can be reused.
  pipelinestage.shader = *result.first;
  return 0;
}

}  // namespace command
