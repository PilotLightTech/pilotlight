/*
   pl_asset_ext.h
*/

#ifndef PL_ASSET_EXT_H
#define PL_ASSET_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pl.inc"
#include <stdint.h> // uint32_t
#include <stdbool.h> // bool
#include "pl_asset_ext.inl"

#define plAssetI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plAssetDesc        plAssetDesc;
typedef struct _plMaterialAssetDesc plMaterialAssetDesc;
typedef struct _plTextureAssetDesc plTextureAssetDesc;
typedef struct _plMeshAssetDesc plMeshAssetDesc;
typedef struct _plAnimationAssetDesc plAnimationAssetDesc;
typedef struct _plSkeletonAssetDesc plSkeletonAssetDesc;

// enums/flags
typedef int plAssetType;
typedef int plAssetFlags;

// external
typedef struct _plMaterial plMaterial; // pl_material_ext.h
typedef struct _plMesh plMesh; // pl_mesh_ext.h
typedef struct _plAnimation plAnimation; // pl_animation_ext.h
typedef struct _plSkeleton plSkeleton; // pl_skeleton_ext.h
typedef int plFormat; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// public api
//-----------------------------------------------------------------------------

PL_API void pl_load_asset_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_asset_ext(plApiRegistryI*, bool reload);

PL_API void pl_asset_initialize(void);
PL_API void pl_asset_cleanup   (void);

// asset creation/loading
PL_API plAssetHandle pl_asset_create(const plAssetDesc*);
PL_API plAssetHandle pl_asset_create_material_asset(const plMaterialAssetDesc*);
PL_API plAssetHandle pl_asset_create_texture_asset(const plTextureAssetDesc*);
PL_API plAssetHandle pl_asset_create_mesh_asset(const plMeshAssetDesc*);
PL_API plAssetHandle pl_asset_create_animation_asset(const plAnimationAssetDesc*);
PL_API plAssetHandle pl_asset_create_skeleton_asset(const plSkeletonAssetDesc*);
PL_API plAssetHandle pl_asset_load  (const char* pcFile);
// PL_API void          pl_asset_unload(plAssetHandle);

// lookup
PL_API plAssetHandle pl_asset_find    (const char* name);
PL_API bool          pl_asset_is_valid(plAssetHandle);
// PL_API plAssetType   pl_asset_get_type(plAssetHandle);

// metadata
PL_API const char* pl_asset_get_name       (plAssetHandle);
PL_API const char* pl_asset_get_source_file(plAssetHandle);

// resource association
PL_API plMaterial*  pl_asset_get_material(plAssetHandle);
PL_API plMesh*      pl_asset_get_mesh(plAssetHandle);
PL_API plAnimation* pl_asset_get_animation(plAssetHandle);

// serialization
PL_API bool pl_asset_save(plAssetHandle);

//-----------------------------------------------------------------------------
// api struct
//-----------------------------------------------------------------------------

typedef struct _plAssetI
{
    void          (*initialize)(void);
    void          (*cleanup)   (void);

    plAssetHandle (*create)     (const plAssetDesc*);
    plAssetHandle (*create_material_asset)(const plMaterialAssetDesc*);
    plAssetHandle (*create_texture_asset)(const plTextureAssetDesc*);
    plAssetHandle (*create_mesh_asset)(const plMeshAssetDesc*);
    plAssetHandle (*create_animation_asset)(const plAnimationAssetDesc*);
    plAssetHandle (*create_skeleton_asset)(const plSkeletonAssetDesc*);
    plAssetHandle (*load)       (const char*);
    // void          (*unload)     (plAssetHandle);

    plAssetHandle (*find)       (const char* name);
    bool          (*is_valid)   (plAssetHandle);
    // plAssetType   (*get_type)   (plAssetHandle);

    const char*   (*get_name)       (plAssetHandle);
    const char*   (*get_source_file)(plAssetHandle);

    plMaterial* (*get_material)(plAssetHandle);
    plMesh* (*get_mesh)(plAssetHandle);
    plAnimation* (*get_animation)(plAssetHandle);
    plSkeleton* (*get_skeleton)(plAssetHandle);

    bool (*save)(plAssetHandle);
} plAssetI;

//-----------------------------------------------------------------------------
// enums
//-----------------------------------------------------------------------------

enum _plAssetType
{
    PL_ASSET_TYPE_UNKNOWN,
    PL_ASSET_TYPE_TEXTURE,
    PL_ASSET_TYPE_MATERIAL,
    PL_ASSET_TYPE_MESH,
    PL_ASSET_TYPE_ANIMATION,
    PL_ASSET_TYPE_SKELETON,
    // PL_ASSET_TYPE_SCENE
};

enum _plAssetFlags
{
    PL_ASSET_FLAG_NONE = 0
};

//-----------------------------------------------------------------------------
// structs
//-----------------------------------------------------------------------------

typedef struct _plAssetDesc
{
    plAssetType  eType;
    plAssetFlags eFlags;
    const char*  pcName;
    const char*  pcSourceFile;
} plAssetDesc;

typedef struct _plTextureAssetDesc
{
    plAssetDesc tDesc;

    plFormat eFormat;
    // uint32_t uMaxResolution;
    bool     bGenerateMips;
    bool     bCompress;
    bool     bSRGB;
} plTextureAssetDesc;

typedef struct _plMaterialAssetDesc
{
    plAssetDesc tDesc;
    const plMaterial* ptMaterial;
} plMaterialAssetDesc;

typedef struct _plMeshAssetDesc
{
    plAssetDesc tDesc;
    const plMesh* ptMesh;
} plMeshAssetDesc;

typedef struct _plAnimationAssetDesc
{
    plAssetDesc tDesc;
    const plAnimation* ptAnimation;
} plAnimationAssetDesc;

typedef struct _plSkeletonAssetDesc
{
    plAssetDesc tDesc;
    const plSkeleton* ptSkeleton;
} plSkeletonAssetDesc;

#ifdef __cplusplus
}
#endif

#endif