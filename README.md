# [Volcano](https://github.com/ndsol/volcano) [![CI Status](https://travis-ci.org/ndsol/volcano.svg?branch=master)](https://travis-ci.org/ndsol/volcano) [![Build status](https://ci.appveyor.com/api/projects/status/k72xkoh00lax053d?svg=true)](https://ci.appveyor.com/project/ndsol/volcano) [![win2016](https://github.com/ndsol/volcano/workflows/win2016/badge.svg)](https://github.com/ndsol/volcano/actions/)
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

## If I use Volcano, does it remove the low-level purpose of Vulkan?

Volcano is thin. We believe Volcano is Vulkan just like GPUOpen
VulkanMemoryAllocator is Vulkan.

Automation makes the developer experience better (such as shader reflection)
but it is used lightly. Vulkan is what Volcano is about.

Another way to think about this question is: what does the language provide?

C is a low level language; Vulkan is a C API.

Volcano aims for the best of C++. If/when your app transitions to using the
C API, Volcano is designed to make that painless too: Volcano is modular so
you can incrementally refactor out one piece at a time.

Volcano aims to be higher-level without being "fat."

For instance, you write an amazing game prototype in Volcano. But now you
want to get rid of GPLv3-licensed code. (The Volcano Authors also offer
cross-licensing terms.) Volcano can indeed be easily refactored out, and
you are left with "just Vulkan." Many developers have taken a similar
approach to rewrite / strip out the Vulkan SDK "loader," prefering to
directly load the Vulkan ICD GPU driver directly. Volcano enables that
level of hacking.

Volcano strives to use broadly supported C++ idioms:

* C++11 only
* Exception handling is not required (useful for the Android NDK)
* Composition is preferred over inheritance (fewer vtables/fixups on start)

Volcano is also
[extensively documented](https://github.com/ndsol/VolcanoSamples) to speed your
development process.

## Why build vulkan from source?

1. Volcano enables you to run vulkan without going through as many of the
   download / install steps first. For example on Windows, only git and
   python are required. If you don't have administrator access, this can
   make a huge difference.

2. At times the vulkan build has had minor bugs even in a stable release. The
   fix shows up in the source first, making for a better experience.

3. Vulkan files under the `generated` dirs are used by Volcano and provided
   to Volcano users. These files provide useful helper methods that are not
   part of the public API and may change without notice, but are derived from
   the Vulkan Spec and are automatically updated when the spec is updated.
   It's up to you whether you want to use them, but they are indispensible for
   keeping Volcano simple, short and sweet.

4. Volcano uses git shallow clones where possible. This usually saves download
   and disk space.

There are tradeoffs as well. Building the vulkan loader and validation
layers in debug mode enables additional checks such as the "ICD magic" check.
Debug builds may also use more memory and run slower. Debug builds include
debug symbols which can be very helpful tracking down crashes.

## Why include skia? (It's bloated.)

1. It's always easier to [rip it out](../../issues/10) than to integrate a
   2D graphics library and validate it. There are no hard skia dependencies.

2. Including it means a lot of design decisions have "defaults": e.g. the build
   system, `third_party` libs, and platform support carry over. If chrome runs,
   Volcano runs. (Because skia is a part of the Chromium open source project.)

The downsides are: the volcano source uses 371MB for skia, 102MB for other dirs
in `vendor`, and ~2MB for Volcano. Binaries too: libskia.a (linux-amd64) is
421MB, and basic_test is 125MB (the linker eliminates unused code). That is too
fat for a large commercial game, but great for a prototype.

Choosing skia actually saves space. Compare with Android Studio: 2,000 MB for a
fresh install. (That's after installing the SDK and NDK.) Volcano also improves
the build process. Just run `build.cmd` - it downloads everything right at the
start and can
[build a 3MB APK](https://github.com/ndsol/VolcanoSamples/04android/),
bypassing the manual clicks to get Android Studio set up.

## Can I open an issue / submit a pull request?

Yes! Ask
[Smart Questions](http://www.catb.org/esr/faqs/smart-questions.html)
and understand that it may be impossible to grant your wish.
(No wishing for more wishes.)

[The rules for contributing are here.](CONTRIBUTING.md)

Volcano is not affiliated with or endorsed by the Khronos Group. Vulkan™ and the
Vulkan logo are trademarks of the Khronos Group Inc.

Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
