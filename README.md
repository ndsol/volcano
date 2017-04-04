# [Volcano](https://github.com/ndsol/volcano)
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
> On Windows, choose from the [Clone or Download](../../archive/master.zip)
> link, [Github Desktop](https://desktop.github.com), or a similar tool.

### 2. Build Volcano by typing:
```
volcano/build.cmd
```
> On Windows: always replace `/` with `\` like this:
>
> `volcano\build.cmd`

*Note:* the optional first-time setup steps are a good idea.

### 3. Run a demo

# FAQ

## Help! I'm getting "git: command not found"

## Can I open an issue / submit a pull request?

Yes! Ask
[Smart Questions](http://www.catb.org/esr/faqs/smart-questions.html)
and understand that it may be impossible to grant your wish.
(No wishing for more wishes.)

[The rules for contributing are here.](CONTRIBUTING.md)

Copyright (c) 2017 the Volcano Authors. All rights reserved.
