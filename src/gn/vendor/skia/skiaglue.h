/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * skiaglue contains helper functions for calling skia. Instead of defining
 * these functions in a relevant class like memory::Buffer, they are in a
 * separate struct so that skia is an optional dependency instead of required.
 */

#include <src/memory/memory.h>

#ifdef __ANDROID__
struct android_app;
#endif

// skiaglue reads or writes images using skia.
typedef struct skiaglue {
  // loadImage reads the bytes in imgFilename into memory::Stage stage.
  // loadImage calls stage.mmap() to get a memory::Flight. loadImage does *NOT*
  // call stage.flush().
  //
  // loadImage sets up flight->copies for your app. Your app is expected
  // to do a stage.flush(flight) to execute the buffer-to-image copy.
  // Your app may customize the flight->copies or the entire flight.
  //
  // loadImage populates img.info such that a call to img.ctorError immediately
  // after loadImage returns would succeed. loadImage sets the following fields:
  //
  // * img.info.extent = the file's native size
  //
  // * img.info.format = VK_FORMAT_R8G8B8A8_UNORM for image files.
  // (Skia 'SkColorType' only supports a few formats natively, all basically
  // variants of R8G8B8A8_UNORM, so this keeps things simple.)
  //
  // * img.info.format = copied from texture for DDS, KTX files. These type of
  // file are not supported by skia, but this function silently uses another
  // library (github.com/g-truc/gli) to read it. It is assumed you are not
  // reading untrusted assets, specifically the texture format is used as-is
  // without checking if the device supports it.
  //
  // NOTE: On Android, android_app_ *must* be initialized before calling
  //       loadImage or any other methods. loadImage also uses findInPaths()
  //       defined in core/structs.h to search multiple paths for the file.
  //       The actual path used is stored in imgFilenameFound.
  int loadImage(const char* imgFilename, memory::Stage& stage,
                std::shared_ptr<memory::Flight>& flight, memory::Image& img);

  // writeToFile does not use imgFilenameFound. It just writes image at the
  // requested outFilename.
  //
  // If outFilename ends in ".dds" or ".ktx" this uses GLI to save the image as
  // a texture. Otherwise, this attempts to encode the image using skia, which
  // supports encoding to a few formats (png, jpg, webp).
  //
  // If copies is empty (the default) this uses the entire image.
  int writeToFile(memory::Image& image, memory::Stage& stage,
                  std::string outFilename,
                  const std::vector<VkBufferImageCopy>& copies =
                      std::vector<VkBufferImageCopy>());

  // imgFilenameFound is populated by loadImage().
  std::string imgFilenameFound;

#ifdef __ANDROID__
  // android_app_ must be set before calling any methods.
  android_app* android_app_{nullptr};
#endif
} skiaglue;
