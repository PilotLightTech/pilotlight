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

// enums/flags
typedef int plAssetFlags;
typedef int plAssetEncoding;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading/unloading
PL_API void pl_load_asset_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_asset_ext(plApiRegistryI*, bool reload);

// system
PL_API void pl_asset_initialize(plAssetInit);
PL_API void pl_asset_finalize  (void);
PL_API void pl_asset_cleanup   (void);

// Creates an asset and copies szSize bytes from data into asset-owned storage.
// data may be NULL to create a zero-initialized asset.
PL_API plAssetHandle pl_asset_create (const plAssetDesc*, const void*);
PL_API void          pl_asset_destroy(plAssetHandle);

// lookup
PL_API plAssetHandle  pl_asset_find           (const char* path);
PL_API bool           pl_asset_is_valid       (plAssetHandle);
PL_API plAssetTypeKey pl_asset_get_type_key   (plAssetHandle);
PL_API const char*    pl_asset_get_path       (plAssetHandle);
PL_API const char*    pl_asset_get_source_path(plAssetHandle);

// resource association
PL_API void* pl_asset_get_data(plAssetHandle); // can be stored

// serialization
PL_API plAssetHandle pl_asset_load(const char* file);
PL_API bool          pl_asset_save(plAssetHandle, plAssetEncoding);

// type registration
PL_API plAssetTypeKey         pl_asset_register_type(plAssetTypeDesc);
PL_API const plAssetTypeDesc* pl_asset_get_type_description(plAssetTypeKey);
PL_API uint32_t               pl_asset_get_type_descriptions(const plAssetTypeDesc**);

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

    // serialization
    plAssetHandle (*load)(const char*);
    bool          (*save)(plAssetHandle, plAssetEncoding);

    // type registration
    plAssetTypeKey         (*register_type)       (plAssetTypeDesc);
    const plAssetTypeDesc* (*get_type_description)(plAssetTypeKey);
    uint32_t               (*get_type_descriptions)(const plAssetTypeDesc**);
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

#ifdef __cplusplus
}
#endif

#endif