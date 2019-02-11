/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * Glue for loading an image using skia.
 */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "skiaglue.h"

#include <src/command/command.h>
#include <src/memory/memory.h>
#include <src/science/science.h>
#include <vulkan/vk_format_utils.h>

#include <algorithm>
#include <gli/gli.hpp>

#include "SkCanvas.h"
#include "SkCodec.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkImageInfo.h"
#include "SkJpegEncoder.h"
#include "SkPngEncoder.h"
#include "SkRefCnt.h"
#include "SkWebpEncoder.h"

// getPixels is a helper to call SkCodec::getPixels() and log the error result.
static int getPixels(SkCodec& codec, void* mappedMem, uint32_t rowStride,
                     const char* imgFilenameFound, SkColorType format) {
  char msgbuf[256];
  const char* msg;
  auto& info = codec.getInfo();
  auto r = codec.getPixels(
      SkImageInfo::Make(info.width(), info.height(), format, info.alphaType()),
      mappedMem, rowStride);
  switch (r) {
    case SkCodec::kSuccess:
      return 0;
    case SkCodec::kIncompleteInput:
      logW("skia codec: %s \"%s\"\n", "incomplete image", imgFilenameFound);
      return 0;
    case SkCodec::kErrorInInput:
      logW("skia codec: %s \"%s\"\n", "errors in image", imgFilenameFound);
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
  logE("skia codec: %s \"%s\"\n", msg, imgFilenameFound);
  return 1;
}

int loadGLIimage(const char* imgFilenameFound, sk_sp<SkData> data,
                 memory::Stage& stage, std::shared_ptr<memory::Flight>& flight,
                 memory::Image& img) {
  gli::texture2d tex(
      gli::load(reinterpret_cast<const char*>(data->data()), data->size()));
  if (tex.empty()) {
    logE("skiaglue::loadImage: unable to decode texture \"%s\"\n",
         imgFilenameFound);
    return 1;
  }

  if (tex.size() > stage.mmapMax()) {
    logE("skiaglue::loadImage: tex.size=%zu > stage.mmapMax()=%zu\n",
         (size_t)tex.size(), stage.mmapMax());
    logE("skiaglue::loadImage: try VolcanoSamples/src/scanlinedecoder.cpp\n");
    return 1;
  }
  if (stage.mmap(img, tex.size(), flight)) {
    logE("skiaglue::loadImage: tex.size=%zu stage.mmap failed\n",
         (size_t)tex.size());
    return 1;
  }

  img.info.mipLevels = tex.levels();
  // Note that gli uses the exact same format enum as Vulkan, so
  // image.info.format can be set directly from tex.format():
  img.info.format = static_cast<VkFormat>(tex.format());
  img.info.extent.width = tex.extent().x;
  img.info.extent.height = tex.extent().y;
  img.info.extent.depth = 1;
  uint32_t formatBytes = FormatSize(img.info.format);

  auto& copies = flight->copies;
  copies.resize(tex.levels());
  size_t readOfs = 0;
  for (size_t mip = 0; mip < tex.levels(); mip++) {
    VkBufferImageCopy& copy = copies.at(mip);
    memset(&copy, 0, sizeof(copy));
    copy.bufferOffset = readOfs;
    copy.bufferRowLength = std::max(1u, img.info.extent.width >> mip);
    copy.bufferImageHeight = std::max(1u, img.info.extent.height >> mip);
    readOfs += formatBytes * copy.bufferRowLength * copy.bufferImageHeight;
    copy.imageSubresource = img.getSubresourceLayers(mip);
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = img.info.extent;
  }

  memcpy(flight->mmap(), tex.data<char>(), tex.size());
  return 0;
}

int loadSkiaImage(const char* imgFilenameFound, sk_sp<SkData> data,
                  memory::Stage& stage, std::shared_ptr<memory::Flight>& flight,
                  memory::Image& img) {
  std::unique_ptr<SkCodec> codec(SkCodec::MakeFromData(data));
  if (!codec) {
    logE("skiaglue::loadImage: unable to create a codec for \"%s\"\n",
         imgFilenameFound);
    return 1;
  }
  auto& dim = codec->getInfo();
  img.info.extent.width = dim.width();
  img.info.extent.height = dim.height();
  img.info.extent.depth = 1;

  // Skia only natively supports a few formats, all just swizzles of RGBA8 or
  // gray scale (which upconverts fine to RGBA8). Keep things simple.
  SkColorType skfmt = kRGBA_8888_SkColorType;
  VkFormatFeatureFlags flags =
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT |
      VK_FORMAT_FEATURE_BLIT_DST_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  if (img.mem.dev.apiVersionInUse() >= VK_MAKE_VERSION(1, 1, 0)) {
    flags |=
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
  }
  img.info.format = img.mem.dev.chooseFormat(
      img.info.tiling, flags, VK_IMAGE_TYPE_2D,
      {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM});
  if (img.info.format == VK_FORMAT_UNDEFINED) {
    logE("skiaglue::loadImage(): no format supports BLIT_DST\n");
    return 1;
  }

  uint32_t rowStride = FormatSize(img.info.format) * dim.width();
  uint32_t decodedSize = rowStride * dim.height();
  if (decodedSize > stage.mmapMax()) {
    logE("skiaglue::loadImage: decodedSize=%zu > stage.mmapMax()=%zu\n",
         (size_t)decodedSize, stage.mmapMax());
    logE("skiaglue::loadImage: try VolcanoSamples/src/scanlinedecoder.cpp\n");
    return 1;
  }
  if (stage.mmap(img, decodedSize, flight)) {
    logE("skiaglue::loadImage: decodedSize=%zu stage.mmap failed\n",
         (size_t)decodedSize);
    return 1;
  }

  auto& copies = flight->copies;
  copies.resize(1);
  VkBufferImageCopy& copy = copies.at(0);
  memset(&copy, 0, sizeof(copy));
  copy.bufferRowLength = dim.width();
  copy.bufferImageHeight = dim.height();
  copy.imageExtent = img.info.extent;
  copy.imageSubresource = img.getSubresourceLayers(0);

  // Load 1 image. If img is set up as an image array, copy to [0] in array.
  copy.imageSubresource.layerCount = 1;

  if (getPixels(*codec, flight->mmap(), rowStride, imgFilenameFound, skfmt)) {
    logE("skiaglue::loadImage: getPixels failed\n");
    return 1;
  }
  return 0;
}

#ifdef __ANDROID__
#include <android_native_app_glue.h>

struct AndroidAssetBufferLoader {
  AndroidAssetBufferLoader(struct android_app* app_) : app_(app_) {}
  virtual ~AndroidAssetBufferLoader() {
    if (aa) {
      AAsset_close(aa);
      aa = nullptr;
    }
  }
  struct android_app* app_;
  AAsset* aa{nullptr};

  // load calls AAssetManager_open(). The destructor calls AAsset_close().
  sk_sp<SkData> load(const std::string& filename) {
    aa = AAssetManager_open(app_->activity->assetManager, filename.c_str(),
                            AASSET_MODE_BUFFER);
    if (!aa) {
      logE("skiaglue::loadImage: unable to open \"%s\"\n", filename.c_str());
      return sk_sp<SkData>();
    }
    off64_t assetLen = AAsset_getLength64(aa);
    if (assetLen < 0) {
      logE("skiaglue::loadImage: unable to size \"%s\"\n", filename.c_str());
      return sk_sp<SkData>();
    }
    const void* buf = AAsset_getBuffer(aa);
    if (!buf) {
      logE("skiaglue::loadImage: unable to read \"%s\"\n", filename.c_str());
      return sk_sp<SkData>();
    }
    sk_sp<SkData> data = SkData::MakeWithoutCopy(buf, (size_t)assetLen);
    if (!data) {
      logE("skiaglue::loadImage: unable to copy \"%s\"\n", filename.c_str());
      return sk_sp<SkData>();
    }
    return data;
  }
};
#endif

int skiaglue::loadImage(const char* imgFilename, memory::Stage& stage,
                        std::shared_ptr<memory::Flight>& flight,
                        memory::Image& img) {
  // findInPaths is defined in core/structs.h.
  if (findInPaths(imgFilename, imgFilenameFound)) {
    return 1;
  }

#ifdef __ANDROID__
  if (!android_app_) {
    logE("skiaglue::loadImage: android_app_ is NULL for \"%s\"\n",
         imgFilenameFound.c_str());
    return 1;
  }
  AndroidAssetBufferLoader aaLoader(android_app_);
  sk_sp<SkData> data = aaLoader.load(imgFilenameFound);
  if (!data) {
    return 1;
  }
#else
  sk_sp<SkData> data = SkData::MakeFromFileName(imgFilenameFound.c_str());
  if (!data) {
    logE("skiaglue::loadImage: unable to read \"%s\"\n",
         imgFilenameFound.c_str());
    return 1;
  }
#endif
  data->ref();
  if (data->size() < 4) {
    logE("skiaglue::loadImage: invalid file \"%s\"\n",
         imgFilenameFound.c_str());
    return 1;
  }

  // skia cannot read DDS or KTX files. Use gli for files with certain magic
  // bytes at the start of the file.
  static const char ddsMagicIdentifier[] = {'D', 'D', 'S', ' '};
  static const char ktxMagicIdentifier[] = {
      // From https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
      '\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n',
  };
  if ((data->size() >= sizeof(ddsMagicIdentifier) &&
       !memcmp(data->data(), ddsMagicIdentifier, sizeof(ddsMagicIdentifier))) ||
      (data->size() >= sizeof(ktxMagicIdentifier) &&
       !memcmp(data->data(), ktxMagicIdentifier, sizeof(ktxMagicIdentifier)))) {
    return loadGLIimage(imgFilenameFound.c_str(), data, stage, flight, img);
  }
  return loadSkiaImage(imgFilenameFound.c_str(), data, stage, flight, img);
}

enum FileType {
  FT_UNKNOWN = 0,
  FT_DDS,  // GLI supports writing these formats
  FT_KTX,
  FT_PNG,  // Skia supports writing these formats
  FT_JPG,
  FT_WEBP,
};

static int writeMmapped(VkExtent3D extent, gli::format format,
                        uint32_t mipLevels, const void* mappedMem,
                        VkDeviceSize bytes, const char* outFilename) {
  gli::extent2d gliExtent(extent.width, extent.height);

  // Get file type
  FileType fileType{FT_UNKNOWN};
  char type[5];
  size_t len = strlen(outFilename);
  if (len < 4) {
    logE("skiaglue::writeToFile(%s): missing extension?\n", outFilename);
    return 1;
  } else if (outFilename[len - 4] == '.') {
    strncpy(type, outFilename + len - 3, sizeof(type) - 1);
  } else if (len > 4 && outFilename[len - 5] == '.') {
    strncpy(type, outFilename + len - 4, sizeof(type) - 1);
  } else {
    logE("skiaglue::writeToFile(%s): missing extension?\n", outFilename);
    return 1;
  }

  type[sizeof(type) - 1] = 0;
  for (size_t i = 0; i < sizeof(type) - 1; i++) {
    type[i] = toupper(type[i]);
  }

  if (!strcmp(type, "DDS")) {
    fileType = FT_DDS;
  } else if (!strcmp(type, "KTX")) {
    fileType = FT_KTX;
  } else if (!strcmp(type, "PNG")) {
    fileType = FT_PNG;
  } else if (!strcmp(type, "JPG") || !strcmp(type, "JPEG")) {
    fileType = FT_JPG;
  } else if (!strcmp(type, "WEBP")) {
    fileType = FT_WEBP;
  } else {
    logE("skiaglue::writeToFile(%s): unsupported file type\n", outFilename);
    return 1;
  }

  if (fileType == FT_UNKNOWN) {
    logE("skiaglue::writeToFile(%s): BUG: no fileType.\n", outFilename);
    return 1;
  } else if (fileType == FT_DDS) {
    gli::texture2d tex(format, gliExtent, mipLevels);
    if (bytes != tex.size()) {
      logE("writeToFile(%s): BUG%d bytes got %zu want %zu\n", outFilename,
           (int)__LINE__, (size_t)bytes, (size_t)tex.size());
      return 1;
    }
    memcpy(tex.data<char>(), mappedMem, bytes);
    if (!gli::save_dds(tex, outFilename)) {
      logE("gli::save_dds(%s) failed\n", outFilename);
      return 1;
    }
  } else if (fileType == FT_KTX) {
    gli::texture2d tex(format, gliExtent, mipLevels);
    if (bytes != tex.size()) {
      logE("writeToFile(%s): BUG%d bytes got %zu want %zu\n", outFilename,
           (int)__LINE__, (size_t)bytes, (size_t)tex.size());
      return 1;
    }
    memcpy(tex.data<char>(), mappedMem, bytes);
    if (!gli::save_ktx(tex, outFilename)) {
      logE("gli::save_ktx(%s) failed\n", outFilename);
      return 1;
    }
  } else if (fileType == FT_PNG || fileType == FT_JPG || fileType == FT_WEBP) {
    // Only mip level 0 of mappedMem is used by recalculating FormatSize().
    SkPixmap src(SkImageInfo::Make(extent.width, extent.height,
                                   kRGBA_8888_SkColorType, kPremul_SkAlphaType),
                 mappedMem, FormatSize((VkFormat)format) * extent.width);

    SkFILEWStream skFile(outFilename);
    if (!skFile.isValid()) {
      // Skia swallows the error message, though in DEBUG builds it is printed.
      logE("skiaglue::writeToFile(%s): SkFILEWStream failed\n", outFilename);
      return 1;
    }

    if (fileType == FT_PNG) {
      if (!SkPngEncoder::Encode(&skFile, src, SkPngEncoder::Options{})) {
        // Skia swallows the error message, though DEBUG builds print it.
        logE("skiaglue::writeToFile(%s): PNG failed\n", outFilename);
        return 1;
      }
    } else if (fileType == FT_JPG) {
      SkJpegEncoder::Options options;
      options.fQuality = 95;
      if (!SkJpegEncoder::Encode(&skFile, src, options)) {
        // Skia swallows the error message, though DEBUG builds print it.
        logE("skiaglue::writeToFile(%s): JPG failed\n", outFilename);
        return 1;
      }
    } else if (fileType == FT_WEBP) {
      SkWebpEncoder::Options options;
      options.fQuality = 95;
      if (!SkWebpEncoder::Encode(&skFile, src, options)) {
        // Skia swallows the error message, though DEBUG builds print it.
        logE("skiaglue::writeToFile(%s): WEBP failed\n", outFilename);
        return 1;
      }
    }
  }
  return 0;
}

VkDeviceSize getBytesUsedFor(const std::vector<VkBufferImageCopy>& copies,
                             uint32_t mipLevels, VkDeviceSize formatSize) {
  VkDeviceSize bytes = 0;
  for (size_t mip = mipLevels; mip > 0;) {
    mip--;
    // Find anything in copies at this mip level. Get width and height.
    uint32_t width = 0;
    uint32_t height = 0;
    for (size_t i = 0; i < copies.size(); i++) {
      const VkBufferImageCopy& c = copies.at(i);
      if (c.imageSubresource.mipLevel == mip) {
        width = std::max(width, c.bufferRowLength);
        height = std::max(height, c.bufferImageHeight);
      }
    }
    bytes += formatSize * width * height;
  }
  return bytes;
}

VkExtent3D getSizeFor(memory::Image& image,
                      const std::vector<VkBufferImageCopy>& copies) {
  VkExtent3D r;
  r.width = 0;
  r.height = 0;
  r.depth = 1;
  for (size_t i = 0; i < copies.size(); i++) {
    const VkBufferImageCopy& c = copies.at(i);
    // Assume the largest copy is the final size.
    r.width = std::max(r.width, c.bufferRowLength);
    r.height = std::max(r.height, c.bufferImageHeight);
  }
  if (r.width < 1 || r.height < 1) {
    // Fall back to image's full size
    r.width = image.info.extent.width;
    r.height = image.info.extent.height;
  }
  return r;
}

int skiaglue::writeToFile(memory::Image& image, memory::Stage& stage,
                          std::string outFilename,
                          const std::vector<VkBufferImageCopy>& copies) {
  VkDeviceSize offset = 0;
  VkDeviceSize formatSize = FormatSize(image.info.format);
  // All mipmaps will never be more than 2x what mip level 0 size is:
  VkDeviceSize mipSize = (image.info.mipLevels > 1) ? 2 : 1;
  VkExtent3D ex = image.info.extent;
  if (formatSize * ex.width * ex.height * mipSize <= stage.mmapMax()) {
    // One transfer from the GPU will read the entire image.
    std::vector<VkBufferImageCopy> allMips;
    if (copies.empty()) {
      // Build VkBufferImageCopy vector to copy all mip levels of image.
      allMips.resize(image.info.mipLevels);
      for (size_t mip = 0; mip < image.info.mipLevels; mip++) {
        VkBufferImageCopy& c = allMips.at(mip);
        memset(&c, 0, sizeof(c));
        c.bufferOffset = offset;
        c.bufferRowLength = std::max(1u, ex.width >> mip);
        c.bufferImageHeight = std::max(1u, ex.height >> mip);
        c.imageExtent = {c.bufferRowLength, c.bufferImageHeight, 1};
        c.imageSubresource = image.getSubresourceLayers(mip);
        offset += formatSize * c.bufferRowLength * c.bufferImageHeight;
      }
    } else {
      offset = getBytesUsedFor(copies, image.info.mipLevels, formatSize);
      ex = getSizeFor(image, copies);
    }
    // Need to know number of bytes in offset, so need to get VkBufferImageCopy
    // vector *before* stage.read() even though it has to then be put in flight
    std::shared_ptr<memory::Flight> flight;
    if (stage.read(image, offset /*number of bytes*/, flight)) {
      logE("writeToFile(%s): stage.read failed\n", outFilename.c_str());
      return 1;
    }
    auto& v = copies.empty() ? allMips : copies;
    flight->copies.insert(flight->copies.begin(), v.begin(), v.end());
    // flushAndWait() submits flight to GPU. Then flight->mmap() is available.
    if (stage.flushAndWait(flight)) {
      logE("writeToFile(%s): stage.flush failed\n", outFilename.c_str());
      return 1;
    }
    return writeMmapped(ex, static_cast<gli::format>(image.info.format),
                        image.info.mipLevels, flight->mmap(), offset,
                        outFilename.c_str());
  }

  // The image is too large for one flight. Break it into chunks.
  VkDeviceSize rowStride = formatSize * ex.width;
  int32_t rowStep = stage.mmapMax() / rowStride;
  if (rowStep < 1) {
    logE("writeToFile(%s): width=%zu x formatSize=%zu is over mmapMax=%zu\n",
         outFilename.c_str(), (size_t)ex.width, (size_t)formatSize,
         stage.mmapMax());
    return 1;
  }
  // Allocate enough bytes to hold all the data.
  if (!copies.empty()) {
    // Don't use stage at all. Run user-specified vector of copies as-is.
    memory::Buffer host(stage.pool.vk.dev);
    host.info.size = getBytesUsedFor(copies, image.info.mipLevels, formatSize);
    if (host.ctorAndBindHostCoherent()) {
      logE("writeToFile(%s): host.ctorAndBind failed\n", outFilename.c_str());
      return 1;
    }
    {
      science::SmartCommandBuffer buffer{stage.pool, stage.poolQindex};
      if (buffer.ctorError() || buffer.autoSubmit() ||
          buffer.barrier(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) ||
          buffer.copyImage(image, host, copies)) {
        logE("buffer failed\n");
        return 1;
      }
    }

    char* mappedMem;
    if (host.mem.mmap((void**)&mappedMem)) {
      logE("host.mmap() failed\n");
      return 1;
    }
    int r = writeMmapped(
        getSizeFor(image, copies), static_cast<gli::format>(image.info.format),
        image.info.mipLevels, mappedMem, host.info.size, outFilename.c_str());
    host.mem.munmap();
    return r;
  }

  // TODO: use an encoder that consumes a few scanlines at a time.
  std::vector<char> allBytes(rowStride * ex.height * 2);
  memset(allBytes.data(), 0, allBytes.size());
  offset = 0;
  // Transfer one mip level at a time, even if more would fit in stage.
  for (size_t mip = 0; mip < image.info.mipLevels; mip++) {
    rowStride = formatSize * std::max(1u, ex.width >> mip);
    rowStep = stage.mmapMax() / rowStride;
    // Break up the mip level into rows.
    for (int32_t row = 0; row < (int32_t)ex.height; row += rowStep) {
      std::vector<VkBufferImageCopy> oneMip(1);
      VkBufferImageCopy& c = oneMip.at(0);
      memset(&c, 0, sizeof(c));
      c.bufferOffset = 0;
      c.bufferRowLength = std::max(1u, ex.width >> mip);
      uint32_t imgH = std::max(1u, ex.height >> mip);
      c.bufferImageHeight = std::min(imgH, (decltype(imgH))rowStep);
      c.imageExtent = {c.bufferRowLength, imgH, 1};
      c.imageOffset = {0, row, 0};
      c.imageSubresource = image.getSubresourceLayers(mip);
      VkDeviceSize bytes = formatSize * c.bufferRowLength * c.bufferImageHeight;

      std::shared_ptr<memory::Flight> flight;
      if (stage.read(image, bytes, flight)) {
        logE("writeToFile(%s): stage.read failed\n", outFilename.c_str());
        return 1;
      }
      flight->copies.insert(flight->copies.begin(), oneMip.at(0));
      // flushAndWait() submits flight to GPU. Then flight->mmap() is available.
      if (stage.flushAndWait(flight)) {
        logE("writeToFile(%s): stage.flush failed\n", outFilename.c_str());
        return 1;
      }

      if (offset + bytes > allBytes.size()) {
        logE("writeToFile(%s): BUG: mip=%zu row=%zu offset=%zu:\n",
             outFilename.c_str(), mip, (size_t)row, (size_t)offset);
        logE("writeToFile: bytes=%zu but allBytes.size=%zu\n", (size_t)bytes,
             allBytes.size());
        return 1;
      }
      memcpy(allBytes.data() + offset, flight->mmap(), bytes);
      offset += bytes;
    }
  }
  return writeMmapped(ex, static_cast<gli::format>(image.info.format),
                      image.info.mipLevels, allBytes.data(), offset,
                      outFilename.c_str());
}
