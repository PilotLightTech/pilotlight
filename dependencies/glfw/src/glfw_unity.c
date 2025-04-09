#include "context.c"
#include "init.c"
#include "input.c"
#include "monitor.c"
#include "window.c"
#include "platform.c"

#include "osmesa_context.c"
#include "egl_context.c"
#include "vulkan.c"

#ifdef _GLFW_WIN32
    #include "wgl_context.c"
    #include "win32_time.c"
    #include "win32_thread.c"
    #include "win32_init.c"
    #include "win32_joystick.c"
    #include "win32_monitor.c"
    #include "win32_window.c"
    #include "win32_module.c"
#endif

#ifdef _GLFW_X11
    #include "glx_context.c"
    #include "posix_module.c"
    #include "posix_thread.c"
    #include "posix_time.c"
    #include "x11_init.c"
    #include "x11_monitor.c"
    #include "x11_window.c"
    #include "xkb_unicode.c"
    #include "linux_joystick.c"
#endif

#ifdef _GLFW_COCOA
#include "posix_module.c"
#include "posix_thread.c"
#include "cocoa_time.c"
#include "cocoa_init.m"
#include "cocoa_joystick.m"
#include "cocoa_monitor.m"
#include "cocoa_window.m"
#include "nsgl_context.m"
#endif

#include "null_init.c"
#include "null_monitor.c"
#include "null_joystick.c"