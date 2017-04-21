# Contributing to Volcano

In order to make it easier to contribute to Volcano, please observe the
following in your pull requests:

1. The CLA assistant and CI build status must pass.
1. Code must follow the
   [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
   (see [How to use **clang-format**](#how-do-i-use-clang-format)).
1. Commit messages must follow the patch submission guidelines for the Linux
   kernel. The official guidelines are
   [here](http://lxr.linux.no/#linux/Documentation/process/submitting-patches.rst),
   and a decent summary is
   [here](https://kernelnewbies.org/PatchPhilosophy).

   Use `git commit --amend` and `git push -f origin master` to
   amend your commit message and force-push to your pull request.
   Github can handle it!

# API Design Guide

The follownig design patterns help [Volcano reach its goal](README.md):

* **Makes it easy to get started with Vulkan.**

  The API design keeps classes well defined and avoid premature optimization
  which might add unnecessary complexity.

* **Keeps Vulkan simple, short and sweet.**

  The Vulkan API is quite verbose, giving the application complete control over
  the device. Volcano makes some [trade offs](README.md#the-trade) which can
  make prototyping and debugging much easier.

### 1. Builder Pattern

Volcano classes follow the builder pattern:

1. Provide reasonable defaults.
1. Mutable while being constructed.
1. Avoids having to choose between `throw` and `exit(1)`
1. Can be considered immutable after the class is fully constructed.

Volcano classes occasionally break the last guideline, and can be mutated after
the class is fully constructed. Some Vulkan APIs necessitate this, and rather
than create a lot of duplicate objects, Volcano follows Vulkan:

* `vkCreateSwapchainKHR` and `vkCreateFramebuffer` must be called repeatedly
  with slightly different parameters (`onResize` methods do this).
* Pipeline derivatives
* `language::Instance` marshalling devices, the swapchain, and queues. Without
  losing either flexibility or simplicity, the constructor is split into: (1)
  the C++ constructor, (2) `ctorError`, and (3) `open`.

Because Vulkan always makies a deep copy of any data passed as a function
argument, Vulkan objects are *de facto* immutable, even in the cases where
Volcano does not use `const` to enforce it statically.

#### Having to choose between `throw` and `exit(1)`

Since some applications disable exceptions to reduce total binary size,
[Volcano](https://github.com/ndsol/volcano) does not use exceptions. It will
work with code that uses exceptions or code that does not.

For error handling, the code that would otherwise be in the constructor is moved
to a `ctorError` method. Thus the caller:

1. Constructs something (which has reasonable defaults that make it usable
   "out of the box")
1. Optionally mutates it through public members (carefully check the class
   comments for more information)
1. Finalizes the class by calling `ctorError`, and
1. Checks the return value of `ctorError`.

If that code was all placed in the constructor, the only choices would be to
`throw` or call `exit(1)` on an error.

### 2. Avoid Premature Optimization

The [trade offs](README.md#the-trade) Volcano makes can reduce performance and
increase memory usage.

Examples:

* The create info struct is kept for the lifetime of the object. This makes
  building, finalizing, and debugging easier at the cost of more RAM used.

#### Checking Return Codes
[Volcano](https://github.com/ndsol/volcano) checks each Vulkan call for an
error.

Typical code blocks end up looking like
[01glfw.cpp](https://github.com/ndsol/volcanosamples/01glfw/01glfw.cpp):
```C++
if (imageAvailableSemaphore.ctorError(dev) || renderSemaphore.ctorError() ||
    // Read compiled-in shaders from app into Vulkan.
    vshader->loadSPV(spv_01glfw_vert, sizeof(spv_01glfw_vert)) ||
    fshader->loadSPV(spv_01glfw_frag, sizeof(spv_01glfw_frag)) ||
    // Add VkShaderModule objects to pipeline. Build RenderPass.
    pipe.info.addShader(vshader, pass, VK_SHADER_STAGE_VERTEX_BIT) ||
    pipe.info.addShader(fshader, pass, VK_SHADER_STAGE_FRAGMENT_BIT) ||
    pass.ctorError(dev) || cpool.ctorError(0) ||
    // Manually set up language::Framebuf the first time.
    // (vkCreateFramebuffer requires a valid RenderPass, and builder
    // requires a valid CommandPool.)
    onResized(dev.swapChainInfo.imageExtent)) {
  return 1;
}
```
This type of error checking could still be considered "simple, short, and
sweet."

### 3. Use Raw Vulkan Types Rarely

The Vulkan Types wrapped in Volcano types can be used without directly accessing
them. When needed, the Vulkan Type is available as a public member named `vk`.
However, avoid accessing the `vk` member.

The exception to this rule is:

* To avoid circular dependencies. If one Volcano header file consumes a type
  that is defined in another Volcano header, the `vk` member is used to break
  the cycle.

### 4. Code Smells

Volcano follows these "code smell" guidelines:

1. Don't repeat yourself
1. Combinatorial explosion
1. Short parameter lists
1. Comments explain "why" more than "what" or "how"
1. Names that are meaningful, consistent, and concise

Volcano generally does not obey:

1. Long methods should be refactored into shorter methods

   Volcano methods are verbose, but well commented.

1. Speculative generality should be avoided

   Volcano is only speculative in the same way Vulkan is speculative: there are
   numerous parameters you can tweak, but you are not required to tweak any.

1. Exposing class internals

   Volcano exposes the internals in the same way Vulkan exposes the internals:
   the numerous parameters available for tweaking.

## Contributing FAQ

### Why do I get merge conflicts?

To get the latest updates from github, especially right before submitting a
pull request, follow these steps to avoid merge conflicts in submodules:

1. `cd` to the top level Volcano directory.
1. `cd vendor/glfw`
1. `git checkout -- .` # silently erase any changes made in this dir
1. `cd ../../vendor/skia`
1. `git checkout -- .` # silently erase any changes made in this dir
1. Now get the latest updates: (or some people use `git pull`)
   ```
   git fetch
   git merge
   ```
1. Rebuild with `build.cmd` (this will re-patch glfw and skia)

### How do I use *clang-format*?

clang-format is installed with `depot_tools`, one of the first-time build
setup steps that `volcano/build.cmd` handles.

* **Recommended**: your text editor can run **clang-format** automatically:
  [Instructions](https://clang.llvm.org/docs/ClangFormat.html).
* **Not recommended:** Format a whole file from the command line:
  `clang-format path/to/file.c > /tmp/formatted.c`

### Why do I get an error using *clang-format*?

------

* **If you get this error:**
  * Unix-like OSes: `bash: clang-format: command not found`
  * Windows: `'clang-format' is not recognized as an internal or external command`

**Then** `depot_tools` was not installed correctly by `volcano/build.cmd`.

------

* **If you get this error:**
```
Problem while looking for clang-format in Chromium source tree:
File does not exist: .../volcano/buildtools/$OS/clang-format
```

**Then** run `volcano/build.cmd` again. The
[buildtools](https://chromium.googlesource.com/chromium/buildtools.git)
repo may have a broken link for clang-format on your OS.
Please also alert the Volcano Authors. This repo may be able to use a different
`buildtools` commit hash that fixes the problem.

### How does Volcano compare to `vulkan.hpp`?

**tl;dr:** Volcano is a simpler, shorter, sweeter layer, but it is still built
on the power of Vulkan.

------

Compare `language::Instance` to `vk::Instance` for example:

* `vk::Instance` is defined in `vulkan.hpp`, a header file auto-generated to
  exactly match the Vulkan spec.
* `vk::Instance` contains the `create...SurfaceKHR` functions as members, but...
* `vk::Instance` makes no attempt to integrate with VkPhysicalDevice or
  VkDevice.
* A separate `vk::UniqueInstance` can be used for scoped allocation and
  deallocation.

------

On the other hand,

* `language::Instance` has hand-written documentation (right next to the code,
  in [`language.h`](blob/master/src/language/language.h)) to make things easier
  like debugging, IDE integration, and "browsing the code".
* `language::Instance` and a
  [WSI glue class](https://github.com/ndsol/VolcanoSamples/01glfw/) together
  cut down the initial app boilerplate to 3 lines of code.
* `language::Instance` automatically enumerates devices and conveniently
  initializes queue families across the available devices, without losing the
  power to customize which device, queue families, swapChain, etc. the app may
  need.
* `language::Instance` disallows copy and assign to encourage correct usage,
  but there is only the one class, `Instance`.

Copyright (c) 2017 the Volcano Authors. All rights reserved.
