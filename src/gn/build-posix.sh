#!/bin/bash
# Copyright (c) 2017 the Volcano Authors. Licensed under GPLv3.
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
  echo "WARNING: Your system uses an unknown install method."
  installer="unknown"
fi

git_aptget="git"
git_pacman="git"
git_emerge="dev-vcs/git"
git_dnf="git"
git_yum="git"
python3_aptget="python3"
python3_pacman="python3"
python3_emerge="dev-lang/python-3"
python3_dnf="python3"
python3_yum="python3"
patch_aptget="patch"
patch_pacman="patch"
patch_emerge="sys-devel/patch"
patch_dnf="patch"
patch_yum="patch"
pkgconfig_aptget="pkg-config"
pkgconfig_pacman="pkg-config"
pkgconfig_emerge="dev-util/pkgconfig"
pkgconfig_dnf="pkgconfig"
pkgconfig_yum="pkgconfig"
gcc_cpp_aptget="build-essential"
gcc_cpp_pacman="base-devel"
gcc_cpp_emerge="gcc"
gcc_cpp_dnf="gcc-c++"
gcc_cpp_yum="gcc-c++"
clang_format_aptget="clang-format"
clang_format_pacman="clang"
clang_format_emerge="sys-devel/llvm"
clang_format_dnf="clang"
clang_format_yum="clang"
fontconfig_aptget="libfontconfig1-dev"
fontconfig_pacman="fontconfig"
fontconfig_emerge="media-libs/fontconfig"
fontconfig_dnf="fontconfig-devel"
fontconfig_yum="fontconfig-devel"
icu_aptget="libicu-dev"
icu_pacman="icu"
icu_emerge="icu"
icu_dnf="libicu-devel"
icu_yum="libicu-devel"
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
    else
      clist="$clist $c"
    fi
  fi
done
if ! pkg-config --exists fontconfig; then
  clist="$clist fontconfig"
fi
if ! pkg-config --exists gl; then
  clist="$clist mesagl"
fi
if ! pkg-config --exists glu; then
  clist="$clist mesaglu"
fi
if ! pkg-config --exists xrandr; then
  clist="$clist xrandr"
fi
if ! pkg-config --exists xcursor; then
  clist="$clist xcursor"
fi
if ! pkg-config --exists xinerama; then
  clist="$clist xinerama"
fi
if ! pkg-config --exists xi; then
  clist="$clist xinput2"
fi
if ! pkg-config --exists icu-i18n; then
  clist="$clist icu"
fi

if [ -n "$clist" ]; then
  case $installer in
  unknown)
    echo "Please install the following commands before continuing:"
    echo "  $clist"
    ;;
  brew|port)
    echo "Please install the following commands before continuing:"
    echo "  $clist"
    echo "WARNING: iOS and macOS are known to not even compile!"
    echo "WARNING: https://github.com/ndsol/volcano/issues/2"
    ;;
  *)
    pkglist=""
    for c in $clist; do
      i="${c}_${installer/-/}"
      pkglist="$pkglist ${!i}"
    done
    case $installer in
    apt-get|dnf|yum)
      echo "Please $installer install$pkglist"
      ;;
    pacman)
      echo "Please pacman -S$pkglist"
      ;;
    emerge)
      echo "Please emerge$pkglist"
      ;;
    esac
    ;;
  esac
  exit 1
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
30489c5aa1392f698e904c6aced1d7a486634c40 vendor/glfw
93c0c449dab34039800d00dd7e36aba98fa59b49 vendor/gli
715c353a15836e5ae192a64a4cf54e2ce7e8d66a vendor/glslang
f4772dd00459f05bd37ed94a0c7ecc736dc26a02 vendor/skia
a02b008ce01758b5e511c0816d7e3757b4d8cb7d vendor/spirv_cross
408f17e000938e0add7b49aedafa7ce96eca235b vendor/subgn
f7fc86aacd948109c56fd606f98273fdf9771377 vendor/vulkansamples
EOF
  git commit -q -m "repo was downloaded without .git, build.cmd rebuilt it"
fi

SUBMODULE_CMD="git submodule update --init --recursive"
set +e
$SUBMODULE_CMD --depth=51
RV=$?
set -e
while [ $RV -ne 0 ]; do
  echo ""
  echo "WARN: $SUBMODULE_CMD --depth=51 # failed, deepening..."
  set +e
  D=$($SUBMODULE_CMD 2>&1 | sed -e \
'/^\(Fetched in submodule\|Unable to checkout\)/'\
's/.*in submodule path '"'"'\(.*\)'"'"'.*/\1/p;d')
  set -e

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
  set +e
  $SUBMODULE_CMD
  RV=$?
  set -e
  if [ $RV -eq 0 ]; then
    # Work around a git submodule bug: if the initial submodule update fails,
    # some submodules may not be checked out, only fetched.
    #
    # This should be a no-op for any successfully checked out submodule, only
    # forcing all submodules into a "detached HEAD" state.
    git submodule foreach --recursive git checkout HEAD@{0}
  fi
done

# has glfw been patched?
if [ ! -d vendor/glfw/deps ]; then
  echo "Fatal: missing dir vendor/glfw/deps"
  exit 1
fi
if [ -d vendor/glfw/deps/vulkan ]; then
  missing="$missing patch-glfw"
fi

# has spirv_cross been patched?
if ! grep '__android_log_assert' vendor/spirv_cross/spirv_common.hpp >/dev/null 2>&1; then
  missing="$missing patch-spirv-cross"
fi

# has vulkansamples been patched?
if ! grep '#include <vulkan/vulkan.h>' \
    vendor/vulkansamples/layers/vk_format_utils.cpp >/dev/null 2>&1; then
  missing="$missing patch-vulkansamples"
fi

# just patch gli
sed -i -e \
's,\(HeaderDesc\.format\.flags = Storage\.layers() > 1 ? [^ ][^ ]* : \)\(Desc\.Flags\),'\
'\1detail::DDPF(\2),;'\
's,\(HeaderDesc\.format\.fourCC = Storage\.layers() > 1 ? [^ ][^ ]* : \)\(Desc\.FourCC\),'\
'\1detail::D3DFORMAT(\2),' vendor/gli/gli/core/save_dds.inl

# just patch glslang and imgui
patch --no-backup-if-mismatch -sN -r - -p1 <<EOF >/dev/null || true
--- a/vendor/glslang/glslang/MachineIndependent/Initialize.cpp
+++ b/vendor/glslang/glslang/MachineIndependent/Initialize.cpp
@@ -6341,8 +6341,8 @@ void TBuiltIns::identifyBuiltIns(int version, EProfile profile, const SpvVersion
 #ifdef AMD_EXTENSIONS
         if (profile != EEsProfile)
             symbolTable.relateToOperator("interpolateAtVertexAMD", EOpInterpolateAtVertex);
-        break;
 #endif
+        break;
 
     case EShLangCompute:
         symbolTable.relateToOperator("memoryBarrierShared",     EOpMemoryBarrierShared);
--- a/vendor/skia/third_party/externals/imgui/imgui.cpp
+++ b/vendor/skia/third_party/externals/imgui/imgui.cpp
@@ -859,7 +859,7 @@ ImGuiIO::ImGuiIO()
 void ImGuiIO::AddInputCharacter(ImWchar c)
 {
     const int n = ImStrlenW(InputCharacters);
-    if (n + 1 < IM_ARRAYSIZE(InputCharacters))
+    if (n < IM_ARRAYSIZE(InputCharacters) - 1)
     {
         InputCharacters[n] = c;
         InputCharacters[n+1] = '\0';
@@ -3423,7 +3423,7 @@ void ImGui::OpenPopupEx(const char* str_id, bool reopen_existing)
     ImGuiID id = window->GetID(str_id);
     int current_stack_size = g.CurrentPopupStack.Size;
     ImGuiPopupRef popup_ref = ImGuiPopupRef(id, window, window->GetID("##menus"), g.IO.MousePos); // Tagged as new ref because constructor sets Window to NULL (we are passing the ParentWindow info here)
-    if (g.OpenPopupStack.Size < current_stack_size + 1)
+    if (g.OpenPopupStack.Size <= current_stack_size)
         g.OpenPopupStack.push_back(popup_ref);
     else if (reopen_existing || g.OpenPopupStack[current_stack_size].PopupId != id)
     {
@@ -5847,7 +5847,14 @@ void ImGui::LogFinish()
             fclose(g.LogFile);
         g.LogFile = NULL;
     }
+#ifdef __GNUC__
+#pragma GCC diagnostic push
+#pragma GCC diagnostic ignored "-Wstrict-overflow"
+#endif
     if (g.LogClipboard->size() > 1)
+#ifdef __GNUC__
+#pragma GCC diagnostic pop
+#endif
     {
         SetClipboardText(g.LogClipboard->begin());
         g.LogClipboard->clear();
@@ -7515,11 +7522,11 @@ static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj, int pos, const Im
 {
     const int text_len = obj->CurLenW;
     IM_ASSERT(pos <= text_len);
-    if (new_text_len + text_len + 1 > obj->Text.Size)
+    if (new_text_len + text_len >= obj->Text.Size)
         return false;
 
     const int new_text_len_utf8 = ImTextCountUtf8BytesFromStr(new_text, new_text + new_text_len);
-    if (new_text_len_utf8 + obj->CurLenA + 1 > obj->BufSizeA)
+    if (new_text_len_utf8 + obj->CurLenA >= obj->BufSizeA)
         return false;
 
     ImWchar* text = obj->Text.Data;
@@ -9496,7 +9503,7 @@ void ImGui::Columns(int columns_count, const char* id, bool border)
     {
         // Cache column offsets
         window->DC.ColumnsData.resize(columns_count + 1);
-        for (int column_index = 0; column_index < columns_count + 1; column_index++)
+        for (int column_index = 0; column_index <= columns_count; column_index++)
         {
             const ImGuiID column_id = window->DC.ColumnsSetId + ImGuiID(column_index);
             KeepAliveID(column_id);
--- a/vendor/skia/third_party/externals/imgui/imgui_draw.cpp
+++ b/vendor/skia/third_party/externals/imgui/imgui_draw.cpp
@@ -1739,7 +1739,7 @@ void ImFont::BuildLookupTable()
     FallbackGlyph = NULL;
     FallbackGlyph = FindGlyph(FallbackChar);
     FallbackXAdvance = FallbackGlyph ? FallbackGlyph->XAdvance : 0.0f;
-    for (int i = 0; i < max_codepoint + 1; i++)
+    for (int i = 0; i <= max_codepoint; i++)
         if (IndexXAdvance[i] < 0.0f)
             IndexXAdvance[i] = FallbackXAdvance;
 }
--- a/vendor/skia/third_party/externals/imgui/stb_rect_pack.h
+++ b/vendor/skia/third_party/externals/imgui/stb_rect_pack.h
@@ -488,17 +488,14 @@ static stbrp__findresult stbrp__skyline_pack_rectangle(stbrp_context *context, i
    STBRP_ASSERT(cur->next == NULL);
 
    {
-      stbrp_node *L1 = NULL, *L2 = NULL;
       int count=0;
       cur = context->active_head;
       while (cur) {
-         L1 = cur;
          cur = cur->next;
          ++count;
       }
       cur = context->free_head;
       while (cur) {
-         L2 = cur;
          cur = cur->next;
          ++count;
       }
--- a/vendor/skia/third_party/externals/imgui/stb_truetype.h
+++ b/vendor/skia/third_party/externals/imgui/stb_truetype.h
@@ -3281,7 +3281,7 @@ static int stbtt_BakeFontBitmap_internal(unsigned char *data, int offset,  // fo
       chardata[i].xoff     = (float) x0;
       chardata[i].yoff     = (float) y0;
       x = x + gw + 1;
-      if (y+gh+1 > bottom_y)
+      if (y+gh >= bottom_y)
          bottom_y = y+gh+1;
    }
    return bottom_y;
EOF

missing="$missing git-sync-deps install-gn-ninja"
echo "volcano updating:$missing"

for buildstep in $missing; do
  case $buildstep in
  patch-glfw)
    # patch glfw to remove its version of vulkan.h
    rm -r vendor/glfw/deps/vulkan
    # Fix DWM_BB_ENABLE bug.
    patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/glfw/src/win32_platform.h
+++ b/vendor/glfw/src/win32_platform.h
@@ -116,7 +116,7 @@ typedef struct tagCHANGEFILTERSTRUCT
 #endif
 #endif /*Windows 7*/
 
-#if WINVER < 0x0600
+#ifndef DWM_BB_ENABLE
 #define DWM_BB_ENABLE 0x00000001
 #define DWM_BB_BLURREGION 0x00000002
 typedef struct
EOF
    ;;

  patch-spirv-cross)
    patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/spirv_cross/spirv_common.hpp
+++ b/vendor/spirv_cross/spirv_common.hpp
@@ -32,6 +32,9 @@
 #include <unordered_set>
 #include <utility>
 #include <vector>
+#ifdef __ANDROID__
+#include <android/log.h>
+#endif
 
 namespace spirv_cross
 {
@@ -43,6 +46,9 @@ namespace spirv_cross
     inline void
     report_and_abort(const std::string &msg)
 {
+#ifdef __ANDROID__
+	__android_log_assert(__FILE__, "volcano", "spirv_cross error: %s\n", msg.c_str());
+#else
 #ifdef NDEBUG
 	(void)msg;
 #else
@@ -50,6 +56,7 @@ namespace spirv_cross
 #endif
 	fflush(stderr);
 	abort();
+#endif
 }
 
 #define SPIRV_CROSS_THROW(x) report_and_abort(x)
@@ -116,7 +123,9 @@ inline std::string merge(const std::vector<std::string> &list)
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

  patch-vulkansamples)
    for a in \
      core_validation \
      object_tracker_utils \
      threading \
      unique_objects
    do
      sed -i -e 's'\
'/pVersionStruct->pfn\(Get\(Instance\|Device\|PhysicalDevice\)ProcAddr\)'\
' = vk\(\(GetInstance\|GetDevice\|_layerGetPhysicalDevice\)ProcAddr\);$'\
'/pVersionStruct->pfn\1 = '"${a%_utils}"'::\1;/' \
        vendor/vulkansamples/layers/"${a}".cpp
    done
    sed -i -e 's'\
'/static const char \* GetPhysDevFeatureString'\
'/inline const char * GetPhysDevFeatureString/' \
        vendor/vulkansamples/scripts/helper_file_generator.py
    sed -i -e 's,^#include "vulkan/vulkan.h",#include <vulkan/vulkan.h>,' \
        vendor/vulkansamples/layers/vk_format_utils.cpp
    ;;

  git-sync-deps)
    (
      touch git-sync-deps-pid
      (
        $lscolor vendor/skia/tools/git-sync-deps
        cd vendor/skia
        $python tools/git-sync-deps
      )
      rm -f git-sync-deps-pid
    ) &
    export GIT_SYNC_DEPS_PID=$!
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
    if ! grep -q "vendor/subgn" <<<"$PATH"; then
      export PATH="$PATH:$( cd -P -- "$PWD/vendor/subgn" && echo "$PWD" )"
      SUBGN_WAS_ADDED=1
    fi
    if [ -n "$GIT_SYNC_DEPS_PID" ]; then
      [ -f git-sync-deps-pid ] && echo "git-sync-deps..."
      wait $GIT_SYNC_DEPS_PID
    fi
    ;;

  *)
    echo "ERROR: unknown buildstep \"$buildstep\""
    exit 1
  esac
done

# If you choose, you may set the environment variable VOLCANO_NO_OUT to prevent
# gn and ninja running and producing a Volcano build. By design, gn only allows
# build artifacts to be in a top-level directory (typically "out"), so when
# volcano is a git submodule, you want to wait and run gn at the top level.
#
# install-gn-ninja will still *build* gn and ninja for you.
if [ -z "$VOLCANO_NO_OUT" ]; then
  TARGET=Debug

  # Pass $CC and $CXX to gn / ninja.
  args=""
  if [ -n "$CC" ]; then
    args="$args gcc_cc=\"$CC\""
  fi
  if [ -n "$CXX" ]; then
    args="$args gcc_cxx=\"$CXX\""
  fi
  if [ -n "$args" ]; then
    args="--args=$args"
    gn gen out/$TARGET "$args"
  else
    gn gen out/$TARGET
  fi
  NINJA_ARGS=""
  if [ -n "$BOOTSTRAP_ON_TRAVIS_J" ]; then
    NINJA_ARGS="$NINJA_ARGS -j$BOOTSTRAP_ON_TRAVIS_J"
  fi
  ninja -C out/$TARGET $NINJA_ARGS

  if [ -n "$SUBGN_WAS_ADDED" ]; then
    echo "Note: please add the following to your .bashrc / .cshrc / .zshrc:"
    echo ""
    echo "      PATH=\$PATH:$PWD/vendor/subgn"
    echo ""
    echo "      Which makes \"ninja -C out/$TARGET\" work."
    echo "      This script does that, so another option is to just re-run me."
    echo "      Then PATH changes are optional."
  fi
fi
