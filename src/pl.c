
// platform specifics
#ifdef _WIN32
#include "win32_pl_os.c"
#elif defined(__APPLE__)
#include "apple_pl_os.m"
#else // linux
#include "linux_pl_os.c"
#endif

// graphics backend specifics
#ifdef PL_VULKAN_BACKEND
#include "vulkan_pl_graphics.c"
#endif
