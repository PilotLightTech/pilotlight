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

// stable extensions
#include "pl_vfs_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    // required APIs
    static const plVfsI* gptVfs = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool
pl__get_info_from_memory(const unsigned char* pcBuffer, int iLength, plImageInfo* ptInfoOut)
{
    ptInfoOut->b16Bit = stbi_is_16_bit_from_memory(pcBuffer, iLength);
    ptInfoOut->bHDR = stbi_is_hdr_from_memory(pcBuffer, iLength);

    return stbi_info_from_memory(pcBuffer, iLength, &ptInfoOut->iWidth, &ptInfoOut->iHeight, &ptInfoOut->iChannels);
}

bool
pl_get_info_from_file(const char* pcPath, plImageInfo* ptInfoOut)
{
    // load image file from disk
    size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
    plVfsFileHandle tFile = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFile, NULL, &szImageFileSize);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    gptVfs->read_file(tFile, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tFile);

    bool bResult = pl__get_info_from_memory(pucBuffer, (int)szImageFileSize, ptInfoOut);
    PL_FREE(pucBuffer);
    return bResult;
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

unsigned char*
pl_image_load_from_file(const char* pcPath, int* piX, int* piY, int* piChannels, int iDesiredChannels)
{

    // load image file from disk
    size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
    plVfsFileHandle tFile = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFile, NULL, &szImageFileSize);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    gptVfs->read_file(tFile, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tFile);

    unsigned char* pucImageData = stbi_load_from_memory(pucBuffer, (int)szImageFileSize, piX, piY, piChannels, iDesiredChannels);
    PL_FREE(pucBuffer);
    return pucImageData;
}

unsigned short*
pl_image_load_16bit_from_file(const char* pcPath, int* piX, int* piY, int* piChannels, int iDesiredChannels)
{
    size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
    plVfsFileHandle tFile = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFile, NULL, &szImageFileSize);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    gptVfs->read_file(tFile, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tFile);

    unsigned short* pucImageData = stbi_load_16_from_memory(pucBuffer, (int)szImageFileSize, piX, piY, piChannels, iDesiredChannels);
    PL_FREE(pucBuffer);
    return pucImageData;
}

float*
pl_image_load_hdr_from_file(const char* pcPath, int* piX, int* piY, int* piChannels, int iDesiredChannels)
{
    size_t szImageFileSize = gptVfs->get_file_size_str(pcPath);
    plVfsFileHandle tFile = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFile, NULL, &szImageFileSize);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    gptVfs->read_file(tFile, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tFile);

    float* pucImageData = stbi_loadf_from_memory(pucBuffer, (int)szImageFileSize, piX, piY, piChannels, iDesiredChannels);
    PL_FREE(pucBuffer);
    return pucImageData;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_image_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plImageI tApi = {
        .get_info               = pl__get_info_from_memory,
        .get_info_from_file     = pl_get_info_from_file,
        .load                   = stbi_load_from_memory,
        .load_16bit             = stbi_load_16_from_memory,
        .load_hdr               = stbi_loadf_from_memory,
        .free                   = stbi_image_free,
        .write                  = pl_write_image,
        .set_hdr_to_ldr_gamma   = stbi_hdr_to_ldr_gamma,
        .set_hdr_to_ldr_scale   = stbi_hdr_to_ldr_scale,
        .set_ldr_to_hdr_gamma   = stbi_ldr_to_hdr_gamma,
        .set_ldr_to_hdr_scale   = stbi_ldr_to_hdr_scale,
        .load_from_file         = pl_image_load_from_file,
        .load_16bit_from_file   = pl_image_load_16bit_from_file,
        .load_hdr_from_file     = pl_image_load_hdr_from_file
    };
    pl_set_api(ptApiRegistry, plImageI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
}

PL_EXPORT void
pl_unload_image_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plImageI* ptApi = pl_get_api_latest(ptApiRegistry, plImageI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

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

#endif