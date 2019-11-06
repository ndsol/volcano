/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This emulates libao on Android.
 * NOTE: Desktop platforms use ao_glue.cpp, as specified in ../BUILD.gn.
 */

#include <aaudio/AAudio.h>
#include <src/core/VkPtr.h>

#include "ao_glue.h"

#ifndef __ANDROID__
#error This file is an Android-only file.
#endif /*__ANDROID__*/

static void errorCbWrapper(AAudioStream* stream, void* self,
                           aaudio_result_t error);

typedef struct ao_device {
  ao_device() {}
  ~ao_device() {
    if (stream) {
      if (started) {
        AAudioStream_requestStop(stream);
        started = false;
      }
      AAudioStream_close(stream);
      stream = nullptr;
    }
  }

  int ctorError(ao_sample_format& fmt) {
    memcpy(&format, &fmt, sizeof(format));

    AAudioStreamBuilder* builder{nullptr};
    auto r = AAudio_createStreamBuilder(&builder);
    if (r != AAUDIO_OK) {
      logE("AAudio_createStreamBuilder failed: %d %s\n", (int)r,
           AAudio_convertResultToText(r));
      return 1;
    }

    AAudioStreamBuilder_setSamplesPerFrame(builder, 2);
    // FIXME: AAudioStreamBuilder_setBufferCapacityInFrames(builder, numFrames);
    AAudioStreamBuilder_setSampleRate(builder, format.rate);
    AAudioStreamBuilder_setChannelCount(builder, format.channels);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
#if ANDROID_NDK_MAJOR >= 28
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
#endif /*ANDROID_NDK_MAJOR >= 28*/

    AAudioStreamBuilder_setErrorCallback(builder, errorCbWrapper, this);
    r = AAudioStreamBuilder_openStream(builder, &stream);
    // Clean up builder before reacting to openStream result r.
    AAudioStreamBuilder_delete(builder);
    if (r != AAUDIO_OK) {
      logE("AAudioStreamBuilder_openStream failed: %d %s\n", (int)r,
           AAudio_convertResultToText(r));
      return 1;
    }
    return 0;
  }

  AAudioStream* stream{nullptr};
  bool starting{false};
  bool started{false};
  ao_sample_format format;

  void errorCb(AAudioStream* stream, aaudio_result_t error) {
    if (stream == this->stream) {
      logE("errorCb: %d %s\n", (int)error, AAudio_convertResultToText(error));
    } else {
      logE("errorCb(got %p want %p): %d %s\n", stream, this->stream, (int)error,
           AAudio_convertResultToText(error));
    }
  }
} ao_device;

ao_dev::~ao_dev() {
  ao_device* d = const_cast<ao_device*>(dev);
  delete d;
}

int ao_dev::play(char* rawData, uint32_t len) {
  int16_t* data = reinterpret_cast<int16_t*>(rawData);
  int32_t numFrames = len / 2 / sizeof(data[0]);
  auto r = AAudioStream_write(dev->stream, data, numFrames, (int64_t)10000ll);
  if (r < 0) {
    logW("AAudioStream_write: %d %s\n", (int)r, AAudio_convertResultToText(r));
    return 0;
  }
  // FIXME: r may be less than numFrames.
  // FIXME: tune buffer capacity / audio latency.

  auto curState = AAudioStream_getState(dev->stream);
  if (curState == AAUDIO_STREAM_STATE_STARTED) {
    dev->started = true;
  } else if (!dev->starting) {
    r = AAudioStream_requestStart(dev->stream);
    if (r != AAUDIO_OK) {
      logW("AAudioStream_requestStart: %d %s\n", (int)r,
           AAudio_convertResultToText(r));
      return 0;
    }
    dev->starting = true;
  } else if (curState != AAUDIO_STREAM_STATE_STARTING) {
    dev->starting = false;
    dev->started = false;
    switch (curState) {
      case AAUDIO_STREAM_STATE_OPEN:
      case AAUDIO_STREAM_STATE_CLOSING:
      case AAUDIO_STREAM_STATE_CLOSED:
      case AAUDIO_STREAM_STATE_DISCONNECTED:
        break;
      default:
        logW("AAudio state %d %s is not in CLOSED,DISCONNECTED,OPEN\n",
             (int)curState, AAudio_convertStreamStateToText(curState));
        return 1;
    }
    // Close and reopen the stream.
    AAudioStream_close(dev->stream);
    dev->stream = nullptr;
    ao_sample_format fmt;
    memcpy(&fmt, &dev->format, sizeof(fmt));
    if (dev->ctorError(fmt)) {
      logE("AAudio state %d %s, unable to ctorError\n", (int)curState,
           AAudio_convertStreamStateToText(curState));
      return 1;
    }
  }
  return 0;
}

ao_glue::~ao_glue() {
  if (drivers) {
    if (drivers[0]) {
      delete drivers[0];
    }
    delete[] drivers;
  }
}

int ao_glue::ctorError() {
  if (isInitialized) {
    return 0;
  }
  drivers = new ao_info*[1];
  drivers[0] = new ao_info;
  memset(drivers[0], 0, sizeof(*drivers[0]));
  drivers[0]->type = AO_TYPE_LIVE;
  drivers[0]->name = "android-AAudio";
  drivers[0]->short_name = "android-AAudio";
  drivers[0]->author = "android-osp";
  drivers[0]->comment = "";
  drivers[0]->preferred_byte_format = AO_FMT_NATIVE;
  live_driver.emplace_back(drivers[0]);
  isInitialized = true;
  return 0;
}

static void errorCbWrapper(AAudioStream* stream, void* self,
                           aaudio_result_t error) {
  reinterpret_cast<ao_device*>(self)->errorCb(stream, error);
}

std::shared_ptr<ao_dev> ao_glue::open(
    const ao_info* driver, ao_sample_format& format, std::string filename,
    const std::map<std::string, std::string>& opts) {
  if (!isInitialized) {
    logE("ao_glue::open: must call ctorError first\n");
    return std::shared_ptr<ao_dev>();
  }
  if (driver != drivers[0]) {
    logE("ao_glue::open: invalid driver %p (want %p)\n", driver, drivers[0]);
    return std::shared_ptr<ao_dev>();
  }
  if (format.byte_format != drivers[0]->preferred_byte_format) {
    logE("ao_glue::open: byte_format %d is not supported\n",
         format.byte_format);
    return std::shared_ptr<ao_dev>();
  }

  ao_device* dev = new ao_device;
  if (dev->ctorError(format)) {
    delete dev;
    return std::shared_ptr<ao_dev>();
  }
  return std::make_shared<ao_dev>(dev);
}
