/*
   pl_texture_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TEXTURE_EXT_H
#define PL_TEXTURE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h> // bool
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plTextureI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTextureAsset plTextureAsset;

// external
typedef int plFormat; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading/unloading
PL_API void pl_load_asset_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_asset_ext(plApiRegistryI*, bool reload);

PL_API void           pl_texture_register_asset_type(void);
PL_API plAssetTypeKey pl_texture_get_asset_type_key(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plTextureI
{
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key)(void);
} plTextureI;


//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTextureAsset
{
    plFormat    eFormat;
    bool        bGenerateMips;
    bool        bCompress;
    bool        bSRGB;
    const char* pcSourceFile;
} plTextureAsset;

#ifdef __cplusplus
}
#endif

#endif // PL_TEXTURE_EXT_H