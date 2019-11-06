#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# The patches of build-posix.sh written in python for win32 platforms.

from fileinput import FileInput
import re
import sys

class FileEditor:
  def __init__(self, filename):
    self.contents = []
    self.filename = filename

  def readFile(self):
    for line in open(self.filename, "r"):
      line = line.rstrip("\n\r")
      self.contents.append(line)
      yield line

  def run(self, customGenerator):
    changed = False
    result = []
    for line in customGenerator(self.readFile()):
      result.append(line)
      if len(self.contents) != len(result):
        changed = True
      else:
        n = len(self.contents)
        if self.contents[n - 1] != result[n - 1]:
          changed = True
    if changed:
      print("patching %s" % self.filename)
      outf = open(self.filename, "w")
      for line in result:
        outf.write(line + "\n")

def gliFilter(srcFile):
  p1 = re.compile("(HeaderDesc\\.format\\.flags = Storage\\.layers"
    "\\(\\) > 1 \\? [^ ][^ ]* : )(Desc\\.Flags)")
  p2 = re.compile("(HeaderDesc\\.format\\.fourCC = Storage\\.layers"
    "\\(\\) > 1 \\? [^ ][^ ]* : )(Desc\\.FourCC)")
  for line in srcFile:
    r1 = re.sub(p1, "\\1detail::DDPF(\\2)", line)
    yield re.sub(p2, "\\1detail::D3DFORMAT(\\2)", r1)

def spirvCommonFilter(srcFile):
  already_patched = False
  include_vector = False
  report_and_abort = False
  convert_to_string = False
  convert_to_string_target = "	return std::to_string(std::forward<T>(t));"
  for line in srcFile:
    if already_patched:
      yield line
    elif include_vector:
      if line == "":
        yield "#ifdef __ANDROID__"
        yield "#include <android/log.h>"
        yield "#endif"
        yield line
      else:
        already_patched = True
        yield line
    elif line == "{" and report_and_abort:
      yield line
      yield "#ifdef __ANDROID__"
      yield ("	__android_log_assert(__FILE__, \"volcano\", \"spirv_cross "
          "error: %s\\n\", msg.c_str());")
      continue # prevent report_and_abort from being reset to False
    elif line == "#ifdef NDEBUG" and report_and_abort:
      yield "#elif defined(NDEBUG)"
    elif line == "{" and convert_to_string:
      yield line
      continue # prevent convert_to_string from being reset to False
    elif line == convert_to_string_target and convert_to_string:
      yield "	std::ostringstream converter;"
      yield "	converter << std::forward<T>(t);"
      yield "	return converter.str();"
    else:
      yield line
    include_vector = (line == "#include <vector>")
    report_and_abort = (line == "    report_and_abort(const std::string &msg)")
    convert_to_string = (line == "inline std::string convert_to_string(T &&t)")

def windefFilter(srcFile):
  def_match = re.compile("vkGetPhysicalDeviceImageFormatProperties2")
  need_to_add = True
  for line in srcFile:
    yield line
    if def_match.search(line) is not None:
      need_to_add = False
  if need_to_add:
    yield "   vkGetPhysicalDeviceImageFormatProperties2"

def gpuvalidationFilter(srcFile):
  generatestage_match = "// Generate the stage-specific part of the message."
  for line in srcFile:
    if line == generatestage_match:
      yield """
// Deprecated constants no longer found in spirv-tools/instrument.hpp:
static constexpr int kInstTessOutInvocationId = spvtools::kInstCommonOutCnt;
static constexpr int kInstCompOutGlobalInvocationId = spvtools::kInstCommonOutCnt;
static constexpr int kInstBindlessOutDescIndex = spvtools::kInstStageOutCnt + 1;
static constexpr int kInstBindlessOutDescBound = spvtools::kInstStageOutCnt + 2;

"""
    yield line

def wdkFilter(srcFile):
  inc_match = re.compile("include <d3dkmthk.h>")
  wdk_more = re.compile("typedef _Check_return_ NTSTATUS.*D3DKMT(EnumAdapters2|QueryAdapterInfo)")
  wdk_already_patched = re.compile("^typedef struct _D3DKMT_ADAPTERINFO")
  wdk_use_trigger = re.compile("// Read manifest JSON files uing the Windows driver interface")
  need_to_add = True
  look_for_more_wdk = 0
  for line in srcFile:
    if inc_match.search(line) is not None:
      look_for_more_wdk = 5 # look for up to 5 more lines
      continue # delete inc_match
    if look_for_more_wdk > 0:
      look_for_more_wdk -= 1
      if wdk_more.search(line) is not None:
        continue # delete wdk_more
    if wdk_already_patched.search(line) is not None:
      need_to_add = False
    if wdk_use_trigger.search(line) is not None and need_to_add:
      # Add wdk information as documented on docs.microsoft.com as of 2019-08-19.
      yield """
typedef UINT D3DKMT_HANDLE;

typedef struct _D3DKMT_ADAPTERINFO {
  D3DKMT_HANDLE hAdapter;
  LUID          AdapterLuid;
  ULONG         NumOfSources;
  BOOL          bPresentMoveRegionsPreferred;
} D3DKMT_ADAPTERINFO;

typedef struct _D3DKMT_ENUMADAPTERS2 {
  ULONG              NumAdapters;
  D3DKMT_ADAPTERINFO *pAdapters;
} D3DKMT_ENUMADAPTERS2;
typedef _Check_return_ NTSTATUS(APIENTRY *PFN_D3DKMTEnumAdapters2)(const D3DKMT_ENUMADAPTERS2*);

typedef enum _KMTQUERYADAPTERINFOTYPE {
  KMTQAITYPE_UMDRIVERPRIVATE,
  KMTQAITYPE_UMDRIVERNAME,
  KMTQAITYPE_UMOPENGLINFO,
  KMTQAITYPE_GETSEGMENTSIZE,
  KMTQAITYPE_ADAPTERGUID,
  KMTQAITYPE_FLIPQUEUEINFO,
  KMTQAITYPE_ADAPTERADDRESS,
  KMTQAITYPE_SETWORKINGSETINFO,
  KMTQAITYPE_ADAPTERREGISTRYINFO,
  KMTQAITYPE_CURRENTDISPLAYMODE,
  KMTQAITYPE_MODELIST,
  KMTQAITYPE_CHECKDRIVERUPDATESTATUS,
  KMTQAITYPE_VIRTUALADDRESSINFO,
  KMTQAITYPE_DRIVERVERSION,
  KMTQAITYPE_ADAPTERTYPE,
  KMTQAITYPE_OUTPUTDUPLCONTEXTSCOUNT,
  KMTQAITYPE_WDDM_1_2_CAPS,
  KMTQAITYPE_UMD_DRIVER_VERSION,
  KMTQAITYPE_DIRECTFLIP_SUPPORT,
  KMTQAITYPE_MULTIPLANEOVERLAY_SUPPORT,
  KMTQAITYPE_DLIST_DRIVER_NAME,
  KMTQAITYPE_WDDM_1_3_CAPS,
  KMTQAITYPE_MULTIPLANEOVERLAY_HUD_SUPPORT,
  KMTQAITYPE_WDDM_2_0_CAPS,
  KMTQAITYPE_NODEMETADATA,
  KMTQAITYPE_CPDRIVERNAME,
  KMTQAITYPE_XBOX,
  KMTQAITYPE_INDEPENDENTFLIP_SUPPORT,
  KMTQAITYPE_MIRACASTCOMPANIONDRIVERNAME,
  KMTQAITYPE_PHYSICALADAPTERCOUNT,
  KMTQAITYPE_PHYSICALADAPTERDEVICEIDS,
  KMTQAITYPE_DRIVERCAPS_EXT,
  KMTQAITYPE_QUERY_MIRACAST_DRIVER_TYPE,
  KMTQAITYPE_QUERY_GPUMMU_CAPS,
  KMTQAITYPE_QUERY_MULTIPLANEOVERLAY_DECODE_SUPPORT,
  KMTQAITYPE_QUERY_HW_PROTECTION_TEARDOWN_COUNT,
  KMTQAITYPE_QUERY_ISBADDRIVERFORHWPROTECTIONDISABLED,
  KMTQAITYPE_MULTIPLANEOVERLAY_SECONDARY_SUPPORT,
  KMTQAITYPE_INDEPENDENTFLIP_SECONDARY_SUPPORT,
  KMTQAITYPE_PANELFITTER_SUPPORT,
  KMTQAITYPE_PHYSICALADAPTERPNPKEY,
  KMTQAITYPE_GETSEGMENTGROUPSIZE,
  KMTQAITYPE_MPO3DDI_SUPPORT,
  KMTQAITYPE_HWDRM_SUPPORT,
  KMTQAITYPE_MPOKERNELCAPS_SUPPORT,
  KMTQAITYPE_MULTIPLANEOVERLAY_STRETCH_SUPPORT,
  KMTQAITYPE_GET_DEVICE_VIDPN_OWNERSHIP_INFO,
  KMTQAITYPE_QUERYREGISTRY,
  KMTQAITYPE_KMD_DRIVER_VERSION,
  KMTQAITYPE_BLOCKLIST_KERNEL,
  KMTQAITYPE_BLOCKLIST_RUNTIME,
  KMTQAITYPE_ADAPTERGUID_RENDER,
  KMTQAITYPE_ADAPTERADDRESS_RENDER,
  KMTQAITYPE_ADAPTERREGISTRYINFO_RENDER,
  KMTQAITYPE_CHECKDRIVERUPDATESTATUS_RENDER,
  KMTQAITYPE_DRIVERVERSION_RENDER,
  KMTQAITYPE_ADAPTERTYPE_RENDER,
  KMTQAITYPE_WDDM_1_2_CAPS_RENDER,
  KMTQAITYPE_WDDM_1_3_CAPS_RENDER,
  KMTQAITYPE_QUERY_ADAPTER_UNIQUE_GUID,
  KMTQAITYPE_NODEPERFDATA,
  KMTQAITYPE_ADAPTERPERFDATA,
  KMTQAITYPE_ADAPTERPERFDATA_CAPS,
  KMTQUITYPE_GPUVERSION,
  KMTQAITYPE_DRIVER_DESCRIPTION,
  KMTQAITYPE_DRIVER_DESCRIPTION_RENDER,
  KMTQAITYPE_SCANOUT_CAPS
} KMTQUERYADAPTERINFOTYPE;

typedef struct _D3DKMT_QUERYADAPTERINFO {
  D3DKMT_HANDLE           hAdapter;
  KMTQUERYADAPTERINFOTYPE Type;
  VOID                    *pPrivateDriverData;
  UINT                    PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;
typedef _Check_return_ NTSTATUS(APIENTRY *PFN_D3DKMTQueryAdapterInfo)(const D3DKMT_QUERYADAPTERINFO*);

typedef enum _D3DDDI_QUERYREGISTRY_TYPE {
  D3DDDI_QUERYREGISTRY_SERVICEKEY,
  D3DDDI_QUERYREGISTRY_ADAPTERKEY,
  D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH,
  D3DDDI_QUERYREGISTRY_DRIVERIMAGEPATH,
  D3DDDI_QUERYREGISTRY_MAX
} D3DDDI_QUERYREGISTRY_TYPE;

typedef struct _D3DDDI_QUERYREGISTRY_FLAGS {
  union {
    struct {
      UINT TranslatePath : 1;
      UINT MutableValue : 1;
      UINT Reserved : 30;
    };
    UINT Value;
  };
} D3DDDI_QUERYREGISTRY_FLAGS;

typedef enum _D3DDDI_QUERYREGISTRY_STATUS {
  D3DDDI_QUERYREGISTRY_STATUS_SUCCESS,
  D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW,
  D3DDDI_QUERYREGISTRY_STATUS_FAIL,
  D3DDDI_QUERYREGISTRY_STATUS_MAX
} D3DDDI_QUERYREGISTRY_STATUS;

typedef struct _D3DDDI_QUERYREGISTRY_INFO {
  D3DDDI_QUERYREGISTRY_TYPE   QueryType;
  D3DDDI_QUERYREGISTRY_FLAGS  QueryFlags;
  WCHAR                       ValueName[MAX_PATH];
  ULONG                       ValueType;
  ULONG                       PhysicalAdapterIndex;
  ULONG                       OutputValueSize;
  D3DDDI_QUERYREGISTRY_STATUS Status;
  union {
    DWORD  OutputDword;
    UINT64 OutputQword;
    WCHAR  OutputString[1];
    BYTE   OutputBinary[1];
  };
} D3DDDI_QUERYREGISTRY_INFO;
"""
    yield line

if __name__ == "__main__":
  FileEditor("vendor/gli/gli/core/save_dds.inl").run(gliFilter)
  FileEditor("vendor/spirv_cross/spirv_common.hpp").run(spirvCommonFilter)
  FileEditor("vendor/vulkan-loader/loader/vulkan-1.def").run(windefFilter)
  FileEditor("vendor/vulkan-loader/loader/loader.c").run(wdkFilter)
  FileEditor("vendor/vulkan-validationlayers/layers/gpu_validation.cpp").run(gpuvalidationFilter)
