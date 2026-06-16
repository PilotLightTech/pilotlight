/*
   pl_image_ops_ext.h
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
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IMAGE_OPS_EXT_H
#define PL_IMAGE_OPS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plImageOpsI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plImageOpData   plImageOpData;
typedef struct _plImageOpInit   plImageOpInit;
typedef struct _plImageOpRegion plImageOpRegion; // internal

// mipmap types
typedef struct _plMipMapCpuDesc plMipMapCpuDesc;
typedef struct _plMipLevel      plMipLevel;
typedef struct _plMipMapChain   plMipMapChain;

// enums
typedef int plImageOpFlags;
typedef int plImageOpColor;

// external enums/flags
typedef int plFormat;      // pl_graphics_ext.h
typedef int plTextureType; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_image_ops_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_image_ops_ext(plApiRegistryI*, bool reload);

PL_API void pl_image_ops_initialize(plImageOpInit*, plImageOpData* dataOut);
PL_API void pl_image_ops_cleanup   (plImageOpData*);

// building operations
PL_API void pl_image_ops_add       (plImageOpData*, int x, int y, uint32_t w, uint32_t h, uint8_t*);
PL_API void pl_image_ops_add_region(plImageOpData*, int x, int y, uint32_t w, uint32_t h, plImageOpColor);

// in-place place operations
PL_API void pl_image_ops_square(plImageOpData*);

// misc.
PL_API uint8_t* pl_image_ops_extract        (plImageOpData* dataIn, int x, int y, uint32_t w, uint32_t h, uint64_t* sizeOut);
PL_API void     pl_image_ops_cleanup_extract(uint8_t*);

//-----------------------------MIPMAPPING--------------------------------------

PL_API bool pl_image_ops_generate_mip_chain(const plMipMapCpuDesc*, plMipMapChain* chainOut);
PL_API void pl_image_ops_free_mip_chain(plMipMapChain* chainOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plImageOpsI
{
    void (*initialize)(plImageOpInit*, plImageOpData* dataOut);
    void (*cleanup)   (plImageOpData*);

    // building operations
    void (*add)       (plImageOpData*, int x, int y, uint32_t w, uint32_t h, uint8_t*);
    void (*add_region)(plImageOpData*, int x, int y, uint32_t w, uint32_t h, plImageOpColor);

    // in-place place operations
    void (*square)(plImageOpData*);

    // misc.
    uint8_t* (*extract)        (plImageOpData* dataIn, int x, int y, uint32_t w, uint32_t h, uint64_t* sizeOut);
    void     (*cleanup_extract)(uint8_t*);

    //-----------------------------MIPMAPPING--------------------------------------

    bool (*generate_mip_chain)(const plMipMapCpuDesc*, plMipMapChain* chainOut);
    void (*free_mip_chain)    (plMipMapChain*);
} plImageOpsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plImageOpInit
{
    uint32_t uVirtualWidth;
    uint32_t uVirtualHeight;
    uint8_t  uChannels;
    uint8_t  uStride; // bytes
} plImageOpInit;

typedef struct _plImageOpData
{
    // virtual region
    uint32_t uVirtualWidth;
    uint32_t uVirtualHeight;

    // active region
    uint32_t uActiveXOffset;
    uint32_t uActiveYOffset;
    uint32_t uActiveWidth;
    uint32_t uActiveHeight;

    // [INTERNAL]
    plImageOpRegion* _atRegions;
    uint32_t         _uRegionCount;
    uint32_t         _uRegionCapacity;
    uint8_t          _uChannels;
    uint8_t          _uStride; // bytes
    uint8_t          _uChannelStride; // bytes
} plImageOpData;

typedef struct _plMipMapCpuDesc
{
    void*                pData;
    uint32_t             uWidth;
    uint32_t             uHeight;
    uint32_t             uLayers;

    plFormat             eFormat;
    plTextureType        tTextureType;
    // plMipMapFilter       tFilter;
    // plMipMapColorSpace   tColorSpace;
    // plMipMapContentType  tContentType;
    // plMipMapFlags        tFlags;

    uint32_t             uBaseMip;
    uint32_t             uMipCount;     // 0 = full chain

    // Source layout
    size_t               szRowStride;
    size_t               szLayerStride;

} plMipMapCpuDesc;

typedef struct _plMipLevel
{
    void*    pData;
    uint32_t uWidth;
    uint32_t uHeight;
    size_t   szSize;
    size_t   szRowStride;
    size_t   szFaceStride;
} plMipLevel;

typedef struct _plMipMapChain
{
    plFormat    eFormat;
    uint32_t    uMipCount;
    uint32_t    uLayerCount;
    plMipLevel* atLevels;
} plMipMapChain;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plImageOpFlags
{
    PL_IMAGE_OP_FLAGS_NONE = 0
};

enum _plImageOpColor
{
    PL_IMAGE_OP_COLOR_TRANSPARENT = 0,
    PL_IMAGE_OP_COLOR_WHITE,
};

#ifdef __cplusplus
}
#endif

#endif // PL_IMAGE_OPS_EXT_H