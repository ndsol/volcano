/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * VkPtr is a container for vkDestroy* functions. vkDestroy* functions come
 * in 3 types:
 * 1. vkDestroyInstance(VkInstance, const VkAllocationCallbacks*)
 *    vkDestroyInstance and vkDestroyDevice are the only functions like this.
 * 2. vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) and
 *    similar functions that take a VkDevice.
 * 3. vkDestroySurfaceKHR(VkInstance, VkSurfaceKHR, const VkAlloca..s*)
 *    and similar functions that take a VkInstance.
 *
 * Put simply, Vulkan leaves objects' life cycle up to you, including keeping
 * track of the instance or device where the object lives.
 *
 * Your app can just #include <src/science/science.h> and this header (and
 * others) will be automatically included. This header has logic to
 * #include <vulkan/vulkan.h> but also handles Android where a different
 * #include is needed.
 *
 * VkPtr "solves" the Vulkan object life cycle by:
 * 1. Destroying the object in the ~VkPtr() destructor.
 * 2. Checking for potential memory leaks by checking that the object is not
 *    null when it is read (dereferenced), and that the object IS null when
 *    it is written (address-of).
 * 3. Adds type safety. If the destroy function takes a device, then your app
 *    must pass in a VkDevice at the same time. Etc. This is further improved
 *    upon with language::VkDebugPtr in src/language/language.h.
 *
 * Probably the main reason for reading this is when you get an error related
 * to correct usage of VkPtr. The VkPtr class is written "long" to avoid too
 * heavy use of templates, optimizing instead for clearer compiler error
 * messages.
 *
 * Declare a VkPtr like this:
 *   VkPtr<VkInstance> instance{vkDestroyInstance};
 *
 * The Vulkan API uses functions named VkCreate* to create objects. They
 * accept a pointer which receives the created object, so VkPtr overloads
 * the & operator and checks for correct usage:
 *   vkCreateInstance(pInstanceCreateInfo, pAllocator, &instance)
 *                            Using the & operator ----^
 *
 * The VkPtr can be used just like the underlying type (it transparently gets
 * type-casted using template<typename T> operator T()).
 *   vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT")
 *                         ^---- cast from VkPtr<VkInstance> to VkInstance
 *
 * When the object needs to be destroyed and recreated "in place," call reset()
 * to destroy the VkPtr and reset it to empty. There are 3 reset() functions,
 * because reset() with 0 arguments also resets the stored VkInstance and
 * VkDevice. Since your app probably doesn't need to call reset() much, the
 * VkDebugPtr class in src/language/language.h makes reset() much simpler. Use
 * that reset(). This class's reset(VkDevice) would look something like:
 *   scci.oldSwapchain = swapChain;
 *   v = vkCreateSwapchainKHR(dev, &scci, dev.allocator, &newSwapChain);
 *   if (v != VK_SUCCESS) { ... }
 *   // This avoids deleting dev.swapChain until after vkCreateSwapchainKHR().
 *   swapChain.reset(dev);          // Delete the old dev.swapChain.
 *   *(&swapChain) = newSwapChain;  // Install the new dev.swapChain.
 *
 * The destructor, ~VkPtr(), just calls reset().
 *
 * Debugging tips:
 * 1. If you have a compiler error on a VkPtr declaration:
 *    Check if the destroy function matches the 3 VkPtr constructors
 *    (for the 3 types of destroy functions Vulkan uses).
 *
 * 2. If your app aborts with a VkPtr error:
 *    Remember to call reset() to make sure the VkPtr is empty before calling
 *    operator &() to set it.
 *
 *    Check if the code is supposed to be destroying/recreating that Vulkan
 *    object - did you mean to do that?
 *
 *    The app can also abort if the VkPtr is empty but an attempt was made to
 *    use it. When T operator() casts it to the underlying type, the attempt to
 *    dereference a VK_NULL_HANDLE will fail.
 *
 *    To find the stack tracke, set a breakpoint and rerun it in a debugger.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <typeinfo>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif

#pragma once

#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(COMPILER_MSVC)
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

// On non-64-bit platforms, Vulkan uses typedefs. Then a standards-conforming
// C++ compiler will complain that:
//   MemoryRequirements(language::Device& dev, VkImage img) and
//   MemoryRequirements(language::Device& dev, VkBuffer buf)
// are no different. typedefs do not create a new type, and vulkan.h has this:
// #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
//
// Fortunately, vulkan.h also permits VK_DEFINE_NON_DISPATCHABLE_HANDLE to be
// "overridden":
#if defined(__LP64__) || defined(_WIN64) ||                            \
    (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) ||     \
    defined(__powerpc64__)

// 64-bit platform. Leave vulkan.h to define VK_DEFINE_NON_DISPATCHABLE_HANDLE.

#define VOLCANO_CAST_UINTPTR(object) reinterpret_cast<uintptr_t>(object)

#else  // Not a 64-bit platform. Fix the bug:

#ifdef _MSC_VER
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object)           \
  __pragma(pack(push, 1)) typedef struct volcano_##object { \
    volcano_##object() { hnd = 0; };                        \
    volcano_##object(std::nullptr_t) { hnd = 0; };          \
    operator bool() const { return !!hnd; };                \
    uint64_t hnd;                                           \
  } __pragma(pack(pop)) object;

#else

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object)               \
  typedef struct __attribute__((__packed__)) volcano_##object { \
    volcano_##object() { hnd = 0; };                            \
    volcano_##object(std::nullptr_t) { hnd = 0; };              \
    operator bool() const { return !!hnd; };                    \
    uint64_t hnd;                                               \
  } object;

#endif

#define VOLCANO_CAST_UINTPTR(object) static_cast<uintptr_t>((object).hnd)

#endif  // If 64-bit platform.
#endif  /*__cplusplus*/

// #include the correct header (not vulkan/vulkan.h on Android!)
#ifdef __ANDROID__
#include <common/vulkan_wrapper.h>
#else
#include <vulkan/vulkan.h>
#endif

#ifdef VOLCANO_TYPEDEF_STRUCT
#undef VOLCANO_TYPEDEF_STRUCT

#undef VK_NULL_HANDLE
#define VK_NULL_HANDLE nullptr
#endif /*VOLCANO_TYPEDEF_STRUCT*/

#ifdef _MSC_VER
#define VOLCANO_PRINTF(y, z)
#else
#define VOLCANO_PRINTF(y, z) __attribute__((format(printf, y, z)))
#endif

// clang-format off
// clang-format keeps wanting to use "PointerAlignment: Right" on these:
#ifdef __cplusplus
// vk_enum_string_helper.h is shipped with vulkan in the layers/generated dir.
// It assumes C++ so only #include it if this is used in a C++ context.
#include <layers/generated/vk_enum_string_helper.h>

// autostype.h auto-generates autoSType() functions.
#include <autostype.h>

extern "C" {
#endif /*__cplusplus*/

void logV(const char* fmt, ...)  // Verbose log.
    VOLCANO_PRINTF(1, 2);
void logD(const char* fmt, ...)  // Debug log.
    VOLCANO_PRINTF(1, 2);
void logI(const char* fmt, ...)  // Info log.
    VOLCANO_PRINTF(1, 2);
void logW(const char* fmt, ...)  // Warn log.
    VOLCANO_PRINTF(1, 2);
void logE(const char* fmt, ...)  // Error log.
    VOLCANO_PRINTF(1, 2);
void logF(const char* fmt, ...)  // Fatal log.
    VOLCANO_PRINTF(1, 2);
// logVolcano can be called to output a log with a dynamically-selected
// log level.
//
// logVolcano is actually a function pointer. Set logVolcano to point to your
// own function to redirect log output.
typedef void (*VolcanoLogFn)(char level, const char* fmt, va_list ap);
extern VolcanoLogFn logVolcano;

// explainVkResult is a convenience function. It always returns 1. It calls
// logE() to write to the error log and adds more info about the VkResult 'why'
// 'what' describes what was happening when the error was returned.
int explainVkResult(const char* what, VkResult why);

#ifdef __cplusplus
} // terminate the extern "C" block

#define VKDEBUG(...)
#ifndef VKDEBUG
#define VKDEBUG(...)                                          \
  do {                                                        \
    auto typeID = getTypeID_you_must_free_the_return_value(); \
    logD(__VA_ARGS__);                                        \
    free(typeID);                                             \
  } while (0)
#endif

// class VkPtr wraps the object returned from some create...() function and
// automatically calls a destroy...() function when VkPtr is destroyed.
//
// In other words, VkPtr is just a wrapper around the destroy_fn, calling it
// at the right time.
//
// For example:
//   VkPtr<VkInstance> instance{vkDestroyInstance}
//   auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
//
// VkPtr has a member 'allocator' which is always nullptr. A derived class may
// set allocator to a custom allocator.
template <typename T>
class VkPtr {
 public:
  VkPtr() = delete;
  VkPtr(VkPtr &&other) {
    object = other.object;
    deleterT = other.deleterT;
    deleterInst = other.deleterInst;
    deleterDev = other.deleterDev;
    allocator = other.allocator;
    inst = other.inst;
    dev = other.dev;

    // The difference between VK_NULL_HANDLE and nullptr comes down to 64-bit vs
    // 32-bit:
    // https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VK_DEFINE_HANDLE.html
    // claims VK_DEFINE_HANDLE is a macro for typedef struct object##_T* object
    // but this is apparently not true for 32-bit x86, where VK_DEFINE_HANDLE is
    // a macro for uint64_t. And the compiler rejects assigning uint64_t object
    // = nullptr.
    other.object = VK_NULL_HANDLE;
    other.deleterT = nullptr;
    other.deleterInst = nullptr;
    other.deleterDev = nullptr;
    other.allocator = nullptr;
    other.inst = VK_NULL_HANDLE;
    other.dev = VK_NULL_HANDLE;
  }
  VkPtr(const VkPtr &) = delete;

  // Only allow access to the object if it has been allocated.
  operator T() const {
    if (object) {
      return object;
    }
    auto typeID = getTypeID_you_must_free_the_return_value();
    // If your app dies with this error, check that you are casting the return
    // value to (bool) so the compiler uses operator bool() below.
    logF("VkPtr.h:%d FATAL: VkPtr::operator %s() on an empty VkPtr!\n",
         __LINE__, typeID);
    free(typeID);
    exit(1);
    return VK_NULL_HANDLE;
  }

  // Allow testing the VkPtr if only bool is requested.
  operator bool() const { return object; }

  // Allow printing the object (casting it to void*).
  void *printf() const {
    // VOLCANO_CAST_UINTPTR() is needed on 32-bit platforms.
    return reinterpret_cast<void *>(VOLCANO_CAST_UINTPTR(object));
  }

  // Constructor that has a destroy_fn which takes two arguments: the obj and
  // the allocator.
  explicit VkPtr(VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
      T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr.h:%d VkPtr<%s>::VkPtr(T,allocator) with destroy_fn=%p\n",
           __LINE__, typeID, destroy_fn);
      free(typeID);
      exit(1);
    }
    deleterT = destroy_fn;
    deleterInst = nullptr;
    deleterDev = nullptr;
    allocator = nullptr;
    reset();
  }

  // Constructor that has a destroy_fn which takes 3 arguments: a VkInstance,
  // the obj, and the allocator. Note that the VkInstance is wrapped in a VkPtr
  // itself.
  explicit VkPtr(const VkPtr<VkInstance> &instPtr,
                 VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
                     VkInstance, T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr.h:%d VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n",
           __LINE__, typeID, destroy_fn);
      free(typeID);
      exit(1);
    }
    VKDEBUG("this=%p VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n",
            this, typeID, destroy_fn);
    deleterT = nullptr;
    deleterInst = destroy_fn;
    deleterDev = nullptr;
    allocator = nullptr;
    reset(instPtr);
  }

  // Constructor that has a destroy_fn which takes 3 arguments: a VkDevice, the
  // obj, and the allocator. Note that the VkDevice is wrapped in a VkPtr
  // itself.
  explicit VkPtr(const VkPtr<VkDevice> &devPtr,
                 VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
                     VkDevice, T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr.h:%d VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p\n",
           __LINE__, typeID, destroy_fn);
      free(typeID);
      exit(1);
    }
    VKDEBUG(
        "this=%p VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p "
        "object=%p\n",
        this, typeID, destroy_fn, reinterpret_cast<void *>(object));
    deleterT = nullptr;
    deleterInst = nullptr;
    deleterDev = destroy_fn;
    allocator = nullptr;
    reset(devPtr);
  }

  virtual ~VkPtr() { reset(); }

  // reset with 0 arguments clears the instance and device after
  // calling the deleter function on the object.
  void reset() {
    if (!object) {
      inst = VK_NULL_HANDLE;
      dev = VK_NULL_HANDLE;
      return;
    }
    if (deleterT) {
      VKDEBUG("VkPtr<%s>::reset() calling deleterT(%p, allocator)\n", typeID,
              reinterpret_cast<void *>(object));
      deleterT(object, allocator);
    } else if (deleterInst) {
      uint64_t instv = 0;
#if defined(__GNUC__) /* MSVC complains about pragma GCC */
#pragma GCC diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
// Silence GCC bug that triggers the maybe-uninitialized warning.
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
      memcpy(&instv, &inst, sizeof(instv));
#if defined(__GNUC__) /* MSVC complains about pragma GCC */
#pragma GCC diagnostic pop
#endif
      VKDEBUG(
          "VkPtr<%s>::reset() calling deleterInst(inst=%llx, %p, allocator)\n",
          typeID, instv, reinterpret_cast<void *>(object));
      if (inst == VK_NULL_HANDLE) {
        auto typeID = getTypeID_you_must_free_the_return_value();
        logE("VkPtr<%s>::reset(): inst=VK_NULL_HANDLE\n", typeID);
        free(typeID);
      } else {
        deleterInst(inst, object, allocator);
      }
    } else if (deleterDev) {
      uint64_t devv = 0;
#if defined(__GNUC__) /* MSVC complains about pragma GCC */
#pragma GCC diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
// Silence GCC bug that triggers the maybe-uninitialized warning.
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
      memcpy(&devv, &dev, sizeof(devv));
#if defined(__GNUC__) /* MSVC complains about pragma GCC */
#pragma GCC diagnostic pop
#endif
      VKDEBUG(
          "VkPtr<%s>::reset() calling deleterDev(dev=%llx, %p, allocator)\n",
          typeID, devv, reinterpret_cast<void *>(object));
      if (dev == VK_NULL_HANDLE) {
        auto typeID = getTypeID_you_must_free_the_return_value();
        logE("VkPtr<%s>::reset(): dev=VK_NULL_HANDLE\n", typeID);
        free(typeID);
      } else {
        deleterDev(dev, object, allocator);
      }
    } else {
      VKDEBUG("this=%p VkPtr<%s> deleter = null object = %p!\n", this, typeID,
              reinterpret_cast<void *>(object));
    }
    inst = VK_NULL_HANDLE;
    dev = VK_NULL_HANDLE;
    object = VK_NULL_HANDLE;
  }

  void reset(const VkPtr<VkInstance> &instPtr) {
    reset();
    if (!deleterInst) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF(
          "VkPtr.h:%d VkPtr<%s>::reset(instPtr) with deleterT=%p "
          "deleterInst=%p deleterDev=%p\n",
          __LINE__, typeID, deleterT, deleterInst, deleterDev);
      free(typeID);
      exit(1);
    }
    inst = instPtr.object;
  }

  void reset(const VkPtr<VkDevice> &devPtr) {
    reset();
    if (!deleterDev) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF(
          "VkPtr.h:%d VkPtr<%s>::reset(devPtr) with deleterT=%p "
          "deleterInst=%p deleterDev=%p\n",
          __LINE__, typeID, deleterT, deleterInst, deleterDev);
      free(typeID);
      exit(1);
    }
    dev = devPtr.object;
  }

  // Restrict non-const access. The object must be VK_NULL_HANDLE before it can
  // be written. You must call reset() first if you really want to do this.
  T *operator&() {
    if (object) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr.h:%d FATAL: VkPtr<%s>::operator& before reset()\n", __LINE__,
           typeID);
      free(typeID);
      exit(1);
    }
    return &object;
  }

  VkAllocationCallbacks *allocator;

 protected:
  // Allow any VkPtr<U> as a friend of VkPtr<T>.
  template <typename U>
  friend class VkPtr;

  T object;

  VKAPI_ATTR void(VKAPI_CALL *deleterT)(T, const VkAllocationCallbacks *);
  VKAPI_ATTR void(VKAPI_CALL *deleterInst)(VkInstance, T,
                                           const VkAllocationCallbacks *);
  VKAPI_ATTR void(VKAPI_CALL *deleterDev)(VkDevice, T,
                                          const VkAllocationCallbacks *);
  // inst is passed to deleterInst (if deleterInst != nullptr)
  VkInstance inst;
  // dev is passed to deleterDev (if deleterDev != nullptr)
  VkDevice dev;

  char* getTypeID_you_must_free_the_return_value() const {
    int status;
#ifdef _MSC_VER
    status = 0;
    auto ct = typeid(T).name();
    return strcpy(reinterpret_cast<char*>(malloc(strlen(ct) + 1)), ct);
#else
    return abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
#endif
  }
};

#ifdef VKDEBUG
#undef VKDEBUG
#endif
#endif /*__cplusplus*/

#ifdef _WIN32
#include <stdlib.h>
typedef unsigned int uint;
#endif
