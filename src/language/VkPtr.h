/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <typeinfo>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif

#pragma once

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

#else  // Not a 64-bit platform. Fix the bug:

#ifdef _MSC_VER
#define VOLCANO_TYPEDEF_STRUCT(o) \
  __pragma(pack(push, 1)) typedef struct o __pragma(pack(pop))
#else
#define VOLCANO_TYPEDEF_STRUCT(o) typedef struct __attribute__((__packed__)) o
#endif

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object)  \
  VOLCANO_TYPEDEF_STRUCT(volcano_##object) {       \
    volcano_##object() { hnd = 0; };               \
    volcano_##object(std::nullptr_t) { hnd = 0; }; \
    operator bool() const { return !!hnd; };       \
    uint64_t hnd;                                  \
  }                                                \
  object;

#endif  // If 64-bit platform.

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

void logV(const char *fmt, ...)  // Verbose log.
    VOLCANO_PRINTF(1, 2);
void logD(const char *fmt, ...)  // Debug log.
    VOLCANO_PRINTF(1, 2);
void logI(const char *fmt, ...)  // Info log.
    VOLCANO_PRINTF(1, 2);
void logW(const char *fmt, ...)  // Warn log.
    VOLCANO_PRINTF(1, 2);
void logE(const char *fmt, ...)  // Error log.
    VOLCANO_PRINTF(1, 2);
void logF(const char *fmt, ...)  // Fatal log.
    VOLCANO_PRINTF(1, 2);

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
    VKDEBUG("this=%p VkPtr<%s> steal from %p dD=%p pD=%p\n", this, typeID,
            std::addressof(other), other.deleterDev, other.pDev);
    object = other.object;
    deleterT = other.deleterT;
    deleterInst = other.deleterInst;
    deleterDev = other.deleterDev;
    allocator = other.allocator;
    pInst = other.pInst;
    pDev = other.pDev;

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
    other.pInst = nullptr;
    other.pDev = nullptr;
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
    logF("FATAL: VkPtr::operator %s() on an empty VkPtr!\n", typeID);
    free(typeID);
    exit(1);
    return VK_NULL_HANDLE;
  }

  // Allow testing the VkPtr if only bool is requested.
  operator bool() const { return object; }

  // Constructor that has a destroy_fn which takes two arguments: the obj and
  // the allocator.
  explicit VkPtr(VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
      T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr<%s>::VkPtr(T,allocator) with destroy_fn=%p\n", typeID,
           destroy_fn);
      free(typeID);
      exit(1);
    }
    deleterT = destroy_fn;
    deleterInst = nullptr;
    deleterDev = nullptr;
    allocator = nullptr;
    pInst = nullptr;
    pDev = nullptr;
  }

  // Constructor that has a destroy_fn which takes 3 arguments: a VkInstance,
  // the obj, and the allocator. Note that the VkInstance is wrapped in a VkPtr
  // itself.
  explicit VkPtr(VkPtr<VkInstance> &instance,
                 VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
                     VkInstance, T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n", typeID,
           destroy_fn);
      free(typeID);
      exit(1);
    }
    VKDEBUG("this=%p VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n",
            this, typeID, destroy_fn);
    deleterT = nullptr;
    deleterInst = destroy_fn;
    deleterDev = nullptr;
    allocator = nullptr;
    pInst = &instance.object;
    pDev = nullptr;
  }

  // Constructor that has a destroy_fn which takes 3 arguments: a VkDevice, the
  // obj, and the allocator. Note that the VkDevice is wrapped in a VkPtr
  // itself.
  explicit VkPtr(VkPtr<VkDevice> &device,
                 VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
                     VkDevice, T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p\n", typeID,
           destroy_fn);
      free(typeID);
      exit(1);
    }
    VKDEBUG(
        "this=%p VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p "
        "object=%p\n",
        this, typeID, destroy_fn, *reinterpret_cast<void **>(&object));
    deleterT = nullptr;
    deleterInst = nullptr;
    deleterDev = destroy_fn;
    allocator = nullptr;
    pInst = nullptr;
    pDev = &device.object;
  }

  virtual ~VkPtr() { reset(); }

  void reset() {
    if (!object) {
      return;
    }
    if (deleterT) {
      VKDEBUG("VkPtr<%s>::reset() calling deleterT(%p, allocator)\n", typeID,
              *reinterpret_cast<void **>(&object));
      deleterT(object, allocator);
    } else if (deleterInst) {
      VKDEBUG(
          "VkPtr<%s>::reset() calling deleterInst(inst=%p, %p, allocator)\n",
          typeID, pInst, *reinterpret_cast<void **>(&object));
      deleterInst(*pInst, object, allocator);
    } else if (deleterDev) {
      VKDEBUG("VkPtr<%s>::reset() calling deleterDev(dev=%p, %p, allocator)\n",
              typeID, pDev, *reinterpret_cast<void **>(&object));
      deleterDev(*pDev, object, allocator);
    } else {
      VKDEBUG("this=%p VkPtr<%s> deleter = null object = %p!\n", this, typeID,
              *reinterpret_cast<void **>(&object));
    }
    object = VK_NULL_HANDLE;
  }

  // Restrict non-const access. The object must be VK_NULL_HANDLE before it can
  // be written. Call reset() if necessary.
  T *operator&() {
    if (object) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      logF("FATAL: VkPtr<%s>::operator& before reset()\n", typeID);
      free(typeID);
      exit(1);
    }
    return &object;
  }

  T object;

  VkAllocationCallbacks *allocator;

 private:
  VKAPI_ATTR void(VKAPI_CALL *deleterT)(T, const VkAllocationCallbacks *);
  VKAPI_ATTR void(VKAPI_CALL *deleterInst)(VkInstance, T,
                                           const VkAllocationCallbacks *);
  VKAPI_ATTR void(VKAPI_CALL *deleterDev)(VkDevice, T,
                                          const VkAllocationCallbacks *);
  VkInstance *pInst;
  VkDevice *pDev;

  char *getTypeID_you_must_free_the_return_value() const {
    int status;
#ifdef _MSC_VER
    status = 0;
    auto ct = typeid(T).name();
    return strcpy(reinterpret_cast<char *>(malloc(strlen(ct) + 1)), ct);
#else
    return abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
#endif
  }
};

#ifdef VKDEBUG
#undef VKDEBUG
#endif

#ifdef _WIN32
#include <stdlib.h>
typedef unsigned int uint;
#endif
