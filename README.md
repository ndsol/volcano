# [Volcano](https://github.com/ndsol/volcano) [![CI Status](https://travis-ci.org/ndsol/volcano.svg?branch=master)](https://travis-ci.org/ndsol/volcano) [![Build status](https://ci.appveyor.com/api/projects/status/k72xkoh00lax053d?svg=true)](https://ci.appveyor.com/project/ndsol/volcano)
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

Volcano strives to be as thin as possible. When automating a step (such as
shader reflection) the single responsibility principle guides: less is more.
Each member or method is carefully chosen to illuminate and simplify
Vulkan. Vulkan is what Volcano is about.

Vulkan is a C API, because C is a low level language. Volcano is a C++ API and
very modular. If/when Vulkan serves you better, your app can use it instead.
Volcano is designed to be ripped out, piece by piece, such as if you prefer a
different license. The Volcano Authors also offer cross-licensing terms.

Volcano's strives to use broadly supported C++ idioms:

* C++11 only
* Exception handling is not required (useful for the Android NDK)
* Composition is preferred over inheritance (fewer vtables/fixups on start)

Volcano is also
[extensively documented](https://github.com/ndsol/VolcanoSamples) to speed your
development process.

## Why include skia? (It's bloated.)

1. It's always easier to [rip it out](../../issues/10) than to integrate a
   2D graphics library and validate it. There are no hard skia dependencies.

2. Including it means a lot of design decisions have "defaults": e.g. the build
   system, `third_party` libs, and platform support carry over. If chrome runs,
   Volcano runs. (Because skia is a part of the Chromium open source project.)

The downsides are: the volcano source uses 386MB for skia, 137MB for other dirs
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
