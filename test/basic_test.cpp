/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN
#ifdef _WIN32
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include <src/gn/vendor/skia/skiaglue.h>
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
// Stow auto-generated vertex shader struct into a namespace.
namespace vert {
#include "test/struct_basic_test.vert.h"
}
// Stow auto-generated fragment shader struct into a namespace.
namespace frag {
#include "test/struct_basic_test.frag.h"
}

#include <chrono>

// TODO: change vsync on the fly (and it must work the same at init time)
// TODO: switch VK_PRESENT_MODE_MAILBOX_KHR on the fly
// TODO: switch single, double, or triple buffering

const char* imgFilename = nullptr;
bool automatedTest = false;

const std::vector<vert::st_basic_test_vert> vertices = {
    {glm::vec4(-0.5f, -0.5f, -0.5f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1),
     glm::vec2(1.0f, 0.0f)},
    {glm::vec4(0.5f, -0.5f, -0.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1),
     glm::vec2(0.0f, 0.0f)},
    {glm::vec4(0.5f, 0.5f, -0.5f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1),
     glm::vec2(0.0f, 1.0f)},
    {glm::vec4(-0.5f, 0.5f, -0.5f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1),
     glm::vec2(1.0f, 1.0f)},

    {glm::vec4(-0.5f, -0.5f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1),
     glm::vec2(1.0f, 0.0f)},
    {glm::vec4(0.5f, -0.5f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1),
     glm::vec2(0.0f, 0.0f)},
    {glm::vec4(0.5f, 0.5f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1),
     glm::vec2(0.0f, 1.0f)},
    {glm::vec4(-0.5f, 0.5f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1),
     glm::vec2(1.0f, 1.0f)}};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
};

class SimplePipeline : public science::CommandPoolContainer {
 public:
  SimplePipeline(language::Instance& instance)
      : CommandPoolContainer{*instance.devs.at(0)} {
    startTime = std::chrono::high_resolution_clock::now();
    // Register onResizeFramebuf in CommandPoolContainer.
    resizeFramebufListeners.emplace_back(std::make_pair(
        [](void* self, language::Framebuf& fb, size_t fbi, size_t) -> int {
          return static_cast<SimplePipeline*>(self)->onResizeFramebuf(fb, fbi);
        },
        this));
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
  unsigned lastDisplayedFrameCount = 0;
  int timeDelta = 0;

  int updateUniformBuffer() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(
                     currentTime - startTime)
                     .count() /
                 1000.0f;
    if (time > 1.0) {
      logI("%d fps\n", frameCount - lastDisplayedFrameCount);
      startTime = currentTime;
      time = 0;
      lastDisplayedFrameCount = frameCount;
      timeDelta++;
      timeDelta &= 3;
    }
    time += timeDelta;

    vert::UniformBufferObject ubo = {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.proj = glm::perspective(glm::radians(45.0f), cpool.vk.dev.aspectRatio(),
                                0.1f, 10.0f);

    // convert from OpenGL where clip coordinates +Y is up
    // to Vulkan where clip coordinates +Y is down. The other OpenGL/Vulkan
    // coordinate change is GLM_FORCE_DEPTH_ZERO_TO_ONE. For more information:
    // https://github.com/LunarG/VulkanSamples/commit/0dd3617
    // https://forums.khronos.org/showthread.php/13152-Understand-Vulkan-Clipping
    // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
    // https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#vertexpostproc-clipping
    ubo.proj[1][1] *= -1;

    std::shared_ptr<memory::Flight> flight;
    if (stage.mmap(uniform, 0 /*offset*/, sizeof(ubo), flight)) {
      logE("stage.mmap failed\n");
      return 1;
    }
    memcpy(flight->mmap(), &ubo, sizeof(ubo));
    return stage.flushAndWait(flight);
  }

 protected:
  science::ShaderLibrary shaders{cpool.vk.dev};
  science::DescriptorLibrary descriptorLibrary{cpool.vk.dev};
  std::unique_ptr<memory::DescriptorSet> descriptorSet;
  memory::Stage stage{cpool, memory::ASSUME_POOL_QINDEX};
  memory::Buffer uniform{cpool.vk.dev};
  memory::Buffer vertexBuffer{cpool.vk.dev};
  memory::Buffer indexBuffer{cpool.vk.dev};
  science::Sampler samp{cpool.vk.dev};
  science::PipeBuilder pipe0{pass};

  // buildUniform builds the uniform buffers, descriptor sets, and other
  // objects needed during startup.
  int buildUniform() {
    language::Device& dev = cpool.vk.dev;
    vertexBuffer.info.size = sizeof(vertices[0]) * vertices.size();
    vertexBuffer.info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    indexBuffer.info.size = sizeof(indices[0]) * indices.size();
    indexBuffer.info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    uniform.info.size = sizeof(vert::UniformBufferObject);
    uniform.info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (vertexBuffer.ctorAndBindDeviceLocal() ||
        indexBuffer.ctorAndBindDeviceLocal() ||
        uniform.ctorAndBindDeviceLocal() ||
        stage.copy(vertexBuffer, 0 /*offset*/, vertices) ||
        stage.copy(indexBuffer, 0 /*offset*/, indices)) {
      return 1;
    }

    samp.info.magFilter = VK_FILTER_LINEAR;
    samp.info.minFilter = VK_FILTER_LINEAR;
    samp.info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    {
      std::shared_ptr<memory::Flight> flight;
      skiaglue skGlue;
      if (skGlue.loadImage(imgFilename, stage, flight, *samp.image)) {
        logE("failed to load \"%s\"\n", imgFilename);
        return 1;
      }
      samp.imageView.info.subresourceRange = samp.image->getSubresourceRange();
      if (samp.ctorError() || stage.flushAndWait(flight)) {
        logE("samp.ctorError or stage.flushAndWait failed\n");
        return 1;
      }
      // Transition the image layout of imGuiFontSampler.
      science::SmartCommandBuffer smart(stage.pool, stage.poolQindex);
      if (smart.ctorError() || smart.autoSubmit() ||
          smart.barrier(*samp.image,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        logE("barrier(SHADER_READ_ONLY) failed\n");
        return 1;
      }
    }

    auto& pipeInfo = pipe0.info();
    pipeInfo.perFramebufColorBlend.at(0) =
        command::PipelineCreateInfo::withEnabledAlpha();
    pipeInfo.dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
    pipeInfo.dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);
    if (pipe0.addDepthImage({
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        }) ||
        pipe0.addVertexInput<vert::st_basic_test_vert>()) {
      return 1;
    }

    logI("main.vert.spv (0x%zx bytes) main.frag.spv (0x%zx bytes)\n",
         sizeof(spv_basic_test_vert), sizeof(spv_basic_test_frag));

    constexpr size_t LI = 0;
    auto vert = std::shared_ptr<command::Shader>(new command::Shader(dev));
    auto frag = std::shared_ptr<command::Shader>(new command::Shader(dev));
    if (vert->loadSPV(spv_basic_test_vert, sizeof(spv_basic_test_vert)) ||
        frag->loadSPV(spv_basic_test_frag, sizeof(spv_basic_test_frag)) ||
        shaders.add(pipe0, vert, LI) || shaders.add(pipe0, frag, LI) ||
        shaders.finalizeDescriptorLibrary(descriptorLibrary)) {
      return 1;
    }

    descriptorSet = descriptorLibrary.makeSet(0, LI);
    if (!descriptorSet) {
      logE("descriptorLibrary.makeSet failed\n");
      return 1;
    }
    for (auto& layout : descriptorLibrary.layouts.at(LI)) {
      pipe0.info().setLayouts.emplace_back(layout.vk);
    }
    VkDescriptorBufferInfo dsBuf;
    memset(&dsBuf, 0, sizeof(dsBuf));
    dsBuf.buffer = uniform.vk;
    dsBuf.range = uniform.info.size;

    return descriptorSet->write(vert::bindingIndexOfUniformBufferObject(),
                                {dsBuf}) ||
           descriptorSet->write(frag::bindingIndexOftexSampler(),
                                std::vector<science::Sampler*>{&samp}) ||
           onResized(cpool.vk.dev.swapChainInfo.imageExtent,
                     memory::ASSUME_POOL_QINDEX);
  }

  // onResizeFramebuf is called for each framebuf that needs to be resized.
  int onResizeFramebuf(language::Framebuf& framebuf, size_t framebuf_i) {
    auto& dev = cpool.vk.dev;
    if (framebuf_i == 0 &&
        cpool.reallocCmdBufs(cmdBuffers, dev.framebufs.size(), pass, 0)) {
      return 1;
    }
    // Patch viewport with new size.
    auto& newSize = dev.swapChainInfo.imageExtent;
    VkViewport& viewport = pipe0.info().viewports.at(0);
    viewport.width = (float)newSize.width;
    viewport.height = (float)newSize.height;

    // Patch scissors with new size.
    pipe0.info().scissors.at(0).extent = newSize;

    auto& cmdBuffer = cmdBuffers.at(framebuf_i);
    VkBuffer vertexBuffers[] = {vertexBuffer.vk};
    VkDeviceSize offsets[] = {0};
    if (cmdBuffer.beginSimultaneousUse() ||
        cmdBuffer.setViewport(0, 1, &viewport) ||
        cmdBuffer.setScissor(0, 1, &pipe0.info().scissors.at(0)) ||
        cmdBuffer.beginSubpass(pass, framebuf, 0) ||
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
  language::Device& dev = simple.cpool.vk.dev;

  command::Semaphore imageAvailableSemaphore(dev);
  if (imageAvailableSemaphore.setName("imageAvailableSemaphore") ||
      imageAvailableSemaphore.ctorError()) {
    logE("imageAvailableSemaphore failed\n");
    return 1;
  }
  command::Semaphore renderSemaphore(dev);
  if (renderSemaphore.ctorError() ||
      renderSemaphore.setName("renderSemaphore")) {
    logE("renderSemaphore failed\n");
    return 1;
  }

  auto qfam_i = dev.getQfamI(language::PRESENT);
  if (qfam_i == (decltype(qfam_i))(-1)) {
    logE("dev.getQfamI(%d) failed\n", language::PRESENT);
    return 1;
  }
  auto& qfam = dev.qfams.at(qfam_i);
  if (qfam.queues.size() < 1) {
    logE("BUG: queue family PRESENT with %zu queues\n", qfam.queues.size());
    return 1;
  }
  VkQueue presentQueue = qfam.queues.at(memory::ASSUME_PRESENT_QINDEX);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (automatedTest && simple.timeDelta == 3) {
      break;
    }
    if (simple.updateUniformBuffer()) {
      return 1;
    }

    dev.setFrameNumber(simple.frameCount);
    uint32_t nextImage = 0;
    VkResult result = vkAcquireNextImageKHR(
        dev.dev, dev.swapChain, std::numeric_limits<uint64_t>::max(),
        imageAvailableSemaphore.vk, VK_NULL_HANDLE, &nextImage);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (
#ifdef __ANDROID__ /* surface being destroyed may return OUT_OF_DATE */
            dev.getSurface() &&
#endif
            simple.onResized(dev.swapChainInfo.imageExtent,
                             memory::ASSUME_POOL_QINDEX)) {
          logE("vkAcquireNextImageKHR: OUT_OF_DATE, but onResized failed\n");
          return 1;
        }
        continue;
      } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
        // VK_ERROR_SURFACE_LOST_KHR can be recovered by rebuilding it.
        continue;
      }
      logE("%s failed: %d (%s)\n", "vkAcquireNextImageKHR", result,
           string_VkResult(result));
      return 1;
    }

    command::SubmitInfo info;
    info.waitFor.emplace_back(imageAvailableSemaphore,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    info.toSignal.emplace_back(renderSemaphore.vk);
    {
      command::CommandPool::lock_guard_t lock(simple.cpool.lockmutex);
      if (simple.cmdBuffers.at(nextImage).enqueue(lock, info) ||
          simple.cpool.submit(lock, memory::ASSUME_POOL_QINDEX, {info})) {
        return 1;
      }
    }

    if (dev.framebufs.at(nextImage).dirty) {
      logW("framebuf[%u] dirty and has not been rebuilt before present\n",
           nextImage);
    }
    VkSemaphore semaphores[] = {renderSemaphore.vk};
    VkSwapchainKHR swapChains[] = {dev.swapChain};

    VkPresentInfoKHR presentInfo;
    memset(&presentInfo, 0, sizeof(presentInfo));
    presentInfo.sType = autoSType(presentInfo);
    presentInfo.waitSemaphoreCount = sizeof(semaphores) / sizeof(semaphores[0]);
    presentInfo.pWaitSemaphores = semaphores;
    presentInfo.swapchainCount = sizeof(swapChains) / sizeof(swapChains[0]);
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &nextImage;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (simple.onResized(dev.swapChainInfo.imageExtent,
                             memory::ASSUME_POOL_QINDEX)) {
          logE("present: OUT_OF_DATE, but onResized failed\n");
          return 1;
        }
      } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
        // VK_ERROR_SURFACE_LOST_KHR can be recovered by rebuilding it.
      } else {
        logE("%s failed: %d (%s)\n", "vkQueuePresentKHR", result,
             string_VkResult(result));
        return 1;
      }
      continue;
    }
    // vkQueueWaitIdle() cleans up resource leaks from validation layers.
    if ((simple.frameCount % 64) == 63) {
      result = vkQueueWaitIdle(presentQueue);
      if (result != VK_SUCCESS) {
        logE("%s failed: %d (%s)\n", "vkQueueWaitIdle", result,
             string_VkResult(result));
        return 1;
      }
    }
    simple.frameCount++;

    auto name1 = imageAvailableSemaphore.getName();
    if (name1 != "imageAvailableSemaphore") {
      logE("%s name \"%s\" want \"%s\"\n", "imageAvailableSemaphore",
           name1.c_str(), "imageAvailableSemaphore");
      return 1;
    }
    auto name2 = renderSemaphore.getName();
    if (name2 != "renderSemaphore") {
      logE("%s name \"%s\" want \"%s\"\n", "renderSemaphore", name2.c_str(),
           "renderSemaphore");
      return 1;
    }
  }

  VkResult v = vkDeviceWaitIdle(dev.dev);
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
  for (unsigned int i = 0; i < extensionSize; i++) {
    inst.requiredExtensions.push_back(extensions[i]);
  }
  if (inst.ctorError(createWindowSurface, window)) {
    return 1;
  }
  if (inst.open(size)) {
    return 1;
  }
  if (!inst.devs.size()) {
    logE("BUG: no devices created\n");
    return 1;
  }
  auto simple = std::unique_ptr<SimplePipeline>(new SimplePipeline(inst));
  return mainLoop(window, *simple);
}

int runGLFW() {
  if (!strcmp(imgFilename, "--auto")) {
    // Enable automated testing
    imgFilename = "test/logo101.png";
    automatedTest = true;
  }
  if (!glfwInit()) {
    logE("glfwInit failed. Windowing system probably disabled.\n");
    return 1;
  }
  glfwSetErrorCallback([](int code, const char* msg) -> void {
    logE("glfw error %x: %s\n", code, msg);
  });

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  VkExtent2D size{800, 600};
  GLFWwindow* window = glfwCreateWindow(
      size.width, size.height, "Vulkan window",
      nullptr /*monitor for fullscreen*/, nullptr /*context object sharing*/);
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
    fprintf(stderr, "usage: %s [ filename | --auto ]\n", argv[0]);
    return 1;
  }
  imgFilename = argv[1];
  return runGLFW();
}
#endif
