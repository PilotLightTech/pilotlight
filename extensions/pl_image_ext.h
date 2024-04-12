/*
   pl_image_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] public api
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IMAGE_EXT_H
#define PL_IMAGE_EXT_H

#define PL_IMAGE_EXT_VERSION    "1.0.0"
#define PL_IMAGE_EXT_VERSION_NUM 100000

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_IMAGE "PL_API_IMAGE"
typedef struct _plImageI plImageI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plImageI* pl_load_image_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plImageI
{
    // query for which interface to use
    bool (*is_hdr)            (const char* pcFileName);
    bool (*is_hdr_from_memory)(const unsigned char* pcBuffer, int iLength);

    // reading LDR (HDR will be remapped through this interface)
    unsigned char* (*load_from_memory) (const unsigned char* pcBuffer, int iLength, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    unsigned char* (*load)             (char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    void           (*hdr_to_ldr_gamma) (float fGamma); // default 2.2f
    void           (*hdr_to_ldr_scale) (float fScale); // default 1.0f
    
    // reading HDR (LDR will be promoted to floating point values)
    float* (*load_hdr_from_memory)(const unsigned char* pcBuffer, int iLength, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    float* (*load_hdr)            (char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    void   (*ldr_to_hdr_gamma)    (float fGamma); // default 2.2f
    void   (*ldr_to_hdr_scale)    (float fScale); // default 1.0f
    
    // call when finished with memory
    void (*free)(void* pRetValueFromLoad);

    // writing to disk
    bool (*write_png)(char const *pcFileName, int iW, int iH, int iComp, const void *pData, int iByteStride);
    bool (*write_bmp)(char const *pcFileName, int iW, int iH, int iComp, const void *pData);
    bool (*write_tga)(char const *pcFileName, int iW, int iH, int iComp, const void *pData);
    bool (*write_jpg)(char const *pcFileName, int iW, int iH, int iComp, const void *pData, int iQuality);
    bool (*write_hdr)(char const *pcFileName, int iW, int iH, int iComp, const float *pfData);
} plImageI;

#endif // PL_IMAGE_EXT_H