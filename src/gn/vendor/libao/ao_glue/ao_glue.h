/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * ao_glue is a helper class for using libao, and implements an Android version
 * that does not use libao at all.
 */

#ifdef __ANDROID__
// To use ao_glue, these symbols must be defined.
// NOTE: This is a small subset of the libao API, only to enable the use of
// ao_glue transparently even on Android. If you are here because of a
// compiler error, first try putting that code behind a #ifndef __ANDROID__
// and see if that resolves the problem.

enum {
  AO_TYPE_this_value_is_invalid = 0,
  AO_TYPE_LIVE,
  AO_TYPE_FILE,
};

enum {
  AO_FMT_this_value_is_invalid = 0,
  AO_FMT_LITTLE,
  AO_FMT_BIG,
  AO_FMT_NATIVE,
};

typedef struct ao_device ao_device;

typedef struct ao_info {
  int type;                  /* will always be AO_TYPE_LIVE. */
  const char* name;          /* will be "android-AAudio" */
  const char* short_name;    /* will be "android-AAudio" */
  const char* author;        /* will be "android-osp" */
  const char* comment;       /* will be "" (empty string) */
  int preferred_byte_format; /* your app must use this format */
  int priority;              /* will be 0 */
  const char** options;      /* ignored for Android */
  int option_count;          /* ignored for Android */
} ao_info;

typedef struct ao_sample_format {
  int bits;        /* bits per sample */
  int rate;        /* samples per second (in a single channel) */
  int channels;    /* number of audio channels */
  int byte_format; /* must exactly match preferred_byte_format. */
  char* matrix;    /* ignored for Android */
} ao_sample_format;

#else /*__ANDROID__*/
#include <ao/ao.h>
#endif /*__ANDROID__*/

#include <map>
#include <memory>
#include <string>
#include <vector>

#pragma once

class ao_dev;

class ao_glue {
 public:
  ao_glue() {}
  ~ao_glue();

  // ctorError initializes libao.
  int ctorError();

  // live() returns a vector of ao_info about ao drivers that can output
  // sound without a filename.
  const std::vector<const ao_info*>& live() { return live_driver; }

  // file() returns a vector of ao_info about ao drivers that can output
  // sound without needing hardware, but need a filename.
  const std::vector<const ao_info*>& file() { return file_driver; }

  // open() creates an ao_dev using driver.
  // If it is a live driver filename *must* be the empty string.
  // If it is a file driver, filename *must* *not* be the empty string; the
  // file is immediately created. If the file already exists it is truncated.
  //
  // A map of key-value pairs can also be pasesd to the driver.
  std::shared_ptr<ao_dev> open(const ao_info* driver, ao_sample_format& format,
                               std::string filename = "",
                               const std::map<std::string, std::string>& opts =
                                   std::map<std::string, std::string>());

 protected:
  bool isInitialized{false};
  std::vector<const ao_info*> live_driver;
  std::vector<const ao_info*> file_driver;
  const ao_info* null_driver{nullptr};
  // drivers is a pointer to a libao structure. libao retains ownership.
  ao_info** drivers{nullptr};
};

class ao_dev {
 public:
  ao_dev(ao_device* dev) : dev(dev) {}
  ~ao_dev();

  int play(char* data, uint32_t len);

 protected:
  ao_device* const dev;
};
