// dear imgui: Platform Backend for GLFW
// This needs to be used along with a Renderer (e.g. OpenGL3, Vulkan, WebGPU..)
// (Info: GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (Requires: GLFW 3.1+. Prefer GLFW 3.3+ for full feature support.)

// Implemented features:
//  [X] Platform: Clipboard support.
//  [X] Platform: Mouse support. Can discriminate Mouse/TouchScreen/Pen (Windows only).
//  [X] Platform: Keyboard support. Since 1.87 we are using the io.AddKeyEvent() function. Pass ImGuiKey values to all key functions e.g. ImGui::IsKeyPressed(ImGuiKey_Space). [Legacy GLFW_KEY_* values are obsolete since 1.87 and not supported since 1.91.5]
//  [X] Platform: Gamepad support. Enable with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.
//  [X] Platform: Mouse cursor shape and visibility (ImGuiBackendFlags_HasMouseCursors). Resizing cursors requires GLFW 3.4+! Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.
// Missing features or Issues:
//  [ ] Touch events are only correctly identified as Touch on Windows. This create issues with some interactions. GLFW doesn't provide a way to identify touch inputs from mouse inputs, we cannot call io.AddMouseSourceEvent() to identify the source. We provide a Windows-specific workaround.
//  [ ] Missing ImGuiMouseCursor_Wait and ImGuiMouseCursor_Progress cursors.
//  [ ] Multi-viewport: ParentViewportID not honored, and so io.ConfigViewportsNoDefaultParent has no effect (minor).

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct GLFWwindow;
struct GLFWmonitor;

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool     ImGui_ImplGlfw_InitForVulkan(GLFWwindow* window);
IMGUI_IMPL_API bool     ImGui_ImplGlfw_InitForOther(GLFWwindow* window);
IMGUI_IMPL_API void     ImGui_ImplGlfw_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplGlfw_NewFrame();

// GFLW callbacks options:
// - Set 'chain_for_all_windows=true' to enable chaining callbacks for all windows (including secondary viewports created by backends or by user)
IMGUI_IMPL_API void     ImGui_ImplGlfw_SetCallbacksChainForAllWindows(bool chain_for_all_windows);

// GLFW callbacks (individual callbacks to call yourself if you didn't install callbacks)
IMGUI_IMPL_API void     ImGui_ImplGlfw_WindowFocusCallback(GLFWwindow* window, int focused);        // Since 1.84
IMGUI_IMPL_API void     ImGui_ImplGlfw_CursorEnterCallback(GLFWwindow* window, int entered);        // Since 1.84
IMGUI_IMPL_API void     ImGui_ImplGlfw_CursorPosCallback(GLFWwindow* window, double x, double y);   // Since 1.87
IMGUI_IMPL_API void     ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
IMGUI_IMPL_API void     ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
IMGUI_IMPL_API void     ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
IMGUI_IMPL_API void     ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c);
IMGUI_IMPL_API void     ImGui_ImplGlfw_MonitorCallback(GLFWmonitor* monitor, int event);

#endif // #ifndef IMGUI_DISABLE
