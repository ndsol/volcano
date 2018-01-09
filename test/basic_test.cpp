/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN
#ifdef _WIN32
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include <src/command/command.h>
#include <src/gn/vendor/skia/skiaglue.h>
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

#include <chrono>

// TODO: change vsync on the fly (and it must work the same at init time)
// TODO: switch VK_PRESENT_MODE_MAILBOX_KHR on the fly
// Use a utility class *outside* lib/language to:
//   TODO: show how to do double buffering, triple buffering
//   TODO: permit customization of the enabled instance layers.

const char* imgFilename = nullptr;

const std::vector<test::st_basic_test_vert> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
};

class SimplePipeline : public science::CommandPoolContainer {
 public:
  SimplePipeline(language::Instance& instance)
      : CommandPoolContainer{*instance.devs.at(0)} {
    startTime = std::chrono::high_resolution_clock::now();
    // Register onResizeFramebuf in CommandPoolContainer.
    resizeFramebufListeners.emplace_back(
        std::make_pair(onResizeFramebuf, this));
  }

  std::vector<command::CommandBuffer> cmdBuffers;

  int ctorError(GLFWwindow* window) {
    if (cpool.ctorError()) {
      return 1;
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, windowResized);

    return buildUniform();
  }

  static void windowResized(GLFWwindow* window, int glfw_w, int glfw_h) {
    glfwGetWindowSize(window, &glfw_w, &glfw_h);
    if (glfw_w == 0 || glfw_h == 0) {
      // Window was minimized or moved offscreen. Pretend nothing happened.
      return;
    }
    uint32_t width = glfw_w, height = glfw_h;
    SimplePipeline* self = (SimplePipeline*)glfwGetWindowUserPointer(window);
    if (self->onResized({width, height}, memory::ASSUME_POOL_QINDEX)) {
      logF("onResized failed!\n");
      exit(1);
    }
  }

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
      logI("%d fps\n", frameCount);
      startTime = currentTime;
      frameCount = 0;
      timeDelta++;
      timeDelta &= 3;
    }
    time += timeDelta;

    test::UniformBufferObject ubo = {};
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
  }

 protected:
  science::ShaderLibrary shaders{cpool.dev};
  science::DescriptorLibrary descriptorLibrary{cpool.dev};
  std::unique_ptr<memory::DescriptorSet> descriptorSet;
  memory::UniformBuffer uniform{cpool.dev};
  memory::Buffer vertexBuffer{cpool.dev};
  memory::Buffer indexBuffer{cpool.dev};
  memory::Sampler textureSampler{cpool.dev};
  science::PipeBuilder pipe0{cpool.dev, pass};

  // buildUniform builds the uniform buffers, descriptor sets, and other
  // objects needed during startup.
  int buildUniform() {
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

    if (uniform.ctorError(dev, sizeof(test::UniformBufferObject))) {
      return 1;
    }

    memory::Buffer stage(cpool.dev);
    skiaglue skGlue(cpool, textureSampler.image.info);
    textureSampler.info.magFilter = VK_FILTER_LINEAR;
    textureSampler.info.minFilter = VK_FILTER_LINEAR;
    textureSampler.info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (skGlue.loadImage(imgFilename, stage) ||
        textureSampler.ctorError(cpool, stage, skGlue.copies)) {
      return 1;
    }

    pipe0.info().dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
    pipe0.info().dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);
    if (pipe0.addDepthImage({
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        }) ||
        pipe0.addVertexInput<test::st_basic_test_vert>()) {
      return 1;
    }

    logI("main.vert.spv (0x%zx bytes) main.frag.spv (0x%zx bytes)\n",
         sizeof(spv_basic_test_vert), sizeof(spv_basic_test_frag));

    auto vshader =
        shaders.load(spv_basic_test_vert, sizeof(spv_basic_test_vert));
    auto fshader =
        shaders.load(spv_basic_test_frag, sizeof(spv_basic_test_frag));
    if (!vshader || !fshader ||
        shaders.stage(pass, pipe0, VK_SHADER_STAGE_VERTEX_BIT, vshader) ||
        shaders.stage(pass, pipe0, VK_SHADER_STAGE_FRAGMENT_BIT, fshader) ||
        shaders.makeDescriptorLibrary(descriptorLibrary)) {
      return 1;
    }

    constexpr size_t LI = 0;
    descriptorSet = descriptorLibrary.makeSet(LI);
    if (!descriptorSet) {
      logE("descriptorLibrary.makeSet failed\n");
      return 1;
    }
    pipe0.info().setLayouts.emplace_back(descriptorLibrary.layouts.at(LI).vk);

    return descriptorSet->write(0, {&uniform}) ||
           descriptorSet->write(1, {&textureSampler}) ||
           onResized(cpool.dev.swapChainInfo.imageExtent,
                     memory::ASSUME_POOL_QINDEX);
  }

  // onResizeFramebuf is called for each framebuf that needs to be resized.
  static int onResizeFramebuf(void* self, language::Framebuf& framebuf,
                              size_t framebuf_i, size_t /*poolQindex*/) {
    return static_cast<SimplePipeline*>(self)->onResizeFramebufImpl(framebuf,
                                                                    framebuf_i);
  }

  int onResizeFramebufImpl(language::Framebuf& framebuf, size_t framebuf_i) {
    if (framebuf_i == 0) {
      cpool.updateBuffersAndPass(cmdBuffers, pass);
    }
    // Patch viewport with new size.
    auto& newSize = cpool.dev.swapChainInfo.imageExtent;
    VkViewport& viewport = pipe0.info().viewports.at(0);
    viewport.width = (float)newSize.width;
    viewport.height = (float)newSize.height;

    // Patch scissors with new size.
    pipe0.info().scissors.at(0).extent = newSize;

    auto& cmdBuffer = cmdBuffers.at(framebuf_i);
    VkBuffer vertexBuffers[] = {vertexBuffer.vk};
    VkDeviceSize offsets[] = {0};
    if (cmdBuffer.beginSimultaneousUse() || cmdBuffer.setViewport(pass) ||
        cmdBuffer.setScissor(pass) ||
        cmdBuffer.beginPrimaryPass(pass, framebuf) ||
        cmdBuffer.bindGraphicsPipelineAndDescriptors(*pipe0.pipe, 0, 1,
                                                     &descriptorSet->vk) ||
        cmdBuffer.bindVertexBuffers(
            0, sizeof(vertexBuffers) / sizeof(vertexBuffers[0]), vertexBuffers,
            offsets) ||
        cmdBuffer.bindAndDraw(indices, indexBuffer.vk, 0 /*indexBufOffset*/) ||
        cmdBuffer.draw(3, 1, 0, 0) || cmdBuffer.endRenderPass() ||
        cmdBuffer.end()) {
      logE("onResizeFramebuf: command buffer [%zu] failed\n", framebuf_i);
      return 1;
    }
    return 0;
  }
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
  science::PresentSemaphore renderSemaphore(simple);
  if (renderSemaphore.ctorError()) {
    return 1;
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (simple.updateUniformBuffer()) {
      return 1;
    }

    uint32_t next_image_i;
    if (simple.acquireNextImage(&next_image_i, imageAvailableSemaphore)) {
      return 1;
    }
    if (next_image_i != (uint32_t)-1) {
      if (simple.cmdBuffers.at(next_image_i)
              .submit(0, {imageAvailableSemaphore.vk},
                      {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                      {renderSemaphore.vk}) ||
          renderSemaphore.present(&next_image_i)) {
        return 1;
      }
      if (next_image_i != (uint32_t)-1 && renderSemaphore.waitIdle()) {
        return 1;
      }
    }
  }

  VkResult v = vkDeviceWaitIdle(simple.cpool.dev.dev);
  if (v != VK_SUCCESS) {
    logE("vkDeviceWaitIdle returned %d\n", v);
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
  logI("Instance::open\n");
  if (inst.open(size)) {
    return 1;
  }
  logI("Instance::open done\n");
  if (!inst.devs.size()) {
    logE("BUG: no devices created\n");
    return 1;
  }
  auto simple = std::unique_ptr<SimplePipeline>(new SimplePipeline(inst));
  return mainLoop(window, *simple);
}

void printGLFWerr(int code, const char* msg) {
  logE("glfw error %x: %s\n", code, msg);
}

int runGLFW() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  VkExtent2D size{800, 600};
  GLFWwindow* window = glfwCreateWindow(
      size.width, size.height, "Vulkan window",
      nullptr /*monitor for fullscreen*/, nullptr /*context object sharing*/);
  glfwSetErrorCallback(printGLFWerr);
  int r = runLanguage(window, size);
  glfwDestroyWindow(window);
  glfwTerminate();
  return r;
}

}  // anonymous namespace

#if defined(_WIN32)
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int /*nCmdShow*/) {
  imgFilename = lpCmdLine;
  return runGLFW();  // HINSTANCE can be retrieved with GetModuleHandle(0).
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
  imgFilename = argv[1];
  return runGLFW();
}
#endif
