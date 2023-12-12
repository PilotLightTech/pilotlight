/*
   pl_image_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IMAGE_EXT_H
#define PL_IMAGE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_IMAGE "PL_API_IMAGE"
typedef struct _plImageApiI plImageApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plImageApiI* pl_load_image_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plImageApiI
{
    // read
    unsigned char* (*load_from_memory)(const unsigned char* pcBuffer, int iLength, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    unsigned char* (*load)            (char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    void           (*free)            (void* pRetValueFromLoad);

    // write
     int (*write_png)(char const *pcFileName, int iW, int iH, int iComp, const void *pData, int iByteStride);
     int (*write_bmp)(char const *pcFileName, int iW, int iH, int iComp, const void *pData);
     int (*write_tga)(char const *pcFileName, int iW, int iH, int iComp, const void *pData);
     int (*write_jpg)(char const *pcFileName, int iW, int iH, int iComp, const void *pData, int iQuality);
     int (*write_hdr)(char const *pcFileName, int iW, int iH, int iComp, const float *pfData);
} plImageApiI;

#endif // PL_IMAGE_EXT_H