/*
   pl_stl_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal API
// [SECTION] implementation
// [SECTION] internal API implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include "pl.h"
#include "pl_stl_ext.h"
#include "pl_stl.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_mesh_ext.h"
#include "pl_renderer_ext.h"
#include "pl_vfs_ext.h"
#include "pl_material_ext.h"
#include "pl_graphics_ext.h"
#include "pl_asset_ext.h"
#include "pl_transform_ext.h"

// shaders
#include "pl_shader_interop_renderer.h" // PL_MESH_FORMAT_FLAG_XXXX

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    static const plEcsI*         gptECS         = NULL;
    static const plRendererI*    gptRenderer    = NULL;
    static const plRendererEcsI* gptRendererEcs = NULL;
    static const plMeshI*        gptMesh        = NULL;
    static const plVfsI*         gptVfs         = NULL;
    static const plMaterialI*    gptMaterial    = NULL;
    static const plAssetI*       gptAsset       = NULL;
    static const plTransformI*   gptTransform   = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

plEntity
pl_stl_import(plComponentLibrary* ptLibrary, const char* pcPath, plVec4 tColor, const plMat4* ptTransform)
{
    // read in STL file
    size_t szFileSize = gptVfs->get_file_size_str(pcPath);
    uint8_t* pcBuffer = PL_ALLOC(szFileSize);
    memset(pcBuffer, 0, szFileSize);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, pcBuffer, &szFileSize);
    gptVfs->close_file(tFileHandle);

    // create ECS object component
    plEntity tEntity = gptRendererEcs->create_object(ptLibrary, pcPath, NULL);

    // retrieve actual components
    plObjectComponent* ptObjectComp = gptECS->get_component(ptLibrary, gptRendererEcs->get_ecs_type_key_object(), tEntity);
    plTransformComponent* ptTransformComp = gptECS->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tEntity);
    ptObjectComp->uSubmeshCount = 1;
    
    // set transform if present
    if(ptTransform)
    {
        ptTransformComp->tWorld = *ptTransform;
        pl_decompose_matrix(&ptTransformComp->tWorld, &ptTransformComp->tScale, &ptTransformComp->tRotation, &ptTransformComp->tTranslation);
    }

    // create simple material
    plMaterial tMaterial = {0};
    gptMaterial->init(&tMaterial);

    char acFileNameOnly[128] = {0};
    pl_str_get_file_name_only(pcPath, acFileNameOnly, 128);
    
    char acBuffer[256] = {0};
    pl_sprintf(acBuffer, "/assets/materials/%s.plmaterial", acFileNameOnly);

    tMaterial.tBaseColor = tColor;
    plMaterialAssetDesc tAssetDesc = {
        .ptMaterial = &tMaterial,
        .tDesc = {
            .eType = PL_ASSET_TYPE_MATERIAL,
            .pcName = acBuffer
        }
    };
    plMesh tMesh = {0};

    // load STL model
    plStlInfo tInfo = {0};
    pl_load_stl((const char*)pcBuffer, szFileSize, NULL, NULL, NULL, &tInfo);

    plSubmeshAllocationDesc tAllocDesc = {
        .uVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .szVertexCount = tInfo.szPositionStreamSize / 3,
        .szIndexCount = (uint32_t)tInfo.szIndexBufferSize
    };
    gptMesh->allocate(&tMesh, &tAllocDesc, 1);
    tMesh.atSubmeshes[0].tMaterial = gptAsset->create_material_asset(&tAssetDesc);

    pl_load_stl((const char*)pcBuffer, szFileSize, (float*)tMesh.atSubmeshes->ptVertexPositions, (float*)tMesh.atSubmeshes->ptVertexNormals, (uint32_t*)tMesh.atSubmeshes->puIndices, &tInfo);
    PL_FREE(pcBuffer);

    // calculate AABB
    tMesh.tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    tMesh.tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t i = 0; i < tMesh.atSubmeshes->szVertexCount; i++)
    {
        if(tMesh.atSubmeshes->ptVertexPositions[i].x > tMesh.tAABB.tMax.x) tMesh.tAABB.tMax.x = tMesh.atSubmeshes->ptVertexPositions[i].x;
        if(tMesh.atSubmeshes->ptVertexPositions[i].y > tMesh.tAABB.tMax.y) tMesh.tAABB.tMax.y = tMesh.atSubmeshes->ptVertexPositions[i].y;
        if(tMesh.atSubmeshes->ptVertexPositions[i].z > tMesh.tAABB.tMax.z) tMesh.tAABB.tMax.z = tMesh.atSubmeshes->ptVertexPositions[i].z;
        if(tMesh.atSubmeshes->ptVertexPositions[i].x < tMesh.tAABB.tMin.x) tMesh.tAABB.tMin.x = tMesh.atSubmeshes->ptVertexPositions[i].x;
        if(tMesh.atSubmeshes->ptVertexPositions[i].y < tMesh.tAABB.tMin.y) tMesh.tAABB.tMin.y = tMesh.atSubmeshes->ptVertexPositions[i].y;
        if(tMesh.atSubmeshes->ptVertexPositions[i].z < tMesh.tAABB.tMin.z) tMesh.tAABB.tMin.z = tMesh.atSubmeshes->ptVertexPositions[i].z;
    }
    tMesh.atSubmeshes[0].tAABB = tMesh.tAABB;

    pl_sprintf(acBuffer, "/assets/meshes/%s.plmesh", acFileNameOnly);

    plMeshAssetDesc tMeshDesc = {
        .tDesc = {
            .pcName = acBuffer,
            .eType = PL_ASSET_TYPE_MESH
        },
        .ptMesh = &tMesh
    };
    ptObjectComp->tMesh = gptAsset->create_mesh_asset(&tMeshDesc);
    return tEntity;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_stl_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStlI tApi = {
        .import = pl_stl_import
    };
    pl_set_api(ptApiRegistry, plStlI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptECS         = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptRenderer    = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptMesh        = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptMaterial    = pl_get_api_latest(ptApiRegistry, plMaterialI);
        gptRendererEcs = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
        gptAsset       = pl_get_api_latest(ptApiRegistry, plAssetI);
        gptTransform   = pl_get_api_latest(ptApiRegistry, plTransformI);
    #endif
}

void
pl_unload_stl_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plStlI* ptApi = pl_get_api_latest(ptApiRegistry, plStlI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_STL_IMPLEMENTATION
    #include "pl_stl.h"
    #undef PL_STL_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif