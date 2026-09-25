/*
   pl_asset_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ASSET_EXT_H
#define PL_ASSET_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_ASSET_MAGIC PL_FOURCC('P', 'L', 'A', 'S')

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plAssetI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h> // uint32_t
#include <stdbool.h> // bool
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plAssetInit     plAssetInit; 
typedef struct _plAssetDesc     plAssetDesc;
typedef struct _plAssetTypeDesc plAssetTypeDesc;
typedef struct _plAssetChange   plAssetChange; // asset changes

// enums/flags
typedef int plAssetFlags;
typedef int plAssetEncoding;
typedef int plAssetChangeType; // -> enum _plAssetChangeType // Enum: asset change type (PL_ASSET_CHANGE_XXXX)

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading/unloading
PL_API void pl_load_asset_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_asset_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plAssetI
{
    // system
    void (*initialize)(plAssetInit);
    void (*finalize)  (void);
    void (*cleanup)   (void);

    // Creates an asset and copies szSize bytes from data into asset-owned storage.
    // data may be NULL to create a zero-initialized asset.
    plAssetHandle (*create) (const plAssetDesc*, const void*);
    void          (*destroy)(plAssetHandle);

    plAssetHandle (*find)           (const char* path);
    bool          (*is_valid)       (plAssetHandle);
    plAssetTypeKey(*get_type_key)   (plAssetHandle);
    const char*   (*get_path)       (plAssetHandle);
    const char*   (*get_source_path)(plAssetHandle);
    void*         (*get_data)       (plAssetHandle); // can be stored
    uint32_t      (*get_version)    (plAssetHandle);

    // changes
    void (*get_changes)  (plAssetChange**, uint32_t*);
    void (*clear_changes)(void);
    void (*mark_changed) (plAssetHandle);

    // serialization
    plAssetHandle (*load)(const char*);
    bool          (*save)(plAssetHandle, plAssetEncoding);

    // type registration
    plAssetTypeKey         (*register_type)       (plAssetTypeDesc);
    const plAssetTypeDesc* (*get_type_description)(plAssetTypeKey);
    uint32_t               (*get_type_descriptions)(const plAssetTypeDesc**);

    // tooling
    const plAssetHandle* (*get_assets)        (uint32_t*); // do not store
    const plAssetHandle* (*get_assets_by_type)(plAssetTypeKey, uint32_t*); // do not store
} plAssetI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAssetInit
{
    // [INTERNAL]
    uint32_t _uUnused;
} plAssetInit; 

typedef struct _plAssetDesc
{
    plAssetTypeKey tType;
    plAssetFlags   eFlags;
    const char*    pcPath;       // canonical VFS path of the asset
    const char*    pcSourcePath; // optional source used to produce/import the asset
} plAssetDesc;

typedef struct _plAssetTypeDesc
{
    const char*     pcName;
    size_t          szSize;
    const char*     pcFileExtension;
    plAssetEncoding eDefaultEncoding;

    // optional callbacks
    void (*cleanup)(void*);
    
    // serialization
    bool (*serialize)  (const char* path, const void*, plAssetEncoding); // required
    bool (*deserialize)(const char* path, void*); // required
} plAssetTypeDesc;

typedef struct _plAssetFileHeader // for binary assets
{
    uint32_t uMagic;      // 'PLAS'
    uint32_t uVersion;    // type-specific file format version
    uint32_t uAssetMagic;
} plAssetFileHeader;

typedef struct _plAssetChange
{
    plAssetChangeType eType;
    plAssetHandle     tAsset;
    plAssetTypeKey    tAssetType;
    uint32_t          uVersion;
} plAssetChange;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plAssetFlags
{
    PL_ASSET_FLAG_NONE = 0
};

enum _plAssetEncoding
{
    PL_ASSET_ENCODING_TEXT = 0,
    PL_ASSET_ENCODING_BINARY,
    PL_ASSET_ENCODING_AUTO = 256,
};

enum _plAssetChangeType
{
    PL_ASSET_CHANGE_ADDED,
    PL_ASSET_CHANGE_REMOVED,
    PL_ASSET_CHANGE_CHANGED,
    // PL_ASSET_CHANGE_RELOADED
};

#ifdef __cplusplus
}
#endif

#endif