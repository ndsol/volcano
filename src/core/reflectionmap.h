/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * VolcanoReflectionMap flattens multi-struct structures like
 * VkPhysicalDeviceFeatures2 and, by deriving from std::map<std::string, void*>
 * allows iterator access. This allows your app to use a for loop to
 * process all the fields instead of copy/pasting the field names.
 *
 * "reflectionmap.h" #includes "VkPtr.h" for you.
 */

#include <string.h>

#include <map>
#include <string>

#include "VkPtr.h"

#pragma once

namespace language {

namespace reflect {

enum PointerType {
  UNDEFINED = 0,
  value_const_string,

#define SPECIALIZE(vartype, varname, specialSuffix) varname,
/* Use multiple include sites to apply the same type map multiple places. */
#include "reflectionmaptypes.h"
#undef SPECIALIZE

};

enum PointerAttr {
  NONE = 0,
  ARRAY = 1,
};

struct TypeDesc {
  PointerType baseType{UNDEFINED};
  PointerAttr attr{NONE};

  bool operator==(const TypeDesc& other) const {
    return other.baseType == baseType && other.attr == attr;
  }

  std::string toString() const {
    const char* baseTypeStr = "{BUG}";
    switch (baseType) {
      case UNDEFINED:
        baseTypeStr = "UNDEFINED";
        break;
      case value_const_string:
        baseTypeStr = "const_string";
        break;

#define SPECIALIZE(vartype, varname, specialSuffix) \
  case varname:                                     \
    baseTypeStr = #vartype;                         \
    break;
/* Use multiple include sites to apply the same type map multiple places. */
#include "reflectionmaptypes.h"
#undef SPECIALIZE
    }

    switch (attr) {
      case NONE:
        return baseTypeStr;
      case ARRAY:
        return std::string(baseTypeStr) + "[]";
    }
    return "{UNDEFINED attr}";
  }
};

struct Pointer {
  TypeDesc desc;
  // arraylen is 0 (not 1) for non-array types.
  size_t arraylen{0};
  union {
    const char* const_string{nullptr};
#define SPECIALIZE(vartype, varname, specialSuffix) \
  vartype* varname; /* add a field to the union */
/* Use multiple include sites to apply the same type map multiple places. */
#include "reflectionmaptypes.h"
#undef SPECIALIZE
  };

  int checkType(const char* method, const char* fieldName, PointerType wantBT,
                PointerAttr wantAttr) const {
    TypeDesc want;
    want.baseType = wantBT;
    want.attr = wantAttr;
    if (want == desc) {
      return 0;
    }

    logE("reflection: %s(%s): want type %s, got %s\n", method, fieldName,
         want.toString().c_str(), desc.toString().c_str());
    return 1;
  }

#define SPECIALIZE(vartype, varname, specialSuffix)                      \
  int get##specialSuffix(const char* fieldName, vartype& value) const {  \
    if (checkType("get" #specialSuffix, fieldName, PointerType::varname, \
                  PointerAttr::NONE)) {                                  \
      return 1;                                                          \
    }                                                                    \
    value = *(varname);                                                  \
    return 0;                                                            \
  }                                                                      \
                                                                         \
  int set##specialSuffix(const char* fieldName, vartype value) {         \
    if (checkType("set" #specialSuffix, fieldName, PointerType::varname, \
                  PointerAttr::NONE)) {                                  \
      return 1;                                                          \
    }                                                                    \
    *(varname) = value;                                                  \
    return 0;                                                            \
  }                                                                      \
                                                                         \
  int addField##specialSuffix(vartype* field) {                          \
    desc.baseType = PointerType::varname;                                \
    desc.attr = PointerAttr::NONE;                                       \
    varname = field;                                                     \
    return 0;                                                            \
  }                                                                      \
                                                                         \
  int addArrayField##specialSuffix(vartype* field, size_t len) {         \
    desc.baseType = PointerType::varname;                                \
    desc.attr = PointerAttr::ARRAY;                                      \
    arraylen = len;                                                      \
    varname = field;                                                     \
    return 0;                                                            \
  }
/* Use multiple include sites to apply the same type map multiple places. */
#include "reflectionmaptypes.h"
#undef SPECIALIZE

  int addFieldConstString(const char* field) {
    desc.baseType = PointerType::value_const_string;
    desc.attr = PointerAttr::NONE;
    const_string = field;
    return 0;
  }
};

}  // namespace reflect

typedef struct VolcanoReflectionMap
    : public std::map<std::string, reflect::Pointer> {
  // get(fieldName, value) gets the field value or returns 1 if an error occurs
  template <typename T>
  int get(const char* fieldName, T& value) {
    auto pointer = getField(fieldName);
    if (pointer) {
      return pointer->get(fieldName, value);
    }
    logE("%s(%s): field not found\n", "get", fieldName);
    return 1;
  }

  // set(fieldName, value) sets the value. If the field is not found or is the
  // wrong type, set returns 1.
  template <typename T>
  int set(const char* fieldName, T value) {
    auto pointer = getField(fieldName);
    if (pointer) {
      return pointer->set(fieldName, value);
    }
    logE("%s(%s): field not found\n", "set", fieldName);
    return 1;
  }

  // addField(fieldName, value) defines a new field, or returns 1 if an error
  template <typename T>
  int addField(const char* fieldName, T* field) {
    auto pointer = getField(fieldName);
    if (pointer) {
      logW("addField(%s): already exists, type %s\n", fieldName,
           pointer->desc.toString().c_str());
      return 1;
    }
    pointer = &operator[](fieldName);
    pointer->addField(field);
    return 0;
  }

  // addArrayField(arrayName, value, len) defines a new array of fields, or
  // returns 1 if an error
  template <typename T>
  int addArrayField(const char* arrayName, T* field, size_t len) {
    auto pointer = getField(arrayName);
    if (pointer) {
      logW("addArrayField(%s): already exists, type %s\n", arrayName,
           pointer->desc.toString().c_str());
      return 1;
    }
    pointer = &operator[](arrayName);
    pointer->addArrayField(field, len);
    return 0;
  }

  // addFieldConstString can't overload other pointer types, must be separate.
  int addFieldConstString(const char* fieldName, const char* field) {
    auto pointer = getField(fieldName);
    if (pointer) {
      logW("addField(%s): already exists, type %s\n", fieldName,
           pointer->desc.toString().c_str());
      return 1;
    }
    pointer = &operator[](fieldName);
    pointer->addFieldConstString(field);
    return 0;
  }

 protected:
  reflect::Pointer* getField(const char* fieldName);
} VolcanoReflectionMap;

// get specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::get(const char* fieldName, VkBool32& value);

// set specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::set(const char* fieldName, VkBool32 value);

// addField specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::addField(const char* fieldName, VkBool32* field);

// addArrayField specialized because VkBool32 is a typedef of uint32_t
template <>
int VolcanoReflectionMap::addArrayField(const char* arrayName, VkBool32* field,
                                        size_t len);

// get specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::get(const char* fieldName, size_t& value);

// set specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::set(const char* fieldName, size_t value);

// addField specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::addField(const char* fieldName, size_t* field);

// addArrayField specialized because size_t is a typedef of unsigned long
template <>
int VolcanoReflectionMap::addArrayField(const char* arrayName, size_t* field,
                                        size_t len);

// get specialized because string is read-only
template <>
int VolcanoReflectionMap::get(const char* fieldName, std::string& value);

}  // namespace language
