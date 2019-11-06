/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */

#ifndef __APPLE__
#error This file should only be included for macOS builds.
#endif /*__APPLE__*/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

// glfwCocoaWindowLionMaximized returns non-zero if the window is fullscreen.
// NOTE: macOS 10.7 and later define this new "spaces fullscreen." GLFW can
// still do a borderless floating window, but this is different.
int glfwCocoaWindowLionMaximized(GLFWwindow* window);

// glfwCocoaWindowSetLionMaximized sets the window to fullscreen or clears it.
// NOTE: macOS 10.7 and later define this new "spaces fullscreen." GLFW can
// still do a borderless floating window, but this is different.
void glfwCocoaWindowSetLionMaximized(GLFWwindow* window, int fullscreen);

#ifdef __cplusplus
}
#endif /*__cplusplus*/
