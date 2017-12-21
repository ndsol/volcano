/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 * Glue for loading an image using skia.
 */

#include "skiaglue.h"
#include <src/command/command.h>
#include <src/memory/memory.h>
#include <src/science/science.h>
#include <vulkan/vk_format_utils.h>

#include <gli/gli.hpp>
#include "SkCanvas.h"
#include "SkCodec.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkImageInfo.h"
#include "SkPngEncoder.h"
#include "SkRefCnt.h"

// getPixels is a helper to call SkCodec::getPixels() and log the error result.
static int getPixels(SkCodec& codec, void* mappedMem, uint32_t rowStride,
                     std::string& imgFilenameFound) {
  char msgbuf[256];
  const char* msg;
  auto r = codec.getPixels(
      SkImageInfo::Make(codec.getInfo().width(), codec.getInfo().height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      mappedMem, rowStride);
  switch (r) {
    case SkCodec::kSuccess:
      return 0;
    case SkCodec::kIncompleteInput:
      logW("skia codec: %s \"%s\"\n", "incomplete image",
           imgFilenameFound.c_str());
      return 0;
    case SkCodec::kErrorInInput:
      logW("skia codec: %s \"%s\"\n", "errors in image",
           imgFilenameFound.c_str());
      return 0;
    case SkCodec::kInvalidConversion:
      msg = "unable to output in this pixel format";
      break;
    case SkCodec::kInvalidScale:
      msg = "unable to rescale the image to this size";
      break;
    case SkCodec::kInvalidParameters:
      msg = "invalid parameters or memory to write to";
      break;
    case SkCodec::kInvalidInput:
      msg = "invalid input";
      break;
    case SkCodec::kCouldNotRewind:
      msg = "could not rewind";
      break;
    case SkCodec::kInternalError:
      msg = "internal error (out of memory?)";
      break;
    case SkCodec::kUnimplemented:
      msg = "TODO: this method is not implemented";
      break;
    default:
      snprintf(msgbuf, sizeof(msgbuf), "SkCodec::Result(%d) ???", int(r));
      msg = msgbuf;
      break;
  }
  logE("skia codec: %s \"%s\"\n", msg, imgFilenameFound.c_str());
  return 1;
}

// loadImage loads imgFilename as a VK_FORMAT_R8G8B8A8_UNORM
// image in stage (a Buffer). copy1 is set up for a later copy from stage to
// an Image of your choosing. info is set up to create your Image.
int skiaglue::loadImage(const char* imgFilename, memory::Buffer& stage) {
  // findInPaths is defined in command.h.
  if (findInPaths(imgFilename, imgFilenameFound)) {
    return 1;
  }

  sk_sp<SkData> data = SkData::MakeFromFileName(imgFilenameFound.c_str());
  if (!data) {
    logE("unable to read \"%s\"\n", imgFilenameFound.c_str());
    return 1;
  }
  // skia cannot read DDS format. Use gli if DDS magic is found.
  // TODO: add KTX support when vulkansamples git repo gets an updated glm.
  // gli is pinned to 0.5.1.1 until vulkansamples updates its glm.
  if (data->size() >= 4 && !memcmp(data->data(), "DDS ", 4)) {
    // TODO: a newer gli adds the ability to read from a buffer.
    gli::texture2D tex(gli::load_dds(imgFilenameFound.c_str()));
    if (tex.format() != gli::RGBA8_UNORM) {
      logE("invalid dds: wrong format\n");
      return 1;
    }
    info.mipLevels = tex.levels();
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent.width = tex.dimensions().x;
    info.extent.height = tex.dimensions().y;
    info.extent.depth = 1;
    uint32_t formatBytes = FormatSize(info.format);

    copies.resize(tex.levels());
    size_t readOfs = 0;
    for (size_t mip = 0; mip < tex.levels(); mip++) {
      VkBufferImageCopy& copy = copies.at(mip);
      memset(&copy, 0, sizeof(copy));
      copy.bufferOffset = readOfs;
      copy.bufferRowLength = info.extent.width >> mip;
      copy.bufferImageHeight = info.extent.height >> mip;
      readOfs += formatBytes * copy.bufferRowLength * copy.bufferImageHeight;
      science::Subres(copy.imageSubresource).addColor().setMipLevel(mip);
      copy.imageOffset = {0, 0, 0};
      copy.imageExtent = {copy.bufferRowLength, copy.bufferImageHeight, 1};
    }

    stage.info.size = tex.size();
    if (stage.ctorHostCoherent(cpool.dev) || stage.bindMemory(cpool.dev)) {
      logE("stage.ctorHostCoherent or bindMemory failed\n");
      return 1;
    }
    void* mappedMem;
    if (stage.mem.mmap(cpool.dev, &mappedMem)) {
      logE("stage.mmap() failed\n");
      return 1;
    }
    memcpy(mappedMem, tex.data<char>(), tex.size());
    stage.mem.munmap(cpool.dev);
    return 0;
  }

  // Found a format skia should be able to decode. Proceed with skia.
  std::unique_ptr<SkCodec> codec(SkCodec::MakeFromData(data));
  if (!codec) {
    logE("unable to create a codec for \"%s\"\n", imgFilenameFound.c_str());
    return 1;
  }
  auto& dim = codec->getInfo();
  info.extent.width = dim.width();
  info.extent.height = dim.height();
  info.extent.depth = 1;
  info.format = VK_FORMAT_R8G8B8A8_UNORM;

  copies.resize(1);
  VkBufferImageCopy& copy = copies.at(0);
  memset(&copy, 0, sizeof(copy));
  copy.bufferRowLength = dim.width();
  copy.bufferImageHeight = dim.height();
  copy.imageExtent = {uint32_t(dim.width()), uint32_t(dim.height()), 1};
  science::Subres(copy.imageSubresource).addColor();

  uint32_t rowStride = FormatSize(info.format) * copy.bufferRowLength;
  stage.info.size = rowStride * dim.height();
  if (stage.ctorHostCoherent(cpool.dev) || stage.bindMemory(cpool.dev)) {
    logE("stage.ctorHostCoherent or bindMemory failed\n");
    return 1;
  }

  void* mappedMem;
  if (stage.mem.mmap(cpool.dev, &mappedMem)) {
    logE("stage.mem.mmap() failed\n");
    return 1;
  }
  int r = getPixels(*codec, mappedMem, rowStride, imgFilenameFound);
  stage.mem.munmap(cpool.dev);
  return r;
}

static int writePNGfromMappedMem(memory::Image& image, VkDeviceSize rowPitch,
                                 const char* outFilename, void* mappedMem) {
  SkPixmap src(
      SkImageInfo::Make(image.info.extent.width, image.info.extent.height,
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      mappedMem, rowPitch);

  SkFILEWStream skFile(outFilename);
  if (!skFile.isValid()) {
    // Skia swallows the error message, though in DEBUG builds it is printed.
    logE("SkFILEWStream failed\n");
    return 1;
  }

  if (!SkPngEncoder::Encode(&skFile, src, SkPngEncoder::Options{})) {
    // Skia swallows the error message, though in DEBUG builds it is printed.
    logE("SkPngEncoder failed\n");
    return 1;
  }
  return 0;
}

int skiaglue::writePNG(memory::Image& image, std::string outFilename) {
  if (image.info.tiling != VK_IMAGE_TILING_LINEAR) {
    logE("writePNG: image must be VK_IMAGE_TILING_LINEAR\n");
    return 1;
  }
  if (image.colorMem.size() < 1) {
    // This should always be present if VK_IMAGE_TILING_LINEAR.
    logE("writePNG: image does not have colorMem layout\n");
    return 1;
  }
  char* mappedMem;
  if (image.mem.mmap(cpool.dev, (void**)&mappedMem)) {
    logE("mmap() failed\n");
    return 1;
  }

  VkSubresourceLayout& layout = image.colorMem.at(0);
  int r = writePNGfromMappedMem(image, layout.rowPitch, outFilename.c_str(),
                                mappedMem + layout.offset);
  image.mem.munmap(cpool.dev);
  return r;
}

static int writeDDSfromMappedMem(memory::Image& image, const void* mappedMem,
                                 VkDeviceSize bytes, const char* outFilename) {
  gli::format format = gli::RGBA8_UNORM;
  if (image.info.format != VK_FORMAT_R8G8B8A8_UNORM) {
    logE("when gli is updated this will be easier. gli format mismatch\n");
    return 1;
  }
  glm::uvec2 extent(image.info.extent.width, image.info.extent.height);
  gli::texture2D tex(image.info.mipLevels, format, extent);
  memcpy(tex.data<char>(), mappedMem, bytes);
  // yolo? gli 0.5.1.1 does not return an error from save_dds.
  gli::save_dds(tex, outFilename);
  return 0;
}

int skiaglue::writeDDS(memory::Image& image, std::string outFilename) {
  memory::Buffer host(cpool.dev);

  // Build a vector of VkBufferImageCopy to copy image, a mip level at a time.
  VkDeviceSize offset = 0;
  std::vector<VkBufferImageCopy> copies(image.info.mipLevels);
  for (size_t mip = 0; mip < image.info.mipLevels; mip++) {
    VkBufferImageCopy& c = copies.at(mip);
    memset(&c, 0, sizeof(c));
    c.bufferOffset = offset;
    c.bufferRowLength = image.info.extent.width >> mip;
    c.bufferImageHeight = image.info.extent.height >> mip;
    c.imageExtent = {image.info.extent.width >> mip,
                     image.info.extent.height >> mip, 1};
    science::Subres(c.imageSubresource).addColor().setMipLevel(mip);
    offset +=
        FormatSize(image.info.format) * c.bufferRowLength * c.bufferImageHeight;
  }

  // offset now contains the total number of bytes.
  host.info.size = offset;
  if (host.ctorHostCoherent(cpool.dev) || host.bindMemory(cpool.dev)) {
    logE("host.ctorHostCoherent or bindMemory failed\n");
    return 1;
  }
  {
    science::SmartCommandBuffer buffer{cpool, memory::ASSUME_POOL_QINDEX};
    if (buffer.ctorError() || buffer.autoSubmit() ||
        buffer.transition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) ||
        buffer.copyImageToBuffer(image.vk, image.currentLayout, host.vk,
                                 copies)) {
      logE("buffer failed\n");
      return 1;
    }
  }

  char* mappedMem;
  if (host.mem.mmap(cpool.dev, (void**)&mappedMem)) {
    logE("host.mmap() failed\n");
    return 1;
  }
  int r = writeDDSfromMappedMem(image, mappedMem, host.info.size,
                                outFilename.c_str());
  host.mem.munmap(cpool.dev);
  return r;
}
