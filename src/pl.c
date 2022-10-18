#include "pl_drawing.c"

#define PL_MEMORY_IMPLEMENTATION
#include "pl_memory.h"
#undef PL_MEMORY_IMPLEMENTATION

#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION

// platform specifics
#ifdef _WIN32
#include "win32_pl_os.c"
#elif defined(__APPLE__)
#include "apple_pl_os.m"
#else // linux
#include "linux_pl_os.c"
#endif

#ifdef PL_USE_STB_SPRINTF
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#endif

#ifdef PL_METAL_BACKEND
#define METAL_PL_DRAWING_IMPLEMENTATION
#include "metal_pl_drawing.h"
#undef METAL_PL_DRAWING_IMPLEMENTATION
#endif

// graphics backend specifics
#ifdef PL_VULKAN_BACKEND
#include "vulkan_pl_graphics.c"
#define VULKAN_PL_DRAWING_IMPLEMENTATION
#include "vulkan_pl_drawing.h"
#undef VULKAN_PL_DRAWING_IMPLEMENTATION
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION
