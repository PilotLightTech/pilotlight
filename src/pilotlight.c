#include "pilotlight.h"

// platform specifics
#ifdef _WIN32
#include "../backends/pl_win32.c"
#elif defined(__APPLE__)
#include "../backends/pl_macos.m"
#else // linux
#include "../backends/pl_linux.c"
#endif

#include "pl_draw.c"

// extensions
#ifdef PL_VULKAN_BACKEND
#include "../extensions/pl_gltf_extension.c"
#endif

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION

#ifdef PL_VULKAN_BACKEND
#include "pl_renderer.c"
#endif
#include "pl_ui.c"

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

#define PL_REGISTRY_IMPLEMENTATION
#include "pl_registry.h"
#undef PL_REGISTRY_IMPLEMENTATION

#define PL_EXT_IMPLEMENTATION
#include "pl_ext.h"
#undef PL_EXT_IMPLEMENTATION

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
#undef PL_STRING_IMPLEMENTATION

#define PL_CAMERA_IMPLEMENTATION
#include "pl_camera.h"
#undef PL_CAMERA_IMPLEMENTATION

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"
#undef PL_JSON_IMPLEMENTATION

#ifdef PL_USE_STB_SPRINTF
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#endif

// graphics backend specifics
#ifdef PL_METAL_BACKEND
#include "../backends/pl_metal.m"
#endif

#ifdef PL_VULKAN_BACKEND
#include "pl_graphics_vulkan.c"
#include "../backends/pl_vulkan.c"
#include "pl_prototype.c"
#endif

#ifdef PL_DX11_BACKEND
#include "../backends/pl_dx11.c"
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

