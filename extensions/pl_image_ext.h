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

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plImageI_version {1, 0, 0}

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
    bool (*get_info)(const unsigned char* buffer, int length, plImageInfo* infoOut);

    // reading LDR (HDR will be remapped through this interface)
    unsigned char*  (*load)      (const unsigned char* buffer, int length, int* x, int* y, int* channels, int desiredChannels);
    unsigned short* (*load_16bit)(const unsigned char* buffer, int length, int* x, int* y, int* channels, int desiredChannels);
    void            (*set_hdr_to_ldr_gamma)  (float); // default 2.2f
    void            (*set_hdr_to_ldr_scale)  (float); // default 1.0f
    
    // reading HDR (LDR will be promoted to floating point values)
    float* (*load_hdr)            (const unsigned char* buffer, int length, int* x, int* y, int* channels, int desiredChannels);
    void   (*set_ldr_to_hdr_gamma)(float); // default 2.2f
    void   (*set_ldr_to_hdr_scale)(float); // default 1.0f
    
    // call when finished with memory
    void (*free)(void* returnValueFromLoad);

    // writing to disk (currently supports png, jpg, bmp, hdr, tga)
    bool (*write)(char const* filename, const void* data, const plImageWriteInfo*);
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