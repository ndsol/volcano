/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include "VkPtr.h"

void logV(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('V', fmt, ap);
  va_end(ap);
}
void logD(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('D', fmt, ap);
  va_end(ap);
}
void logI(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('I', fmt, ap);
  va_end(ap);
}
void logW(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('W', fmt, ap);
  va_end(ap);
}
void logE(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('E', fmt, ap);
  va_end(ap);
}
void logF(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('F', fmt, ap);
  va_end(ap);
}

#ifdef __ANDROID__
#include <android/log.h>
static const char logTag[] = "volcano";
static void logVolcanoImpl(char level, const char* fmt, va_list ap) {
  android_LogPriority prio = ANDROID_LOG_UNKNOWN;
  switch (level) {
    case 'V':
      prio = ANDROID_LOG_VERBOSE;
      break;
    case 'D':
      prio = ANDROID_LOG_DEBUG;
      break;
    case 'I':
      prio = ANDROID_LOG_INFO;
      break;
    case 'W':
      prio = ANDROID_LOG_WARN;
      break;
    case 'E':
      prio = ANDROID_LOG_ERROR;
      break;
    case 'F':
      prio = ANDROID_LOG_FATAL;
      break;
    default:
      break;
  }
  __android_log_vprint(prio, logTag, fmt, ap);
  if (prio == ANDROID_LOG_FATAL) {
    __android_log_assert("call to logF()", logTag, "printing backtrace:");
  }
}

#elif defined(_WIN32) /* !defined(__ANDROID__) */
#include <time.h>
static FILE* errorLog = nullptr;
static void logVolcanoImpl(char level, const char* fmt, va_list ap) {
  if (!errorLog) {
    errorLog = fopen("volcano.log", "a");
  }
  time_t raw;
  time(&raw);
  struct tm tm_results;
  struct tm* t = &tm_results;
#ifdef _WIN32
  if (localtime_s(&tm_results, &raw)) {
    t = 0;
  }
#else
  // Yes, this function is only for _WIN32 already. Here is the posix line FYI:
  t = localtime_r(&raw, &tm_result);
#endif
  char timestamp[512];
  if (t) {
    strftime(timestamp, sizeof(timestamp), "%Y.%m.%d %H:%M:%S", t);
  } else {
    snprintf(timestamp, sizeof(timestamp), "?%llu?", (unsigned long long)raw);
  }
  if (errorLog) {
    fputs(timestamp, errorLog);
  }
  OutputDebugString(timestamp);
  snprintf(timestamp, sizeof(timestamp), " %c ", level);
  OutputDebugString(timestamp);
  if (errorLog) {
    fprintf(errorLog, " %c ", level);
  }
  vsnprintf(timestamp, sizeof(timestamp), fmt, ap);
  if (errorLog) {
    fputs(timestamp, errorLog);
    if (strlen(timestamp) == sizeof(timestamp) - 1) {
      fputs("\n", errorLog);
    }
    fflush(errorLog);
  }
  OutputDebugString(timestamp);

  if (level == 'F') {
    exit(1);
  }
}

#else /* !defined(_WIN32) && !defined(__ANDROID__) */

static void logVolcanoImpl(char level, const char* fmt, va_list ap)
    __attribute__((format(printf, 2, 0)));
static void logVolcanoImpl(char level, const char* fmt, va_list ap) {
  fprintf(stderr, "%c ", level);
  vfprintf(stderr, fmt, ap);
  if (level == 'F') {
    exit(1);
  }
}

#endif

int explainVkResult(const char* what, VkResult why) {
  switch (why) {
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      logE("%s failed: %d (%s)\n", what, why, string_VkResult(why));
      logE("Most likely cause: your GPU does not support Vulkan yet.\n");
      logE("You may try updating your graphics driver.\n");
      break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      logE("%s failed: %d (%s)\n", what, why, string_VkResult(why));
      if (!strcmp(what, "vkCreateInstance")) {
        logE(
            "Primary cause: you *might* be out of memory (unlikely).\n"
            "Secondary causes: conflicting vulkan drivers installed.\n"
            "Secondary causes: broken driver installation.\n"
            "You may want to search the web for more information.\n");
      }
      break;
    case VK_ERROR_INITIALIZATION_FAILED:
      logE("%s failed: %d (%s)\n", what, why, string_VkResult(why));
#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(__APPLE__)
      if (!strcmp(what, "vkEnumeratePhysicalDevices")) {
        logE("Hint: check you are in the 'video' group and have read/write\n");
        logE("      permission to the GPU in /dev.\n");
      }
#endif
      break;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      // The caller may have already dumped the specific format.
      logE("%s failed: %d (%s)\n", what, why, string_VkResult(why));
      logE("Check for your device on https://vulkan.gpuinfo.org\n");
      break;
    default:
      logE("%s failed: %d (%s)\n", what, why, string_VkResult(why));
  }
  return 1;
}

void (*logVolcano)(char level, const char* fmt, va_list ap) = logVolcanoImpl;
