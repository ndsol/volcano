/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * findInPaths() is a useful cross-platform function to find a file
 * using a built-in set of paths and return where the file is found.
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "command.h"

#ifdef __ANDROID__
int findInPaths(const char* filename, std::string& foundPath) {
  foundPath = filename;
  return 0;
}

#else

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define O_RDONLY _O_RDONLY
#else
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__sun) && defined(__SVR4) /* Solaris */
#include <stdlib.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

static std::vector<std::string> findInPrefixes;
static int isFindInPrefixesInitialized = 0;

#if defined(__NetBSD__) || defined(__DragonFly__) || defined(__linux__)
static void getSelfPathFrom(std::vector<char>& selfPath, const char* proc_exe) {
  selfPath.resize(1024);
  for (;;) {
    ssize_t r = readlink(proc_exe, &selfPath[0], selfPath.size());
    if (r < 0) {
      logE("readlink(%s) failed: %d %s\n", proc_exe, errno, strerror(errno));
    } else if ((size_t)r < selfPath.size()) {
      selfPath[r] = 0;
    } else {
      selfPath.resize(selfPath.size() + 1024);
      continue;
    }
    return;
  }
}
#endif

static int initFindInPrefixes() {
  if (isFindInPrefixesInitialized) {
    return 0;
  }
  // Get the path of the current executable using platform-specific code.
  std::vector<char> selfPath;
#if defined(_WIN32)
  selfPath.resize(1024);
  for (;;) {
    auto r = GetModuleFileName(NULL /*ask for exe, not a dll*/, &selfPath[0],
                               selfPath.size());
    if (!r) {
      logE("GetModuleFileName(NULL) failed: %d\n", GetLastError());
      return 1;
    } else if (r == selfPath.size()) {
      selfPath.resize(selfPath.size() + 1024);
      continue;
    }
    break;
  }
#define OS_SEPARATOR '\\'
#elif defined(__APPLE__)
  uint32_t size = 1024;
  do {
    selfPath.resize(size);
  } while (_NSGetExecutablePath(&selfPath[0], &size));
#elif defined(__sun) && defined(__SVR4) /* Solaris */
  const char* execName = getexecname();
  size_t size = strlen(execName) + 1;
  selfPath.resize(size);
  memcpy(&selfPath[0], execName, size);
#elif defined(__FreeBSD__)
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
  size_t size = 1024;
  while (size != selfPath.size()) {
    selfPath.resize(size);
    if (sysctl(mib, 4, buf, &size, NULL, 0) < 0) {
      if (errno == ENOMEM) continue;
      logE("KERN_PROC_PATHNAME failed: %d %s\n", errno, strerror(errno));
      return 1;
    }
  }
#elif defined(__NetBSD__) || defined(__DragonFly__)
  getSelfPathFrom(selfPath, "/proc/curproc/exe");
#elif defined(__linux__)
  getSelfPathFrom(selfPath, "/proc/self/exe");
#else
#error selfPath is not supported on this OS.
#endif

#ifndef _WIN32
#define OS_SEPARATOR '/'
#endif

  std::string resDir(&selfPath[0]);
  auto lastSlash = resDir.rfind(OS_SEPARATOR);
  if (lastSlash == std::string::npos) {
    // selfPath contains a filename and no path. Try a relative "res" dir.
    resDir = "res";
  } else {
    resDir.replace(lastSlash, resDir.size() - lastSlash, "");
    resDir += OS_SEPARATOR;
    resDir += "res";
  }
  resDir += OS_SEPARATOR;

  // Always prefer a file in the current working directory.
  findInPrefixes.emplace_back("");
  findInPrefixes.emplace_back(resDir);
  isFindInPrefixesInitialized = 1;
  return 0;
}

// On non-Android platforms, search several paths for a filename.
// Note: foundPath is corrupted during the search. If findInPaths returns
// 0, foundPath is valid. Otherwise, foundPath is the last failed path tried.
int findInPaths(const char* filename, std::string& foundPath) {
  if (initFindInPrefixes()) {
    logE("findInPaths(%s): initFindInPrefixes failed\n", filename);
    return 1;
  }
  for (const auto prefix : findInPrefixes) {
    foundPath = prefix + filename;
    int fd = open(foundPath.c_str(), O_RDONLY);
    if (fd < 0) {
      continue;
    }
    close(fd);
    // foundPath is valid.
    return 0;
  }
  // filename not found.
  return -1;
}

#endif /* __ANDROID__ */
