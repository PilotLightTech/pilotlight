#include "pl_drawing.c"

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
#define VULKAN_PL_DRAWING_IMPLEMENTATION
#include "vulkan_pl_drawing.h"
#undef VULKAN_PL_DRAWING_IMPLEMENTATION
#endif

#ifdef PL_METAL_BACKEND
#define METAL_PL_DRAWING_IMPLEMENTATION
#include "metal_pl_drawing.h"
#undef METAL_PL_DRAWING_IMPLEMENTATION
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION