
#ifdef _WIN32
    #include "win32_pl_os.c"
    #include "vulkan_pl_graphics.c"
#elif defined(__APPLE__)
    #include "apple_pl_os.m"
#else // linux
    #include "linux_pl_os.c"
    #include "vulkan_pl_graphics.c"
#endif

