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
#include <string.h> // memset
#include "pl.h"
#include "pl_stl_ext.h"
#include "pl_stl.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_mesh_ext.h"
#include "pl_renderer_ext.h"
#include "pl_vfs_ext.h"
#include "pl_graphics_ext.h"
#include "pl_asset_ext.h"
#include "pl_mesh_ext.h"

// shaders
#include "pl_shader_interop_renderer.h" // PL_MESH_FORMAT_FLAG_XXXX

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    static const plMeshI*  gptMesh  = NULL;
    static const plVfsI*   gptVfs   = NULL;
    static const plAssetI* gptAsset = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

plAssetHandle
pl_stl_import(const char* pcPath)
{
    // read in STL file
    size_t szFileSize = gptVfs->get_file_size_str(pcPath);
    uint8_t* pcBuffer = PL_ALLOC(szFileSize);
    memset(pcBuffer, 0, szFileSize);
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcPath, PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tFileHandle, pcBuffer, &szFileSize);
    gptVfs->close_file(tFileHandle);

    // load STL model
    plStlInfo tInfo = {0};
    pl_load_stl((const char*)pcBuffer, szFileSize, NULL, NULL, NULL, &tInfo);

    plMesh tMesh = {0};
    plSubmeshAllocationDesc tAllocDesc = {
        .uVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .szVertexCount = tInfo.szPositionStreamSize / 3,
        .szIndexCount = (uint32_t)tInfo.szIndexBufferSize
    };
    gptMesh->allocate(&tMesh, &tAllocDesc, 1);
    tMesh.atSubmeshes[0].tMaterial = gptAsset->load("/assets/materials/default.plmaterial");

    pl_load_stl((const char*)pcBuffer, szFileSize, (float*)tMesh.atSubmeshes->ptVertexPositions, (float*)tMesh.atSubmeshes->ptVertexNormals, (uint32_t*)tMesh.atSubmeshes->puIndices, &tInfo);
    PL_FREE(pcBuffer);

    gptMesh->calculate_bounds(&tMesh);

    char acFileNameOnly[128] = {0};
    pl_str_get_file_name_only(pcPath, acFileNameOnly, 128);
    
    char acBuffer[256] = {0};
    pl_sprintf(acBuffer, "/assets/meshes/%s.plmesh", acFileNameOnly);

    plAssetDesc tMeshDesc = {
        .tType = gptMesh->get_asset_type_key(),
        .pcPath = acBuffer
    };
    plAssetHandle tAsset = gptAsset->create(&tMeshDesc, &tMesh);
    gptAsset->save(tAsset, PL_ASSET_ENCODING_TEXT);
    return tAsset;
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
        gptMesh        = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptAsset       = pl_get_api_latest(ptApiRegistry, plAssetI);
        gptMesh        = pl_get_api_latest(ptApiRegistry, plMeshI);
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