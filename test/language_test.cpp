/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * Unit tests for code in src/language.
 */

#include "gtest/gtest.h"

// language.h must be #included after gtest/gtest.h
#include <src/language/language.h>

namespace {  // An anonymous namespace keeps any definition local to this file.

static const int TEST_VALUE_1 = 123;
static const int TEST_VALUE_2 = 234;
static const uint64_t MAGIC_VALUE = 6789;
static const uint64_t TEST_INST_VALUE = 5678;
static const uint64_t TEST_DEV_VALUE = 4567;

template <typename T>
struct Something {
  void runThis(T n, void (*cb)(T)) { cb(n); }
};

// VkPtrWithAccess overrides VkPtr<T> and exposes protected fields for testing.
template <typename T>
struct VkPtrWithAccess : public VkPtr<T> {
  const uint64_t magic = MAGIC_VALUE;
  VkAllocationCallbacks callbacks;
  int deleterCallCount{0};
  int instDeleterCallCount{0};
  int devDeleterCallCount{0};
  int wantDeleterCallCount{0};
  int wantInstDeleterCallCount{0};
  int wantDevDeleterCallCount{0};

  typedef VKAPI_ATTR void(VKAPI_CALL* deleterFn)(T,
                                                 const VkAllocationCallbacks*);
  deleterFn getDeleterT() const { return VkPtr<T>::deleterT; };

  typedef VKAPI_ATTR void(VKAPI_CALL* deleterInstFn)(
      VkInstance, T, const VkAllocationCallbacks*);
  deleterInstFn getDeleterInst() const { return VkPtr<T>::deleterInst; };

  typedef VKAPI_ATTR void(VKAPI_CALL* deleterDevFn)(
      VkDevice, T, const VkAllocationCallbacks*);
  deleterDevFn getDeleterDev() const { return VkPtr<T>::deleterDev; };

  VkInstance* getPInst() const { return VkPtr<T>::pInst; };
  VkDevice* getPDev() const { return VkPtr<T>::pDev; };
  void set(T value) { VkPtr<T>::object = value; }

  explicit VkPtrWithAccess(deleterFn f) : VkPtr<T>(f) {
    VkPtr<T>::allocator = &callbacks;
    callbacks.pUserData = this;
  }
  explicit VkPtrWithAccess(VkPtrWithAccess<VkInstance>& instance,
                           deleterInstFn f)
      : VkPtr<T>(instance, f) {
    VkPtr<T>::allocator = &callbacks;
    callbacks.pUserData = this;
  }
  explicit VkPtrWithAccess(VkPtrWithAccess<VkDevice>& device, deleterDevFn f)
      : VkPtr<T>(device, f) {
    VkPtr<T>::allocator = &callbacks;
    callbacks.pUserData = this;
  }

  VkPtrWithAccess(VkPtrWithAccess&& other) : VkPtr<T>(std::move(other)) {
    VkPtr<T>::allocator = &callbacks;
    callbacks = other.callbacks;
    callbacks.pUserData = this;
  }

  void assertWantCallCounts() {
    ASSERT_EQ(deleterCallCount, wantDeleterCallCount);
    ASSERT_EQ(instDeleterCallCount, wantInstDeleterCallCount);
    ASSERT_EQ(devDeleterCallCount, wantDevDeleterCallCount);
  }

  virtual ~VkPtrWithAccess() {
    // Impossible to force ~VkPtr to run first (and still have the pUserData
    // pointer point to this), so just run reset() here.
    VkPtr<T>::reset();
    assertWantCallCounts();
  }
};

// VkPtr tests that run without calling any Vulkan APIs.
class VkPtrBasics : public ::testing::Test {
 protected:
  VkPtrWithAccess<VkInstance> presetInst{instCheck};
  VkPtrWithAccess<VkDevice> presetDev{devCheck};
  // T is a type to use, though it does not matter.
  typedef int T;

  void SetUp() override {
    VkInstance* i = &presetInst;
    *reinterpret_cast<uint64_t*>(i) = TEST_INST_VALUE;
    VkDevice* d = &presetDev;
    *reinterpret_cast<uint64_t*>(d) = TEST_DEV_VALUE;
  }

 public:
  static void checkVkPtrOk(VkPtrWithAccess<T>* p) {
    ASSERT_EQ(p->magic, MAGIC_VALUE);
  }

  static VkPtrWithAccess<T>* ptr(const VkAllocationCallbacks* pCallbacks) {
    VkPtrWithAccess<T>* p =
        reinterpret_cast<VkPtrWithAccess<T>*>(pCallbacks->pUserData);
    checkVkPtrOk(p);
    return p;
  }

  VKAPI_ATTR static void VKAPI_CALL
  intDeleter(T n, const VkAllocationCallbacks* pCallbacks) {
    ASSERT_EQ(n, TEST_VALUE_1);
    ptr(pCallbacks)->deleterCallCount++;
  }

  VKAPI_ATTR static void VKAPI_CALL
  intInstDeleter(VkInstance i, T n, const VkAllocationCallbacks* pCallbacks) {
    assertInstOk(i);
    ASSERT_EQ(n, TEST_VALUE_1);
    ptr(pCallbacks)->instDeleterCallCount++;
  }

  VKAPI_ATTR static void VKAPI_CALL
  intDevDeleter(VkDevice d, T n, const VkAllocationCallbacks* pCallbacks) {
    assertDevOk(d);
    ASSERT_EQ(n, TEST_VALUE_1);
    ptr(pCallbacks)->devDeleterCallCount++;
  }

  VKAPI_ATTR static void VKAPI_CALL
  instCheck(VkInstance i, const VkAllocationCallbacks* pCallbacks) {
    ptr(pCallbacks);
    assertInstOk(i);
  }

  VKAPI_ATTR static void VKAPI_CALL
  devCheck(VkDevice d, const VkAllocationCallbacks* pCallbacks) {
    ptr(pCallbacks);
    assertDevOk(d);
  }

  static void assertInstOk(VkInstance i) {
    // VkInstance is intentionally an undefined type.
    // Use a dirty "memcpy-from-the-void" to cheat.
    uint64_t v = 0;
    memcpy(&v, &i, sizeof(v));
    ASSERT_EQ(v, TEST_INST_VALUE);
  }

  static void assertDevOk(VkDevice d) {
    // VkDevice is intentionally an undefined type.
    // Use a dirty "memcpy-from-the-void" to cheat.
    uint64_t v = 0;
    memcpy(&v, &d, sizeof(v));
    ASSERT_EQ(v, TEST_DEV_VALUE);
  }
};

TEST_F(VkPtrBasics, With1Arg) {
  VkPtrWithAccess<T> pInt(intDeleter);
  *(&pInt) = TEST_VALUE_1;
  ASSERT_EQ(pInt.getDeleterT(), &intDeleter);
  ASSERT_EQ(pInt.getDeleterInst(), nullptr);
  ASSERT_EQ(pInt.getDeleterDev(), nullptr);
  pInt.wantDeleterCallCount = 1;
}

TEST_F(VkPtrBasics, WithInstance) {
  VkPtrWithAccess<T> pInt(presetInst, intInstDeleter);
  *(&pInt) = TEST_VALUE_1;
  ASSERT_EQ(pInt.getDeleterT(), nullptr);
  ASSERT_EQ(pInt.getDeleterInst(), &intInstDeleter);
  ASSERT_EQ(pInt.getDeleterDev(), nullptr);
  pInt.wantInstDeleterCallCount = 1;
}

TEST_F(VkPtrBasics, WithDevice) {
  VkPtrWithAccess<T> pInt(presetDev, intDevDeleter);
  *(&pInt) = TEST_VALUE_1;
  ASSERT_EQ(pInt.getDeleterT(), nullptr);
  ASSERT_EQ(pInt.getDeleterInst(), nullptr);
  ASSERT_EQ(pInt.getDeleterDev(), &intDevDeleter);
  pInt.wantDevDeleterCallCount = 1;
}

#ifdef _WIN32 /*logs are not captured by gtest*/
#define ASSERT_DEATH_FIXED(x, y) ASSERT_DEATH(x, "")
#else /*_WIN32*/
#define ASSERT_DEATH_FIXED(x, y) ASSERT_DEATH(x, y)
#endif /*_WIN32*/

TEST_F(VkPtrBasics, DoubleAssignShouldFail) {
  VkInstance* secondI = nullptr;
  ASSERT_DEATH_FIXED(secondI = &presetInst, "operator& before reset()");
  ASSERT_EQ(secondI, nullptr);
}

TEST_F(VkPtrBasics, MoveConstructor) {
  VkPtrWithAccess<T> pInt1(intDeleter);
  *(&pInt1) = TEST_VALUE_1;
  ASSERT_EQ(pInt1.getDeleterT(), &intDeleter);
  ASSERT_EQ(pInt1.getDeleterInst(), nullptr);
  ASSERT_EQ(pInt1.getDeleterDev(), nullptr);
  ASSERT_EQ(int(pInt1), TEST_VALUE_1);

  VkPtrWithAccess<T> pInt2 = std::move(pInt1);
  ASSERT_EQ(pInt2.getDeleterT(), &intDeleter);
  ASSERT_EQ(pInt2.getDeleterInst(), nullptr);
  ASSERT_EQ(pInt2.getDeleterDev(), nullptr);
  ASSERT_EQ(int(pInt2), TEST_VALUE_1);
  pInt2.wantDeleterCallCount = 1;
}

TEST_F(VkPtrBasics, CastToBool) {
  VkPtrWithAccess<T> pInt(intDeleter);
  ASSERT_EQ(bool(pInt), false);
  *(&pInt) = TEST_VALUE_1;
  ASSERT_EQ(bool(pInt), true);
  pInt.wantDeleterCallCount = 1;
}

TEST_F(VkPtrBasics, EmptyWhenCastToIntShouldFail) {
  VkPtrWithAccess<T> pInt(intDeleter);
  ASSERT_EQ(bool(pInt), false);
  int n = 0;
  ASSERT_DEATH_FIXED(n = pInt, "operator .* on an empty VkPtr!");
  ASSERT_EQ(n, 0);
}

TEST_F(VkPtrBasics, Reset) {
  // Repeat the With1Arg test.
  VkPtrWithAccess<T> pInt(intDeleter);
  *(&pInt) = TEST_VALUE_1;
  ASSERT_EQ(pInt.getDeleterT(), &intDeleter);
  ASSERT_EQ(pInt.getDeleterInst(), nullptr);
  ASSERT_EQ(pInt.getDeleterDev(), nullptr);
  pInt.wantDeleterCallCount = 1;

  // Call reset and verify that it does what With1Arg did.
  pInt.reset();
  ASSERT_EQ(pInt.deleterCallCount, 1);
  ASSERT_EQ(pInt.instDeleterCallCount, 0);
  ASSERT_EQ(pInt.devDeleterCallCount, 0);
}

// VolcanoReflectionMap tests that run without calling any Vulkan APIs.
class VolcanoReflectionMapBasics : public ::testing::Test {
 protected:
  language::VolcanoReflectionMap reflect;

  int fieldInt{TEST_VALUE_1};
  uint32_t fieldUInt32{TEST_VALUE_2};
};

TEST_F(VolcanoReflectionMapBasics, AddField) {
  int gotInt = 0;
  ASSERT_DEATH_FIXED(if (reflect.get("fieldInt", gotInt)) exit(1),
                     "get\\(fieldInt\\): field not found");
  ASSERT_EQ(reflect.addField("fieldInt", &fieldInt), 0);
  ASSERT_DEATH_FIXED(if (reflect.addField("fieldInt", &fieldUInt32)) exit(1),
                     "addField\\(fieldInt\\): already exists, type int");
  gotInt = 0;
  ASSERT_EQ(reflect.get("fieldInt", gotInt), 0);
  ASSERT_EQ(gotInt, (int)TEST_VALUE_1);
}

TEST_F(VolcanoReflectionMapBasics, GetSetInt) {
  ASSERT_EQ(reflect.addField("fieldInt", &fieldInt), 0);
  ASSERT_EQ(reflect.addField("fieldUInt32", &fieldUInt32), 0);
  ASSERT_EQ(fieldInt, (int)TEST_VALUE_1);
  ASSERT_EQ(fieldUInt32, (uint32_t)TEST_VALUE_2);
  int gotInt = 0;
  ASSERT_EQ(reflect.get("fieldInt", gotInt), 0);
  ASSERT_EQ(gotInt, (int)TEST_VALUE_1);
  uint32_t gotUInt32 = 0;
  ASSERT_EQ(reflect.get("fieldUInt32", gotUInt32), 0);
  ASSERT_EQ(gotUInt32, (uint32_t)TEST_VALUE_2);
  ASSERT_DEATH_FIXED(if (reflect.set("fieldInt", (uint32_t)TEST_VALUE_2))
                         exit(1),
                     "setVkBool32\\(fieldInt\\): want type VkBool32, got int");
  ASSERT_DEATH_FIXED(if (reflect.set("fieldUInt32", (int)TEST_VALUE_1)) exit(1),
                     "set\\(fieldUInt32\\): want type int, got VkBool32");
  ASSERT_EQ(fieldInt, (int)TEST_VALUE_1);
  ASSERT_EQ(reflect.set("fieldInt", (int)TEST_VALUE_2), 0);
  ASSERT_EQ(fieldInt, (int)TEST_VALUE_2);
  ASSERT_EQ(fieldUInt32, (uint32_t)TEST_VALUE_2);
  ASSERT_EQ(reflect.set("fieldUInt32", (uint32_t)TEST_VALUE_1), 0);
  ASSERT_EQ(fieldUInt32, (uint32_t)TEST_VALUE_1);
  gotInt = 0;
  ASSERT_EQ(reflect.get("fieldInt", gotInt), 0);
  ASSERT_EQ(gotInt, (int)TEST_VALUE_2);
  gotUInt32 = 0;
  ASSERT_EQ(reflect.get("fieldUInt32", gotUInt32), 0);
  ASSERT_EQ(gotUInt32, (uint32_t)TEST_VALUE_1);
}

// TODO: Increase test coverage of VolcanoReflectionMap.
// TODO: Such as iterating over the map.

// Instance tests, uses Vulkan API (skip for Travis CI).
class InstanceTests : public ::testing::Test {
 protected:
  language::Instance i;

  void DoCtorError() {
    ASSERT_EQ(bool(i.vk), false);
    ASSERT_EQ(bool(i.surface), false);
    ASSERT_EQ(i.devs.size(), size_t(0));

    // Prepare instance for headless unit test
    bool r = i.minSurfaceSupport.erase(language::PRESENT);
    ASSERT_EQ(r, true);

    ASSERT_EQ(i.ctorError(InstanceTests::emptySurfaceFn, nullptr), 0);
  }

  static VkResult emptySurfaceFn(language::Instance&, void* /*window*/) {
    return VK_SUCCESS;
  }
};

TEST_F(InstanceTests, WithoutCtorError) {
  ASSERT_EQ(bool(i.vk), false);
  ASSERT_EQ(bool(i.surface), false);
  ASSERT_EQ(i.devs.size(), size_t(0));
}

TEST_F(InstanceTests, CtorError) { DoCtorError(); }

// TODO: Increase test coverage of Instance.

// TODO: Device unit tests.

}  // End of anonymous namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
