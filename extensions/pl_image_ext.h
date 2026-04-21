/*
   pl_image_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IMAGE_EXT_H
#define PL_IMAGE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plImageI_version {1, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plImageInfo      plImageInfo;
typedef struct _plImageWriteInfo plImageWriteInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_image_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_image_ext(plApiRegistryI*, bool reload);

// query image info
PL_API bool            pl_image_get_info          (const unsigned char* buffer, int length, plImageInfo* infoOut);
PL_API bool            pl_image_get_info_from_file(const char* path, plImageInfo* infoOut);

// reading LDR (HDR will be remapped through this interface)
PL_API unsigned char*  pl_image_load                (const unsigned char* buffer, int length, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
PL_API unsigned char*  pl_image_load_from_file      (const char* path, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
PL_API unsigned short* pl_image_load_16bit          (const unsigned char* buffer, int length, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
PL_API unsigned short* pl_image_load_16bit_from_file(const char* path, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
PL_API void            pl_image_set_hdr_to_ldr_gamma  (float); // default 2.2f
PL_API void            pl_image_set_hdr_to_ldr_scale  (float); // default 1.0f

// reading HDR (LDR will be promoted to floating point values)
PL_API float*          pl_image_load_hdr            (const unsigned char* buffer, int length, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
PL_API float*          pl_image_load_hdr_from_file  (const char* path, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
PL_API void            pl_image_set_ldr_to_hdr_gamma(float); // default 2.2f
PL_API void            pl_image_set_ldr_to_hdr_scale(float); // default 1.0f

// call when finished with memory
PL_API void            pl_image_free(void* returnValueFromLoad);

// writing to disk (currently supports png, jpg, bmp, hdr, tga)
PL_API bool            pl_image_write(char const* filename, const void* data, const plImageWriteInfo*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plImageI
{
    bool            (*get_info)            (const unsigned char* buffer, int length, plImageInfo* infoOut);
    bool            (*get_info_from_file)  (const char* path, plImageInfo* infoOut);
    unsigned char*  (*load)                (const unsigned char* buffer, int length, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
    unsigned char*  (*load_from_file)      (const char* path, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
    unsigned short* (*load_16bit)          (const unsigned char* buffer, int length, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
    unsigned short* (*load_16bit_from_file)(const char* path, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
    void            (*set_hdr_to_ldr_gamma)(float);
    void            (*set_hdr_to_ldr_scale)(float);
    float*          (*load_hdr)            (const unsigned char* buffer, int length, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
    float*          (*load_hdr_from_file)  (const char* path, int* widthOut, int* heightOut, int* channelsOut, int desiredChannels);
    void            (*set_ldr_to_hdr_gamma)(float);
    void            (*set_ldr_to_hdr_scale)(float);
    void            (*free)                (void* returnValueFromLoad);
    bool            (*write)               (char const* filename, const void* data, const plImageWriteInfo*);
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

#ifdef __cplusplus
}
#endif

#endif // PL_IMAGE_EXT_H