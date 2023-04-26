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

plImageApiI* pl_load_image_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plImageApiI
{
    unsigned char* (*load)(char const* pcFilename, int* piX, int* piY, int* piChannels, int iDesiredChannels);
    void           (*free)(void* pRetValueFromLoad);
} plImageApiI;

#endif // PL_IMAGE_EXT_H