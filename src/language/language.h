/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
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
 * Note: src/language attempts to avoid needing a "whole app INI or config
 *       library" to avoid adding unnecessary complexity.
 *
 * For example, the following code uses src/language to create a vulkan window:
 *
 * // this file is yourprogram.cpp:
 * #define GLFW_INCLUDE_VULKAN
 * #include <GLFW/glfw3.h>
 *
 * #ifdef _WIN32
 * #define GLFW_EXPOSE_NATIVE_WIN32
 * #include <GLFW/glfw3native.h>
 * #endif
 *
 * #include <src/language/language.h>
 *
 * // Wrap the function glfwCreateWindowSurface for Instance::ctorError():
 * static VkResult createWindowSurface(language::Instance& inst, void* window)
 * {
 *   return glfwCreateWindowSurface(inst.vk, (GLFWwindow*) window, nullptr,
 *                                  &inst.surface);
 * }
 *
 * int main() {
 *   glfwInit();
 *   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
 *   const int WIDTH = 800, HEIGHT = 600;
 *   GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT...);
 *   unsigned int glfwExtCount = 0;
 *   const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
 *   language::Instance inst;
 *   for (unsigned int i = 0; i < glfwExtCount; i++) {
 *     inst.requiredExtensions.push_back(glfwExts[i]);
 *   }
 *   if (inst.ctorError(createWindowSurface, window)) {
 *     return 1;
 *   }
 *   if (inst.open({WIDTH, HEIGHT})) {
 *     return 1;
 *   }
 *   while(!glfwWindowShouldClose(window)) {
 *     glfwPollEvents();  // This is the main loop.
 *   }
 *   glfwDestroyWindow(window);
 *   glfwTerminate();
 *   return r;
 * }
 */

#include <src/core/structs.h>

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#pragma once

// Forward decliaration of VmaAllocator for Device and memory.h.
// Only used if vulkanmemoryallocator is enabled in memory.h; otherwise does
// nothing.
VK_DEFINE_HANDLE(VmaAllocator);

namespace command {
// Forward declaration of CommandPool for resetSwapChain().
class CommandPool;
// Forward declaration of Pipeline for Device.
typedef struct Pipeline Pipeline;
}  // namespace command
namespace memory {
// Forward declaration of Image for Device::depthImage.
typedef struct Image Image;
}  // namespace memory

namespace language {

// getVkPtrVkDevice allows the Device struct to not be fully defined yet, but
// VkDebugPtr<> can get a VkPtr<VkDevice> out of it.
//
// You should not need to call this. This is only here for VkDebugPtr<> to use.
VkPtr<VkDevice>& getVkPtrVkDevice(Device& dev);

// setObjectName is the implementation of vkDebugMarkerSetObjectNameEXT with
// fallback to VK_EXT_debug_report + VK_EXT_debug_marker.
int setObjectName(Device& dev, uintptr_t handle, VkObjectType objectType,
                  const char* name);

// VkDebugPtr<> wraps VkPtr<> but only supports the form that takes a VkDevice
// because it adds VK_EXT_debug_utils support which requires the VkDevice.
// This means it works for all Vulkan Object Handles except VkInstance and
// VkDevice, since all the other handles have a destroy_fn that takes a
// VkDevice.
//
// It also supports fallback to VK_EXT_debug_report + VK_EXT_debug_marker -
// see the setObjectName() function.
template <typename T>
class VkDebugPtr : public VkPtr<T> {
 public:
  Device& dev;

  VkDebugPtr() = delete;
  VkDebugPtr(VkDebugPtr&& other)
      : VkPtr<T>(std::move(other)), dev(other.dev), name(other.name) {}
  VkDebugPtr(const VkDebugPtr&) = delete;

  // Constructor that has a VkPtr<T>::destroy_fn which takes 3 arguments:
  // a language::Device, the obj, and the allocator. Note that the
  // language::Device must have a non-null dev VkPtr.
  explicit VkDebugPtr(Device& dev,
                      VKAPI_ATTR void(VKAPI_CALL* destroy_fn)(
                          VkDevice, T, const VkAllocationCallbacks*))
      : VkPtr<T>(getVkPtrVkDevice(dev), destroy_fn), dev(dev) {}

  virtual ~VkDebugPtr() { VkPtr<T>::reset(); }

  // Hide VkPtr<T>::reset() because calling the right overload of reset() can
  // easily cause hard-to-find bugs.
  void reset() { VkPtr<T>::reset(getVkPtrVkDevice(dev)); }

  // setName calls vkSetDebugUtilsObjectNameEXT or
  // vkDebugMarkerSetObjectNameEXT.
  // setName(""), setting the empty string, removes the name from the object.
  int setName(const std::string& name) {
    this->name = name;
    if (!VkPtr<T>::object) {
      return 0;
    }
    return setObjectName(dev, VOLCANO_CAST_UINTPTR(VkPtr<T>::object),
                         getObjectType(VkPtr<T>::object), name.c_str());
  }

  // getName returns the stored name. This does not mean that
  // vkSetDebugUtilsObjectNameEXT has been called.
  const std::string& getName() const { return name; }

  // onCreate must be called when vkCreateFoo() succeeds, because setName()
  // can be called before the object receives a non-null value.
  int onCreate() {
    // onCreate could also implement an observer pattern if desired.
    if (VkPtr<T>::object) {
      return setObjectName(dev, VOLCANO_CAST_UINTPTR(VkPtr<T>::object),
                           getObjectType(VkPtr<T>::object), name.c_str());
    }
    return 0;
  }

 protected:
  // name is automatically set using VkDebugUtilsObjectNameInfoEXT
  // (or fallback to vkDebugMarkerSetObjectNameEXT())
  std::string name;
};

// ImageView wraps VkImageView. A VkImageView is required when using a VkImage
// to enable subresources within a single VkImage. Vulkan makes subresources and
// aliasing (two VkImageViews that overlap) possible by making the ImageView
// explicit.
//
// ImageView is set up automatically by Device. Feel free to stop reading and
// skip to the Device definition now.
typedef struct ImageView {
  ImageView(Device& dev);  // ctor is in imageview.cpp for Device definition.
  ImageView(ImageView&&) = default;
  ImageView(const ImageView&) = delete;

  // ctorError() must be called with a valid VkImage for this to refer to.
  // Your application may customize this->info before calling ctorError().
  WARN_UNUSED_RESULT int ctorError(VkImage image, VkFormat format);

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  VkImageViewCreateInfo info;
  VkDebugPtr<VkImageView> vk;
} ImageView;

// Framebuf is the pixels that the RenderPass will draw to, typically the
// screen pixels in the application window.
//
// Although you do not need to use it directly, its lifecycle is:
// 1. vector<Framebuf> framebufs are created in Instance::ctorError().
// 2. Your application can customize Framebuf before calling
//    RenderPass::ctorError(), such as RenderPass::setTargetImage().
// 3. When you call Framebuf::ctorError(), image and attachments are used to
//    create vk.
// 4. Later, your application may call Device::resetSwapChain() again if the
//    swapChain extent needs to be resized. This destroys and recreates
//    Framebuf and Framebuf::attachments with the new size.
typedef struct Framebuf {
  Framebuf(Device& dev);  // ctor is in imageview.cpp for Device definition.
  Framebuf(Framebuf&&) = default;
  Framebuf(const Framebuf&) = delete;

  // ctorError() creates the VkFramebuffer, typically called from
  // Pipeline::init() in <src/command/command.h>.
  WARN_UNUSED_RESULT int ctorError(VkRenderPass renderPass, uint32_t width,
                                   uint32_t height);

  // image.at(0) is overwritten with one VkImage from Device::swapChain in
  // resetSwapChain(). Your application should immediately replace image.at(0)
  // after resetSwapChain() returns if this is being overridden.
  // Note: these VkImage are not automatically cleaned up with VkDestroyImage
  // because images from vkGetSwapchainImagesKHR do not need to be destroyed.
  std::vector<VkImage> image;

  // attachments must all have ImageView::info.subresourceRange.layerCount be
  // identical. resetSwapChain() overwrites attachments.at(0) with one ImageView
  // pointing to image.at(0).
  std::vector<ImageView> attachments;

  // setName forwards the setName call to vk.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    return vk.setName(name);
  }
  // getName forwards the getName call to vk.
  const std::string& getName() const { return vk.getName(); }

  // vk is the vulkan VkFrameBuffer object. resetSwapChain() overwrites it.
  VkDebugPtr<VkFramebuffer> vk;

  // depthImageViewAt1 indicates whether attachments.at(1) is the depth buffer.
  // This prevents resetSwapChain() from getting confused and mistaking an
  // application attachment in attachments.at(1) for the depthImageView.
  bool depthImageViewAt1{false};

  // markDirty will mark this Framebuf as invalid, needing to be destroyed.
  // CommandPool::updateBuffersAndPass() calls markDirty(). Possibly others.
  void markDirty() { dirty = true; }

  bool dirty{false};
} Framebuf;

// Forward declaration of Instance for Device and InstanceExtensionChooser.
class Instance;

// Device is explicitly used almost everywhere. A Device is created after the
// Vulkan driver decides you have hardware that can support Vulkan. Device has
// lots of members (physProp, enabledFeatures, memProps, ...) to tell you what
// exactly the device supports.
//
// Device wraps both the Vulkan logical and physical devices.
//
// Take care to observe the notes about Device::swapChainInfo below.
//
// Device has a qfams list of what is supported by the physical devices.
// Instance::initQueues() chooses which queues to actually use.
typedef struct Device {
  Device(language::Instance& inst, VkPhysicalDevice phys);
  Device(Device&&) = default;
  Device(const Device&) = delete;
  virtual ~Device();

  // Logical Device. Populated only after open().
  VkPtr<VkDevice> dev{vkDestroyDevice};

  // Physical Device.
  const VkPhysicalDevice phys;

  // physProp is properties like device name. Populated after ctorError().
  PhysicalDeviceProperties physProp;

  // Features, like samplerAnisotropy. Populated after ctorError().
  DeviceFeatures availableFeatures;

  // Features, like samplerAnisotropy. Populated after open().
  //
  // Note: your app should set the bits it wants enabled before calling open().
  // After open(), your app should check that the feature bit is still set
  // (meaning that the request was successful).
  DeviceFeatures enabledFeatures;

  // Memory properties like memory type. Populated after ctorError().
  DeviceMemoryProperties memProps;

  // Device extensions to choose from. Populated after ctorError().
  std::vector<VkExtensionProperties> availableExtensions;

  // qfams is populated after ctorError() but qfams.queue is populated
  // only after open().
  std::vector<QueueFamilyProperties> qfams;

  // getQfamI() is a convenience method to get the queue family index that
  // supports the given SurfaceSupport. Returns (size_t) -1 on error.
  size_t getQfamI(SurfaceSupport support) const;

  // Request device extensions by adding to requiredExtensions before open().
  // After open() this is the list of active device extensions.
  // NOTE: a few extensions are added automatically (supported_queues.cpp):
  //       * VK_KHR_SWAPCHAIN_EXTENSION_NAME
  //       * VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
  // NOTE: If an extension is present after open(), all its function pointers
  //       are non-null in Device::fp.
  std::vector<std::string> requiredExtensions;

  DeviceFunctionPointers fp;

  // surfaceFormats is populated by Instance as soon as the Device is created.
  std::vector<VkSurfaceFormatKHR> surfaceFormats;

  // presetModes is populated by Instance as soon as the Device is created.
  std::vector<VkPresentModeKHR> presentModes;

  // swapChainInfo is populated by the Device constructor. Your application can
  // customize all but the fields below, then call onResized() which calls
  // resetSwapChain, which consumes swapChainInfo.
  //
  // (swapChainInfo.{imageFormat,imageColorSpace,presentMode} are populated
  // by Instance::ctorError if it is given a valid VkSurfaceKHR)
  //
  // Changes to these will be ignored:
  //   swapChainInfo.{minImageCount,preTransform,oldSwapchain,imageSharingMode},
  //   swapChainInfo.{queueFamilyIndexCount,pQueueFamilyIndices,surface}.
  VkSwapchainCreateInfoKHR swapChainInfo;

  // aspectRatio is a convenience method to compute the aspect ratio of the
  // swapChain.
  float aspectRatio() const {
    return swapChainInfo.imageExtent.width /
           (float)swapChainInfo.imageExtent.height;
  }

  // chooseFormat is a convenience method to select the first matching format
  // that has the given tiling and feature flags.
  // If no format meets the criteria, VK_FORMAT_UNDEFINED is returned.
  // chooseFormat() checks both VkFormatProperties and VkImageFormatProperties.
  WARN_UNUSED_RESULT VkFormat chooseFormat(VkImageTiling tiling,
                                           VkFormatFeatureFlags flags,
                                           VkImageType imageType,
                                           const std::vector<VkFormat>& fmts);

  // isExtensionAvailable is a convenience method to check if a string is in
  // the availableExtensions vector. The string must match exactly.
  int isExtensionAvailable(const char* name);

  // isExtensionLoaded is a convenience method to check if a string is in
  // the requiredExtensions vector OR was loaded in
  // Instance::requiredExtensions.
  int isExtensionLoaded(const char* name);

  // getSurfaceCapabilities is a convenience method for retrieving the physical
  // device surface info from vkGetPhysicalDeviceSurfaceCapabilitiesKHR.
  WARN_UNUSED_RESULT VkResult
  getSurfaceCapabilities(VkSurfaceCapabilitiesKHR& scap) {
    return vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, getSurface(), &scap);
  }

  // valid after resetSwapChain(), typically via onResized()
  VkDebugPtr<VkSwapchainKHR> swapChain{*this, vkDestroySwapchainKHR};
  // framebufs is populated after resetSwapChain().
  std::vector<Framebuf> framebufs;

  // Only used if memory.h enables vulkanmemoryallocator.
  VmaAllocator vmaAllocator{VK_NULL_HANDLE};
  std::recursive_mutex lockmutex;

  // resetSwapChain() re-initializes swapChain with the updated
  // swapChainInfo.imageExtent that should have just been populated by
  // onResized. It also rewrites framebufs to match.
  //
  // The CommandPool and poolQindex arguments specify a queue that is used to
  // re-setup any buffers in the new swapChain.
  WARN_UNUSED_RESULT int resetSwapChain(command::CommandPool& cpool,
                                        size_t poolQindex);

  // initSurfaceFormatAndPresentMode initializes surfaceFormats and
  // presentModes. This is called by Instance::ctorError as soon as the Device
  // is created.
  WARN_UNUSED_RESULT virtual int initSurfaceFormatAndPresentMode();

  // GetDepthFormat can be used to detect if addDepthImage() was ever called.
  VkFormat GetDepthFormat() const { return depthFormat; };

  // setFrameNumber is required by vulkanmemoryallocator if the CAN_BECOME_LOST
  // feature is used. In order to keep it simple, just pass in the frameNumber
  // each frame regardless. If you use
  // science::CommandPoolContainer::acquireNextImage(), it calls
  // Device::setFrameNumber() for you.
  void setFrameNumber(uint32_t frameNumber);

  // apiVersionInUse reports the lowest apiVersion from instance and all devs.
  uint32_t apiVersionInUse() const;

  // apiUsage will logW(fmt) if pred is true and the API is not supported.
  void apiUsage(int v1, int v2, int v3, bool pred, const char* fmt, ...)
      VOLCANO_PRINTF(6, 7);

  // extensionUsage will logW(fmt) if pred is true and the extension is not
  // loaded (this checks device AND instance extensions).
  void extensionUsage(const char* name, bool pred, const char* fmt, ...)
      VOLCANO_PRINTF(4, 5);

  // getInstanceProcAddr() is a convenience for vkGetInstanceProcAddr.
  PFN_vkVoidFunction getInstanceProcAddr(const char* funcName);

  VkSurfaceKHR getSurface();

  // setName calls setObjectName for the Device.
  WARN_UNUSED_RESULT int setName(const std::string& name) {
    this->name = name;
    if (!dev) {
      return 0;
    }
    return setObjectName(*this, VOLCANO_CAST_UINTPTR((VkDevice)dev),
                         getObjectType((VkDevice)dev), name.c_str());
  }

  // getName returns the Device name.
  const std::string& getName() const { return name; }

  // setInstanceName would be to setObjectName for the inst.
  // FIXME: This causes a segfault on NVidia Linux.
  // WARN_UNUSED_RESULT int setInstanceName(const std::string& name);

  // setSurfaceName calls setObjectName for the inst->surface.
  WARN_UNUSED_RESULT int setSurfaceName(const std::string& name);

 protected:
  friend struct command::Pipeline;
  friend class Instance;

  // name is automatically set using VkDebugUtilsObjectNameInfoEXT
  // (or fallback to vkDebugMarkerSetObjectNameEXT())
  std::string name;

  // depthFormat is set by Pipeline::addDepthImage() to communicate with
  // resetSwapChain() and addOrUpdateFramebufs().
  VkFormat depthFormat{VK_FORMAT_UNDEFINED};

  // depthImage is set in addOrUpdateFramebufs(). One Image is used among all
  // framebufs without any concurrency issues.
  memory::Image* depthImage{nullptr};

  language::Instance* inst;

  // addOrUpdateFramebufs updates framebufs preserving existing FrameBuf
  // elements and adding new ones.
  //
  // addOrUpdateFramebufs is defined separately from resetSwapChain because it
  // depends on src/memory/memory.h, so the code is found in
  // src/memory/memory.cpp.
  int addOrUpdateFramebufs(std::vector<VkImage>& images,
                           command::CommandPool& cpool, size_t poolQindex);
} Device;

// QueueRequest communicates the physical device within Instance, and the
// queue family within the device -- a request by initQueues() (below).
// initQueues() returns one QueueRequest instance per queue, so if requesting
// two identical queues of the same queue family, that would be two
// QueueRequest instances.
typedef struct QueueRequest {
  size_t dev_index;
  size_t dev_qfam_index;
  float priority;

  // The default priority is the lowest possible (0.0), but can be changed.
  // Many GPUs only have the minimum
  // VkPhysicalDeviceLimits.discreteQueuePriorities, which is 2: 0.0 and 1.0.
  QueueRequest(size_t dev_i, size_t dev_qfam_i) {
    dev_index = dev_i;
    dev_qfam_index = dev_qfam_i;
    priority = 0.0;
  }
  virtual ~QueueRequest();
} QueueRequest;

// InstanceExtensionChooser enumerates available extensions and chooses the
// extensions to submit during Instance::ctorError().
//
// The InstanceExtensionChooser is not exposed for your app to customize yet.
// Typical apps can just add strings to Instance::requiredExtensions.
typedef struct InstanceExtensionChooser {
  // Construct an InstanceExtensionChooser and initialize with the list of
  // required extensions.
  //
  // This also enumerates instance layers because that affects the logic.
  InstanceExtensionChooser(Instance& inst);
  virtual ~InstanceExtensionChooser();

  // choose generates the chosen vector using the extension names supplied in
  // required. Since the instance is not yet created, it is not possible to
  // know the instance version, but the available instance extensions can give
  // a hint what is supported.
  WARN_UNUSED_RESULT virtual int choose();

  std::vector<std::string> required;
  std::vector<std::string> chosen;
  std::vector<VkLayerProperties> instanceLayers;
  bool is_EXT_debug_utils_available;
} InstanceExtensionChooser;

