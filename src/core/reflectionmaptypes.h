/* Copyright (c) 2018 the Volcano Authors. Licensed under the GPLv3.
 *
 * This list is #include'd multiple times in reflectionmap.h to run the
 * SPECIALIZE() macro over the list.
 *
 * VolcanoReflectionMap will automatically deduce the type if it is in the
 * list. Be careful not to add "equivalent" types to the list (such as
 * "unsigned int" - uint32_t and uint64_t are already listed). A compiler
 * error such as "overloaded return types are ambiguous" will result.
 */

SPECIALIZE(VkBool32, Bool32, VkBool32)
SPECIALIZE(char, value_char, )
SPECIALIZE(unsigned char, value_unsigned_char, )
SPECIALIZE(int, value_int, )
SPECIALIZE(size_t, value_size_t, size_t)
SPECIALIZE(uint32_t, value_uint32_t, )
SPECIALIZE(uint64_t, value_uint64_t, )
SPECIALIZE(float, value_float, )
SPECIALIZE(VkExtent2D, Extent2D, )

/* SPECIALIZE each enum separately for type safety */
SPECIALIZE(VkPhysicalDeviceType, PhysicalDeviceType, )
SPECIALIZE(VkPointClippingBehavior, PointClippingBehavior, )
SPECIALIZE(VkDriverIdKHR, DriverIdKHR, )
