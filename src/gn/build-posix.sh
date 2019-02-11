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
    echo "Please install homebrew: https://brew.sh"
    exit 1
  fi
  echo "WARNING: Your system uses an unknown install method."
  installer="unknown"
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
      clist="$clist pkgconfig"
    else
      clist="$clist $c"
    fi
  fi
done

if [ "$installer" == "brew" ] || [ "$installer" == "port" ]; then
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
1993ebd859d5c5b2f488cc294920ba8202ed9ae6 vendor/glfw
9ed38739b974f9d6585e8c5f11184409b11d3817 vendor/glslang
0f88e6bb5cbcfa88c8c101aadc69233db3a60c69 vendor/skia
2c09c51fbafcb67e0ca02fab15d8d2b262f26da2 vendor/spirv_cross
1f05783f6b338287dadcc51e6b7d978ba6286f69 vendor/subgn
f54e45b92374b99de8556cacffc3602a03187b68 vendor/vulkan-headers
c876454742a4f600e80b4d1054e9051c15780779 vendor/vulkan-loader
119b38ad3f46d30a77361fb14d71bc195f8d54ab vendor/vulkan-validationlayers
EOF
  git commit -q -m "repo was downloaded without .git, build.cmd rebuilt it"
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
if ! grep '__android_log_assert' vendor/spirv_cross/spirv_common.hpp >/dev/null 2>&1; then
  missing="$missing patch-spirv-cross"
fi

VVL=vendor/vulkan-validationlayers
# has vulkansamples been patched?
if ! grep '#include <vulkan/vulkan.h>' \
    $VVL/layers/vk_format_utils.cpp >/dev/null 2>&1; then
  missing="$missing patch-vulkan"
fi

if ! grep -q 'CEREAL_CUSTOM_CEREALABORT' \
vendor/cereal/include/cereal/macros.hpp; then
  missing="$missing patch-cereal"
fi

missing="$missing git-sync-deps install-gn-ninja"

# has spirv-tools been patched? (must be after git-sync-deps)
if ! grep -q \
'with open(changes_file, mode='"'"'r'"'"', errors='"'"'replace'"'"')' \
vendor/skia/third_party/externals/spirv-tools/utils/update_build_version.py \
>/dev/null 2>&1; then
  missing="$missing patch-spirv-tools"
fi
echo "volcano updating:$missing"

for buildstep in $missing; do
  case $buildstep in
  patch-spirv-cross)
    patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/spirv_cross/spirv_common.hpp
+++ b/vendor/spirv_cross/spirv_common.hpp
@@ -34,6 +34,9 @@
 #include <unordered_set>
 #include <utility>
 #include <vector>
+#ifdef __ANDROID__
+#include <android/log.h>
+#endif
 
 namespace spirv_cross
 {
@@ -45,6 +48,9 @@ namespace spirv_cross
 inline void
 report_and_abort(const std::string &msg)
 {
+#ifdef __ANDROID__
+	__android_log_assert(__FILE__, "volcano", "spirv_cross error: %s\n", msg.c_str());
+#else
 #ifdef NDEBUG
 	(void)msg;
 #else
@@ -52,6 +58,7 @@ report_and_abort(const std::string &msg)
 #endif
 	fflush(stderr);
 	abort();
+#endif
 }
 
 #define SPIRV_CROSS_THROW(x) report_and_abort(x)
@@ -248,7 +255,9 @@ inline std::string merge(const std::vector<std::string> &list)
 template <typename T>
 inline std::string convert_to_string(T &&t)
 {
-	return std::to_string(std::forward<T>(t));
+	std::ostringstream converter;
+	converter << std::forward<T>(t);
+	return converter.str();
 }
 
 // Allow implementations to set a convenient standard precision
EOF
    ;;

  patch-spirv-tools)
    sed -i -e 's/with open(changes_file, mode='"'"'rU'"'"') as f:/with open(changes_file, mode='"'"'r'"'"', errors='"'"'replace'"'"') as f:/' \
      vendor/skia/third_party/externals/spirv-tools/utils/update_build_version.py
    # macOS sed leaves files behind
    rm -f vendor/skia/third_party/externals/spirv-tools/utils/update_build_version.py-e
    ;;

  patch-vulkan)
    sed -i -e 's/valid_usage_path  = args.scripts/valid_usage_path  = '\
'os.path.join(os.path.dirname(os.path.abspath(__file__)), args.scripts)/' \
        $VVL/scripts/lvl_genvk.py
    # macOS sed leaves files behind
    rm -f $VVL/scripts/lvl_genvk.py-e
    awk '{
        sub("^#include \"vulkan/vulkan.h\"","#include <vulkan/vulkan.h>");
        print
      }' $VVL/layers/vk_format_utils.cpp > $VVL/layers/vkfutil.cpptmp
    mv $VVL/layers/{vkfutil.cpptmp,vk_format_utils.cpp}
    awk '{
        sub("    for obj in self.non_dispatchable_types:","    for obj in sorted(self.non_dispatchable_types):");
        print
      }' $VVL/scripts/thread_safety_generator.py > $VVL/scripts/thread_safety_generator.pytmp
    mv $VVL/scripts/thread_safety_generator.{pytmp,py}
    HFG=vendor/vulkan-loader/scripts/helper_file_generator
    awk '{
        sub("    for item in value_list:","    for item in sorted(value_list):");
        print
      }' $HFG.py > $HFG.pytmp
    mv $HFG.{pytmp,py}
    patch -p1 <<EOF
--- a/vendor/vulkan-loader/loader/loader.c
+++ b/vendor/vulkan-loader/loader/loader.c
@@ -3652,6 +3652,31 @@ static VkResult ReadDataFilesInSearchPaths(const struct loader_instance *inst, e
 #endif
     }
 
+    // Remove duplicate paths, or it would result in duplicate extensions, duplicate devices, etc.
+    // This uses minimal memory, but is O(N^2) on the number of paths. Expect only a few paths.
+    char path_sep_str[2] = { PATH_SEPARATOR, '\\0' };
+    size_t search_path_updated_size = strlen(search_path);
+    for (size_t first = 0; first < search_path_updated_size - 1; ) {
+        size_t first_end = first + 1;
+        first_end += strcspn(&search_path[first_end], path_sep_str);
+        for (size_t second = first_end + 1; second < search_path_updated_size; ) {
+            size_t second_end = second + 1;
+            second_end += strcspn(&search_path[second_end], path_sep_str);
+            if (first_end - first == second_end - second && !strncmp(&search_path[first], &search_path[second], second_end - second)) {
+                // Found duplicate. Include PATH_SEPARATOR in second_end, then erase it from search_path.
+                if (search_path[second_end] == PATH_SEPARATOR) {
+                    second_end++;
+                }
+                memmove(&search_path[second], &search_path[second_end], search_path_updated_size - second_end + 1);
+                search_path_updated_size -= second_end - second;
+            } else {
+                second = second_end + 1;
+            }
+        }
+        first = first_end + 1;
+    }
+    search_path_size = search_path_updated_size;
+
     // Print out the paths being searched if debugging is enabled
     if (search_path_size > 0) {
         loader_log(inst, VK_DEBUG_REPORT_DEBUG_BIT_EXT, 0,
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
      set -e
      rm -f git-sync-deps-pid
      exit $RV
    ) &
    export GIT_SYNC_DEPS_PID=$!
    set -e

    # patch imgui if needed
    # Note: this must be done after git-sync-deps
    grep -q 'if (g.OpenPopupStack.Size < current_stack_size + 1)' \
        vendor/skia/third_party/externals/imgui/imgui.cpp && \
    patch --no-backup-if-mismatch -sN -r - -p1 <<EOF >/dev/null || true
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
@@ -2938,11 +2938,11 @@ static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj, int pos, const Im
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
@@ -1719,7 +1719,7 @@ static void ShowDemoWindowLayout()
             ImGui::Button("Box", button_sz);
             float last_button_x2 = ImGui::GetItemRectMax().x;
             float next_button_x2 = last_button_x2 + style.ItemSpacing.x + button_sz.x; // Expected position if next button was on same line
-            if (n + 1 < buttons_count && next_button_x2 < window_visible_x2)
+            if (n < buttons_count - 1 && next_button_x2 < window_visible_x2)
                 ImGui::SameLine();
             ImGui::PopID();
         }
EOF
    ;;

  install-gn-ninja)
    # build gn and ninja from source for aarch64 (chromium omits aarch64), and
    # subgn and subninja have some additional features used in volcano.
    (
      cd vendor/subgn
      if [ ! -x gn ]; then
        ./build.cmd
        cp subninja/ninja out_bootstrap/gn .
      fi
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
    MVK_COMMIT=5e0f624b340fd639debd568b3f9179d954aed7c7
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
      sed -i -e 's0#include "../SPIRV-Cross/spirv.hpp"0#include "spirv.hpp"0' \
        MoltenVKShaderConverter/MoltenVKSPIRVToMSLConverter/SPIRVToMSLConverter.h
      sed -i -e 's0#include "SPIRVSupport.h"0#include "Common/SPIRVSupport.h"0' \
        MoltenVKShaderConverter/MoltenVKSPIRVToMSLConverter/SPIRVToMSLConverter.cpp
      # macOS sed leaves files behind
      rm -f MoltenVK/icd/MoltenVK_icd.json-e
      rm -f MoltenVKShaderConverter/MoltenVKSPIRVToMSLConverter/SPIRVToMSLConverter.{cpp,h}-e
      patch --no-backup-if-mismatch -sN -r - -p1 <<EOF
--- a/MoltenVK/MoltenVK/API/mvk_vulkan.h
+++ b/MoltenVK/MoltenVK/API/mvk_vulkan.h
@@ -40,6 +40,6 @@
 #endif
 
 #include <vulkan/vulkan.h>
-#include <vulkan-portability/vk_extx_portability_subset.h>
+#include <../../vulkan-portability/include/vulkan/vk_extx_portability_subset.h>
 
 #endif
--- a/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm
+++ b/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm
@@ -619,6 +619,7 @@ VkResult MVKPhysicalDevice::getPhysicalDeviceMemoryProperties(VkPhysicalDeviceMe
 		auto* next = (MVKVkAPIStructHeader*)pMemoryProperties->pNext;
 		while (next) {
 			switch ((uint32_t)next->sType) {
+#ifdef VK_EXT_memory_budget
 			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT: {
 				auto* budgetProps = (VkPhysicalDeviceMemoryBudgetPropertiesEXT*)next;
 				memset(budgetProps->heapBudget, 0, sizeof(budgetProps->heapBudget));
@@ -630,6 +631,7 @@ VkResult MVKPhysicalDevice::getPhysicalDeviceMemoryProperties(VkPhysicalDeviceMe
 				next = (MVKVkAPIStructHeader*)budgetProps->pNext;
 				break;
 			}
+#endif
 			default:
 				next = (MVKVkAPIStructHeader*)next->pNext;
 				break;
@@ -747,10 +749,12 @@ void MVKPhysicalDevice::initMetalFeatures() {
 		_metalFeatures.presentModeImmediate = true;
     }
 
+#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v4] ) {
 		_metalFeatures.mslVersion = SPIRVToMSLConverterOptions::makeMSLVersion(2, 1);
         _metalFeatures.multisampleArrayTextures = true;
     }
+#endif
 
 #endif
 
@@ -1447,9 +1451,11 @@ void MVKPhysicalDevice::logGPUInfo() {
 #endif
 
 #if MVK_MACOS
+#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
 	if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily2_v1] ) { logMsg += "\\n\\t\\tmacOS GPU Family 2 v1"; }
 
 	if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v4] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v4"; }
+#endif
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v3] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v3"; }
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v2] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v2"; }
     if ( [_mtlDevice supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v1] ) { logMsg += "\\n\\t\\tmacOS GPU Family 1 v1"; }
--- a/MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm
+++ b/MoltenVK/MoltenVK/GPUObjects/MVKPipeline.mm
@@ -741,7 +741,7 @@ protected:
 VkResult MVKPipelineCache::writeData(size_t* pDataSize, void* pData) {
 	lock_guard<mutex> lock(_shaderCacheLock);
 
-	try {
+	{
 
 		if ( !pDataSize ) { return VK_SUCCESS; }
 
@@ -767,9 +767,6 @@ VkResult MVKPipelineCache::writeData(size_t* pDataSize, void* pData) {
 			return VK_SUCCESS;
 		}
 
-	} catch (cereal::Exception& ex) {
-		*pDataSize = 0;
-		return mvkNotifyErrorWithText(VK_INCOMPLETE, "Error writing pipeline cache data: %s", ex.what());
 	}
 }
 
@@ -816,7 +813,7 @@ void MVKPipelineCache::writeData(ostream& outstream, bool isCounting) {
 // Loads any data indicated by the creation info.
 // This is the compliment of the writeData() function. The two must be kept aligned.
 void MVKPipelineCache::readData(const VkPipelineCacheCreateInfo* pCreateInfo) {
-	try {
+	{
 
 		size_t byteCount = pCreateInfo->initialDataSize;
 		uint32_t cacheEntryType;
@@ -882,8 +879,6 @@ void MVKPipelineCache::readData(const VkPipelineCacheCreateInfo* pCreateInfo) {
 			}
 		}
 
-	} catch (cereal::Exception& ex) {
-		setConfigurationResult(mvkNotifyErrorWithText(VK_SUCCESS, "Error reading pipeline cache data: %s", ex.what()));
 	}
 }
 
--- a/MoltenVK/MoltenVK/Layers/MVKExtensions.cpp
+++ b/MoltenVK/MoltenVK/Layers/MVKExtensions.cpp
@@ -104,17 +104,21 @@ string MVKExtensionList::enabledNamesString(const char* separator, bool prefixFi
 // Returns whether the specified properties are valid for this platform
 static bool mvkIsSupportedOnPlatform(VkExtensionProperties* pProperties) {
 #if !(MVK_IOS)
+#ifdef VK_EXT_memory_budget
 	if (pProperties == &kVkExtProps_EXT_MEMORY_BUDGET) {
 		return mvkOSVersion() >= 10.13;
 	}
+#endif
 	if (pProperties == &kVkExtProps_MVK_IOS_SURFACE) { return false; }
 	if (pProperties == &kVkExtProps_IMG_FORMAT_PVRTC) { return false; }
 #endif
 #if !(MVK_MACOS)
 	if (pProperties == &kVkExtProps_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE) { return false; }
+#ifdef VK_EXT_memory_budget
 	if (pProperties == &kVkExtProps_EXT_MEMORY_BUDGET) {
 		return mvkOSVersion() >= 11.0;
 	}
+#endif
 	if (pProperties == &kVkExtProps_MVK_MACOS_SURFACE) { return false; }
 #endif
 
--- a/MoltenVK/MoltenVK/Layers/MVKExtensions.def
+++ b/MoltenVK/MoltenVK/Layers/MVKExtensions.def
@@ -51,7 +51,9 @@ MVK_EXTENSION(KHR_surface, KHR_SURFACE)
 MVK_EXTENSION(KHR_swapchain, KHR_SWAPCHAIN)
 MVK_EXTENSION(KHR_swapchain_mutable_format, KHR_SWAPCHAIN_MUTABLE_FORMAT)
 MVK_EXTENSION(KHR_variable_pointers, KHR_VARIABLE_POINTERS)
+#ifdef VK_EXT_memory_budget
 MVK_EXTENSION(EXT_memory_budget, EXT_MEMORY_BUDGET)
+#endif
 MVK_EXTENSION(EXT_shader_viewport_index_layer, EXT_SHADER_VIEWPORT_INDEX_LAYER)
 MVK_EXTENSION(EXT_vertex_attribute_divisor, EXT_VERTEX_ATTRIBUTE_DIVISOR)
 MVK_EXTENSION(EXTX_portability_subset, EXTX_PORTABILITY_SUBSET)
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
@@ -787,7 +787,7 @@ MVK_PUBLIC_SYMBOL MTLTextureType mvkMTLTextureTypeFromVkImageType(VkImageType vk
 		case VK_IMAGE_TYPE_3D: return MTLTextureType3D;
 		case VK_IMAGE_TYPE_2D:
 		default: {
-#if !MVK_IOS  // This is marked unavailable on iOS :/
+#if !MVK_IOS && MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
 			if (arraySize > 1 && isMultisample) { return MTLTextureType2DMultisampleArray; }
 #endif
 			if (arraySize > 1) { return MTLTextureType2DArray; }
@@ -813,7 +813,7 @@ MVK_PUBLIC_SYMBOL MTLTextureType mvkMTLTextureTypeFromVkImageViewType(VkImageVie
         case VK_IMAGE_VIEW_TYPE_1D_ARRAY:       return MTLTextureType1DArray;
         case VK_IMAGE_VIEW_TYPE_2D:             return (isMultisample ? MTLTextureType2DMultisample : MTLTextureType2D);
         case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
-#if MVK_MACOS
+#if !MVK_IOS && MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_13
             if (isMultisample) {
                 return MTLTextureType2DMultisampleArray;
             }
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
  # FIXME: ccache 3.4 has --keep_comments_cpp, but travis dist: xenial is 3.2.4
  # thus gcc -C is used to workaround a false -Wimplicit-fallthrough warning.
  CFLAGS="$CFLAGS -C"
  CXXFLAGS="$CXXFLAGS -C"
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
VGA=volcano-gn-args
if [ -f /tmp/${VGA}-request.txt ]; then
  echo "$args" > /tmp/${VGA}.txt
  rm /tmp/${VGA}-request.txt
fi

# If you choose, you may set the environment variable VOLCANO_NO_OUT to prevent
# gn and ninja running and producing a Volcano build. By design, gn only allows
# build artifacts to be in a top-level directory (typically "out"), so when
# volcano is a git submodule, you want to wait and run gn at the top level.
#
# install-gn-ninja will still *build* gn and ninja for you.
if [ -z "$VOLCANO_NO_OUT" ]; then
  TARGET=Debug

  if [ -n "$args" ]; then
    args="--args=$args"
    vendor/subgn/gn gen out/$TARGET "$args"
  else
    vendor/subgn/gn gen out/$TARGET
  fi
  NINJA_ARGS=""
  if [ -n "$BOOTSTRAP_ON_TRAVIS_J" ]; then
    NINJA_ARGS="$NINJA_ARGS -j$BOOTSTRAP_ON_TRAVIS_J"
  fi

  # some files use a lot of memory, bulid them first if necessary
  SMALL_MEM=""
  if [ -f /proc/meminfo ]; then
    SMALL_MEM=$(awk 'BEGIN{m=0}
                     {if ($1 ~ "MemTotal:") m=$2}
                     END{if (m/1024/1024 < 7) print "small"}' /proc/meminfo)
  fi
  if [ "$SMALL_MEM" == "small" ]; then
    LARGE_TARGETS="skiax64/libspirv-tools.a"
    LARGE_TARGETS="$LARGE_TARGETS skiax64/obj/gpu.stamp"
    for t in core_validation parameter_validation thread_safety \
             unique_objects object_lifetimes; do
      LARGE_TARGETS="$LARGE_TARGETS vulkan/explicit_layer.d/libVkLayer_${t}.so"
    done

    echo -n "large_targets: "
    vendor/subgn/ninja -C out/$TARGET $NINJA_ARGS -j2 $LARGE_TARGETS
    echo -n "all_cores: "
  fi

  vendor/subgn/ninja -C out/$TARGET $NINJA_ARGS

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
