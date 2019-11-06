/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This file does the #define and #include steps to include GLFW in your
 * application. It also provides glue code for handling fullscreen apps.
 *
 * NOTE: Your app will fail to link if you do not also add a dependency to the
 * GLFW library: deps += [ "//src/gn/vendor/glfw" ]
 */

#include <src/core/VkPtr.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif /*_WIN32*/
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#include <GLFW/morecocoa.h>
#endif /*__APPLE__*/
#if defined(__linux__) && !defined(__ANDROID__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif /*__linux__*/

#pragma once

#ifdef __cplusplus

#include <set>

// GLFWfullscreen can be the only part of this header your app needs to talk
// to. The other types defined can be accessed through GLFWfullscreen.
typedef struct GLFWfullscreen GLFWfullscreen;

// monitorWithName stores the name of the monitor as a way to de-duplicate.
typedef struct monitorWithName {
  monitorWithName(GLFWmonitor* mon) : mon(mon) {
    name = glfwGetMonitorName(mon);
  }

  GLFWmonitor* const mon;
  std::string name;
} monitorWithName;

// This allows std::set to compare one monitorWithName with another.
inline bool operator<(const monitorWithName& a, const monitorWithName& b) {
  return a.name < b.name;
}

// monitors is a global because the GLFW callback lacks any user data pointer.
extern std::set<monitorWithName> monitors;

// onGLFWmonitorChange handles any monitor change by updating monitors.
// Your app may also call it manually to poll monitors.
void onGLFWmonitorChange(GLFWmonitor* monitor, int event);

// GLFWfullscreen can be the only part of this header your app needs to talk
// to. The other types defined can be accessed through GLFWfullscreen.
typedef struct GLFWfullscreen {
  const std::set<monitorWithName>& getMonitors() const { return monitors; }

// Create a shortcut for excluding isLionMax() as a state. This macro gets
// undefined just below and is only for these functions.
#ifdef __APPLE__
#define IS_LION_MAX(w) isLionMax(w)
#else /*__APPLE__*/
#define IS_LION_MAX(w) false
#endif /*__APPLE__*/

  bool isNormal(GLFWwindow* win) const {
    return !(IS_LION_MAX(win) || isFullscreen(win) || isMaximized(win));
  }

  const char* getFullscreenType() const;

  bool isFullscreen(GLFWwindow* win) const {
    return glfwGetWindowMonitor(win) != NULL;
  }

  GLFWmonitor* getMonitor(GLFWwindow* win) const {
    return glfwGetWindowMonitor(win);
  }

  void setFullscreen(GLFWwindow* win, GLFWmonitor* mon) {
    if (!!mon == isFullscreen(win)) {
      return;
    }
    if (mon) {
      saveOldPosition(win);
      auto mode = glfwGetVideoMode(mon);
      glfwSetWindowMonitor(win, mon, 0, 0, mode->width, mode->height,
                           mode->refreshRate);
      return;
    }
    if (oldWinW < 1 || oldWinH < 1) {
      oldWinW = 800;  // Provide reasonable defaults.
      oldWinH = 600;
      if (oldWinX < 1 && oldWinY < 1) {
        auto mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        oldWinX = (mode->width - oldWinW) / 2;
        oldWinY = (mode->height - oldWinH) / 2;
      }
    }
    glfwSetWindowMonitor(win, NULL, oldWinX, oldWinY, oldWinW, oldWinH,
                         GLFW_DONT_CARE);
  }

  bool isMaximized(GLFWwindow* win) const {
    return !IS_LION_MAX(win) && !isFullscreen(win) &&
           glfwGetWindowAttrib(win, GLFW_MAXIMIZED);
  }

  void setMaximized(GLFWwindow* win, bool maximized) {
    if (maximized) {
      saveOldPosition(win);
      glfwMaximizeWindow(win);
    } else {
      glfwRestoreWindow(win);
    }
  }

#ifdef __APPLE__
  bool isLionMax(GLFWwindow* win) const {
    return glfwCocoaWindowLionMaximized(win);
  }

  void setLionMax(GLFWwindow* win, bool maximized) const {
    glfwCocoaWindowSetLionMaximized(win, maximized);
  }
#endif /*__APPLE__*/

  bool canDrawTransparent(GLFWwindow* win) const;

 protected:
  int oldWinX{0}, oldWinY{0}, oldWinW{0}, oldWinH{0};
  void saveOldPosition(GLFWwindow* win) {
    if (!IS_LION_MAX(win) && !isMaximized(win) && !isFullscreen(win)) {
      // Save un-fullscreen window position
      glfwGetWindowPos(win, &oldWinX, &oldWinY);
      glfwGetWindowSize(win, &oldWinW, &oldWinH);
    }
  }
#undef IS_LION_MAX
} GLFWfullscreen;

#endif /*__cplusplus*/
