/*
   pl_image_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IMAGE_EXT_H
#define PL_IMAGE_EXT_H

// extension version (format XYYZZ)
#define PL_IMAGE_EXT_VERSION    "1.0.0"
#define PL_IMAGE_EXT_VERSION_NUM 10000

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
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plImageInfo      plImageInfo;
typedef struct _plImageWriteInfo plImageWriteInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plImageI
{
    // query image info
    bool (*get_info)           (char const* pcFilename, plImageInfo* ptInfoOut);
    bool (*get_info_from_memory)(const unsigned char* pcBuffer, int iLength, plImageInfo* ptInfoOut);

    // reading LDR (HDR will be remapped through this interface)
    unsigned char*  (*load_from_memory)      (const unsigned char* pcBuffer, int iLength, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    unsigned char*  (*load)                  (char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    unsigned short* (*load_16bit_from_memory)(const unsigned char* pcBuffer, int iLength, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    unsigned short* (*load_16bit)            (char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    void            (*set_hdr_to_ldr_gamma)  (float); // default 2.2f
    void            (*set_hdr_to_ldr_scale)  (float); // default 1.0f
    
    // reading HDR (LDR will be promoted to floating point values)
    float* (*load_hdr_from_memory)(const unsigned char* pcBuffer, int iLength, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    float* (*load_hdr)            (char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    void   (*set_ldr_to_hdr_gamma)(float); // default 2.2f
    void   (*set_ldr_to_hdr_scale)(float); // default 1.0f
    
    // call when finished with memory
    void (*free)(void* pRetValueFromLoad);

    // writing to disk (currently supports png, jpg, bmp, hdr, tga)
    bool (*write)(char const *pcFileName, const void *pData, const plImageWriteInfo*);
} plImageI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plImageInfo
{
    int iWidth;
    int iHeight;
    int iChannels;
    bool b16Bit;
    bool bHDR;
} plImageInfo;

typedef struct _plImageWriteInfo
{
    int iWidth;
    int iHeight;
    int iComponents;

    int iQuality;    // jpeg only
    int iByteStride; // png only
} plImageWriteInfo;

#endif // PL_IMAGE_EXT_H