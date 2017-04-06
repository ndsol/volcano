#!/bin/bash
# Copyright (c) 2017 the Volcano Authors. Licensed under GPLv3.
set -e

cd $( dirname $0 )
cd ..
missing=""

os=""
lscolor="ls --color"
if [ "$(uname)" == "Darwin" ]; then
  os="mac"
  lscolor="ls -G"
elif [ "$(uname -m)" != "x86_64" ]; then
  echo "WARNING: 'uname -m' reports your CPU does not support 64 bits:"
  uname -a
  echo ""
  os="linux64"
else
  # Should work for *BSD. Untested.
  os="linux64"
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
python_aptget="python"
python2_pacman="python2"
python_emerge="dev-lang/python"
python_dnf="python"
python_yum="python"
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
gcc_cpp_aptget="build-essentials"
gcc_cpp_pacman="base-devel"
gcc_cpp_emerge="gcc"
gcc_cpp_dnf="gcc-c++"
gcc_cpp_yum="gcc-c++"
fontconfig_aptget="libfontconfig1-dev"
fontconfig_pacman="fontconfig"
fontconfig_emerge="media-libs/fontconfig"
fontconfig_dnf="fontconfig-devel"
fontconfig_yum="fontconfig-devel"
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

python="python"
if [ "$installer" == "pacman" ]; then
  python="python2"
fi

clist=""
for c in git $python patch pkg-config c++; do
  if ! which $c >/dev/null 2>&1; then
    if [ "$c" == "$python" -a "$installer" == "pacman" ]; then
      echo "WARNING: If you see: \"exec: python: not found\""
      echo "WARNING: Arch Linux has moved to python3."
      echo "WARNING: depot_tools has not! You will need to manually edit"
      echo "WARNING: the files that 'grep -r python ../depot_tools' finds"
      echo "WARNING: where ../depot_tools may already be in your PATH"
    elif [ "$c" == "c++" ]; then
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

# has depot_tools been installed correctly?
if ! which download_from_google_storage >/dev/null 2>&1; then
  if [ -d ../depot_tools ]; then
    export PATH="$PATH:$(realpath ../depot_tools)"
    if ! which download_from_google_storage >/dev/null 2>&1; then
      echo "I found dir $(realpath ../depot_tools) but not"
      echo "  download_from_google_storage"
      echo "Please set up your PATH to point to depot_tools."
      echo "  http://www.chromium.org/developers/how-tos/install-depot-tools"
      exit 1
    fi
  else
    missing="$missing depot_tools"
  fi
fi

SUBMODULE_CMD="git submodule update --init --recursive"
if ! $SUBMODULE_CMD --depth 51; then
  echo ""
  echo "WARN: $SUBMODULE_CMD --depth 51 # failed, retry without --depth"
  $SUBMODULE_CMD
fi

# has glfw been patched?
if grep -q '_glfw_dlopen("libvulkan\.so\.1' vendor/glfw/src/vulkan.c >/dev/null 2>&1; then
  missing="$missing patch-glfw"
fi

# has skia been patched?
if grep -q 'getenv("VULKAN_SDK")' vendor/skia/BUILD.gn >/dev/null 2>&1; then
  missing="$missing patch-skia"
fi



missing="$missing git-sync-deps"
echo "volcano updating:$missing"

for buildstep in $missing; do
  case $buildstep in
  depot_tools)
    (
      cd ..
      git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    )
    export PATH="$PATH:$(realpath ../depot_tools)"
    ;;

  patch-glfw)
    # patch glfw to use libvulkan.so instead of libvulkan.so.1
    # and remove its version of vulkan.h
    sed -i -e 's/\(_glfw_dlopen("libvulkan.so\).1")/\1")/g' vendor/glfw/src/vulkan.c
    rm -r vendor/glfw/deps/vulkan
    ;;

  patch-skia)
    # patch skia to use libvulkan.so instead of lib/libvulkan.so
    # and to depend on vulkansamples (outside the skia tree)
    sed -i -e 's/\(skia_vulkan_sdk = \)getenv("VULKAN_SDK")/\1root_out_dir/' \
           -e 's:\(lib_dirs [+]= \[ "$skia_vulkan_sdk/\)lib/" \]:\1" ]:' \
           vendor/skia/BUILD.gn
    # patch skia to honor {{output_dir}} in toolchain("gcc_like")
    # this is needed by //vendor/vulkansamples because libVkLayer_*.so must be
    # installed with the layer json files, and that crowds {{root_out_dir}}.
    patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/skia/BUILD.gn
+++ b/vendor/skia/BUILD.gn
@@ -461,6 +461,10 @@ optional("gpu") {
     ]
     public_defines += [ "SK_ENABLE_SPIRV_VALIDATION" ]
   }
+  if (!defined(deps)) {
+    deps = []
+  }
+  deps += [ "//vendor/vulkansamples:vulkan_headers" ]
 }
 
 optional("jpeg") {
--- a/vendor/skia/gn/BUILD.gn
+++ b/vendor/skia/gn/BUILD.gn
@@ -694,8 +694,9 @@ toolchain("gcc_like") {
 
     command = "\$cc_wrapper \$cxx -shared {{ldflags}} {{inputs}} {{solibs}} {{libs}} \$rpath -o {{output}}"
     outputs = [
-      "{{root_out_dir}}/\$soname",
+      "{{output_dir}}/\$soname",
     ]
+    default_output_dir = "{{root_out_dir}}"
     output_prefix = "lib"
     default_output_extension = ".so"
     description = "link {{output}}"
EOF
    # skia does not handle Arch Linux's python3 well
    patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/skia/gn/is_clang.py
+++ b/vendor/skia/gn/is_clang.py
@@ -9,9 +9,9 @@ import subprocess
 import sys
 cc,cxx = sys.argv[1:3]
 
-if ('clang' in subprocess.check_output('%s --version' % cc, shell=True) and
-    'clang' in subprocess.check_output('%s --version' % cxx, shell=True)):
-  print 'true'
+if (b'clang' in subprocess.check_output('%s --version' % cc, shell=True) and
+    b'clang' in subprocess.check_output('%s --version' % cxx, shell=True)):
+  print('true')
 else:
-  print 'false'
+  print('false')
 
EOF
    ;;

  git-sync-deps)
    $lscolor -d buildtools/$os
    for tool in gn clang-format; do
      set +e
      R="$( cd "buildtools/$os" && download_from_google_storage -n -b "chromium-$tool" -s "$tool.sha1" 2>&1 >/dev/null )"
      RV=$?
      set -e
      if [ $RV -ne 0 ]; then
        echo "ERROR: cd buildtools/$os && download_from_google_storage -n -b chromium-$tool -s $tool.sha1"
        echo ""
        echo "$R"
        exit $RV
      fi
      if ldd "buildtools/$os/$tool" | grep -q "not found"; then
        echo "ERROR: $tool is installed in buildtools/$os/$tool"
        echo "ERROR: but is missing a dependency according to"
        echo "       ldd buildtools/$os/$tool:"

        ldd "buildtools/$os/$tool"

        case "$installer" in
        pacman)
          echo "    libtinfo.so.5 is at https://aur.archlinux.org/packages/ncurses5-compat-libs"
          ;;
        dnf|yum)
          echo "    for libtinfo.so.5 please $installer install ncurses-compat-libs"
          ;;
        esac
        exit 1
      fi
      if ! "$tool" --help >/dev/null; then
        echo "ERROR: depot_tools should provide $tool, something is fishy."
        exit 1
      fi
    done

    (
      $lscolor vendor/skia/tools/git-sync-deps
      cd vendor/skia
      $python tools/git-sync-deps
    )
    # All first-level files under gn must exactly match vendor/skia/gn (because
    # chromium's gn does not have a one-size-fits all definition, a project with
    # a submodule that uses gn -- like vendor/skia -- must keep all rules in
    # lock-step between all submodules!)
    #
    # Note that .gn and gn/vendor are unique to volcano and not part of skia.
    cp -f vendor/skia/gn/* gn/

    # vendor/skia/BUILD.gn assumes it is the source root, an assumption that can
    # be simulated except when it attempts its third_party integration.
    # This hack fixes third_party for vendor/skia.
    ln -sf vendor/skia/third_party third_party
    ;;

  *)
    echo "ERROR: unknown buildstep \"$buildstep\""
    exit 1
  esac
done

gn gen out/Debug
ninja -C out/Debug

if [ "${missing/depot_tools/}" != "$missing" ]; then
  echo ""
  echo "INFO: depot_tools was installed in $(realpath ..)"
  echo "      http://www.chromium.org/developers/how-tos/install-depot-tools"
  echo "      Many projects can use the same installation. It just needs to be"
  echo "      in your PATH. Please edit $HOME/.bashrc and add this line to it:"
  echo "          export PATH=\$PATH:$(realpath ..)/depot_tools"
  echo ""
  echo "      Every few months or so, update depot_tools with this command:"
  echo "        gsutil.py update"
fi
