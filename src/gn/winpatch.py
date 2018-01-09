#!/usr/bin/env python
# Copyright (c) 2017 the Volcano Authors. Licensed under GPLv3.
# The patches of build-posix.sh written in python for win32 platforms.

from fileinput import FileInput
import re
import sys

if __name__ == "__main__":
  gli_patch1 = re.compile(("(HeaderDesc\\.format\\.flags = Storage\\.layers"
      "\\(\\) > 1 \\? [^ ][^ ]* : )(Desc\\.Flags)"))
  gli_patch2 = re.compile(("(HeaderDesc\\.format\\.fourCC = Storage\\.layers"
      "\\(\\) > 1 \\? [^ ][^ ]* : )(Desc\\.FourCC)"))
  f = FileInput(("vendor/gli/gli/core/save_dds.inl"), True)
  for line in f:
    line = line.rstrip("\n\r")
    print(re.sub(gli_patch2, "\\1detail::D3DFORMAT(\\2)",
        re.sub(gli_patch1, "\\1detail::DDPF(\\2)", line)))
  f.close()

  f = FileInput(("vendor/spirv_cross/spirv_common.hpp"), True)
  already_patched = False
  include_vector = False
  report_and_abort = False
  convert_to_string = False
  convert_to_string_target = "	return std::to_string(std::forward<T>(t));"
  for line in f:
    line = line.rstrip("\n\r")
    if already_patched:
      print(line)
    elif include_vector:
      if line == "":
        print("#ifdef __ANDROID__")
        print("#include <android/log.h>")
        print("#endif")
        print(line)
      else:
        already_patched = True
        print(line)
    elif line == "{" and report_and_abort:
      print(line)
      print("#ifdef __ANDROID__")
      print(("	__android_log_assert(__FILE__, \"volcano\", \"spirv_cross "
          "error: %s\\n\", msg.c_str());"))
      continue # prevent report_and_abort from being reset to False
    elif line == "#ifdef NDEBUG" and report_and_abort:
      print("#elif defined(NDEBUG)")
    elif line == "{" and convert_to_string:
      print(line)
      continue # prevent convert_to_string from being reset to False
    elif line == convert_to_string_target and convert_to_string:
      print("	std::ostringstream converter;")
      print("	converter << std::forward<T>(t);")
      print("	return converter.str();")
    else:
      print(line)
    include_vector = (line == "#include <vector>")
    report_and_abort = (line == "    report_and_abort(const std::string &msg)")
    convert_to_string = (line == "inline std::string convert_to_string(T &&t)")
  f.close()

  helper_patch = re.compile("#if WINVER < 0x0600")
  f = FileInput("vendor/glfw/src/win32_platform.h", True)
  for line in f:
    line = line.rstrip("\n\r")
    print(re.sub(helper_patch, "#ifndef DWM_BB_ENABLE",
                  line))
  f.close()

  vvs = "vendor/vulkansamples/submodules/Vulkan-LoaderAndValidationLayers/"
  sample_patch = re.compile(("pVersionStruct->pfn(Get(Instance|Device|"
      "PhysicalDevice)ProcAddr) = vk((GetInstance|GetDevice|"
      "_layerGetPhysicalDevice)ProcAddr);$"))
  for n in [
        "core_validation",
        "object_tracker_utils",
        "threading",
        "unique_objects",
      ]:
    scope = re.sub("_utils", "", n)
    f = FileInput(("%slayers/%s.cpp" % (vvs, n)), True)
    for line in f:
      line = line.rstrip("\n\r")
      print(re.sub(sample_patch,
                    "pVersionStruct->pfn\\1 = %s::\\1;" % scope, line))
    f.close()

  helper_patch = re.compile("static const char \\* GetPhysDevFeatureString")
  f = FileInput(("%sscripts/helper_file_generator.py" % vvs), True)
  for line in f:
    line = line.rstrip("\n\r")
    print(re.sub(helper_patch, "inline const char * GetPhysDevFeatureString",
                  line))
  f.close()

  utils_patch = re.compile("^#include \"vulkan/vulkan.h\"")
  f = FileInput(("%slayers/vk_format_utils.cpp" % vvs), True)
  for line in f:
    line = line.rstrip("\n\r")
    print(re.sub(utils_patch, "#include <vulkan/vulkan.h>", line))
  f.close()
