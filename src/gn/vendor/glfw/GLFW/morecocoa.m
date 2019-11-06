/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

int glfwCocoaWindowLionMaximized(GLFWwindow* window) {
  id obj = glfwGetCocoaWindow(window);
  return [obj isZoomed] && ([obj styleMask] & NSFullScreenWindowMask);
}

void glfwCocoaWindowSetLionMaximized(GLFWwindow* window, int fullscreen) {
  if (!!fullscreen == glfwCocoaWindowLionMaximized(window)) {
    return;
  }
  id obj = glfwGetCocoaWindow(window);
  [obj toggleFullScreen:nil];
}
