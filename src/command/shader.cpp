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

}  // namespace command
