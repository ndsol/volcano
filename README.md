# [Volcano](https://github.com/ndsol/volcano) [![CI Status](https://travis-ci.org/ndsol/volcano.svg?branch=master)](https://travis-ci.org/ndsol/volcano)
Volcano is a [Vulkan](https://www.khronos.org/vulkan/) API that:

* Makes it easy to [get started with Vulkan](#getting-started).
* Keeps Vulkan simple, short and sweet.

## The Trade

Volcano is no silver bullet. These are some of the trade-offs it makes:

* Easy to make a prototype.
  * Easy to tweak, experiment, and hack.
  * Easy to explain what it does, share and collaborate openly.
  * Easy to debug.
* May not suit a large commercial game.
  * [GPLv3 license](LICENSE)
  * Uses the [chromium GN build system](https://chromium.googlesources.com/chromium/src/tools/gn/).
  * But the modularity of Volcano makes it easy to rip it out, piece by piece. :)

# Getting started

### 1. Grab Volcano by typing:
```
git clone https://github.com/ndsol/volcano
```
> On Windows, first install [git](https://git-scm.org).

*Note:* [VolcanoSamples](https://github.com/ndsol/VolcanoSamples) has
documentation for getting started with Volcano.

### 2. Build Volcano by typing:
```
volcano/build.cmd
```
> On Windows: always replace `/` with `\` like this:
>
> `volcano\build.cmd`

*Note:* the optional first-time setup steps that `volcano/build.cmd` spits out
on the terminal are a good idea.

### 3. Run a demo

```
cd volcano
vendor/subgn/ninja -C out/Debug
curl -o basic_test.png https://avatars2.githubusercontent.com/u/26904670?v=3
out/Debug/basic_test basic_test.png
```

# FAQ

## Why include skia?

1. You can always [rip it out](../../issues/10). The alternative, making you do
   all the work to integrate a 2D graphics library, isn't nearly as simple,
   short, and sweet.

2. Including it means a lot of design decisions have "defaults": e.g. the build
   system, `third_party` libs, and supported platforms of skia can carry over.

The downsides are: the volcano source uses 386MB for skia, 137MB for other dirs
in `vendor`, and ~2MB for Volcano. Binaries too: libskia.a (linux-amd64) is
421MB, and basic_test is 125MB (the linker eliminates unused code). That is too
fat for a large commercial game, but great for a prototype.

For comparison, Android Studio eats 2,000 MB for a fresh install. Volcano aims to
minimize download pain by always downloading dependencies first in `build.cmd`,
and can [build a 3MB APK](https://github.com/ndsol/VolcanoSamples/04android/)
without requiring the full Android Studio download.

## Build failed: vk_enum_string_helper.h:xxx: error: ‘VK_...’ was not declared

This occurs when the GLFW-provided vulkan.h in `vendor/glfw/vulkan/vulkan.h` is
located before `vendor/vulkansamples/include/vulkan/vulkan.h`. The `build.cmd`
script patches GLFW by removing its version: run `build.cmd` again to fix this
problem.

## 'ShaderLibrary' in namespace 'science' does not name a type

This occurs when `src/science/science.h` is #included but
`//vendor/volcano:science` is not added to the `deps` line in `BUILD.gn`.
Specifically, `//vendor/volcano:science_config` #defines
USE_SPIRV_CROSS_REFLECTION, and if it is missing, ShaderLibrary is not defined.

## Can I open an issue / submit a pull request?

Yes! Ask
[Smart Questions](http://www.catb.org/esr/faqs/smart-questions.html)
and understand that it may be impossible to grant your wish.
(No wishing for more wishes.)

[The rules for contributing are here.](CONTRIBUTING.md)

Volcano is not affiliated with or endorsed by the Khronos Group. Vulkan™ and the
Vulkan logo are trademarks of the Khronos Group Inc.

Copyright (c) 2017 the Volcano Authors. All rights reserved.
