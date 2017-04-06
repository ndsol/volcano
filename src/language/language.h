/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 *
 * src/language is the first-level language bindings for Vulkan.
 * src/language is part of github.com/ndsol/volcano.
 * This library is called "language" as a homage to Star Trek First Contact.
 * Hopefully this "language" is easier than Vulkan.
 *
 * Vulkan is verbose -- but that is a good thing. src/language handles the
 * initialization of the VkInstance, devices, queue selection, and extension
 * selection.
 *
 * Note: src/language attempts to avoid needing a "whole app INI or config"
 *       solution. This avoids pulling in an unnecessary dependency.
 * TODO: Centralize logging instead of sprinkling fprintf(stderr) throughout.
 *
 * Example program:
 *
 * // this file is yourprogram.cpp:
 * #include <src/language/language.h>
 *
 * // Wrap the function glfwCreateWindowSurface for Instance::ctorError():
 * static VkResult createWindowSurface(language::Instance& inst, void * window)
 * {
 *   return glfwCreateWindowSurface(inst.vk, (GLFWwindow *) window, nullptr,
 *     &inst.surface);
 * }
 *
 * int main() {
 *   const int WIDTH = 800, HEIGHT = 600;
 *   GLFWwindow * window = glfwCreateWindow(WIDTH, HEIGHT...);
 *   unsigned int glfwExtCount = 0;
 *   const char ** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
 *   language::Instance inst;
 *   if (inst.ctorError(glfwExts, glfwExtCount, createWindowSurface, window)) {
 *     return 1;
 *   }
 *   int r = inst.open({WIDTH, HEIGHT});
 *   if (r != 0) exit(1);
 *   while(!glfwWindowShouldClose(window)) {
 *     glfwPollEvents();
 *   }
 *   glfwDestroyWindow(window);
 *   return r;
 * }
 */

#include <vulkan/vulkan.h>
#include <set>
#include <string>
#include <vector>
#include "VkPtr.h"

#pragma once

namespace language {

#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(COMPILER_MSVC)
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

// For debugging src/language: set language::dbg_lvl to higher values to log
// more detailed debug info.
extern int dbg_lvl;
extern const char VK_LAYER_LUNARG_standard_validation[];

// Forward declaration of Device for ImageView and Framebuffer.
struct Device;

// ImageView wraps VkImageView. A VkImageView is to a VkImage as a
// VkFramebuffer is to the screen: a VkImageView accesses the memory in the
// VkImage using a specific set of format parameters.
//
// ImageView is set up automatically by Device. Feel free to skip to Device now.
typedef struct ImageView {
  ImageView(Device& dev);  // ctor is in imageview.cpp for Device definition.
  ImageView(ImageView&&) = default;
  ImageView(const ImageView&) = delete;

  // ctorError() must be called with a valid VkImage for this to reference.
  // Your application may customize this->info before calling ctorError().
  WARN_UNUSED_RESULT int ctorError(Device& dev, VkImage image, VkFormat format);

  VkImageViewCreateInfo info;
  VkPtr<VkImageView> vk;
} ImageView;

// Framebuf is the pixels that the RenderPass will draw to, typically the
// screen pixels in the application window.
//
// Although you do not need to use it directly, its lifecycle is:
// 1. vector<Framebuf> framebufs are created in Device as part of
//    Instance::ctorError(). The object contains only zeroes at this point and
//    should not be used.
// 2. Instance::open() calls resetSwapChain() which populates
//    Framebuf::attachments with one VkImageView, Framebuf::imageView0 pointing
//    to Framebuf::image. Framebuf::image is one frame buffer contained in the
//    Device::swapChain, and Device::framebufs holds all the Framebuf objects
//    which make up Device::swapChain.
// 3. Your application can customize Framebuf before calling Pipeline::init().
// 4. Your application should call Pipeline::init(), which calls
//    Framebuf::ctorError().
// 5. Framebuf::ctorError() consumes Framebuf::attachments.
// 6. Later, your application may call resetSwapChain() again if the swapChain
//    extent needs to change. This resets all Framebuf::attachments, so your
//    application should repopulate them immediately after. Typically the
//    attachments setup code goes in a subroutine to be called after
//    Instance::open() and after Device::resetSwapChain().
typedef struct Framebuf {
  Framebuf(Device& dev);  // ctor is in imageview.cpp for Device definition.
  Framebuf(Framebuf&&) = default;
  Framebuf(const Framebuf&) = delete;

  // ctorError() creates the VkFramebuffer, typically called from
  // Pipeline::init() in <src/command/command.h>.
  WARN_UNUSED_RESULT int ctorError(Device& dev, VkRenderPass renderPass,
                                   VkExtent2D size);

  // Image from Device::swapChain. VkImage has no VkDestroyImage function.
  VkImage image;
  ImageView imageView0;
  // attachments must be created with identical
  // ImageView::info.subresourceRange.layerCount.
  std::vector<VkImageView> attachments;

  VkPtr<VkFramebuffer> vk;
} Framebuf;

// SurfaceSupport encodes the result of vkGetPhysicalDeviceSurfaceSupportKHR().
// As an exception, the GRAPHICS value is used to request a QueueFamily
// with vk.queueFlags & VK_QUEUE_GRAPHICS_BIT in Instance::requestQfams() and
// Device::getQfamI().
// TODO: add COMPUTE.
enum SurfaceSupport {
  UNDEFINED = 0,
  NONE = 1,
  PRESENT = 2,

  GRAPHICS = 0x1000,  // Not used in struct QueueFamily.
};

// QueueFamily wraps VkQueueFamilyProperties. QueueFamily also gives whether the
// QueueFamily can be used to "present" on the app surface (i.e. swap a surface
// to the screen if surfaceSupport == PRESENT).
// SurfaceSupport is the result of vkGetPhysicalDeviceSurfaceSupportKHR().
typedef struct QueueFamily {
  QueueFamily(const VkQueueFamilyProperties& vk, SurfaceSupport surfaceSupport)
      : vk(vk), surfaceSupport(surfaceSupport) {}
  QueueFamily(QueueFamily&&) = default;
  QueueFamily(const QueueFamily&) = delete;

  VkQueueFamilyProperties vk;
  const SurfaceSupport surfaceSupport;
  inline bool isGraphics() const {
    return vk.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }

  // Populated only after open().
  std::vector<float> prios;
  // Populated only after open().
  std::vector<VkQueue> queues;
} QueueFamily;

// Device wraps the Vulkan logical and physical devices and a list of
// QueueFamily supported by the physical device. When initQueues() is called,
// Instance::devs are populated with phys and qfams, but Device::dev (the
// logical device) and Device::qfams.queues are not populated.
//
// See Instance::initQueues() which populates Device::qfams during open().
typedef struct Device {
  Device(VkSurfaceKHR surface);
  Device(Device&&) = default;
  Device(const Device&) = delete;
  virtual ~Device();

  // Logical Device. Populated only after open().
  VkPtr<VkDevice> dev{vkDestroyDevice};

  // Physical Device. Populated after ctorError().
  VkPhysicalDevice phys = VK_NULL_HANDLE;

  // Properties, like device name. Populated after ctorError().
  VkPhysicalDeviceProperties physProp;

  // Memory properties like memory type. Populated after ctorError().
  VkPhysicalDeviceMemoryProperties memProps;

  // Device extensions to choose from. Populated after ctorError().
  std::vector<VkExtensionProperties> availableExtensions;

  // qfams is populated after ctorError() but qfams.queue is populated
  // only after open().
  std::vector<QueueFamily> qfams;
  // getQfamI() is a convenience method to get the queue family index that
  // supports the given SurfaceSupport. Returns (size_t) -1 on error.
  size_t getQfamI(SurfaceSupport support) const;

  // Request device extensions by adding to extensionRequests before open().
  std::vector<const char*> extensionRequests;

  // surfaceFormats is populated by Instance as soon as the Device is created.
  std::vector<VkSurfaceFormatKHR> surfaceFormats;
  // presetModes is populated by Instance as soon as the Device is created.
  std::vector<VkPresentModeKHR> presentModes;
  // swapChainInfo is populated by the Device constructor. Your application can
  // customize all but the fields below, then call open() which calls
  // resetSwapChain, which consumes swapChainInfo.
  //
  // (swapChainInfo.{imageFormat,imageColorSpace,presentMode} are populated
  // by Instance::ctorError if it is given a valid VkSurfaceKHR)
  //
  // Do not modify: swapChainInfo.surface (set by the Device constructor)
  //
  // Changes to these will be ignored:
  //   swapChainInfo.{minImageCount,preTransform,oldSwapchain,imageSharingMode},
  //   swapChainInfo.{queueFamilyIndexCount,pQueueFamilyIndices}.
  VkSwapchainCreateInfoKHR swapChainInfo;

  // aspectRatio is a convenience method to compute the aspect ratio of the
  // swapChain.
  float aspectRatio() const {
    return swapChainInfo.imageExtent.width /
           (float)swapChainInfo.imageExtent.height;
  }

  // formatProperties is a convenience wrapper around
  // vkGetPhysicalDeviceFormatProperties.
  WARN_UNUSED_RESULT VkFormatProperties formatProperties(VkFormat format) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(phys, format, &props);
    return props;
  }

  // chooseFormat is a convenience method to select the first matching format
  // that has the given tiling and feature flags.
  // If no format meets the criteria, VK_FORMAT_UNDEFINED is returned.
  WARN_UNUSED_RESULT VkFormat
  chooseFormat(VkImageTiling tiling, VkFormatFeatureFlags flags,
               const std::vector<VkFormat>& choices);

  // open() calls resetSwapChain() so swapChain is valid after open().
  VkPtr<VkSwapchainKHR> swapChain{dev, vkDestroySwapchainKHR};
  // framebufs is valid after resetSwapChain() and open().
  std::vector<Framebuf> framebufs;

  // resetSwapChain() re-initializes swapChain with the updated
  // swapChainInfo.imageExtent that should have just been populated.
  // This recreates the framebufs vector.
  WARN_UNUSED_RESULT virtual int resetSwapChain();

  // initSurfaceFormatAndPresentMode initializes surfaceFormats and
  // presentModes. This is called by Instance::ctorError as soon as the Device
  // is created.
  WARN_UNUSED_RESULT virtual int initSurfaceFormatAndPresentMode();
} Device;

// QueueRequest communicates the physical device within Instance, and the
// queue family within the device -- a request by initQueues() (below).
// initQueues() returns one QueueRequest instance per queue, so if requesting
// two identical queues of the same queue family, that would be two
// QueueRequest instances.
typedef struct QueueRequest {
  uint32_t dev_index;
  uint32_t dev_qfam_index;
  float priority;

  // The default priority is the lowest possible (0.0), but can be changed.
  // Many GPUs only have the minimum
  // VkPhysicalDeviceLimits.discreteQueuePriorities, which is 2: 0.0 and 1.0.
  QueueRequest(uint32_t dev_i, uint32_t dev_qfam_i) {
    dev_index = dev_i;
    dev_qfam_index = dev_qfam_i;
    priority = 0.0;
  }
  virtual ~QueueRequest();
} QueueRequest;

// InstanceExtensionChooser enumerates available extensions and chooses the
// extensions to submit during Instance::ctorError().
typedef struct InstanceExtensionChooser {
  // Construct an InstanceExtensionChooser.
  InstanceExtensionChooser() {}
  virtual ~InstanceExtensionChooser();

  // choose generates the chosen vector using the extension names populated in
  // required.
  WARN_UNUSED_RESULT virtual int choose();

  std::vector<std::string> required;
  std::vector<std::string> chosen;
} InstanceExtensionChooser;

// Instance holds the root of the Vulkan pipeline. Constructor (ctor) is a
// 3-phase process:
// 1. Create an Instance object (step 1 of the constructor)
//    Optionally customize applicationInfo.
// 2. Call ctorError() (step 2 of the constructor)
//    *** Always check the error return ***
//    ctorError() calls your CreateWindowSurfaceFn to create a VkSurfaceKHR
//    (windowing library-specific code, up to you to choose how to implement)
// 3. Optionally choose the number and type of queues (queue requests are how
//    queues are created, and a device with no queues is considered ignored).
//    Choose surface formats, extensions, or present mode. Finally,
//    call open() (step 3 of the constructor) to finish setting up Vulkan:
//    surfaces, queues, and a swapChain.
//
// Afterward, look at src/command/command.h to display things in the swapChain.
//
// * Why so many steps?
//
// Vulkan is pretty verbose. This is an attempt to reduce the amount of
// boilerplate needed to get up and running. The Instance contructor is just
// the default constructor. Then ctorError() actually creates the instance,
// populating as much as possible without any configuration. Finally, open()
// uses all the optional settings to get to a complete swapChain.
//
// * Some discussion about setting up queues:
//
// In many cases it only makes sense to use 1 CPU thread to submit to GPU
// queues even though the GPU can execute the commands in parallel. What
// happens is the GPU only has a single hardware port and Vulkan is forced to
// multiplex commands to that single port when the app starts using multiple
// queues. In other words, the GPU hardware port may be "single-threaded."
// src/language does not enforce a limit of 1 GRAPHICS queue though: Vulkan
// itself has no such limit.
//
//   Web resources: https://lunarg.com/faqs/command-multi-thread-vulkan/
//                  https://forums.khronos.org/showthread.php/13172
//
// It _is_ a good idea however to use multiple threads to build command queues.
// And a multi-GPU system could in theory have multiple GRAPHICS queues (but
// Vulkan 1.0 does not have a final multi-GPU standard yet:
// https://lunarg.com/faqs/vulkan-multiple-gpus-acceleration/
// but there are extensions being developed:
// https://www.khronos.org/registry/vulkan/specs/1.0-extensions/html/vkspec.html#VK_KHX_device_group
// )
class Instance {
 public:
  Instance();
  Instance(Instance&&) = delete;
  Instance(const Instance&) = delete;

  VkPtr<VkInstance> vk{vkDestroyInstance};
  VkPtr<VkSurfaceKHR> surface{vk, vkDestroySurfaceKHR};

  // CreateWindowSurfaceFn defines the function signature for the
  // function that is called to initialize Instance::surface.
  // (e.g. glfwCreateWindowSurface, vkCreateXcbSurfaceKHR, or
  // SDL_CreateVulkanSurface).
  //
  // window is an opaque pointer used only to call this function.
  typedef VkResult (*CreateWindowSurfaceFn)(Instance& instance, void* window);

  // ctorError is step 2 of the constructor (see class comments above).
  // Vulkan errors are returned from ctorError().
  //
  // requiredExtensions is an array of strings naming any required
  // extensions (e.g. glfwGetRequiredInstanceExtensions for glfw).
  // [The SDL API is not as well defined yet but might be
  // SDL_GetVulkanInstanceExtensions]
  //
  // createWindowSurface is a function that is called to initialize
  // Instance::surface. It is called exactly once inside ctorError and
  // not retained.
  //
  // window is an opaque pointer used only in the call to
  // createWindowSurface.
  WARN_UNUSED_RESULT int ctorError(const char** requiredExtensions,
                                   size_t requiredExtensionCount,
                                   CreateWindowSurfaceFn createWindowSurface,
                                   void* window);

  // open() is step 3 of the constructor. Call open() after modifying
  // Device::extensionRequests, Device::surfaceFormats, or
  // Device::presentModes.
  //
  // surfaceSizeRequest is the initial size of the window.
  WARN_UNUSED_RESULT int open(VkExtent2D surfaceSizeRequest);

  virtual ~Instance();

  size_t devs_size() const { return devs.size(); }
  Device& at(size_t i) { return devs.at(i); }

  // requestQfams() is a convenience function. It selects the minimal
  // list of QueueFamily instances from Device dev_i and returns a
  // vector of QueueRequest that cover the requested support.
  //
  // For example:
  // auto r = dev.requestQfams(dev_i, {language::PRESENT,
  //                                   language::GRAPHICS});
  //
  // After requestQfams() returns, multiple queues can be obtained by
  // adding the QueueRequest multiple times in initQueues().
  std::vector<QueueRequest> requestQfams(size_t dev_i,
                                         std::set<SurfaceSupport> support);

  // pDestroyDebugReportCallbackEXT is loaded from the vulkan library at
  // startup (i.e. a .dll / .so function symbol lookup).
  PFN_vkDestroyDebugReportCallbackEXT pDestroyDebugReportCallbackEXT = nullptr;

  VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;

  // applicationInfo is set to defaults in Instance() and is sent to Vulkan
  // in ctorError(). Customize your application before calling ctorError().
  VkApplicationInfo applicationInfo;
  std::string applicationName;
  std::string engineName;

  // Customize minSurfaceSupport to add or remove elements that your
  // application needs. See initQueues().
  std::set<SurfaceSupport> minSurfaceSupport{language::PRESENT,
                                             language::GRAPHICS};

  // pAllocator defaults to nullptr. Your application can install a custom
  // allocator before calling ctorError().
  VkAllocationCallbacks* pAllocator = nullptr;

 protected:
  // Override initDebug() if your app needs different debug settings.
  WARN_UNUSED_RESULT virtual int initDebug();

  // After devs is initialized in ctorError(), it must not be resized.
  // Any operation on the vector that causes it to reallocate its storage
  // will invalidate references held to the individual Device instances,
  // likely causing a segfault.
  std::vector<Device> devs;

  // Override initQueues() if your app needs more than one queue.
  // initQueues() should call request.push_back() for at least one
  // QueueRequest.
  //
  // initQueues() populates a vector<QueueRequest> used to create Device::dev
  // (the logical device). open() then populates Device::qfams.queues.
  WARN_UNUSED_RESULT virtual int initQueues(std::vector<QueueRequest>& request);

  friend int initSupportedDevices(Instance& inst,
                                  std::vector<VkPhysicalDevice>& physDevs);
};

}  // namespace language
