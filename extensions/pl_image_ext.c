/*
   pl_image_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_image_ext.h"
#include "stb_image.h"
#include "stb_image_write.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plImageI*
pl_load_image_api(void)
{
    static const plImageI tApi = {
        .load_from_memory  = stbi_load_from_memory,
        .loadf_from_memory = stbi_loadf_from_memory,
        .load              = stbi_load,
        .loadf             = stbi_loadf,
        .free              = stbi_image_free,
        .write_png         = stbi_write_png,
        .write_bmp         = stbi_write_bmp,
        .write_tga         = stbi_write_tga,
        .write_jpg         = stbi_write_jpg,
        .write_hdr         = stbi_write_hdr
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_image_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_IMAGE), pl_load_image_api());
    else
        ptApiRegistry->add(PL_API_IMAGE, pl_load_image_api());
}

PL_EXPORT void
pl_unload_image_ext(plApiRegistryI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(x) PL_ALLOC(x)
#define STBI_FREE(x) PL_FREE(x)
#define STBI_REALLOC(x, y) PL_REALLOC(x, y)
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC(x) PL_ALLOC(x)
#define STBIW_FREE(x) PL_FREE(x)
#define STBIW_REALLOC(x, y) PL_REALLOC(x, y)
#include "stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION