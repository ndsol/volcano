#!/bin/bash
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
set -e
if [ -n "$BOOTSTRAP_ON_TRAVIS_J" ]; then
  # Echo bash commands as they are executed
  set -v
fi

cd -P -- "$(dirname -- "$0")"
cd ../.. # build-posix.sh is in src/gn dir, cd to repo root (without using git)
missing=""

lscolor="ls --color"
if [ "$(uname)" == "Darwin" ]; then
  lscolor="ls -G"
fi

if [ "$(uname)" == "Darwin" ] && ! which brew >/dev/null 2>&1; then
  # macOS build without homebrew
  installer="macos"
  if [ -n "$CC_WRAPPER" ]; then
    if ! which $CC_WRAPPER >/dev/null 2>&1; then
      echo "macos build: CC_WRAPPER=$CC_WRAPPER"
      echo "             ^ Command not found"
      exit 1
    fi
  fi

  if ! which g++ >/dev/null 2>&1; then
    echo "XCode or another compiler is required."
    echo ""
    echo "Hint: the 'xcode-select --install' command installs Apple Clang."
    exit 1
  fi
  if ! g++ --version >/dev/null 2>&1; then
    echo "XCode incorrectly installed. Please try running:"
    echo "    g++ --version"
    echo ""
    echo "You may need to run 'xcode-select --install' again."
    exit 1
  fi
else
  # have a few basic utilities been installed?
  installer=""
  for p in apt-get pacman emerge dnf yum brew port; do
    if which $p >/dev/null 2>&1; then
      installer="$p"
      break
    fi
  done
  if [ -z "$installer" ]; then
    if [ "$(uname)" == "Darwin" ]; then
      echo "ERROR: which brew # haha, but why did it succeed then the second time, fail"
      exit 1
    fi
    echo "WARNING: Your system uses an unknown install method."
    installer="unknown"
  fi
fi

git_aptget="git"
git_pacman="git"
git_emerge="dev-vcs/git"
git_dnf="git"
git_yum="git"
git_brew="git"
git_port="git"
python3_aptget="python3"
python3_pacman="python3"
python3_emerge="dev-lang/python-3"
python3_dnf="python3"
python3_yum="python3"
python3_brew="python3"
python3_port="python3"
patch_aptget="patch"
patch_pacman="patch"
patch_emerge="sys-devel/patch"
patch_dnf="patch"
patch_yum="patch"
patch_brew="patch"
patch_port="patch"
pkgconfig_aptget="pkg-config"
pkgconfig_pacman="pkg-config"
pkgconfig_emerge="dev-util/pkgconfig"
pkgconfig_dnf="pkgconfig"
pkgconfig_yum="pkgconfig"
pkgconfig_brew="pkg-config"
pkgconfig_port="pkg-config"
gcc_cpp_aptget="build-essential"
gcc_cpp_pacman="base-devel"
gcc_cpp_emerge="gcc"
gcc_cpp_dnf="gcc-c++"
gcc_cpp_yum="gcc-c++"
gcc_cpp_brew="gcc"
gcc_cpp_port="gcc"
clang_format_aptget="clang-format"
clang_format_pacman="clang"
clang_format_emerge="sys-devel/llvm"
clang_format_dnf="clang"
clang_format_yum="clang"
clang_format_brew="clang-format"
clang_format_port="clang-format"
fontconfig_aptget="libfontconfig1-dev"
fontconfig_pacman="fontconfig"
fontconfig_emerge="media-libs/fontconfig"
fontconfig_dnf="fontconfig-devel"
fontconfig_yum="fontconfig-devel"
fontconfig_brew="fontconfig"
fontconfig_port="fontconfig"
icu_aptget="libicu-dev"
icu_pacman="icu"
icu_emerge="icu"
icu_dnf="libicu-devel"
icu_yum="libicu-devel"
icu4c_brew="icu4c"
icu4c_port="icu4c"
mesagl_aptget="libgl1-mesa-dev"
mesagl_pacman="mesagl"
mesagl_emerge="eselect-opengl"
mesagl_dnf="mesa-libGL-devel"
mesagl_yum="mesa-libGL-devel"
mesaglu_aptget="libglu1-mesa-dev"
mesaglu_pacman="glu"
mesaglu_emerge="mesa-libs/glu"
mesaglu_dnf="mesa-libGLU-devel"
mesaglu_yum="mesa-libGLU-devel"
xrandr_aptget="libxrandr-dev"
xrandr_pacman="xorg-xrandr"
xrandr_emerge="x11-apps/xrandr"
xrandr_dnf="libXrandr-devel"
xrandr_yum="libXrandr-devel"
xcursor_aptget="libxcursor-dev"
xcursor_pacman="libxcursor"
xcursor_emerge="x11-libs/libXcursor"
xcursor_dnf="libXcursor-devel"
xcursor_yum="libXcursor-devel"
xinerama_aptget="libxinerama-dev"
xinerama_pacman="libxinerama"
xinerama_emerge="x11-libs/libXinerama"
xinerama_dnf="libXinerama-devel"
xinerama_yum="libXinerama-devel"
xinput2_aptget="libxi-dev"
xinput2_pacman="libxi"
xinput2_emerge="x11-apps/xinput"
xinput2_dnf="libXi-devel"
xinput2_yum="libXi-devel"

python="python3"

clist=""
for c in git $python patch pkg-config c++; do
  if ! which $c >/dev/null 2>&1; then
    if [ "$c" == "c++" ]; then
      # the + symbol is an invalid bash variable name
      clist="$clist gcc_cpp"
    elif [ "$c" == "pkg-config" ]; then
      # the - symbol is an invalid bash variable name
      if [ "$installer" == "macos" ]; then
        : # do nothing, pkgconfig is not available on macos
      else
        clist="$clist pkgconfig"
      fi
    else
      clist="$clist $c"
    fi
  fi
done

if [ "$installer" == "macos" ]; then
  # macOS without Homebrew
  clist=""
elif [ "$installer" == "brew" ] || [ "$installer" == "port" ]; then
  # Homebrew and MacPorts install .pc files in places that require setting
  # PKG_CONFIG_PATH:
  P="$PKG_CONFIG_PATH"
  if [ -n "$P" ]; then
    P="$P:"
  fi
  # use two dirname calls to remove "/bin"
  B=$(dirname $(dirname $(which $installer)))
  export PKG_CONFIG_PATH="$P$B/opt/icu4c/lib/pkgconfig"
  if ! pkg-config --exists icu-i18n; then
    clist="$clist icu4c"
  else
    CFLAGS="$CFLAGS $(pkg-config --cflags icu-i18n)"
    CXXFLAGS="$CXXFLAGS $(pkg-config --cflags icu-i18n)"
    LDFLAGS="$LDFLAGS $(pkg-config --libs-only-L icu-i18n) -L$B/lib"
  fi
else
  # Suppress all error output because pkg-config might not be installed.
  if ! pkg-config --exists fontconfig >/dev/null 2>&1; then
    clist="$clist fontconfig"
  fi
  if ! pkg-config --exists gl >/dev/null 2>&1; then
    clist="$clist mesagl"
  fi
  if ! pkg-config --exists glu >/dev/null 2>&1; then
    clist="$clist mesaglu"
  fi
  if ! pkg-config --exists xrandr >/dev/null 2>&1; then
    clist="$clist xrandr"
  fi
  if ! pkg-config --exists xcursor >/dev/null 2>&1; then
    clist="$clist xcursor"
  fi
  if ! pkg-config --exists xinerama >/dev/null 2>&1; then
    clist="$clist xinerama"
  fi
  if ! pkg-config --exists xi >/dev/null 2>&1; then
    clist="$clist xinput2"
  fi
  if ! pkg-config --exists icu-i18n >/dev/null 2>&1; then
    clist="$clist icu"
  fi
fi

if [ -n "$clist" ]; then
  case $installer in
  unknown)
    echo "Please install the following commands before continuing:"
    echo "  $clist"
    ;;
  *)
    pkglist=""
    for c in $clist; do
      i="${c}_${installer/-/}"
      pkglist="$pkglist ${!i}"
    done
    case $installer in
    apt-get|dnf|yum|brew|port)
      installer="$installer install"
      ;;
    pacman)
      installer="$installer -S"
      ;;
    esac
    echo "Please $installer$pkglist"
    exit 1
    ;;
  esac
fi

# Travis CI omits realpath from apt-get install coreutils.
if ! realpath . >/dev/null 2>&1; then
  realpath() {
    ( cd "$1" && echo "$PWD" )
  }
fi

# git rev-parse --show-superproject-working-tree was added in
# https://github.com/git/git/commit/bf0231c - emulate it knowing when .git is a
# *file*, not a dir this is a submodule.
SEARCH="$PWD"
set +e
while true; do
  GIT_TOPLEVEL=$(cd "$SEARCH" && git rev-parse --show-toplevel 2>/dev/null)
  RV=$?
  if [ $RV -ne 0 ]; then
    break
  fi
  if [ ! -f "$GIT_TOPLEVEL/.git" ]; then
    break
  fi
  SEARCH="$GIT_TOPLEVEL/.."
done
set -e

if [ $RV -ne 0 ]; then
  # The user downloaded the zip instead of git clone.
  git init
  git add .
  git config user.name "volcanoauthors"
  git config user.email "volcanoauthors@gmail.com"
  while read a b; do
    echo "  add submod \"update-index\" $b @ $a"
    git update-index --add --cacheinfo 160000 "$a" "$b"
  done <<EOF
51cbda5f30e56c801c07fe3d3aba5d7fb9e6cca4 vendor/cereal
53a665cc55cd8148227d63fbb8473fbd9b5135bf vendor/glfw
9eef54b2513ca6b40b47b07d24f453848b65c0df vendor/glslang
9a8f6e5ffc6676281a4389aa1503ba6c4352eaca vendor/skia
4ce04480ec5469fe7ebbdd66c3016090a704d81b vendor/spirv_cross
412c4d2e6153c389c7dab4a38e62f6ebacc7e408 vendor/subgn
23b2e8e64bdf3f25b3d73f1593e72977ebfcd39b vendor/vulkan-headers
fdc5ec43b00e03db432cb8b8bc9bdafc9599c522 vendor/vulkan-loader
afa429a656a0c5788b1a783bb90201e86e88fb4b vendor/vulkan-validationlayers
EOF
  git commit -q -m "repo was downloaded without .git, build.cmd rebuilt it"
fi

if [ -z "$VOLCANO_SKIP_CACHE" ]; then
  if [ -f vendor/skia/third_party/externals/zlib/CMakeLists.txt ]; then
    # All submodules have been fetched already, skip this step.
    VOLCANO_SKIP_CACHE=1
  fi
fi

if [ -z "$VOLCANO_SKIP_CACHE" ]; then
  if ! which curl >/dev/null 2>&1; then
    echo "Please install curl to use volcano-cache"
    VOLCANO_SKIP_CACHE=1
  fi
fi

cacheflags="zxf"
if [ -z "$VOLCANO_SKIP_CACHE" ]; then
  H=$(git log -1 --pretty='%H')
  U=https://media.githubusercontent.com/media/ndsol/volcano-cache/master/
  E=-cached.tar.gz
  echo "Using one download for git submodules..."
  if ! curl -LO $U$H$E
  then
    echo "Failed to download $U$H$E"
    VOLCANO_SKIP_CACHE=1
  elif [ ! -f "$H$E" ]; then
    echo "Cannot find $H$E"
    VOLCANO_SKIP_CACHE=1
  elif [ -f .git ]; then
    # is volcano a submodule of another repo?
    GITDIR=$(awk '{if ($0 ~ /^gitdir: /) {sub("^gitdir: ","");print}}' .git)
    if [ -z "$GITDIR" ]; then
      # There is a .git file (like as if this was a submodule) but it is broken
      echo "Found a file, not a dir. Is this a git checkout?"
      VOLCANO_SKIP_CACHE=1
    else
      # Unpack just the .git file but in a subdir
      echo "submodule: use dir=$GITDIR"
      mkdir save-git
      tar zxf "$H$E" -C save-git --anchored --exclude=vendor
      if [ ! -d save-git/.git ]; then
        echo "Unable to find .git dir in $H$E"
        rm -rf save-git
        VOLCANO_SKIP_CACHE=1
      else
        mv save-git/.git/modules $GITDIR/modules
        rm -rf save-git
        # Unpack the cache without trashing the .git file
        cacheflags="-x --anchored --exclude=.git -z -f"
      fi
    fi
  elif [ ! -d .git ]; then
    echo "Cannot find .git dir"
    VOLCANO_SKIP_CACHE=1
  fi
fi

if [ -z "$VOLCANO_SKIP_CACHE" ]; then
  echo "Unpacking git submodules..."
  if ! tar $cacheflags "$H$E"; then
    VOLCANO_SKIP_CACHE=1
  else
    ( cd vendor/skia
      for d in buildtools common \
        $(find third_party/externals -mindepth 1 -maxdepth 1)
      do
        ( cd $d && git checkout HEAD -- . ) || exit 1
      done
    ) || exit 1
    rm "$H$E"
    echo "Cache $H$E unpacked"
  fi
fi

if [ -n "$VOLCANO_SKIP_CACHE" -a -n "$H$E" -a -f "$H$E" ]; then
  if [ -z "$VOLCANO_KEEP_CACHE" ]; then
    rm "$H$E"
    echo "Failed, will try using git clone. Set VOLCANO_KEEP_CACHE to inspect"
  else
    echo "Failed, will try using git clone. Cache kept at $PWD/$H$E"
  fi
fi

SUBMODULE_CMD="git submodule update --init --recursive"
SUBT=$(mktemp -t volcano-build.XXXXXXXXXX)
set +e
( $SUBMODULE_CMD --depth=51 2>&1 && rm $SUBT ) | tee $SUBT
RV=$?
set -e
if [ -f $SUBT ]; then
  RV=1
fi
while [ $RV -ne 0 ]; do
  echo ""
  echo "WARN: $SUBMODULE_CMD --depth=51 # failed, deepening..."
  D=$(cat $SUBT | sed -e \
's/Unable to checkout.*in submodule path '"'"'\(.*\)'"'"'.*/\1/p;'\
's/Fetched in submodule path '"'"'\(.*\)'"'"'.*/\1/p;d')

  while read d; do
    if [ -z "$d" ]; then
      continue
    fi
    (
      echo "WARN: cd $d; git fetch --unshallow"
      cd "$d"
      git fetch --unshallow
    )
  done <<<"$D"
  if [ -z "$D" ]; then
    # Work around a git submodule bug: if the initial submodule update fails,
    # some submodules may not be checked out, only fetched.
    #
    # This should be a no-op for any successfully checked out submodule, only
    # forcing all submodules into a "detached HEAD" state.
    git submodule foreach --recursive git checkout HEAD@{0}
    break
  fi

  set +e
  # Do submodules twice due to a git bug
  ( $SUBMODULE_CMD 2>&1 && rm $SUBT ) | tee $SUBT
  RV=$?
  set -e
  # Repeat top-level while to see if "$D" is now empty.
  RV=1
  touch $SUBT
done
rm -f $SUBT
SUBT=

# has spirv_cross been patched?
if ! grep '__android_log_assert' \
    vendor/spirv_cross/spirv_cross_error_handling.hpp >/dev/null 2>&1
then
  missing="$missing patch-spirv-cross"
fi

# has vulkan-loader been patched?
if ! grep 'Remove duplicate paths, or it would result in duplicate extensions,' \
    vendor/vulkan-loader/loader/loader.c >/dev/null 2>&1
then
  missing="$missing patch-vulkan"
fi

if ! grep -q 'CEREAL_CUSTOM_CEREALABORT' \
    vendor/cereal/include/cereal/macros.hpp
then
  missing="$missing patch-cereal"
fi

missing="$missing git-sync-deps install-gn-ninja"

echo "volcano updating:$missing"

for buildstep in $missing; do
  case $buildstep in
  patch-spirv-cross)
    patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/spirv_cross/spirv_cross_error_handling.hpp
+++ b/vendor/spirv_cross/spirv_cross_error_handling.hpp
@@ -21,6 +21,9 @@
 #include <stdio.h>
 #include <stdlib.h>
 #include <string>
+#ifdef __ANDROID__
+#include <android/log.h>
+#endif
 
 #ifdef SPIRV_CROSS_NAMESPACE_OVERRIDE
 #define SPIRV_CROSS_NAMESPACE SPIRV_CROSS_NAMESPACE_OVERRIDE
@@ -37,6 +40,9 @@ namespace SPIRV_CROSS_NAMESPACE
 inline void
 report_and_abort(const std::string &msg)
 {
+#ifdef __ANDROID__
+	__android_log_assert(__FILE__, "volcano", "spirv_cross error: %s\n", msg.c_str());
+#else
 #ifdef NDEBUG
 	(void)msg;
 #else
@@ -44,6 +50,7 @@ report_and_abort(const std::string &msg)
 #endif
 	fflush(stderr);
 	abort();
+#endif
 }
 
 #define SPIRV_CROSS_THROW(x) report_and_abort(x)
EOF
    ;;

  patch-vulkan)
    patch --no-backup-if-mismatch -N -r - -p1 <<EOF
--- a/vendor/vulkan-validationlayers/scripts/lvl_genvk.py
+++ b/vendor/vulkan-validationlayers/scripts/lvl_genvk.py
@@ -184,7 +184,7 @@ def makeGenOpts(args):
             apientryp         = 'VKAPI_PTR *',
             alignFuncParam    = 48,
             expandEnumerants  = False,
-            valid_usage_path  = args.scripts)
+            valid_usage_path  = os.path.join(os.path.dirname(os.path.abspath(__file__)), args.scripts))
           ]
 
     # Options for stateless validation source file
@@ -208,7 +208,7 @@ def makeGenOpts(args):
             apientryp         = 'VKAPI_PTR *',
             alignFuncParam    = 48,
             expandEnumerants  = False,
-            valid_usage_path  = args.scripts)
+            valid_usage_path  = os.path.join(os.path.dirname(os.path.abspath(__file__)), args.scripts))
           ]
 
     # Options for object_tracker code-generated validation routines
@@ -233,7 +233,7 @@ def makeGenOpts(args):
             apientryp         = 'VKAPI_PTR *',
             alignFuncParam    = 48,
             expandEnumerants  = False,
-            valid_usage_path  = args.scripts)
+            valid_usage_path  = os.path.join(os.path.dirname(os.path.abspath(__file__)), args.scripts))
         ]
 
     # Options for object_tracker code-generated prototypes
@@ -258,7 +258,7 @@ def makeGenOpts(args):
             apientryp         = 'VKAPI_PTR *',
             alignFuncParam    = 48,
             expandEnumerants  = False,
-            valid_usage_path  = args.scripts)
+            valid_usage_path  = os.path.join(os.path.dirname(os.path.abspath(__file__)), args.scripts))
         ]
 
     # Options for dispatch table helper generator
--- a/vendor/vulkan-validationlayers/layers/gpu_validation.cpp
+++ b/vendor/vulkan-validationlayers/layers/gpu_validation.cpp
@@ -717,6 +717,12 @@ bool CoreChecks::GpuPreCallCreateShaderModule(const VkShaderModuleCreateInfo *pC
     return pass;
 }
 
+// Deprecated constants no longer found in spirv-tools/instrument.hpp:
+static constexpr int kInstTessOutInvocationId = spvtools::kInstCommonOutCnt;
+static constexpr int kInstCompOutGlobalInvocationId = spvtools::kInstCommonOutCnt;
+static constexpr int kInstBindlessOutDescIndex = spvtools::kInstStageOutCnt + 1;
+static constexpr int kInstBindlessOutDescBound = spvtools::kInstStageOutCnt + 2;
+
 // Generate the stage-specific part of the message.
 static void GenerateStageMessage(const uint32_t *debug_record, std::string &msg) {
     using namespace spvtools;
--- a/vendor/vulkan-loader/loader/loader.c
+++ b/vendor/vulkan-loader/loader/loader.c
@@ -70,10 +70,6 @@
 #include <initguid.h>
 #include <devpkey.h>
 #include <winternl.h>
-#include <d3dkmthk.h>
-
-typedef _Check_return_ NTSTATUS (APIENTRY *PFN_D3DKMTEnumAdapters2)(const D3DKMT_ENUMADAPTERS2*);
-typedef _Check_return_ NTSTATUS (APIENTRY *PFN_D3DKMTQueryAdapterInfo)(const D3DKMT_QUERYADAPTERINFO*);
 #endif
 
 // This is a CMake generated file with #defines for any functions/includes
@@ -3769,6 +3769,38 @@ static VkResult ReadDataFilesInSearchPaths(const struct loader_instance *inst, e
         loader_log(inst, VK_DEBUG_REPORT_DEBUG_BIT_EXT, 0,
                    "ReadDataFilesInSearchPaths: Searching the following paths for manifest files: %s\\n", search_path);
     }
+    // Remove duplicate paths, or it would result in duplicate extensions, duplicate devices, etc.
+    // This uses minimal memory, but is O(N^2) on the number of paths. Expect only a few paths.
+    char path_sep_str[2] = {PATH_SEPARATOR, '\\0'};
+    // search_path_updated_size does not count the final null character.
+    size_t search_path_updated_size = strlen(search_path);
+    for (size_t first = 0; first < search_path_updated_size;) {
+        // If this is an empty path, erase it
+        if (search_path[first] == PATH_SEPARATOR) {
+            memmove(&search_path[first], &search_path[first + 1], search_path_updated_size - first);
+            search_path_updated_size -= 1;
+            continue;
+        }
+        size_t first_end = first + strcspn(&search_path[first], path_sep_str);
+        for (size_t second = first_end + 1; second < search_path_updated_size;) {
+            size_t second_end = second + strcspn(&search_path[second], path_sep_str);
+            if (first_end - first == second_end - second &&
+                !strncmp(&search_path[first], &search_path[second], second_end - second)) {
+                // Found duplicate. Include PATH_SEPARATOR in erasure.
+                if (search_path[second_end] == PATH_SEPARATOR) {
+                    second_end++;
+                }
+                memmove(&search_path[second], &search_path[second_end],
+                        search_path_updated_size - second_end
+                        + 1 /*include null*/);
+                search_path_updated_size -= second_end - second;
+            } else {
+                second = second_end + 1;
+            }
+        }
+        first = first_end + 1;
+    }
+    search_path_size = search_path_updated_size;
 
     // Now, parse the paths and add any manifest files found in them.
     vk_result = AddDataFilesInPath(inst, search_path, is_directory_list, out_files);
@@ -3810,6 +3806,142 @@ out:
 }
 
 #ifdef _WIN32
+// Header information from docs.microsoft.com as of 2019-08-19.
+typedef UINT D3DKMT_HANDLE;
+
+typedef struct _D3DKMT_ADAPTERINFO {
+  D3DKMT_HANDLE hAdapter;
+  LUID          AdapterLuid;
+  ULONG         NumOfSources;
+  BOOL          bPresentMoveRegionsPreferred;
+} D3DKMT_ADAPTERINFO;
+
+typedef struct _D3DKMT_ENUMADAPTERS2 {
+  ULONG              NumAdapters;
+  D3DKMT_ADAPTERINFO *pAdapters;
+} D3DKMT_ENUMADAPTERS2;
+typedef _Check_return_ NTSTATUS(APIENTRY *PFN_D3DKMTEnumAdapters2)(const D3DKMT_ENUMADAPTERS2*);
+
+typedef enum _KMTQUERYADAPTERINFOTYPE {
+  KMTQAITYPE_UMDRIVERPRIVATE,
+  KMTQAITYPE_UMDRIVERNAME,
+  KMTQAITYPE_UMOPENGLINFO,
+  KMTQAITYPE_GETSEGMENTSIZE,
+  KMTQAITYPE_ADAPTERGUID,
+  KMTQAITYPE_FLIPQUEUEINFO,
+  KMTQAITYPE_ADAPTERADDRESS,
+  KMTQAITYPE_SETWORKINGSETINFO,
+  KMTQAITYPE_ADAPTERREGISTRYINFO,
+  KMTQAITYPE_CURRENTDISPLAYMODE,
+  KMTQAITYPE_MODELIST,
+  KMTQAITYPE_CHECKDRIVERUPDATESTATUS,
+  KMTQAITYPE_VIRTUALADDRESSINFO,
+  KMTQAITYPE_DRIVERVERSION,
+  KMTQAITYPE_ADAPTERTYPE,
+  KMTQAITYPE_OUTPUTDUPLCONTEXTSCOUNT,
+  KMTQAITYPE_WDDM_1_2_CAPS,
+  KMTQAITYPE_UMD_DRIVER_VERSION,
+  KMTQAITYPE_DIRECTFLIP_SUPPORT,
+  KMTQAITYPE_MULTIPLANEOVERLAY_SUPPORT,
+  KMTQAITYPE_DLIST_DRIVER_NAME,
+  KMTQAITYPE_WDDM_1_3_CAPS,
+  KMTQAITYPE_MULTIPLANEOVERLAY_HUD_SUPPORT,
+  KMTQAITYPE_WDDM_2_0_CAPS,
+  KMTQAITYPE_NODEMETADATA,
+  KMTQAITYPE_CPDRIVERNAME,
+  KMTQAITYPE_XBOX,
+  KMTQAITYPE_INDEPENDENTFLIP_SUPPORT,
+  KMTQAITYPE_MIRACASTCOMPANIONDRIVERNAME,
+  KMTQAITYPE_PHYSICALADAPTERCOUNT,
+  KMTQAITYPE_PHYSICALADAPTERDEVICEIDS,
+  KMTQAITYPE_DRIVERCAPS_EXT,
+  KMTQAITYPE_QUERY_MIRACAST_DRIVER_TYPE,
+  KMTQAITYPE_QUERY_GPUMMU_CAPS,
+  KMTQAITYPE_QUERY_MULTIPLANEOVERLAY_DECODE_SUPPORT,
+  KMTQAITYPE_QUERY_HW_PROTECTION_TEARDOWN_COUNT,
+  KMTQAITYPE_QUERY_ISBADDRIVERFORHWPROTECTIONDISABLED,
+  KMTQAITYPE_MULTIPLANEOVERLAY_SECONDARY_SUPPORT,
+  KMTQAITYPE_INDEPENDENTFLIP_SECONDARY_SUPPORT,
+  KMTQAITYPE_PANELFITTER_SUPPORT,
+  KMTQAITYPE_PHYSICALADAPTERPNPKEY,
+  KMTQAITYPE_GETSEGMENTGROUPSIZE,
+  KMTQAITYPE_MPO3DDI_SUPPORT,
+  KMTQAITYPE_HWDRM_SUPPORT,
+  KMTQAITYPE_MPOKERNELCAPS_SUPPORT,
+  KMTQAITYPE_MULTIPLANEOVERLAY_STRETCH_SUPPORT,
+  KMTQAITYPE_GET_DEVICE_VIDPN_OWNERSHIP_INFO,
+  KMTQAITYPE_QUERYREGISTRY,
+  KMTQAITYPE_KMD_DRIVER_VERSION,
+  KMTQAITYPE_BLOCKLIST_KERNEL,
+  KMTQAITYPE_BLOCKLIST_RUNTIME,
+  KMTQAITYPE_ADAPTERGUID_RENDER,
+  KMTQAITYPE_ADAPTERADDRESS_RENDER,
+  KMTQAITYPE_ADAPTERREGISTRYINFO_RENDER,
+  KMTQAITYPE_CHECKDRIVERUPDATESTATUS_RENDER,
+  KMTQAITYPE_DRIVERVERSION_RENDER,
+  KMTQAITYPE_ADAPTERTYPE_RENDER,
+  KMTQAITYPE_WDDM_1_2_CAPS_RENDER,
+  KMTQAITYPE_WDDM_1_3_CAPS_RENDER,
+  KMTQAITYPE_QUERY_ADAPTER_UNIQUE_GUID,
+  KMTQAITYPE_NODEPERFDATA,
+  KMTQAITYPE_ADAPTERPERFDATA,
+  KMTQAITYPE_ADAPTERPERFDATA_CAPS,
+  KMTQUITYPE_GPUVERSION,
+  KMTQAITYPE_DRIVER_DESCRIPTION,
+  KMTQAITYPE_DRIVER_DESCRIPTION_RENDER,
+  KMTQAITYPE_SCANOUT_CAPS
+} KMTQUERYADAPTERINFOTYPE;
+
+typedef struct _D3DKMT_QUERYADAPTERINFO {
+  D3DKMT_HANDLE           hAdapter;
+  KMTQUERYADAPTERINFOTYPE Type;
+  VOID                    *pPrivateDriverData;
+  UINT                    PrivateDriverDataSize;
+} D3DKMT_QUERYADAPTERINFO;
+typedef _Check_return_ NTSTATUS(APIENTRY *PFN_D3DKMTQueryAdapterInfo)(const D3DKMT_QUERYADAPTERINFO*);
+
+typedef enum _D3DDDI_QUERYREGISTRY_TYPE {
+  D3DDDI_QUERYREGISTRY_SERVICEKEY,
+  D3DDDI_QUERYREGISTRY_ADAPTERKEY,
+  D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH,
+  D3DDDI_QUERYREGISTRY_DRIVERIMAGEPATH,
+  D3DDDI_QUERYREGISTRY_MAX
+} D3DDDI_QUERYREGISTRY_TYPE;
+
+typedef struct _D3DDDI_QUERYREGISTRY_FLAGS {
+  union {
+    struct {
+      UINT TranslatePath : 1;
+      UINT MutableValue : 1;
+      UINT Reserved : 30;
+    };
+    UINT Value;
+  };
+} D3DDDI_QUERYREGISTRY_FLAGS;
+
+typedef enum _D3DDDI_QUERYREGISTRY_STATUS {
+  D3DDDI_QUERYREGISTRY_STATUS_SUCCESS,
+  D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW,
+  D3DDDI_QUERYREGISTRY_STATUS_FAIL,
+  D3DDDI_QUERYREGISTRY_STATUS_MAX
+} D3DDDI_QUERYREGISTRY_STATUS;
+
+typedef struct _D3DDDI_QUERYREGISTRY_INFO {
+  D3DDDI_QUERYREGISTRY_TYPE   QueryType;
+  D3DDDI_QUERYREGISTRY_FLAGS  QueryFlags;
+  WCHAR                       ValueName[MAX_PATH];
+  ULONG                       ValueType;
+  ULONG                       PhysicalAdapterIndex;
+  ULONG                       OutputValueSize;
+  D3DDDI_QUERYREGISTRY_STATUS Status;
+  union {
+    DWORD  OutputDword;
+    UINT64 OutputQword;
+    WCHAR  OutputString[1];
+    BYTE   OutputBinary[1];
+  };
+} D3DDDI_QUERYREGISTRY_INFO;
+
 // Read manifest JSON files uing the Windows driver interface
 static VkResult ReadManifestsFromD3DAdapters(const struct loader_instance *inst, char **reg_data, PDWORD reg_data_size,
                                              const wchar_t *value_name) {
--- a/vendor/vulkan-loader/scripts/helper_file_generator.py
+++ b/vendor/vulkan-loader/scripts/helper_file_generator.py
@@ -376,7 +376,7 @@ class HelperFileOutputGenerator(OutputGenerator):
         outstring += '{\n'
         outstring += '    switch ((%s)input_value)\n' % groupName
         outstring += '    {\n'
-        for item in value_list:
+        for item in sorted(value_list):
             outstring += '        case %s:\n' % item
             outstring += '            return "%s";\n' % item
         outstring += '        default:\n'
EOF
    ;;

  git-sync-deps)
    set +e
    (
      touch git-sync-deps-pid
      (
        $lscolor vendor/skia/tools/git-sync-deps
        cd vendor/skia
        $python tools/git-sync-deps
      )
      RV=$?

      # patch imgui if needed
      # Note: this must be done after git-sync-deps
      grep -q 'if (g.OpenPopupStack.Size < current_stack_size + 1)' \
          vendor/skia/third_party/externals/imgui/imgui.cpp && \
      patch --no-backup-if-mismatch -N -r - -p1 <<EOF >/dev/null || true
--- a/vendor/skia/third_party/externals/imgui/imgui.cpp
+++ b/vendor/skia/third_party/externals/imgui/imgui.cpp
@@ -6812,7 +6812,7 @@ void ImGui::OpenPopupEx(ImGuiID id)
     popup_ref.OpenMousePos = IsMousePosValid(&g.IO.MousePos) ? g.IO.MousePos : popup_ref.OpenPopupPos;
 
     //IMGUI_DEBUG_LOG("OpenPopupEx(0x%08X)\n", g.FrameCount, id);
-    if (g.OpenPopupStack.Size < current_stack_size + 1)
+    if (g.OpenPopupStack.Size <= current_stack_size)
     {
         g.OpenPopupStack.push_back(popup_ref);
     }
--- a/vendor/skia/third_party/externals/imgui/imgui_draw.cpp
+++ b/vendor/skia/third_party/externals/imgui/imgui_draw.cpp
@@ -2353,7 +2353,7 @@ void ImFont::BuildLookupTable()
 
     FallbackGlyph = FindGlyphNoFallback(FallbackChar);
     FallbackAdvanceX = FallbackGlyph ? FallbackGlyph->AdvanceX : 0.0f;
-    for (int i = 0; i < max_codepoint + 1; i++)
+    for (int i = 0; i <= max_codepoint; i++)
         if (IndexAdvanceX[i] < 0.0f)
             IndexAdvanceX[i] = FallbackAdvanceX;
 }
--- a/vendor/skia/third_party/externals/imgui/imgui_widgets.cpp
+++ b/vendor/skia/third_party/externals/imgui/imgui_widgets.cpp
@@ -3206,11 +3206,11 @@ static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj, int pos, const Im
     IM_ASSERT(pos <= text_len);
 
     const int new_text_len_utf8 = ImTextCountUtf8BytesFromStr(new_text, new_text + new_text_len);
-    if (!is_resizable && (new_text_len_utf8 + obj->CurLenA + 1 > obj->BufCapacityA))
+    if (!is_resizable && (new_text_len_utf8 + obj->CurLenA >= obj->BufCapacityA))
         return false;
 
     // Grow internal buffer if needed
-    if (new_text_len + text_len + 1 > obj->TextW.Size)
+    if (new_text_len + text_len >= obj->TextW.Size)
     {
         if (!is_resizable)
             return false;
@@ -7447,7 +7447,7 @@ void ImGui::BeginColumns(const char* str_id, int columns_count, ImGuiColumnsFlag
     if (columns->Columns.Size == 0)
     {
         columns->Columns.reserve(columns_count + 1);
-        for (int n = 0; n < columns_count + 1; n++)
+        for (int n = 0; n <= columns_count; n++)
         {
             ImGuiColumnData column;
             column.OffsetNorm = n / (float)columns_count;
@@ -7593,7 +7593,7 @@ void ImGui::EndColumns()
         if (dragging_column != -1)
         {
             if (!columns->IsBeingResized)
-                for (int n = 0; n < columns->Count + 1; n++)
+                for (int n = 0; n <= columns->Count; n++)
                     columns->Columns[n].OffsetNormBeforeResize = columns->Columns[n].OffsetNorm;
             columns->IsBeingResized = is_being_resized = true;
             float x = GetDraggedColumnOffset(columns, dragging_column);
--- a/vendor/skia/third_party/externals/imgui/imstb_truetype.h
+++ b/vendor/skia/third_party/externals/imgui/imstb_truetype.h
@@ -3664,7 +3664,7 @@ static int stbtt_BakeFontBitmap_internal(unsigned char *data, int offset,  // fo
       chardata[i].xoff     = (float) x0;
       chardata[i].yoff     = (float) y0;
       x = x + gw + 1;
-      if (y+gh+1 > bottom_y)
+      if (y+gh >= bottom_y)
          bottom_y = y+gh+1;
    }
    return bottom_y;
--- a/vendor/skia/third_party/externals/imgui/imgui_demo.cpp
+++ b/vendor/skia/third_party/externals/imgui/imgui_demo.cpp
@@ -1959,7 +1959,7 @@ static void ShowDemoWindowLayout()
             ImGui::Button("Box", button_sz);
             float last_button_x2 = ImGui::GetItemRectMax().x;
             float next_button_x2 = last_button_x2 + style.ItemSpacing.x + button_sz.x; // Expected position if next button was on same line
-            if (n + 1 < buttons_count && next_button_x2 < window_visible_x2)
+            if (n < buttons_count - 1 && next_button_x2 < window_visible_x2)
                 ImGui::SameLine();
             ImGui::PopID();
         }
@@ -4491,7 +4491,7 @@ static void ShowExampleAppCustomRendering(bool* p_open)
             static ImVector<ImVec2> points;
             static bool adding_line = false;
             if (ImGui::Button("Clear")) points.clear();
-            if (points.Size >= 2) { ImGui::SameLine(); if (ImGui::Button("Undo")) { points.pop_back(); points.pop_back(); } }
+            if (points.Size >= 2) { ImGui::SameLine(); if (ImGui::Button("Undo")) { points.Size -= 2; } }
             ImGui::Text("Left-click and drag to add lines,\nRight-click to undo");
 
             // Here we are using InvisibleButton() as a convenience to 1) advance the cursor and 2) allows us to use IsItemHovered()
@@ -4521,11 +4521,10 @@ static void ShowExampleAppCustomRendering(bool* p_open)
                     points.push_back(mouse_pos_in_canvas);
                     adding_line = true;
                 }
-                if (ImGui::IsMouseClicked(1) && !points.empty())
+                if (ImGui::IsMouseClicked(1) && points.Size > 1)
                 {
                     adding_line = adding_preview = false;
-                    points.pop_back();
-                    points.pop_back();
+                    points.Size -= 2;
                 }
             }
             draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);      // clip lines within the canvas (if we resize it, etc.)
EOF
      set -e
      rm -f git-sync-deps-pid
      exit $RV
    ) &
    export GIT_SYNC_DEPS_PID=$!
    set -e
    ;;

  install-gn-ninja)
    # build gn and ninja from source for aarch64 (chromium omits aarch64), and
    # subgn and subninja have some additional features used in volcano.
    (
      cd vendor/subgn
      if [ ! -f out_bootstrap/build.ninja ]; then
        ./build.cmd
      fi
      cp subninja/ninja .
      ./ninja -C out_bootstrap gn
      cp out_bootstrap/gn .
    )
    if [ -n "$GIT_SYNC_DEPS_PID" ]; then
      [ -f git-sync-deps-pid ] && echo "git-sync-deps..."
      set +e
      wait $GIT_SYNC_DEPS_PID
      RV=$?
      set -e
      if [ $RV -ne 0 ]; then
        echo "git-sync-deps exited with code $RV"
        exit 1
      fi
    fi
    ;;

  patch-cereal)
    patch -p1 <<EOF
--- a/vendor/cereal/include/cereal/macros.hpp
+++ b/vendor/cereal/include/cereal/macros.hpp
@@ -44,6 +44,18 @@
 #ifndef CEREAL_MACROS_HPP_
 #define CEREAL_MACROS_HPP_
 
+#ifndef CEREAL_CUSTOM_CEREALABORT
+#include <stdlib.h>
+inline void cerealAbortFn(const char* file, unsigned long line, const char* str) {
+  fprintf(stderr, "%s:%lu: %s\n", file, line, str);
+  abort();
+}
+inline void cerealAbortFn(const char* file, unsigned long line, const std::string& str) {
+  cerealAbortFn(file, line, str.c_str());
+}
+#define cerealAbort(x) cerealAbortFn(__FILE__, __LINE__, x);
+#endif // cerealAbort
+
 #ifndef CEREAL_THREAD_SAFE
 //! Whether cereal should be compiled for a threaded environment
 /*! This macro causes cereal to use mutexes to control access to
EOF
    find vendor/cereal/include -type f -exec \
        sed -i -e 's/throw Exception(/cerealAbort(/' {} +
    # macOS sed leaves files behind
    find vendor/cereal/include -name '*-e' -exec rm {} +
    ;;
  *)
    echo "ERROR: unknown buildstep \"$buildstep\""
    exit 1
  esac
done

if [ "$(uname)" == "Darwin" ]; then
  # macOS 'patch' makes a mess of the "-r -" arg
  rm -f -- -
  # macOS: clone MoltenVK
  (
    cd vendor/vulkan-loader
    if [ ! -d MoltenVK ]; then
      echo "git clone https://github.com/KhronosGroup/MoltenVK"
      git clone https://github.com/KhronosGroup/MoltenVK
    fi
    cd MoltenVK
    MVK_COMMIT=1182d822a6c194938af73837f51e2621a986d7b5
    if [ "$(git log -1 --pretty='%H')" != "$MVK_COMMIT" ]; then
      if ! git checkout $MVK_COMMIT; then
        echo "MoltenVK: git checkout $MVK_COMMIT failed, falling back to fetch"
        git fetch origin
        git checkout $MVK_COMMIT
      fi
    fi
    # Patch MoltenVK
    if grep -q 'library_path": "\./libMoltenVK\.dylib' \
        MoltenVK/icd/MoltenVK_icd.json; then
      sed -i -e 's0\("library_path": "\)\./\(libMoltenVK\.dylib",\)$0\1\20' \
        MoltenVK/icd/MoltenVK_icd.json
      for a in \
        MoltenVKShaderConverter/MoltenVKSPIRVToMSLConverter/{SPIRVToMSLConverter,SPIRVReflection}.{h,cpp} \
        MoltenVKShaderConverter/Common/SPIRVSupport.cpp
      do
        sed -i -e 's0#include <SPIRV-Cross/0#include <0' $a
        # macOS sed leaves files behind
        rm -f ${a}-e
      done
      # macOS sed leaves files behind
      rm -f MoltenVK/icd/MoltenVK_icd.json-e
      patch --no-backup-if-mismatch -sN -r - -p1 <<EOF
--- a/MoltenVK/MoltenVK/API/mvk_vulkan.h
+++ b/MoltenVK/MoltenVK/API/mvk_vulkan.h
@@ -40,6 +40,6 @@
 #endif
 
 #include <vulkan/vulkan.h>
-#include <vulkan-portability/vk_extx_portability_subset.h>
+#include <../../vulkan-portability/include/vulkan/vk_extx_portability_subset.h>
 
 #endif
--- a/MoltenVK/MoltenVK/API/vk_mvk_moltenvk.h
+++ b/MoltenVK/MoltenVK/API/vk_mvk_moltenvk.h
@@ -58,6 +58,16 @@ typedef unsigned long MTLLanguageVersion;
 #define VK_MVK_MOLTENVK_SPEC_VERSION            20
 #define VK_MVK_MOLTENVK_EXTENSION_NAME          "VK_MVK_moltenvk"
 
+#define VK_MVK_STRUCTURE_TYPE_OBJECT_RETURN ((VkStructureType)1 /*aliased to INSTANCE_CREATE_INFO*/)
+typedef struct VkMVKObjectReturn {
+	VkStructureType sType;
+	const void* pNext;
+
+	// If this structure is present in VkImageCreateInfo pNext chain,
+	// MoltenVK will write a void* to the location given in 'handle'.
+	void** handle;
+} VkMVKObjectReturn;
+
 /**
  * MoltenVK configuration settings.
  *
--- a/MoltenVK/MoltenVK/GPUObjects/MVKDevice.h
+++ b/MoltenVK/MoltenVK/GPUObjects/MVKDevice.h
@@ -602,7 +602,9 @@ public:
 	const VkPhysicalDevice8BitStorageFeaturesKHR _enabledStorage8Features;
 	const VkPhysicalDeviceFloat16Int8FeaturesKHR _enabledF16I8Features;
 	const VkPhysicalDeviceVariablePointerFeatures _enabledVarPtrFeatures;
+#ifdef VK_EXT_host_query_reset
 	const VkPhysicalDeviceHostQueryResetFeaturesEXT _enabledHostQryResetFeatures;
+#endif
 	const VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT _enabledVtxAttrDivFeatures;
 	const VkPhysicalDevicePortabilitySubsetFeaturesEXTX _enabledPortabilityFeatures;
 
--- a/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm
+++ b/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm
@@ -94,11 +94,13 @@ void MVKPhysicalDevice::getFeatures(VkPhysicalDeviceFeatures2* features) {
                     varPtrFeatures->variablePointers = true;
                     break;
                 }
+#ifdef VK_EXT_host_query_reset
                 case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT: {
                     auto* hostQueryResetFeatures = (VkPhysicalDeviceHostQueryResetFeaturesEXT*)next;
                     hostQueryResetFeatures->hostQueryReset = true;
                     break;
                 }
+#endif
                 case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT: {
                     auto* divisorFeatures = (VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*)next;
                     divisorFeatures->vertexAttributeInstanceRateDivisor = true;
@@ -659,6 +661,7 @@ VkResult MVKPhysicalDevice::getPhysicalDeviceMemoryProperties(VkPhysicalDeviceMe
 		auto* next = (MVKVkAPIStructHeader*)pMemoryProperties->pNext;
 		while (next) {
 			switch ((uint32_t)next->sType) {
+#ifdef VK_EXT_memory_budget
 			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT: {
 				auto* budgetProps = (VkPhysicalDeviceMemoryBudgetPropertiesEXT*)next;
 				memset(budgetProps->heapBudget, 0, sizeof(budgetProps->heapBudget));
@@ -670,6 +673,7 @@ VkResult MVKPhysicalDevice::getPhysicalDeviceMemoryProperties(VkPhysicalDeviceMe
 				next = (MVKVkAPIStructHeader*)budgetProps->pNext;
 				break;
 			}
+#endif
 			default:
 				next = (MVKVkAPIStructHeader*)next->pNext;
 				break;
@@ -790,6 +794,7 @@ void MVKPhysicalDevice::initMetalFeatures() {
 		_metalFeatures.presentModeImmediate = true;
     }
 
+#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v4] ) {
         _metalFeatures.mslVersionEnum = MTLLanguageVersion2_1;
         _metalFeatures.multisampleArrayTextures = true;
@@ -800,6 +805,7 @@ void MVKPhysicalDevice::initMetalFeatures() {
 	if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily2_v1] ) {
 		_metalFeatures.multisampleLayeredRendering = _metalFeatures.layeredRendering;
 	}
+#endif
 
 #endif
 
@@ -1376,7 +1382,11 @@ MTLFeatureSet MVKPhysicalDevice::getHighestMTLFeatureSet() {
 #endif
 
 #if MVK_MACOS
+#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
 	MTLFeatureSet maxFS = MTLFeatureSet_macOS_GPUFamily2_v1;
+#else
+	MTLFeatureSet maxFS = MTLFeatureSet_macOS_GPUFamily1_v3;
+#endif
 	MTLFeatureSet minFS = MTLFeatureSet_macOS_GPUFamily1_v1;
 #endif
 
@@ -1393,7 +1403,7 @@ MTLFeatureSet MVKPhysicalDevice::getHighestMTLFeatureSet() {
 // Retrieve the SPIRV-Cross Git revision hash from a derived header file that was created in the fetchDependencies script.
 uint64_t MVKPhysicalDevice::getSpirvCrossRevision() {
 
-#include <SPIRV-Cross/mvkSpirvCrossRevisionDerived.h>
+static const char* spirvCrossRevisionString = "$( cd ../../spirv_cross; git log -1 --pretty='%H' )";
 
 	static const string revStr(spirvCrossRevisionString, 0, 16);	// We just need the first 16 chars
 	static const string lut("0123456789ABCDEF");
@@ -1554,9 +1564,11 @@ void MVKPhysicalDevice::logGPUInfo() {
 #endif
 
 #if MVK_MACOS
+#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
 	if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily2_v1] ) { logMsg += "\\n\\t\\tmacOS GPU Family 2 v1"; }
 
 	if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v4] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v4"; }
+#endif
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v3] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v3"; }
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v2] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v2"; }
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v1] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v1"; }
@@ -1687,7 +1699,19 @@ void MVKDevice::destroyBufferView(MVKBufferView* mvkBuffView,
 
 MVKImage* MVKDevice::createImage(const VkImageCreateInfo* pCreateInfo,
 								 const VkAllocationCallbacks* pAllocator) {
-	return (MVKImage*)addResource(new MVKImage(this, pCreateInfo));
+	MVKImage* r = (MVKImage*)addResource(new MVKImage(this, pCreateInfo));
+
+	// Search for VK_MVK_STRUCTURE_TYPE_OBJECT_RETURN
+	const VkMVKObjectReturn* pObjectReturn = (const VkMVKObjectReturn*)pCreateInfo->pNext;
+	for (; pObjectReturn; pObjectReturn = (const VkMVKObjectReturn*)pObjectReturn->pNext) {
+		if (pObjectReturn->sType != VK_MVK_STRUCTURE_TYPE_OBJECT_RETURN) {
+			continue;
+		}
+		// If caller included one, fill in the 'handle' member.
+		*pObjectReturn->handle = (void*)r;
+	}
+
+	return r;
 }
 
 void MVKDevice::destroyImage(MVKImage* mvkImg,
@@ -2079,7 +2103,9 @@ MVKDevice::MVKDevice(MVKPhysicalDevice* physicalDevice, const VkDeviceCreateInfo
 	_enabledStorage8Features(),
 	_enabledF16I8Features(),
 	_enabledVarPtrFeatures(),
+#ifdef VK_EXT_host_query_reset
 	_enabledHostQryResetFeatures(),
+#endif
 	_enabledVtxAttrDivFeatures(),
 	_enabledPortabilityFeatures(),
 	_enabledExtensions(this)
@@ -2155,7 +2181,9 @@ void MVKDevice::enableFeatures(const VkDeviceCreateInfo* pCreateInfo) {
 	memset((void*)&_enabledStorage8Features, 0, sizeof(_enabledStorage8Features));
 	memset((void*)&_enabledF16I8Features, 0, sizeof(_enabledF16I8Features));
 	memset((void*)&_enabledVarPtrFeatures, 0, sizeof(_enabledVarPtrFeatures));
+#ifdef VK_EXT_host_query_reset
 	memset((void*)&_enabledHostQryResetFeatures, 0, sizeof(_enabledHostQryResetFeatures));
+#endif
 	memset((void*)&_enabledVtxAttrDivFeatures, 0, sizeof(_enabledVtxAttrDivFeatures));
 	memset((void*)&_enabledPortabilityFeatures, 0, sizeof(_enabledPortabilityFeatures));
 
@@ -2168,13 +2196,19 @@ void MVKDevice::enableFeatures(const VkDeviceCreateInfo* pCreateInfo) {
 	pdVtxAttrDivFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
 	pdVtxAttrDivFeatures.pNext = &pdPortabilityFeatures;
 
+#ifdef VK_EXT_host_query_reset
 	VkPhysicalDeviceHostQueryResetFeaturesEXT pdHostQryResetFeatures;
 	pdHostQryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
 	pdHostQryResetFeatures.pNext = &pdVtxAttrDivFeatures;
+#endif
 
 	VkPhysicalDeviceVariablePointerFeatures pdVarPtrFeatures;
 	pdVarPtrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES;
+#ifdef VK_EXT_host_query_reset
 	pdVarPtrFeatures.pNext = &pdHostQryResetFeatures;
+#else
+	pdVarPtrFeatures.pNext = &pdVtxAttrDivFeatures;
+#endif
 
 	VkPhysicalDeviceFloat16Int8FeaturesKHR pdF16I8Features;
 	pdF16I8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR;
@@ -2239,6 +2273,7 @@ void MVKDevice::enableFeatures(const VkDeviceCreateInfo* pCreateInfo) {
 							   &pdVarPtrFeatures.variablePointersStorageBuffer, 2);
 				break;
 			}
+#ifdef VK_EXT_host_query_reset
 			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT: {
 				auto* requestedFeatures = (VkPhysicalDeviceHostQueryResetFeaturesEXT*)next;
 				enableFeatures(&_enabledHostQryResetFeatures.hostQueryReset,
@@ -2246,6 +2281,7 @@ void MVKDevice::enableFeatures(const VkDeviceCreateInfo* pCreateInfo) {
 							   &pdHostQryResetFeatures.hostQueryReset, 1);
 				break;
 			}
+#endif
 			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT: {
 				auto* requestedFeatures = (VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*)next;
 				enableFeatures(&_enabledVtxAttrDivFeatures.vertexAttributeInstanceRateDivisor,
--- a/MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm
+++ b/MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm
@@ -1407,7 +1407,7 @@ protected:
 VkResult MVKPipelineCache::writeData(size_t* pDataSize, void* pData) {
 	lock_guard<mutex> lock(_shaderCacheLock);
 
-	try {
+	{
 
 		if ( !pDataSize ) { return VK_SUCCESS; }
 
@@ -1433,9 +1433,6 @@ VkResult MVKPipelineCache::writeData(size_t* pDataSize, void* pData) {
 			return VK_SUCCESS;
 		}
 
-	} catch (cereal::Exception& ex) {
-		*pDataSize = 0;
-		return reportError(VK_INCOMPLETE, "Error writing pipeline cache data: %s", ex.what());
 	}
 }
 
@@ -1482,7 +1479,7 @@ void MVKPipelineCache::writeData(ostream& outstream, bool isCounting) {
 // Loads any data indicated by the creation info.
 // This is the compliment of the writeData() function. The two must be kept aligned.
 void MVKPipelineCache::readData(const VkPipelineCacheCreateInfo* pCreateInfo) {
-	try {
+	{
 
 		size_t byteCount = pCreateInfo->initialDataSize;
 		uint32_t cacheEntryType;
@@ -1548,8 +1545,6 @@ void MVKPipelineCache::readData(const VkPipelineCacheCreateInfo* pCreateInfo) {
 			}
 		}
 
-	} catch (cereal::Exception& ex) {
-		setConfigurationResult(reportError(VK_SUCCESS, "Error reading pipeline cache data: %s", ex.what()));
 	}
 }
 
--- a/MoltenVK/MoltenVK/Layers/MVKExtensions.def
+++ b/MoltenVK/MoltenVK/Layers/MVKExtensions.def
@@ -55,8 +55,12 @@ MVK_EXTENSION(KHR_variable_pointers, KHR_VARIABLE_POINTERS)
 MVK_EXTENSION(EXT_debug_marker, EXT_DEBUG_MARKER)
 MVK_EXTENSION(EXT_debug_report, EXT_DEBUG_REPORT)
 MVK_EXTENSION(EXT_debug_utils, EXT_DEBUG_UTILS)
+#ifdef VK_EXT_host_query_reset
 MVK_EXTENSION(EXT_host_query_reset, EXT_HOST_QUERY_RESET)
+#endif
+#ifdef VK_EXT_memory_budget
 MVK_EXTENSION(EXT_memory_budget, EXT_MEMORY_BUDGET)
+#endif
 MVK_EXTENSION(EXT_shader_viewport_index_layer, EXT_SHADER_VIEWPORT_INDEX_LAYER)
 MVK_EXTENSION(EXT_vertex_attribute_divisor, EXT_VERTEX_ATTRIBUTE_DIVISOR)
 MVK_EXTENSION(EXTX_portability_subset, EXTX_PORTABILITY_SUBSET)
--- a/MoltenVK/MoltenVK/Layers/MVKExtensions.mm
+++ b/MoltenVK/MoltenVK/Layers/MVKExtensions.mm
@@ -47,17 +47,21 @@ static VkExtensionProperties kVkExtProps_ ##EXT = mvkMakeExtProps(VK_ ##EXT ##_E
 // Returns whether the specified properties are valid for this platform
 static bool mvkIsSupportedOnPlatform(VkExtensionProperties* pProperties) {
 #if !(MVK_IOS)
+#ifdef VK_EXT_memory_budget
 	if (pProperties == &kVkExtProps_EXT_MEMORY_BUDGET) {
 		return mvkOSVersion() >= 10.13;
 	}
+#endif /*VK_EXT_memory_budget*/
 	if (pProperties == &kVkExtProps_MVK_IOS_SURFACE) { return false; }
 	if (pProperties == &kVkExtProps_IMG_FORMAT_PVRTC) { return false; }
 #endif
 #if !(MVK_MACOS)
 	if (pProperties == &kVkExtProps_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE) { return false; }
+#ifdef VK_EXT_memory_budget
 	if (pProperties == &kVkExtProps_EXT_MEMORY_BUDGET) {
 		return mvkOSVersion() >= 11.0;
 	}
+#endif /*VK_EXT_memory_budget*/
 	if (pProperties == &kVkExtProps_MVK_MACOS_SURFACE) { return false; }
 #endif
 
--- a/MoltenVK/MoltenVK/OS/CAMetalLayer+MoltenVK.m
+++ b/MoltenVK/MoltenVK/OS/CAMetalLayer+MoltenVK.m
@@ -42,24 +42,32 @@
 
 -(BOOL) displaySyncEnabledMVK {
 #if MVK_MACOS
-    if ( [self respondsToSelector: @selector(displaySyncEnabled)] ) { return self.displaySyncEnabled; }
+    if (@available(macOS 10.13, *)) {
+        if ( [self respondsToSelector: @selector(displaySyncEnabled)] ) { return self.displaySyncEnabled; }
+    }
 #endif
     return YES;
 }
 
 -(void) setDisplaySyncEnabledMVK: (BOOL) enabled {
 #if MVK_MACOS
-    if ( [self respondsToSelector: @selector(setDisplaySyncEnabled:)] ) { self.displaySyncEnabled = enabled; }
+    if (@available(macOS 10.13, *)) {
+        if ( [self respondsToSelector: @selector(setDisplaySyncEnabled:)] ) { self.displaySyncEnabled = enabled; }
+    }
 #endif
 }
 
 -(NSUInteger) maximumDrawableCountMVK {
-	if ( [self respondsToSelector: @selector(maximumDrawableCount)] ) { return self.maximumDrawableCount; }
-	return 0;
+    if (@available(macOS 10.13.2, *)) {
+        if ( [self respondsToSelector: @selector(maximumDrawableCount)] ) { return self.maximumDrawableCount; }
+    }
+    return 0;
 }
 
 -(void) setMaximumDrawableCountMVK: (NSUInteger) count {
-	if ( [self respondsToSelector: @selector(setMaximumDrawableCount:)] ) { self.maximumDrawableCount = count; }
+    if (@available(macOS 10.13.2, *)) {
+        if ( [self respondsToSelector: @selector(setMaximumDrawableCount:)] ) { self.maximumDrawableCount = count; }
+    }
 }
 
 @end
--- a/MoltenVK/MoltenVK/Vulkan/mvk_datatypes.mm
+++ b/MoltenVK/MoltenVK/Vulkan/mvk_datatypes.mm
@@ -798,7 +798,7 @@ MVK_PUBLIC_SYMBOL MTLTextureType mvkMTLTextureTypeFromVkImageType(VkImageType vk
 		case VK_IMAGE_TYPE_3D: return MTLTextureType3D;
 		case VK_IMAGE_TYPE_2D:
 		default: {
-#if MVK_MACOS
+#if MVK_MACOS && MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
 			if (arraySize > 1 && isMultisample) { return MTLTextureType2DMultisampleArray; }
 #endif
 			if (arraySize > 1) { return MTLTextureType2DArray; }
@@ -824,7 +824,7 @@ MVK_PUBLIC_SYMBOL MTLTextureType mvkMTLTextureTypeFromVkImageViewType(VkImageVie
         case VK_IMAGE_VIEW_TYPE_1D_ARRAY:       return MTLTextureType1DArray;
         case VK_IMAGE_VIEW_TYPE_2D:             return (isMultisample ? MTLTextureType2DMultisample : MTLTextureType2D);
         case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
-#if MVK_MACOS
+#if !MVK_IOS && MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
             if (isMultisample) {
                 return MTLTextureType2DMultisampleArray;
             }
--- a/MoltenVK/MoltenVK/GPUObjects/MVKInstance.mm
+++ b/MoltenVK/MoltenVK/GPUObjects/MVKInstance.mm
@@ -582,7 +582,9 @@ void MVKInstance::initProcAddrs() {
 	ADD_DVC_EXT2_ENTRY_POINT(vkGetDeviceGroupSurfacePresentModesKHR, KHR_SWAPCHAIN, KHR_DEVICE_GROUP);
 	ADD_DVC_EXT2_ENTRY_POINT(vkGetPhysicalDevicePresentRectanglesKHR, KHR_SWAPCHAIN, KHR_DEVICE_GROUP);
 	ADD_DVC_EXT2_ENTRY_POINT(vkAcquireNextImage2KHR, KHR_SWAPCHAIN, KHR_DEVICE_GROUP);
+#ifdef VK_EXT_host_query_reset
 	ADD_DVC_EXT_ENTRY_POINT(vkResetQueryPoolEXT, EXT_HOST_QUERY_RESET);
+#endif
 	ADD_DVC_EXT_ENTRY_POINT(vkDebugMarkerSetObjectTagEXT, EXT_DEBUG_MARKER);
 	ADD_DVC_EXT_ENTRY_POINT(vkDebugMarkerSetObjectNameEXT, EXT_DEBUG_MARKER);
 	ADD_DVC_EXT_ENTRY_POINT(vkCmdDebugMarkerBeginEXT, EXT_DEBUG_MARKER);
EOF
    fi
    cd ..
    if [ ! -d vulkan-portability ]; then
      echo "git clone https://github.com/KhronosGroup/Vulkan-Portability"
      git clone https://github.com/KhronosGroup/Vulkan-Portability vulkan-portability
    fi
    cd vulkan-portability
    VK_PORTABILITY_COMMIT=53be040f04ce55463d0e5b25fd132f45f003e903
    if [ "$(git log -1 --pretty='%H')" != "$VK_PORTABILITY_COMMIT" ]; then
      if ! git checkout $VK_PORTABILITY_COMMIT; then
        echo "MoltenVK: git checkout $VK_PORTABILITY_COMMIT failed, falling back to fetch"
        git fetch origin
        git checkout $VK_PORTABILITY_COMMIT
      fi
    fi
  )
fi

args_to_gn() {
  pre=" $1=["
  shift
  while [ $# -gt 0 ]; do
    echo -n "$pre \"$1\""
    pre=","
    shift
  done
  if [ "$pre" == "," ]; then
    echo " ]"
  fi
}

# Pass $CC and $CXX to gn / ninja.
args=""
if [ -n "$CC" ]; then
  args="$args gcc_cc=\"$CC\""
fi
if [ -n "$CXX" ]; then
  args="$args gcc_cxx=\"$CXX\""
fi
if [ -n "$CC_WRAPPER" ]; then
  args="$args cc_wrapper=\"$CC_WRAPPER\""
fi
# Pass $CFLAGS, $CXXFLAGS, $LDFLAGS to gn / ninja.
if [ -n "$CFLAGS" ]; then
  args="$args $(args_to_gn extra_cflags_c $CFLAGS)"
fi
if [ -n "$CXXFLAGS" ]; then
  args="$args $(args_to_gn extra_cflags_cc $CXXFLAGS)"
fi
if [ -n "$LDFLAGS" ]; then
  args="$args $(args_to_gn extra_ldflags $LDFLAGS)"
fi
if [ -n "$VOLCANO_ARGS_REQUEST" ]; then
  echo "$args" > "$VOLCANO_ARGS_REQUEST"
fi

# If you choose, you may set the environment variable VOLCANO_NO_OUT to prevent
# gn and ninja running and producing a Volcano build. By design, gn only allows
# build artifacts to be in a top-level directory (typically "out"), so when
# volcano is a git submodule, you want to wait and run gn at the top level.
#
# install-gn-ninja will still *build* gn and ninja for you.
if [ -z "$VOLCANO_NO_OUT" ]; then
  # TARGET can be any directory name under 'out'. A common name is Release:
  # TARGET=Release # To actually compile a release build, also set args:
  # args="$args is_debug=false is_official_build=true"
  TARGET=Debug

  if [ -n "$args" ]; then
    args="--args=$args"
    vendor/subgn/gn gen out/$TARGET "$args"
  else
    vendor/subgn/gn gen out/$TARGET
  fi
  NINJA_J=""

  # Building using all cores will fail in e.g. a VM with limited RAM
  SMALL_MEM=""
  if [ -f /proc/meminfo ]; then
    SMALL_MEM=$(awk 'BEGIN{m=0}
                     {if ($1 ~ "MemTotal:") m=$2}
                     END{if (m/1024/1024 < 7) print "small"}' /proc/meminfo)
  fi
  if [ "$SMALL_MEM" == "small" ]; then
    echo "reducing parallelism due to low memory: -j2"
    NINJA_J="-j2"
  elif [ -n "$BOOTSTRAP_ON_TRAVIS_J" ]; then
    NINJA_J="-j$BOOTSTRAP_ON_TRAVIS_J"
  fi

  vendor/subgn/ninja -C out/$TARGET $NINJA_J

  if ! grep -q "vendor/subgn" <<<"$PATH"; then
    echo "Note: please add the following to your .bashrc / .cshrc / .zshrc:"
    echo ""
    echo "      PATH=\$PATH:$PWD/vendor/subgn"
    echo ""
    echo "      Which makes \"ninja -C out/$TARGET\" work."
    echo "      This script does that, so another option is to just re-run me."
    echo "      Then PATH changes are optional."
  fi
fi
