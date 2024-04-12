/*
   pl_image_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api implementation
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
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool
pl__is_hdr(const char* pcFileName)
{
    return stbi_is_hdr(pcFileName);
}

static bool
pl__is_hdr_from_memory(const unsigned char* pcBuffer, int iLength)
{
    return stbi_is_hdr_from_memory(pcBuffer, iLength);
}

static bool
pl__write_png(char const *pcFileName, int iW, int iH, int iComp, const void *pData, int iByteStride)
{
    return stbi_write_png(pcFileName, iW, iH, iComp, pData, iByteStride);
}

static bool
pl__write_bmp(char const *pcFileName, int iW, int iH, int iComp, const void *pData)
{
    return stbi_write_bmp(pcFileName, iW, iH, iComp, pData);
}

static bool
pl__write_tga(char const *pcFileName, int iW, int iH, int iComp, const void *pData)
{
    return stbi_write_tga(pcFileName, iW, iH, iComp, pData);
}

static bool
pl__write_jpg(char const *pcFileName, int iW, int iH, int iComp, const void *pData, int iQuality)
{
    return stbi_write_jpg(pcFileName, iW, iH, iComp, pData, iQuality);
}

static bool
pl__write_hdr(char const *pcFileName, int iW, int iH, int iComp, const float *pfData)
{
    return stbi_write_hdr(pcFileName, iW, iH, iComp, pfData);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plImageI*
pl_load_image_api(void)
{
    static const plImageI tApi = {
        .load_from_memory     = stbi_load_from_memory,
        .load_hdr_from_memory = stbi_loadf_from_memory,
        .load                 = stbi_load,
        .load_hdr             = stbi_loadf,
        .free                 = stbi_image_free,
        .write_png            = pl__write_png,
        .write_bmp            = pl__write_bmp,
        .write_tga            = pl__write_tga,
        .write_jpg            = pl__write_jpg,
        .write_hdr            = pl__write_hdr,
        .is_hdr               = pl__is_hdr,
        .is_hdr_from_memory   = pl__is_hdr_from_memory,
        .hdr_to_ldr_gamma     = stbi_hdr_to_ldr_gamma,
        .hdr_to_ldr_scale     = stbi_hdr_to_ldr_scale,
        .ldr_to_hdr_gamma     = stbi_ldr_to_hdr_gamma,
        .ldr_to_hdr_scale     = stbi_ldr_to_hdr_scale
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