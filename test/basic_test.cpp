/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <src/command/command.h>
#include <src/language/VkInit.h>
#include <src/language/VkPtr.h>
#include <src/language/language.h>
#include <src/memory/memory.h>
#include <src/science/science.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
// Compile SPIR-V bytecode directly into application.
#include "test/basic_test.frag.h"
#include "test/basic_test.vert.h"
// Stow auto-generated vertex struct into a namespace.
namespace test {
#include "test/struct_basic_test.vert.h"
}

#include "SkCanvas.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkImageInfo.h"
#include "SkRefCnt.h"

#include <array>
#include <chrono>

// TODO: change vsync on the fly (and it must work the same at init time)
// TODO: switch VK_PRESENT_MODE_MAILBOX_KHR on the fly
// Use a utility class *outside* lib/language to:
//   TODO: show how to do double buffering, triple buffering
//   TODO: permit customization of the enabled instance layers.
//   TODO: mipmapping
//
// TODO: show how to do GPU compute
// TODO: passes, subpasses, secondary command buffers, and subpass dependencies

const char* img_filename;

const std::vector<test::basic_test_vert> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
};

class SimplePipeline : public science::SwapChainResizeObserver {
 public:
  command::RenderPass pass;
  command::CommandPool cpool;
  command::CommandBuilder builder;
  science::SwapChainResizeList resizeList;

  SimplePipeline(language::Instance& instance,
                 language::SurfaceSupport queueFamily)
      : pass(instance.at(0)),
        cpool(instance.at(0), queueFamily),
        builder(cpool) {
    startTime = std::chrono::high_resolution_clock::now();
  };

  int ctorError(GLFWwindow* window) {
    if (cpool.ctorError(0)) {
      return 1;
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, windowResized);

    if (buildUniform()) {
      return 1;
    }

    return onResized(cpool.dev, builder, cpool.dev.swapChainInfo.imageExtent);
  };

  static void windowResized(GLFWwindow* window, int glfw_w, int glfw_h) {
    if (glfw_w == 0 || glfw_h == 0) {
      // Window was minimized or moved offscreen. Pretend nothing happened.
      return;
    }
    uint32_t width = glfw_w, height = glfw_h;
    SimplePipeline* self = (SimplePipeline*)glfwGetWindowUserPointer(window);
    if (self->resizeList.syncResize(self->cpool, {width, height})) {
      fprintf(stderr, "syncResize failed!\n");
      exit(1);
    }
  };

  std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
  unsigned frameCount = 0;
  int timeDelta = 0;

  int updateUniformBuffer() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(
                     currentTime - startTime)
                     .count() /
                 1000.0f;
    frameCount++;
    if (time > 1.0) {
      fprintf(stderr, "%d fps\n", frameCount);
      startTime = currentTime;
      frameCount = 0;
      timeDelta++;
      timeDelta &= 3;
    }
    time += timeDelta;

    test::basic_test_vert::ubo ubo = {};
    auto model = glm::rotate(glm::mat4(), time * glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));
    memcpy(&ubo.model[0][0], &model[0][0], sizeof(ubo.model));

    auto view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    memcpy(&ubo.view[0][0], &view[0][0], sizeof(ubo.view));

    auto proj = glm::perspective(glm::radians(45.0f), cpool.dev.aspectRatio(),
                                0.1f, 10.0f);

    // convert from OpenGL where clip coordinates +Y is up
    // to Vulkan where clip coordinates +Y is down. The other OpenGL/Vulkan
    // coordinate change is GLM_FORCE_DEPTH_ZERO_TO_ONE. For more information:
    // https://github.com/LunarG/VulkanSamples/commit/0dd3617
    // https://forums.khronos.org/showthread.php/13152-Understand-Vulkan-Clipping
    // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
    // https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#vertexpostproc-clipping
    proj[1][1] *= -1;
    memcpy(&ubo.proj[0][0], &proj[0][0], sizeof(ubo.proj));

    if (uniform.copy(cpool, &ubo, sizeof(ubo))) {
      return 1;
    }
    return 0;
  };

 protected:
  science::ShaderLibrary shaders{cpool.dev};
  science::DescriptorLibrary descriptorLibrary{cpool.dev};
  std::unique_ptr<memory::DescriptorSet> descriptorSet;
  memory::UniformBuffer uniform{cpool.dev};
  memory::Buffer vertexBuffer{cpool.dev};
  memory::Buffer indexBuffer{cpool.dev};
  memory::Sampler textureSampler{cpool.dev};
  std::unique_ptr<science::PipeBuilder> pipe0;

  // buildUniform builds the uniform buffers, descriptor sets, and other
  // objects needed during startup.
  int buildUniform() {
    void* mappedMem;
    language::Device& dev = cpool.dev;

    {
      memory::Buffer stagingBuffer{dev};
      vertexBuffer.info.size = stagingBuffer.info.size =
          sizeof(vertices[0]) * vertices.size();
      vertexBuffer.info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

      if (stagingBuffer.ctorHostCoherent(dev) ||
          stagingBuffer.bindMemory(dev) ||
          stagingBuffer.copyFromHost(dev, vertices) ||
          vertexBuffer.ctorDeviceLocal(dev) || vertexBuffer.bindMemory(dev) ||
          vertexBuffer.copy(cpool, stagingBuffer)) {
        return 1;
      }
    }
    {
      memory::Buffer stagingBuffer{dev};
      indexBuffer.info.size = stagingBuffer.info.size =
          sizeof(indices[0]) * indices.size();
      indexBuffer.info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

      if (stagingBuffer.ctorHostCoherent(dev) ||
          stagingBuffer.bindMemory(dev) ||
          stagingBuffer.copyFromHost(dev, indices) ||
          indexBuffer.ctorDeviceLocal(dev) || indexBuffer.bindMemory(dev) ||
          indexBuffer.copy(cpool, stagingBuffer)) {
        return 1;
      }
    }

    if (uniform.ctorError(dev, sizeof(test::basic_test_vert::ubo))) {
      return 1;
    }

    // Create textureSampler
    // TODO: keep track of
    // https://skia.googlesource.com/skia/+/master/src/gpu/vk/GrVkImage.h
    // as an alternate way to create a textureSampler.
    //
    // TODO: use SkCodec instead of SkImage

    sk_sp<SkData> data = SkData::MakeFromFileName(img_filename);
    if (!data) {
      fprintf(stderr, "   unable to read image \"%s\"\n", img_filename);
      return 1;
    }
    sk_sp<SkImage> img = SkImage::MakeFromEncoded(data);
    if (!img) {
      // TODO: get exact error message from skia.
      fprintf(stderr, "   unable to decode image \"%s\"\n", img_filename);
      return 1;
    }
    VkExtent3D extent = {1, 1, 1};
    extent.width = img->width();
    extent.height = img->height();

    // TODO: Use a VkBuffer instead:
    // http://xlgames-inc.github.io/posts/vulkantips/
    // TODO: can the GPU create the mip levels from a non-mipmapped image?
    memory::Image stagingImage(dev);
    stagingImage.info.extent = extent;
    stagingImage.info.format = VK_FORMAT_R8G8B8A8_UNORM;
    if (stagingImage.ctorHostCoherent(dev) || stagingImage.bindMemory(dev)) {
      fprintf(stderr, "stagingImage.ctorError or bindMemory failed\n");
      return 1;
    }

    VkImageSubresource subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.arrayLayer = 0;

    VkSubresourceLayout stagingImageLayout;
    {
      // NOTE: vkGetImageSubresourceLayout is only valid for an image with
      // LINEAR tiling.
      //
      // Vulkan Spec: "The layout of a subresource (mipLevel/arrayLayer) of an
      // image created with linear tiling is queried by calling
      // vkGetImageSubresourceLayout".
      vkGetImageSubresourceLayout(dev.dev, stagingImage.vk, &subresource,
                                  &stagingImageLayout);
    }

    if (stagingImage.mem.mmap(dev, &mappedMem)) {
      fprintf(stderr, "stagingImageMemory.mmap() failed\n");
      return 1;
    }
    SkImageInfo dstInfo =
        SkImageInfo::Make(img->width(), img->height(), kRGBA_8888_SkColorType,
                          kPremul_SkAlphaType);
    if (!img->readPixels(dstInfo, mappedMem, stagingImageLayout.rowPitch, 0,
                         0)) {
      fprintf(stderr, "SkImage::readPixels() failed\n");
      stagingImage.mem.munmap(dev);
      return 1;
    }
    stagingImage.mem.munmap(dev);

    textureSampler.info.magFilter = VK_FILTER_LINEAR;
    textureSampler.info.minFilter = VK_FILTER_LINEAR;
    textureSampler.info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    command::CommandBuilder setup(cpool);
    if (setup.beginOneTimeUse()) {
      return 1;
    }
    if (textureSampler.ctorError(dev, setup, stagingImage)) {
      fprintf(stderr, "sampler.copyFrom failed\n");
      return 1;
    }

    if (pipe0) {
      fprintf(stderr, "new science::PipeBuilder can only be called once.\n");
      return 1;
    }
    pipe0.reset(new science::PipeBuilder(dev, pass));

    // pipe0 should be first in resizeList. SimplePipeline assumes
    // Framebuffer attachments are already correctly resized when its
    // onResized is called.
    resizeList.list.emplace_back(&*pipe0);
    resizeList.list.emplace_back(this);

    if (pipe0->addDepthImage(
            dev, pass, setup,
            {
                VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
            }) ||
        pipe0->addVertexInput<test::basic_test_vert>()) {
      return 1;
    }

    if (setup.end() || setup.submit(0)) {
      return 1;
    }
    vkQueueWaitIdle(cpool.q(0));

    fprintf(stderr,
            "renderPass.init: "
            "main.vert.spv (0x%zx bytes) main.frag.spv (0x%zx bytes)\n",
            sizeof(spv_basic_test_vert), sizeof(spv_basic_test_frag));

    auto vshader =
        shaders.load(spv_basic_test_vert, sizeof(spv_basic_test_vert));
    auto fshader =
        shaders.load(spv_basic_test_frag, sizeof(spv_basic_test_frag));
    if (!vshader || !fshader ||
        shaders.stage(pass, *pipe0, VK_SHADER_STAGE_VERTEX_BIT, vshader) ||
        shaders.stage(pass, *pipe0, VK_SHADER_STAGE_FRAGMENT_BIT, fshader) ||
        shaders.makeDescriptorLibrary(descriptorLibrary)) {
      return 1;
    }

    descriptorSet = descriptorLibrary.makeSet(*pipe0);
    if (!descriptorSet) {
      fprintf(stderr, "descriptorLibrary.makeSet failed\n");
      return 1;
    }

    if (descriptorSet->write(0, {&uniform}) ||
        descriptorSet->write(1, {&textureSampler})) {
      return 1;
    }

    pipe0->pipeline.info.dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
    pipe0->pipeline.info.dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);

    if (pass.ctorError(dev)) {
      return 1;
    }
    return 0;
  }

  // onResized rebuilds RenderPass pass. This is done once on startup, then
  // every time the window is resized.
  int onResized(language::Device& dev,
                command::CommandBuilder& unusedSetupCommands,
                VkExtent2D oldSize) {
    (void)oldSize;  // oldSize is not used. Silence the compiler warning.
    auto& newSize = dev.swapChainInfo.imageExtent;
    // CommandBuilder has a feature to manage several VkCommandBuffers. That
    // is particularly useful when setting up per-framebuffer commands.
    if (builder.resize(dev.framebufs.size())) {
      return 1;
    }

    for (size_t i = 0; i < dev.framebufs.size(); i++) {
      language::Framebuf& framebuf = dev.framebufs.at(i);
      if (framebuf.ctorError(dev, pass.vk, newSize)) {
        return 1;
      }
      // Patch pass.passBeginInfo for each framebuffer in the swapChain.
      // (The data in passBeginInfo is copied into each distinct
      // VkCommandBuffer, so patching it like this is ok.)
      pass.passBeginInfo.framebuffer = framebuf.vk;

      // Patch viewport.
      VkViewport& viewport = pass.pipelines.at(0).info.viewports.at(0);
      viewport.width = (float)newSize.width;
      viewport.height = (float)newSize.height;

      // Patch scissors.
      pass.pipelines.at(0).info.scissors.at(0).extent = newSize;

      VkBuffer vertexBuffers[] = {vertexBuffer.vk};
      VkDeviceSize offsets[] = {0};

      // Switch builder to the CommandBuilder::VkCommandBuffer[i] and rebuild
      // the VkCommandBuffer.
      builder.use(i);
      if (builder.beginSimultaneousUse() || builder.setViewport(pass) ||
          builder.setScissor(pass) || builder.beginPrimaryPass(pass) ||
          builder.bindGraphicsPipelineAndDescriptors(pipe0->pipeline, 0, 1,
                                                     &descriptorSet->vk) ||
          builder.bindVertexBuffers(
              0, sizeof(vertexBuffers) / sizeof(vertexBuffers[0]),
              vertexBuffers, offsets) ||
          builder.bindAndDraw(indices, indexBuffer.vk, 0 /*indexBufOffset*/) ||
          builder.draw(3, 1, 0, 0) || builder.endRenderPass() ||
          builder.end()) {
        fprintf(stderr, "build: failed to recreate command buffer %zu\n", i);
        return 1;
      }
    }
    return 0;
  };
};

namespace {  // an anonymous namespace hides its contents outside this file

int mainLoop(GLFWwindow* window, SimplePipeline& simple) {
  if (simple.ctorError(window)) {
    return 1;
  }

  command::Semaphore imageAvailableSemaphore(simple.cpool.dev);
  if (imageAvailableSemaphore.ctorError(simple.cpool.dev)) {
    return 1;
  }
  command::PresentSemaphore renderSemaphore(simple.cpool.dev);
  if (renderSemaphore.ctorError()) {
    return 1;
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (simple.updateUniformBuffer()) {
      return 1;
    }

    uint32_t next_image_i;
    VkResult v = vkAcquireNextImageKHR(
        simple.cpool.dev.dev, simple.cpool.dev.swapChain,
        std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore.vk,
        VK_NULL_HANDLE, &next_image_i);
    if (v != VK_SUCCESS && v != VK_SUBOPTIMAL_KHR) {
      if (v == VK_ERROR_OUT_OF_DATE_KHR) {
        fprintf(stderr, "vkAcquireNextImageKHR: OUT_OF_DATE\n");
        auto& extent = simple.cpool.dev.swapChainInfo.imageExtent;
        SimplePipeline::windowResized(window, extent.width, extent.height);
        continue;
      }
      fprintf(stderr, "vkAcquireNextImageKHR returned error\n");
      return 1;
    }
    simple.builder.use(next_image_i);
    if (simple.builder.submit(0, {imageAvailableSemaphore.vk},
                              {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                              {renderSemaphore.vk}) ||
        renderSemaphore.present(next_image_i)) {
      return 1;
    }
  }

  VkResult v = vkDeviceWaitIdle(simple.cpool.dev.dev);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkDeviceWaitIdle returned %d\n", v);
    return 1;
  }
  return 0;
}

// Wrap glfwCreateWindowSurface so language::Instance inst can call it.
VkResult createWindowSurface(language::Instance& inst, void* window) {
  return glfwCreateWindowSurface(inst.vk, (GLFWwindow*)window, inst.pAllocator,
                                 &inst.surface);
}

int runLanguage(GLFWwindow* window, VkExtent2D size) {
  unsigned int extensionSize = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extensionSize);
  language::Instance inst;
  if (inst.ctorError(extensions, extensionSize, createWindowSurface, window)) {
    return 1;
  }
  fprintf(stderr, "Instance::open\n");
  if (inst.open(size)) {
    return 1;
  }
  fprintf(stderr, "Instance::open done\n");
  if (!inst.devs_size()) {
    fprintf(stderr, "BUG: no devices created\n");
    return 1;
  }
  auto simple = std::unique_ptr<SimplePipeline>(
      new SimplePipeline(inst, language::GRAPHICS));
  return mainLoop(window, *simple);
}

void printGLFWerr(int code, const char* msg) {
  fprintf(stderr, "glfw error %x: %s\n", code, msg);
}

int runGLFW() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  VkExtent2D size{800, 600};
  GLFWwindow* window = glfwCreateWindow(size.width, size.height,
                                        "Vulkan window", nullptr, nullptr);
  glfwSetErrorCallback(printGLFWerr);
  int r = runLanguage(window, size);
  glfwDestroyWindow(window);
  glfwTerminate();
  return r;
}

}  // anonymous namespace

#if defined(_WIN32)
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return runGLFW();
}
#elif defined(__ANDROID__)
void android_main(android_app* app) {
  // Linker will think android_native_app_glue is unused without this app_dummy.
  // https://developer.android.com/ndk/samples/sample_na.html
  // http://altdevblog.com/2012/02/28/running-native-code-on-android-part-2/
  app_dummy();
}
#else
int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    return 1;
  }
  img_filename = argv[1];
  return runGLFW();
}
#endif