// Instance is the root class for your application's Vulkan access.
// Constructor (ctor) is a 3-phase process:
// 1. Create an Instance object.
//    Optionally customize applicationInfo.
// 2. Call ctorError()
//    *** Always check the error return ***
//    ctorError() calls your CreateWindowSurfaceFn to create a VkSurfaceKHR
//    (windowing library-specific code, up to you to choose how to implement)
// 3. Optionally choose the number and type of queues (queue requests are how
//    queues are created, and a device with no queues is considered ignored).
//    Choose surface formats, extensions, or present mode. To complete step 3,
//    call open(), which finishes setting up Vulkan:
//    surfaces, queues, and a swapChain.
//
// Afterward, look at src/command/command.h to start displaying things.
//
// * Why so many steps?
//
// Vulkan is pretty verbose. This class reduces the boilerplate A LOT to get up
// and running. The Instance ctor sets the defaults. Then ctorError() actually
// creates the instance, populating as much as possible. However, info your
// application *needs* to start up is not available until ctorError() returns
// and your application can inspect Instance::devs. Then, open() receives final
// swapChain extent dimensions and any other settings and sets up the swapChain.
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
// And a multi-GPU system could in theory have multiple GRAPHICS queues.
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

  // ctorError is step 2 of the ctor (see class comments above).
  // Vulkan errors are returned from ctorError().
  //
  // Before calling ctorError, set requiredExtensions to the names of any
  // required extensions (e.g. glfwGetRequiredInstanceExtensions for glfw, or
  // SDL_Vulkan_GetInstanceExtensions for SDL).
  //
  // createWindowSurface is a callback which gets called when Instance::surface
  // must be created. It is called exactly once inside ctorError and not
  // retained after.
  //
  // window is an opaque pointer used only in the call to
  // createWindowSurface.
  WARN_UNUSED_RESULT int ctorError(CreateWindowSurfaceFn createWindowSurface,
                                   void* window);

  // open() is step 3 of the ctor. Call open() after modifying
  // Device::requiredExtensions, Device::surfaceFormats, or
  // Device::presentModes.
  //
  // surfaceSizeRequest is the initial size of the window.
  WARN_UNUSED_RESULT int open(VkExtent2D surfaceSizeRequest);

  virtual ~Instance();

  // devs holds all Device instances.
  std::vector<std::shared_ptr<Device>> devs;

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

  // debugUtils flags can be customized before calling ctorError.
  // NOTE: unless Vulkan 1.1 is supported, initDebugUtilsOrDebugReport() sets
  // up VK_EXT_debug_report.
  VkDebugUtilsMessengerCreateInfoEXT debugUtils;

  // pDestroyDebugReportCallbackEXT is loaded from the vulkan library in
  // ctorError (i.e. a .dll / .so function symbol lookup).
  PFN_vkDestroyDebugReportCallbackEXT pDestroyDebugReportCallbackEXT = nullptr;
  // pDestroyDebugUtilsMessengerEXT is loaded from the vulkan library in
  // ctorError.
  PFN_vkDestroyDebugUtilsMessengerEXT pDestroyDebugUtilsMessengerEXT = nullptr;
  // pSubmitDebugUtilsMessageEXT is loaded from the vulkan library in ctorError.
  PFN_vkSubmitDebugUtilsMessageEXT pSubmitDebugUtilsMessageEXT = nullptr;

  // debugReport unfortunately can only be used after a symbol lookup. This
  // duplication of the "loader" functionality cannot be avoided.
  VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;

  // messenger unfortunately can only be used after a symbol lookup. This
  // duplication of the "loader" functionality cannot be avoided.
  // messenger is initialized in ctorError using debugUtils.
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;

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

  // enabledLayers should have all required instance layer names added before
  // calling ctorError(); after it returns, check enabledLayers for what layers
  // were successfully enabled.
  std::set<std::string> enabledLayers;

  // apiVersion is a convenience method to get applicationInfo.apiVersion.
  uint32_t apiVersion() const { return applicationInfo.apiVersion; }

  // apiVersionInUse reports the lowest apiVersion from instance and all devs.
  uint32_t apiVersionInUse() const { return detectedApiVersionInUse; }

  // minApiVersion can be set to VK_MAKE_VERSION(1, 1, 0) or something higher
  // which will exclude any devices with physProp.apiVersion lower than that.
  // The value 0 will use the autodetected apiVersion.
  uint32_t minApiVersion{0};

  // requiredExtensions should be filled by your app before calling ctorError.
  std::vector<std::string> requiredExtensions;

  virtual void debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                     VkDebugUtilsMessageTypeFlagsEXT types,
                     const VkDebugUtilsMessengerCallbackDataEXT* data);

 protected:
  // initDebugUtilsOrDebugReport sets up debug callbacks.
  WARN_UNUSED_RESULT int initDebugUtilsOrDebugReport(
      bool& is_EXT_debug_utils_available);

  // debugUtilsCallback receives the raw debug callback, and unwraps pUserData.
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT types,
      const VkDebugUtilsMessengerCallbackDataEXT* data, void* pUserData);

  // Override initQueues() if your app needs more than one queue.
  // initQueues() should call request.push_back() for at least one
  // QueueRequest.
  //
  // initQueues() populates a vector<QueueRequest> used to create Device::dev
  // (the logical device). open() then populates Device::qfams.queues.
  WARN_UNUSED_RESULT virtual int initQueues(std::vector<QueueRequest>& request);

  // createDevices chooses a Physical Device and creates a Logical Device.
  WARN_UNUSED_RESULT int createDevices(std::vector<VkPhysicalDevice>& physDevs);

  // Override initSupportedQueues() if your app needs to customize surface
  // format or present mode logic beyond what this supports.
  //
  // initSupportedQueues() prepares Device dev: extensions, surface format, and
  // present mode.
  WARN_UNUSED_RESULT virtual VkResult initSupportedQueues(Device& dev);

  // insideVkCreateDevice is useful in "VK_EXT_debug_utils".
  bool insideVkCreateDevice{false};
  uint32_t detectedApiVersionInUse{0};
};

}  // namespace language
