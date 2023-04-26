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

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plImageApiI*
pl_load_image_api(void)
{
    static plImageApiI tApi = {
        .load = stbi_load,
        .free = stbi_image_free
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_image_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_IMAGE), pl_load_image_api());
    else
        ptApiRegistry->add(PL_API_IMAGE, pl_load_image_api());
}

PL_EXPORT void
pl_unload_image_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION