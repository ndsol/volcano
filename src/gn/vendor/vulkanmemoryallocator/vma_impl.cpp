/* Copyright (c) 2018 the Volcano Authors. Licensed under the GPLv3.
 * Just one file should #define VMA_IMPLEMENTATION before
 * #include "vk_mem_alloc.h"
 */

#include <src/core/VkPtr.h>

// Generates the implementation of functions not written as inline.
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
