/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */

#include <src/science/science-glfw.h>

const char* GLFWfullscreen::getFullscreenType() const {
#ifdef _WIN32
  // GLFW Win sets an exclusive mode. On a second monitor it can even be
  // composited, which is kind of weird.
  return "exclusive fullscreen";
#endif /*_WIN32*/
#ifdef __APPLE__
  // macOS has different modes. https://github.com/glfw/glfw/issues/484
  // https://github.com/glfw/glfw/commit/9c15e2ff869903fad393aafe4e0e8798c942aa30
  return "floating window";
#endif /*__APPLE__*/
#ifdef __ANDROID__
  // Android apps are always fullscreen, this just removes the title bar.
  return "fullscreen";
#endif /*__ANDROID__*/
#ifdef __linux__
  // GLFW X11 only switches to a borderless floating window.
  return "floating window";
#endif /*__linux__*/
}

bool GLFWfullscreen::canDrawTransparent(GLFWwindow* win) const {
#ifdef __ANDROID__
  // Android uses AndroidManifest.xml (android:theme).
  return true;
#else /*__ANDROID__*/
  return glfwGetWindowAttrib(win, GLFW_TRANSPARENT_FRAMEBUFFER);
#endif /*__ANDROID__*/
}
