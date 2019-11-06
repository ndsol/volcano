/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This is the libao implementation.
 * NOTE: Android uses ao_glue_android.cpp, as specified in ../BUILD.gn.
 */

#include "ao_glue.h"

#include <ao/ao.h>
#include <src/core/structs.h>

#ifdef __linux__
#ifdef __ANDROID__
#error Android should not build this file.
#endif /*__ANDROID__*/

#include <linux/limits.h>  // for PATH_MAX
#endif                     /*__linux__*/

#ifdef _WIN32
#include <direct.h>  // for _getcwd, _chdir
#include <stdlib.h>  // for _fullpath
inline char* getcwd(char* buffer, int len) { return _getcwd(buffer, len); }
inline int chdir(const char* dir) { return _chdir(dir); }

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif /*PATH_MAX*/
#else /*_WIN32*/
#include <unistd.h>
#endif /*_WIN32*/

ao_dev::~ao_dev() {
  if (dev) {
    ao_close(dev);
  }
}

int ao_dev::play(char* data, uint32_t len) { return !ao_play(dev, data, len); }

ao_glue::~ao_glue() {
  if (isInitialized) {
    ao_shutdown();
  }
}

int ao_glue::ctorError() {
  if (isInitialized) {
    return 0;
  }

  std::string found = getSelfPath();
  auto lastSep = found.rfind(OS_SEPARATOR);
  if (lastSep != std::string::npos) {
    found.erase(lastSep + 1);
  }

  std::vector<char> abspath(PATH_MAX * 2);
#ifdef _WIN32
  if (!_fullpath(abspath.data(), found.c_str(), abspath.size())) {
    logE("_fullpath(%s) failed: %d %s\n", found.c_str(), errno,
         strerror(errno));
    return 1;
  }
#else /*_WIN32*/
  if (!realpath(found.c_str(), abspath.data())) {
    logE("realpath(%s) failed: %d %s\n", found.c_str(), errno, strerror(errno));
    return 1;
  }
  found = abspath.data();
#endif /*_WIN32*/

  // Save getcwd in abspath.
  if (!getcwd(abspath.data(), abspath.size())) {
    logE("getcwd() failed: %d %s\n", errno, strerror(errno));
    return 1;
  }
  if (chdir(found.c_str())) {
    logE("chdir(%s) failed: %d %s\n", found.c_str(), errno, strerror(errno));
    return 1;
  }
  ao_initialize();

  // Restore to previous getcwd.
  if (chdir(abspath.data())) {
    logE("chdir(%s) failed: %d %s\n", abspath.data(), errno, strerror(errno));
    return 1;
  }
  int count = 0;
  drivers = ao_driver_info_list(&count);
  if (!drivers) {
    ao_shutdown();
    return 0;
  }

  for (int i = 0; i < count; i++) {
    switch (drivers[i]->type) {
      case AO_TYPE_LIVE:
        if (!strcmp(drivers[i]->short_name, "null")) {
          null_driver = drivers[i];
          break;
        }
        live_driver.emplace_back(drivers[i]);
        break;
      case AO_TYPE_FILE:
        file_driver.emplace_back(drivers[i]);
        break;
    }
  }
  isInitialized = true;
  return 0;
}

std::shared_ptr<ao_dev> ao_glue::open(
    const ao_info* driver, ao_sample_format& format, std::string filename,
    const std::map<std::string, std::string>& opts) {
  if (!isInitialized) {
    logE("ao_glue::open: must call ctorError first\n");
    return std::shared_ptr<ao_dev>();
  }
  if (!driver) {
    logE("ao_glue::open: NULL driver\n");
    return std::shared_ptr<ao_dev>();
  }
  int driverID = ao_driver_id(driver->short_name);
  if (driverID < 0) {
    logE("ao_glue::open: invalid driver %p\n", driver);
    return std::shared_ptr<ao_dev>();
  }
  if (filename.empty() && driver->type == AO_TYPE_FILE) {
    logE("ao_glue::open(%s): filename must not be empty\n", driver->name);
    return std::shared_ptr<ao_dev>();
  }
  if (!filename.empty() && driver->type == AO_TYPE_LIVE) {
    logE("ao_glue::open(%s): filename must be empty\n", driver->name);
    return std::shared_ptr<ao_dev>();
  }

  ao_option* option = NULL;
  for (auto i = opts.begin(); i != opts.end(); i++) {
    if (!ao_append_option(&option, i->first.c_str(), i->second.c_str())) {
      logE("ao_glue::open(%s): ao_append_option(%s) failed\n", driver->name,
           i->first.c_str());
      return std::shared_ptr<ao_dev>();
    }
  }
  ao_device* aodev;
  switch (driver->type) {
    case AO_TYPE_LIVE:
      aodev = ao_open_live(driverID, &format, option);
      break;
    case AO_TYPE_FILE:
      aodev = ao_open_file(driverID, filename.c_str(), 1, &format, option);
      break;
    default:
      logE("ao_glue::open(%s): unsupported type=%d\n", driver->name,
           driver->type);
      return std::shared_ptr<ao_dev>();
  }
  ao_free_options(option);
  if (!aodev) {
    logE("ao_glue::open(%s): ao_open failed\n", driver->name);
    return std::shared_ptr<ao_dev>();
  }
  return std::make_shared<ao_dev>(aodev);
}
