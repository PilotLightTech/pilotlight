/*
   pl_material_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_asset_ext.h"

// extensions
#include "pl_log_ext.h"
#include "pl_vfs_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_material_ext.h"
#include "pl_graphics_ext.h"
#include "pl_mesh_ext.h"
#include "pl_animation_ext.h"
#include "pl_skeleton_ext.h"

// libraries
#include "pl_json.h"
#include "pl_string.h"
#include "pl_memory.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plLogI*          gptLog       = NULL;
    static const plVfsI*          gptVfs       = NULL;
    static const plMemoryI*       gptMemory    = NULL;
    static const plStringInternI* gptString    = NULL;
    static const plMaterialI*     gptMaterial  = NULL;
    static const plGraphicsI*     gptGfx       = NULL;
    static const plMeshI*         gptMesh      = NULL;
    static const plAnimationI*    gptAnimation = NULL;
    static const plSkeletonI*    gptSkeleton = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    #ifndef PL_JSON_ALLOC
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_JSON_ALLOC(x) gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

// libs
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAsset
{
    plAssetType  eType;
    plAssetFlags eFlags;
    const char*  pcName;
    const char*  pcSource;

    uint32_t uDataIndex;
} plAsset;

typedef struct _plTextureAsset
{
    plFormat eFormat;
    bool bGenerateMips;
    bool bSRGB;
    bool bCompress;
} plTextureAsset;

typedef struct _plAssetContext
{
    plTempAllocator    tTempAllocator;
    plStringRepository* ptStringRepo;
    plAsset*            sbtAssets;
    plHashMap           tAssetLookup;
    uint32_t*           sbtAssetGenerations;

    // materials
    uint32_t*   sbtMaterialFreeSlots;
    plMaterial* sbtMaterials;

    // textures
    uint32_t*       sbtTexturesFreeSlots;
    plTextureAsset* sbtTextures;

    // meshes
    uint32_t* sbtMeshFreeSlots;
    plMesh*   sbtMeshes;

    // animations
    uint32_t*    sbtAnimationFreeSlots;
    plAnimation* sbtAnimations;

    // skeletons
    uint32_t*   sbtSkeletonFreeSlots;
    plSkeleton* sbtSkeletons;
} plAssetContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plAssetContext* gptAssetCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

void
pl_asset_initialize(void)
{
    gptAssetCtx->ptStringRepo = gptString->create_repository();
    pl_sb_push(gptAssetCtx->sbtAssetGenerations, 4194303);
    pl_sb_add(gptAssetCtx->sbtAssets);
}

void
pl_asset_cleanup(void)
{
    gptString->destroy_repository(gptAssetCtx->ptStringRepo);
    gptAssetCtx->ptStringRepo = NULL;

    for(uint32_t i = 0; i < pl_sb_size(gptAssetCtx->sbtAnimations); i++)
    {
        gptAnimation->destroy(&gptAssetCtx->sbtAnimations[i]);
    }

    pl_hm_free(&gptAssetCtx->tAssetLookup);
    pl_sb_free(gptAssetCtx->sbtAssetGenerations);
    pl_sb_free(gptAssetCtx->sbtAssets);
    pl_sb_free(gptAssetCtx->sbtMaterials);
    pl_sb_free(gptAssetCtx->sbtTextures);
    pl_sb_free(gptAssetCtx->sbtMeshes);
    pl_sb_free(gptAssetCtx->sbtAnimations);
    pl_sb_free(gptAssetCtx->sbtSkeletons);
    pl_sb_free(gptAssetCtx->sbtMaterialFreeSlots);
    pl_sb_free(gptAssetCtx->sbtTexturesFreeSlots);
    pl_sb_free(gptAssetCtx->sbtMeshFreeSlots);
    pl_sb_free(gptAssetCtx->sbtAnimationFreeSlots);
    pl_sb_free(gptAssetCtx->sbtSkeletonFreeSlots);
    pl_temp_allocator_free(&gptAssetCtx->tTempAllocator);
}

plAssetHandle
pl_asset_create(const plAssetDesc* ptDesc)
{
    const uint64_t ulHash = pl_hm_hash_str(ptDesc->pcName, 0);
    uint64_t ulExistingSlot = 0;
    if(pl_hm_has_key_ex(&gptAssetCtx->tAssetLookup, ulHash, &ulExistingSlot))
    {
        plAssetHandle tAsset = {0};
        tAsset.uIndex      = (uint32_t)ulExistingSlot;
        tAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[ulExistingSlot];
        return tAsset;
    }

    uint64_t uIndex = pl_hm_get_free_index(&gptAssetCtx->tAssetLookup);
    if(uIndex == PL_DS_HASH_INVALID)
    {
        uIndex = pl_sb_size(gptAssetCtx->sbtAssetGenerations);
        pl_sb_push(gptAssetCtx->sbtAssetGenerations, 0);
        pl_sb_add(gptAssetCtx->sbtAssets);
    }
    pl_hm_insert_str(&gptAssetCtx->tAssetLookup, ptDesc->pcName, uIndex);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[uIndex];
    ptAsset->eType = ptDesc->eType;
    ptAsset->eFlags = ptDesc->eFlags;
    ptAsset->pcName = gptString->intern(gptAssetCtx->ptStringRepo, ptDesc->pcName);
    ptAsset->uDataIndex = UINT32_MAX;

    plAssetHandle tNewAsset = {0};
    tNewAsset.uIndex      = (uint32_t)uIndex;
    tNewAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[uIndex];
    return tNewAsset;
}

plAssetHandle
pl_asset_create_material_asset(const plMaterialAssetDesc* ptDesc)
{
    plAssetHandle tNewAsset = pl_asset_create(&ptDesc->tDesc);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tNewAsset.uIndex];

    if(ptAsset->uDataIndex == UINT32_MAX)
    {
        if(pl_sb_size(gptAssetCtx->sbtMaterialFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtMaterialFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtMaterials);
            pl_sb_add(gptAssetCtx->sbtMaterials);
        }
        gptAssetCtx->sbtMaterials[ptAsset->uDataIndex] = *ptDesc->ptMaterial;
        gptMaterial->serialize(ptDesc->tDesc.pcName, &gptAssetCtx->sbtMaterials[ptAsset->uDataIndex]);
    }
    return tNewAsset;
}

plAssetHandle
pl_asset_create_mesh_asset(const plMeshAssetDesc* ptDesc)
{
    plAssetHandle tNewAsset = pl_asset_create(&ptDesc->tDesc);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tNewAsset.uIndex];

    if(ptAsset->uDataIndex == UINT32_MAX)
    {
        if(pl_sb_size(gptAssetCtx->sbtMeshFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtMeshFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtMeshes);
            pl_sb_add(gptAssetCtx->sbtMeshes);
        }
        gptAssetCtx->sbtMeshes[ptAsset->uDataIndex] = *ptDesc->ptMesh;
        gptMesh->serialize(ptDesc->tDesc.pcName, &gptAssetCtx->sbtMeshes[ptAsset->uDataIndex]);
    }
    return tNewAsset;
}

plAssetHandle
pl_asset_create_animation_asset(const plAnimationAssetDesc* ptDesc)
{
    plAssetHandle tNewAsset = pl_asset_create(&ptDesc->tDesc);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tNewAsset.uIndex];

    if(ptAsset->uDataIndex == UINT32_MAX)
    {
        if(pl_sb_size(gptAssetCtx->sbtAnimationFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtAnimationFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtAnimations);
            pl_sb_add(gptAssetCtx->sbtAnimations);
        }
        gptAssetCtx->sbtAnimations[ptAsset->uDataIndex] = *ptDesc->ptAnimation;
        gptAnimation->serialize(ptDesc->tDesc.pcName, &gptAssetCtx->sbtAnimations[ptAsset->uDataIndex]);
    }
    return tNewAsset;
}

plAssetHandle
pl_asset_create_skeleton_asset(const plSkeletonAssetDesc* ptDesc)
{
    plAssetHandle tNewAsset = pl_asset_create(&ptDesc->tDesc);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tNewAsset.uIndex];

    if(ptAsset->uDataIndex == UINT32_MAX)
    {
        if(pl_sb_size(gptAssetCtx->sbtSkeletonFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtSkeletonFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtSkeletons);
            pl_sb_add(gptAssetCtx->sbtSkeletons);
        }
        gptAssetCtx->sbtSkeletons[ptAsset->uDataIndex] = *ptDesc->ptSkeleton;
        gptSkeleton->serialize(ptDesc->tDesc.pcName, &gptAssetCtx->sbtSkeletons[ptAsset->uDataIndex]);
    }
    return tNewAsset;
}

plAssetHandle
pl_asset_create_texture_asset(const plTextureAssetDesc* ptDesc)
{
    plJsonObject* ptRoot = pl_json_new_root_object("root");

    pl_json_add_string_member(ptRoot, "format", "pltexture");
    plVersion tResourceVersion = plAssetI_version;
    uint32_t auVersion[] = {tResourceVersion.uMajor, tResourceVersion.uMinor};
    pl_json_add_uint_array(ptRoot, "version", auVersion, 2);

    pl_json_add_string_member(ptRoot, "source", ptDesc->tDesc.pcSourceFile);
    pl_json_add_string_member(ptRoot, "type", "2d");
    pl_json_add_string_member(ptRoot, "color_space", ptDesc->bSRGB ? "srgb" : "linear");
    pl_json_add_string_member(ptRoot, "format", gptGfx->get_format_as_string(ptDesc->eFormat));
    pl_json_add_bool_member(ptRoot, "generate_mips", ptDesc->bGenerateMips);
    pl_json_add_bool_member(ptRoot, "compress", ptDesc->bCompress);

    uint32_t uBufferSize = 0;
    pl_write_json(ptRoot, NULL, &uBufferSize);
    char* pcBuffer = PL_ALLOC(uBufferSize);
    memset(pcBuffer, 0, uBufferSize);
    pl_write_json(ptRoot, pcBuffer, &uBufferSize);

    gptVfs->register_file(ptDesc->tDesc.pcName, false);
    plVfsFileHandle tFileHandle = gptVfs->open_file(ptDesc->tDesc.pcName, PL_VFS_FILE_MODE_WRITE);
    gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
    gptVfs->close_file(tFileHandle);
    PL_FREE(pcBuffer);

    return pl_asset_load(ptDesc->tDesc.pcName);
}

plAssetHandle
pl_asset_load(const char* pcFile)
{
    const uint64_t ulHash = pl_hm_hash_str(pcFile, 0);
    uint64_t ulExistingSlot = 0;
    if(pl_hm_has_key_ex(&gptAssetCtx->tAssetLookup, ulHash, &ulExistingSlot))
    {
        plAssetHandle tAsset = {0};
        tAsset.uIndex      = (uint32_t)ulExistingSlot;
        tAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[ulExistingSlot];
        return tAsset;
    }

    // find new asset home slot
    uint64_t uIndex = pl_hm_get_free_index(&gptAssetCtx->tAssetLookup);
    if(uIndex == PL_DS_HASH_INVALID)
    {
        uIndex = pl_sb_size(gptAssetCtx->sbtAssetGenerations);
        pl_sb_push(gptAssetCtx->sbtAssetGenerations, 0);
        pl_sb_add(gptAssetCtx->sbtAssets);
    }
    pl_hm_insert_str(&gptAssetCtx->tAssetLookup, pcFile, uIndex);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[uIndex];
    ptAsset->eType = PL_ASSET_TYPE_UNKNOWN;
    ptAsset->eFlags = PL_ASSET_FLAG_NONE;
    ptAsset->pcName = gptString->intern(gptAssetCtx->ptStringRepo, pcFile);

    char acFileExtension[16] = {0};
    pl_str_get_file_extension(pcFile, acFileExtension, 16);

    for(uint32_t i = 0; i < 16; i++)
        acFileExtension[i] = pl_str_to_upper(acFileExtension[i]);

    if(pl_str_equal(acFileExtension, "PLMATERIAL"))
    {
        if(pl_sb_size(gptAssetCtx->sbtMaterialFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtMaterialFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtMaterials);
            pl_sb_add(gptAssetCtx->sbtMaterials);
        }

        ptAsset->eType = PL_ASSET_TYPE_MATERIAL;
        if(!gptMaterial->load(pcFile, &gptAssetCtx->sbtMaterials[ptAsset->uDataIndex]))
        {
            PL_ASSERT(false && "asset file doesn't exist");
        }
    }
    else if(pl_str_equal(acFileExtension, "PLANIM"))
    {
        if(pl_sb_size(gptAssetCtx->sbtAnimationFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtAnimationFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtAnimations);
            pl_sb_add(gptAssetCtx->sbtAnimations);
        }

        ptAsset->eType = PL_ASSET_TYPE_ANIMATION;
        gptAnimation->deserialize(pcFile, &gptAssetCtx->sbtAnimations[ptAsset->uDataIndex]);
    }
    else if(pl_str_equal(acFileExtension, "PLMESH"))
    {
        if(pl_sb_size(gptAssetCtx->sbtMeshFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtMeshFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtMeshes);
            pl_sb_add(gptAssetCtx->sbtMeshes);
        }

        ptAsset->eType = PL_ASSET_TYPE_MESH;
        gptMesh->deserialize(pcFile, &gptAssetCtx->sbtMeshes[ptAsset->uDataIndex]);
    }
    else if(pl_str_equal(acFileExtension, "PLSKELETON"))
    {
        if(pl_sb_size(gptAssetCtx->sbtSkeletonFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtSkeletonFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtSkeletons);
            pl_sb_add(gptAssetCtx->sbtSkeletons);
        }

        ptAsset->eType = PL_ASSET_TYPE_SKELETON;
        gptSkeleton->deserialize(pcFile, &gptAssetCtx->sbtSkeletons[ptAsset->uDataIndex]);
    }
    else if(pl_str_equal(acFileExtension, "PLTEXTURE"))
    {
        if(pl_sb_size(gptAssetCtx->sbtTexturesFreeSlots) > 0)
        {
            ptAsset->uDataIndex = pl_sb_pop(gptAssetCtx->sbtTexturesFreeSlots);
        }
        else
        {
            ptAsset->uDataIndex = pl_sb_size(gptAssetCtx->sbtTextures);
            pl_sb_add(gptAssetCtx->sbtTextures);
        }

        ptAsset->eType = PL_ASSET_TYPE_TEXTURE;

        if(!gptVfs->does_file_exist(pcFile))
        {
            PL_ASSERT(false && "asset file doesn't exist");
        }

        plTextureAsset* ptTextureAsset = &gptAssetCtx->sbtTextures[ptAsset->uDataIndex];

        size_t szJsonFileSize = gptVfs->get_file_size_str(pcFile);
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);

        plVfsFileHandle tFileHandle = gptVfs->open_file(pcFile, PL_VFS_FILE_MODE_READ);
        gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
        gptVfs->close_file(tFileHandle);

        char acTempBuffer[256] = {0};

        plJsonObject* ptRoot = NULL;
        pl_load_json((const char*)puFileBuffer, &ptRoot);

        plVersion tResourceVersion = plAssetI_version;
        uint32_t auVersion[2] = {0};
        pl_json_uint_array_member(ptRoot, "version", auVersion, NULL);

        pl_json_string_member(ptRoot, "format", acTempBuffer, 256);

        if     (pl_str_equal(acTempBuffer, "PL_FORMAT_R32G32B32A32_FLOAT")) ptTextureAsset->eFormat = PL_FORMAT_R32G32B32A32_FLOAT;
        else if(pl_str_equal(acTempBuffer, "PL_FORMAT_R16G16B16A16_UNORM")) ptTextureAsset->eFormat = PL_FORMAT_R16G16B16A16_UNORM;
        else if(pl_str_equal(acTempBuffer, "PL_FORMAT_BC3_UNORM"))          ptTextureAsset->eFormat = PL_FORMAT_BC3_UNORM;
        else if(pl_str_equal(acTempBuffer, "PL_FORMAT_R8G8B8A8_UNORM"))     ptTextureAsset->eFormat = PL_FORMAT_R8G8B8A8_UNORM;

        ptTextureAsset->bGenerateMips = pl_json_bool_member(ptRoot, "generate_mips", false);
        ptTextureAsset->bCompress = pl_json_bool_member(ptRoot, "compress", false);

        pl_json_string_member(ptRoot, "color_space", acTempBuffer, 256);
        if     (acTempBuffer[0] == 's') ptTextureAsset->bSRGB = true;
        else if(acTempBuffer[0] == 'l') ptTextureAsset->bSRGB = false;

        pl_json_string_member(ptRoot, "source", acTempBuffer, 256);
        ptAsset->pcSource = gptString->intern(gptAssetCtx->ptStringRepo, acTempBuffer);

        PL_FREE(puFileBuffer);
    }
    
    plAssetHandle tNewAsset = {0};
    tNewAsset.uIndex      = (uint32_t)uIndex;
    tNewAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[uIndex];
    return tNewAsset;
}

bool
pl_asset_save(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return false;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];

    switch(ptAsset->eType)
    {
        case PL_ASSET_TYPE_MATERIAL:
            gptMaterial->serialize(ptAsset->pcName, &gptAssetCtx->sbtMaterials[ptAsset->uDataIndex]);
            break;

        case PL_ASSET_TYPE_ANIMATION:
            gptAnimation->serialize(ptAsset->pcName, &gptAssetCtx->sbtAnimations[ptAsset->uDataIndex]);
            break;

        case PL_ASSET_TYPE_MESH:
            gptMesh->serialize(ptAsset->pcName, &gptAssetCtx->sbtMeshes[ptAsset->uDataIndex]);
            break;

        case PL_ASSET_TYPE_SKELETON:
            gptSkeleton->serialize(ptAsset->pcName, &gptAssetCtx->sbtSkeletons[ptAsset->uDataIndex]);
            break;

        default:
            PL_ASSERT(false && "UNKNOWN ASSET TYPE");
            return false;
    }
    return true;
}

plAssetHandle
pl_asset_find(const char* pcName)
{
    if(pcName)
    {
        uint64_t uIndex = 0;
        if(pl_hm_has_key_str_ex(&gptAssetCtx->tAssetLookup, pcName, &uIndex))
        {
            return (plAssetHandle){
                .uIndex = (uint32_t)uIndex,
                .uGeneration = gptAssetCtx->sbtAssetGenerations[uIndex]
            };
        }
    }
    return (plAssetHandle){0};
}

bool
pl_asset_is_valid(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return false;
    return true;
}

const char*
pl_asset_get_name(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return ptAsset->pcName;
}

const char*
pl_asset_get_source_file(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return ptAsset->pcSource;
}

plMaterial*
pl_asset_get_material(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return &gptAssetCtx->sbtMaterials[ptAsset->uDataIndex];
}

plMesh*
pl_asset_get_mesh(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return &gptAssetCtx->sbtMeshes[ptAsset->uDataIndex];
}

plAnimation*
pl_asset_get_animation(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return &gptAssetCtx->sbtAnimations[ptAsset->uDataIndex];
}

plSkeleton*
pl_asset_get_skeleton(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return &gptAssetCtx->sbtSkeletons[ptAsset->uDataIndex];
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_asset_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAssetI tApi = {
        .initialize       = pl_asset_initialize,
        .cleanup          = pl_asset_cleanup,
        .load             = pl_asset_load,
        .find             = pl_asset_find,
        .create           = pl_asset_create,
        .create_material_asset = pl_asset_create_material_asset,
        .create_texture_asset = pl_asset_create_texture_asset,
        .create_mesh_asset = pl_asset_create_mesh_asset,
        .create_animation_asset = pl_asset_create_animation_asset,
        .create_skeleton_asset = pl_asset_create_skeleton_asset,
        .get_material     = pl_asset_get_material,
        .get_mesh     = pl_asset_get_mesh,
        .get_animation     = pl_asset_get_animation,
        .save             = pl_asset_save,
        .get_name         = pl_asset_get_name,
        .get_source_file = pl_asset_get_source_file,
        .is_valid = pl_asset_is_valid,
        .get_skeleton = pl_asset_get_skeleton,
    };
    pl_set_api(ptApiRegistry, plAssetI, &tApi);

    gptLog      = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs      = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptString   = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptMaterial = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptGfx      = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptMesh     = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptAnimation = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptSkeleton = pl_get_api_latest(ptApiRegistry, plSkeletonI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptAssetCtx = ptDataRegistry->get_data("plAssetContext");
    }
    else // first load
    {
        static plAssetContext tCtx = {0};
        gptAssetCtx = &tCtx;
        ptDataRegistry->set_data("plAssetContext", gptAssetCtx);
    }
}

void
pl_unload_asset_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plAssetI* ptApi = pl_get_api_latest(ptApiRegistry, plAssetI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #define PL_JSON_IMPLEMENTATION
    #include "pl_json.h"
    #undef PL_JSON_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif