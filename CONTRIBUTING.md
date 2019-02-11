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
1. Constructors only take references to parent objects.
1. Constructors do not call methods that can return an error, and avoid calling
   any methods in general.
1. Use named method calls and named members, not long lists of unnamed
   parameters to the constructor.
1. Mutable while being built.
1. Avoids having to choose between `throw` and `exit(1)`
1. Are "sort of immutable" after the instance is built.

By using constructors with only parent object references, the constructor call
is easy to read.

Not putting method calls in constructors avoids having to choose between `throw`
and `exit(1)`.

Initialization happens after the constructor. (This does not technically break
the RAII principle.) Using named method calls and named members also makes code
more future-proof if additional members are added. It can even make code
backward-compatible. Best of all, named methods and members are much easier to
read and debug.

The call to `ctorError` builds the instance from the data provided in methods
and members. When it returns, the instance is done, built, "sort of immutable,"
and should be destroyed if a different setup is needed.

The "sort of immutable" status is a quirk of Vulkan: some Vulkan APIs allow
mutable objects to avoid excessive duplication:

1. `vkCreateSwapchainKHR` and `vkCreateFramebuffer` may be required at any time
   while the app is running. A window resize changes only a few parameters, thus
   instead of destroying the `language::Device`, the `resetSwapChain` method
   mutates the current `Device`.
1. When `language::Instance` is marshalling devices, the swapchain, and queues,
   a tradeoff to maintain flexibility and simplicity is to require three method
   calls:
   1. the C++ constructor
   1.`ctorError`
   1.`open`
1. Pipeline derivatives

Because Vulkan always makies a deep copy of any data passed to a function,
Vulkan objects are *de facto* immutable, even if the Volcano members are later
mutated.

#### Having to choose between `throw` and `exit(1)`

Since some applications disable exceptions to reduce total binary size,
[Volcano](https://github.com/ndsol/volcano) does not use exceptions. It will
work with code that uses exceptions or code that does not.

Any method that might return an error must be called in `ctorError`. Avoid
calling methods in the constructor if at all possible.

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
if (cpool.ctorError() || imageAvailableSemaphore.ctorError(dev) ||
    renderSemaphore.ctorError() ||
    // Read compiled-in shaders from app into Vulkan.
    vshader->loadSPV(spv_01glfw_vert, sizeof(spv_01glfw_vert)) ||
    fshader->loadSPV(spv_01glfw_frag, sizeof(spv_01glfw_frag)) ||
    // Add VkShaderModule objects to pipeline. Build RenderPass.
    pipe0->info.addShader(vshader, pass, VK_SHADER_STAGE_VERTEX_BIT) ||
    pipe0->info.addShader(fshader, pass, VK_SHADER_STAGE_FRAGMENT_BIT) ||
    // Manually resize language::Framebuf the first time.
    onResized(dev.swapChainInfo.imageExtent)) {
  return 1;
}
```
This code can still be considered "simple, short, and sweet." The liberal use
of comments keeps terms of the `if ()` statement from piling up on one line.
Avoid grouping too many unrelated method calls in a single `if ()` statement.

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
1. `cd ../../vendor/vulkansamples`
1. `git checkout -- .` # silently erase any changes made in this dir
1. `cd ../../vendor/spirv_cross`
1. `git checkout -- .` # silently erase any changes made in this dir
1. `cd ../..`
1. Now get the latest updates: (or some people use `git pull`)
   ```
   git fetch
   git merge
   ```
1. Rebuild with `build.cmd` (This automatically puts the patches back into
   vendor/glfw, vendor/vulkansamples, and vendor/spirv_cross.)

### How do I use *clang-format*?

`clang-format` will automatically reformat files to the required style for a
pull request.

* **Recommended**: your text editor can run **clang-format** automatically:
  [Instructions](https://clang.llvm.org/docs/ClangFormat.html).
* **Not recommended:** Format a whole file from the command line:
  `clang-format path/to/file.c > /tmp/formatted.c`

### Why do I get an error using *clang-format*?

------

* **If you get this error:**
  * Unix-like OSes: `bash: clang-format: command not found`
  * Windows: `'clang-format' is not recognized as an internal or external command`

**Then** `clang-format` was not installed correctly. Please run
`volcano/build.cmd` to diagnose the problem.

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
