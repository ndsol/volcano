/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include <stdio.h>
#include <stdlib.h>
#include <typeinfo>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif
#include <vulkan/vulkan.h>

#pragma once

#define VKDEBUG(...)
#ifndef VKDEBUG
#define VKDEBUG(...)                                          \
  {                                                           \
    auto typeID = getTypeID_you_must_free_the_return_value(); \
    fprintf(stderr, __VA_ARGS__);                             \
    free(typeID);                                             \
  }
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

  // Constructor that has a destroy_fn which takes two arguments: the obj and
  // the allocator.
  explicit VkPtr(VKAPI_ATTR void(VKAPI_CALL *destroy_fn)(
      T, const VkAllocationCallbacks *)) {
    object = VK_NULL_HANDLE;
    if (!destroy_fn) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      fprintf(stderr, "VkPtr<%s>::VkPtr(T,allocator) with destroy_fn=%p\n",
              typeID, destroy_fn);
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
      fprintf(stderr, "VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n",
              typeID, destroy_fn);
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
      fprintf(stderr, "VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p\n",
              typeID, destroy_fn);
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
    if (object == VK_NULL_HANDLE) {
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

  // Allow const access to the object.
  operator T() const { return object; }

  // Restrict non-const access. The object must be VK_NULL_HANDLE before it can
  // be written. Call reset() if necessary.
  T *operator&() {
    if (object != VK_NULL_HANDLE) {
      auto typeID = getTypeID_you_must_free_the_return_value();
      fprintf(stderr, "FATAL: VkPtr<%s>::operator& before reset()\n", typeID);
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

  char *getTypeID_you_must_free_the_return_value() {
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
