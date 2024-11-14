/*
   pl_image_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_string.h"
#include "pl_image_ext.h"
#include "stb_image.h"
#include "stb_image_write.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool
pl__get_info(char const* pcFilename, plImageInfo* ptInfoOut)
{
    ptInfoOut->b16Bit = stbi_is_16_bit(pcFilename);
    ptInfoOut->bHDR = stbi_is_hdr(pcFilename);

    return stbi_info(pcFilename, &ptInfoOut->iWidth, &ptInfoOut->iHeight, &ptInfoOut->iChannels);
}

static bool
pl__get_info_from_memory(const unsigned char* pcBuffer, int iLength, plImageInfo* ptInfoOut)
{
    ptInfoOut->b16Bit = stbi_is_16_bit_from_memory(pcBuffer, iLength);
    ptInfoOut->bHDR = stbi_is_hdr_from_memory(pcBuffer, iLength);

    return stbi_info_from_memory(pcBuffer, iLength, &ptInfoOut->iWidth, &ptInfoOut->iHeight, &ptInfoOut->iChannels);
}

static bool
pl_write_image(char const *pcFileName, const void *pData, const plImageWriteInfo* ptInfo)
{
    const char* pcExt = pl_str_get_file_extension(pcFileName, NULL, 0);
    
    if(pcExt == NULL)
        return false;

    // png
    if(pcExt[0] == 'p' || pcExt[0] == 'P')
        return stbi_write_png(pcFileName, ptInfo->iWidth, ptInfo->iHeight, ptInfo->iComponents, pData, ptInfo->iByteStride);
    
    // jpg
    else if(pcExt[0] == 'j' || pcExt[0] == 'J')
        return stbi_write_jpg(pcFileName, ptInfo->iWidth, ptInfo->iHeight, ptInfo->iComponents, pData, ptInfo->iQuality);

    // bmp
    else if(pcExt[0] == 'b' || pcExt[0] == 'B')
        return stbi_write_bmp(pcFileName, ptInfo->iWidth, ptInfo->iHeight, ptInfo->iComponents, pData);

    // hdr
    else if(pcExt[0] == 'h' || pcExt[0] == 'H')
        return stbi_write_hdr(pcFileName, ptInfo->iWidth, ptInfo->iHeight, ptInfo->iComponents, (const float*)pData);

    // tga
    else if(pcExt[0] == 't' || pcExt[0] == 'T')
        return stbi_write_tga(pcFileName, ptInfo->iWidth, ptInfo->iHeight, ptInfo->iComponents, pData);

    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_image_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plImageI tApi = {
        .get_info               = pl__get_info,
        .get_info_from_memory   = pl__get_info_from_memory,
        .load_from_memory       = stbi_load_from_memory,
        .load_16bit_from_memory = stbi_load_16_from_memory,
        .load_hdr_from_memory   = stbi_loadf_from_memory,
        .load                   = stbi_load,
        .load_16bit             = stbi_load_16,
        .load_hdr               = stbi_loadf,
        .free                   = stbi_image_free,
        .write                  = pl_write_image,
        .set_hdr_to_ldr_gamma   = stbi_hdr_to_ldr_gamma,
        .set_hdr_to_ldr_scale   = stbi_hdr_to_ldr_scale,
        .set_ldr_to_hdr_gamma   = stbi_ldr_to_hdr_gamma,
        .set_ldr_to_hdr_scale   = stbi_ldr_to_hdr_scale
    };
    pl_set_api(ptApiRegistry, plImageI, &tApi);
}

static void
pl_unload_image_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plImageI* ptApi = pl_get_api_latest(ptApiRegistry, plImageI);
    ptApiRegistry->remove_api(ptApi);
}