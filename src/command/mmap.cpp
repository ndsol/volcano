/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * Platform-independent mmap implementation.
 */
#include "command.h"
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

MMapFile::~MMapFile() { (void)munmap(); }

int MMapFile::munmap() {
#if defined(__GLIBC__) || defined(__APPLE__) || defined(__ANDROID__)
  if (map) {
    if (::munmap(map, len) < 0) {
      logE("~MMapFile: munmap() failed: %d %s\n", errno, strerror(errno));
      if (fd != -1) close(fd);
      return 1;
    }
    map = nullptr;
  }
  if (fd != -1) {
    if (::close(fd) < 0) {
      logE("~MMapFile: close() failed: %d %s\n", errno, strerror(errno));
      return 1;
    }
    fd = -1;
  }
#elif defined(_WIN32)
  if (winMmapHandle) {
    if (!::UnmapViewOfFile(winMmapHandle)) {
      auto e = ::GetLastError();
      logE("~MMapFile: UnmapViewOfFile failed: %u\n", e);
      return 1;
    }
    winMmapHandle = nullptr;
    map = nullptr;
  }
  if (winFileHandle) {
    if (!::CloseHandle(winFileHandle)) {
      auto e = ::GetLastError();
      logE("~MMapFile: CloseHandle failed: %u\n", e);
      return 1;
    }
    winFileHandle = nullptr;
  }
#else
#error unsupported platform for munmap
#endif
  return 0;
}

int MMapFile::mmapRead(const char* filename, int64_t offset /*= 0*/,
                       int64_t len_ /*= 0*/) {
#if defined(__GLIBC__) || defined(__APPLE__) || defined(__ANDROID__)
  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    logE("%sload: open(%s) failed: %d %s\n", "ShaderLibrary::", filename, errno,
         strerror(errno));
    return 1;
  }
  struct stat s;
  if (fstat(fd, &s) == -1) {
    logE("%sload: fstat(%s) failed: %d %s\n", "ShaderLibrary::", filename,
         errno, strerror(errno));
    return 1;
  }
  auto ps = sysconf(_SC_PAGE_SIZE) - 1;
  int64_t afterLastByte = offset + len_;
  offset &= ps;
  if (!len_) {
    len_ = s.st_size;
  } else {
    len_ = ((afterLastByte + ps) & ps) - offset;
  }
  if (offset + len_ > s.st_size) {
    len_ = s.st_size - offset;
  }
  len = len_;
  map = ::mmap(0, len, PROT_READ, MAP_SHARED, fd, offset);
  if (!map) {
    logE("MMapFile: mmap(%s) failed: %d %s\n", filename, errno,
         strerror(errno));
    close(fd);
    fd = -1;
    return 1;
  }
#elif defined(_WIN32)
  SYSTEM_INFO SystemInfo;
  ::GetSystemInfo(&SystemInfo);
  auto ps = SystemInfo.dwAllocationGranularity;
  winFileHandle =
      ::CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                   0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (winFileHandle == INVALID_HANDLE_VALUE) {
    auto e = ::GetLastError();
    logE("MMapFile: CreateFile failed: %u\n", e);
    winFileHandle = nullptr;
    return 1;
  }
  DWORD sizeH;
  DWORD sizeL = ::GetFileSize(winFileHandle, &sizeH);
  int64_t st_size = (int64_t(sizeH) << 32) | sizeL;
  int64_t afterLastByte = offset + len_;
  offset &= ps;
  if (!len_) {
    len_ = st_size;
  } else {
    len_ = ((afterLastByte + ps) & ps) - offset;
  }
  if (offset + len_ > st_size) {
    len_ = st_size - offset;
  }
  len = len_;
  afterLastByte = offset + len_;
  winMmapHandle =
      ::CreateFileMapping(winFileHandle, 0, PAGE_READONLY, afterLastByte >> 32,
                          afterLastByte & 0xFFFFFFFF, 0);
  if (winMmapHandle == INVALID_HANDLE_VALUE) {
    auto e = ::GetLastError();
    logE("MMapFile: CreateFileMapping failed: %u\n", e);
    winMmapHandle = nullptr;
    ::CloseHandle(winFileHandle);
    winFileHandle = nullptr;
    return 1;
  }
  map = ::MapViewOfFile(winMmapHandle, FILE_MAP_READ, offset >> 32,
                        offset & 0xFFFFFFFF, len_);
  if (!map) {
    auto e = ::GetLastError();
    logE("MMapFile: MapViewOfFile failed: %u\n", e);
    ::CloseHandle(winMmapHandle);
    winMmapHandle = nullptr;
    ::CloseHandle(winFileHandle);
    winFileHandle = nullptr;
    return 1;
  }
#else
#error unsupported platform for mmap
#endif
  return 0;
}
