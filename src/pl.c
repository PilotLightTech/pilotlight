
#ifdef _WIN32
    #include "pl_io_win32.c"
    #include "pl_os_win32.c"
#elif // linux
    #include "pl_io_linux.c"
    #include "pl_os_linux.c"
#endif

#include "pl_graphics_vulkan.c"