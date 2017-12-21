/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * skiaglue contains helper functions for calling skia. Instead of defining
 * these functions in a relevant class like memory::Buffer, they are in a
 * separate struct so that skia is an optional dependency instead of required.
 */

#include <src/memory/memory.h>

typedef struct skiaglue {
  skiaglue(command::CommandPool& cpool, VkImageCreateInfo& info)
      : info(info), cpool(cpool) {}

  // loadImage loads imgFilename as a VK_FORMAT_R8G8B8A8_UNORM
  // image in stage (a Buffer). copy1 is set up for a later copy from stage to
  // an Image of your choosing. info is set up to create your Image.
  int loadImage(const char* imgFilename, memory::Buffer& stage);

  // writePNG does not use imgFilenameFound, copy1, or info. It just writes
  // a PNG-encoded image at the requested outFilename.
  int writePNG(memory::Image& image, std::string outFilename);

  // writeDDS does not use imgFilenameFound, copy1, or info. It just writes
  // a DDS-encoded texture at the requested outFilename.
  int writeDDS(memory::Image& image, std::string outFilename);

  // imgFilenameFound is populated by loadImage().
  std::string imgFilenameFound;
  // copy1 is populated by loadImage().
  std::vector<VkBufferImageCopy> copies;
  // info is populated by loadImage().
  VkImageCreateInfo& info;
  // cpool is the CommandPool which holds Buffer stage and can transition it.
  command::CommandPool& cpool;
} skiaglue;
