#include "pilotlight.h"

#include "pl_draw.c"

#define PL_MEMORY_IMPLEMENTATION
#include "pl_memory.h"
#undef PL_MEMORY_IMPLEMENTATION

#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION

#define PL_IO_IMPLEMENTATION
#include "pl_io.h"
#undef PL_IO_IMPLEMENTATION

// platform specifics
#ifdef _WIN32
#include "pl_os_win32.c"
#elif defined(__APPLE__)
#include "pl_os_macos.m"
#else // linux
#include "pl_os_linux.c"
#endif

#ifdef PL_USE_STB_SPRINTF
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#endif

// graphics backend specifics
#ifdef PL_METAL_BACKEND
#define PL_DRAW_METAL_IMPLEMENTATION
#include "pl_draw_metal.h"
#undef PL_DRAW_METAL_IMPLEMENTATION
#endif

#ifdef PL_VULKAN_BACKEND
#include "pl_graphics_vulkan.c"
#define PL_DRAW_VULKAN_IMPLEMENTATION
#include "pl_draw_vulkan.h"
#undef PL_DRAW_VULKAN_IMPLEMENTATION
#endif

#ifdef PL_DX11_BACKEND
#define PL_DRAW_DX11_IMPLEMENTATION
#include "pl_draw_dx11.h"
#undef PL_DRAW_DX11_IMPLEMENTATION
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION
