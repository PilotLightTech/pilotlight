/*
   pl_mesh_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include <string.h>
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_mesh_ext.h"
#include "pl_math.h"

// extensions
#include "pl_log_ext.h"
#include "pl_vfs_ext.h"
#include "pl_asset_ext.h"
#include "pl_json_ext.h"

// shader interop
#include "pl_shader_interop_renderer.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
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
        #define PL_JSON_FREE(x) gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plLogI*   gptLog  = NULL;
    static const plVfsI*   gptVfs  = NULL;
    static const plAssetI* gptAsset = NULL;
    static const plJsonI* gptJson = NULL;
#endif

#include "pl_ds.h"

typedef struct _plMeshContext
{
    plAssetTypeKey tAssetTypeKey;
} plMeshContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plMeshContext* gptMeshCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMeshBuilderTriangle
{
    uint32_t uIndex0;
    uint32_t uIndex1;
    uint32_t uIndex2;
} plMeshBuilderTriangle;

typedef struct _plMeshBuilder
{
    plMeshBuilderOptions   tOptions;
    plVec3*                sbtVertices;
    plVec3d*               sbtVerticesD;
    plMeshBuilderTriangle* sbtTriangles;
} plMeshBuilder;

typedef struct _plSubmeshHeader
{
    uint64_t uVertexStreamMask;
    char     acMaterialName[256];
} plSubmeshHeader;

typedef struct _plMeshFileHeader
{
    uint32_t uSubmeshCount;
    uint32_t uFlags;
    plAABB   tAABB;
    uint64_t uFileSize;
} plMeshFileHeader;

void pl_mesh_allocate (plMesh*, const plSubmeshAllocationDesc*, uint32_t count);

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline plVec4
pl__mesh_fallback_tangent(plVec3 tNormal)
{
    plVec3 tAxis = fabsf(tNormal.z) < 0.999f ? (plVec3){ 0.0f, 0.0f, 1.0f } : (plVec3){ 0.0f, 1.0f, 0.0f };

    plVec3 tTangent = pl_norm_vec3(pl_cross_vec3(tAxis, tNormal));

    return (plVec4){ tTangent.x, tTangent.y, tTangent.z, 1.0f };
}

static bool
pl__mesh_serialize(const char* pcName, const void* pMesh, plAssetEncoding eEncoding)
{
    const plMesh* ptMesh = pMesh;
    uint32_t uVersion = 1;

    if(eEncoding == PL_ASSET_ENCODING_BINARY)
    {
        plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ_WRITE);

        plAssetFileHeader tAssetFileHeader = {
            .uMagic = PL_ASSET_MAGIC,
            .uVersion = uVersion,
            .uAssetMagic = PL_FOURCC('M', 'E', 'S', 'H')
        };

        plMeshFileHeader tHeader = {
            .tAABB = ptMesh->tAABB,
            .uSubmeshCount = ptMesh->uSubmeshCount,
            .uFlags = 0,

        };
        tHeader.uFileSize = sizeof(plAssetFileHeader) + sizeof(plMeshFileHeader) + ptMesh->szRawDataSize + sizeof(plSubmeshHeader) * ptMesh->uSubmeshCount;
        gptVfs->write_file_stream(tFileHandle, 1, sizeof(plAssetFileHeader), &tAssetFileHeader);
        gptVfs->write_file_stream(tFileHandle, 1, sizeof(tHeader), &tHeader);
        for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
        {
            const char* pcMaterialName = gptAsset->get_path(ptMesh->atSubmeshes[i].tMaterial);
            plSubmeshHeader tSubHeader = {
                .uVertexStreamMask = ptMesh->atSubmeshes[i].uVertexStreamMask
            };
            
            if(pcMaterialName)
                strncpy(tSubHeader.acMaterialName, pcMaterialName, 256);

            gptVfs->write_file_stream(tFileHandle, 1, sizeof(plSubmeshHeader), &tSubHeader);
        }
        gptVfs->write_file_stream(tFileHandle, 1, ptMesh->szRawDataSize, ptMesh->puRawData);
        gptVfs->close_file(tFileHandle);
    }
    else
    {
        // PL_ASSERT(false && "not implemented year");

        plJsonObject* ptRoot = gptJson->new_root_object("root");
        gptJson->add_string_member(ptRoot, "format", "plmesh");
        gptJson->add_uint32_member(ptRoot, "version", uVersion);

        float afAabb[6] = {
            ptMesh->tAABB.tMin.x,
            ptMesh->tAABB.tMin.y,
            ptMesh->tAABB.tMin.z,
            ptMesh->tAABB.tMax.x,
            ptMesh->tAABB.tMax.y,
            ptMesh->tAABB.tMax.z
        };
        gptJson->add_float_array(ptRoot, "aabb", afAabb, 6);

        plJsonObject* ptSubmeshes = gptJson->add_member_array(ptRoot, "submeshes", ptMesh->uSubmeshCount);
        for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
        {
            plJsonObject* ptSubmesh = gptJson->member_by_index(ptSubmeshes, i);

            gptJson->add_string_member(ptSubmesh, "material", gptAsset->get_path(ptMesh->atSubmeshes[i].tMaterial));

            afAabb[0] = ptMesh->atSubmeshes[i].tAABB.tMin.x;
            afAabb[1] = ptMesh->atSubmeshes[i].tAABB.tMin.y;
            afAabb[2] = ptMesh->atSubmeshes[i].tAABB.tMin.z;
            afAabb[3] = ptMesh->atSubmeshes[i].tAABB.tMax.x;
            afAabb[4] = ptMesh->atSubmeshes[i].tAABB.tMax.y;
            afAabb[5] = ptMesh->atSubmeshes[i].tAABB.tMax.z;
            gptJson->add_float_array(ptSubmesh, "aabb", afAabb, 6);

            if(ptMesh->atSubmeshes[i].ptVertexPositions)
            {
                gptJson->add_float_array(ptSubmesh, "positions", (float*)ptMesh->atSubmeshes[i].ptVertexPositions, (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 3);
            }

            if(ptMesh->atSubmeshes[i].ptVertexNormals)
            {
                gptJson->add_float_array(ptSubmesh, "normals", (float*)ptMesh->atSubmeshes[i].ptVertexNormals, (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 3);
            }

            if(ptMesh->atSubmeshes[i].ptVertexTangents)
            {
                gptJson->add_float_array(ptSubmesh, "tangents", (float*)ptMesh->atSubmeshes[i].ptVertexTangents, (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 4);
            }

            if(ptMesh->atSubmeshes[i].ptVertexColors)
            {
                gptJson->add_float_array(ptSubmesh, "colors", (float*)ptMesh->atSubmeshes[i].ptVertexColors, (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 4);
            }

            if(ptMesh->atSubmeshes[i].ptVertexJoints[0])
            {
                gptJson->add_float_array(ptSubmesh, "joints_0", (float*)ptMesh->atSubmeshes[i].ptVertexJoints[0], (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 4);
            }

            if(ptMesh->atSubmeshes[i].ptVertexJoints[1])
            {
                gptJson->add_float_array(ptSubmesh, "joints_1", (float*)ptMesh->atSubmeshes[i].ptVertexJoints[1], (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 4);
            }

            if(ptMesh->atSubmeshes[i].ptVertexWeights[0])
            {
                gptJson->add_float_array(ptSubmesh, "weights_0", (float*)ptMesh->atSubmeshes[i].ptVertexWeights[0], (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 4);
            }

            if(ptMesh->atSubmeshes[i].ptVertexWeights[1])
            {
                gptJson->add_float_array(ptSubmesh, "weights_1", (float*)ptMesh->atSubmeshes[i].ptVertexWeights[1], (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 4);
            }

            if(ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[0])
            {
                gptJson->add_float_array(ptSubmesh, "uv_0", (float*)ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[0], (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 2);
            }

            if(ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[1])
            {
                gptJson->add_float_array(ptSubmesh, "uv_1", (float*)ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[1], (uint32_t)ptMesh->atSubmeshes[i].szVertexCount * 2);
            }

            if(ptMesh->atSubmeshes[i].puIndices)
            {
                gptJson->add_uint32_array(ptSubmesh, "indices", ptMesh->atSubmeshes[i].puIndices, (uint32_t)ptMesh->atSubmeshes[i].szIndexCount);
            }
        }

        uint32_t uBufferSize = 0;
        gptJson->write(ptRoot, NULL, &uBufferSize);
        char* pcBuffer = PL_ALLOC(uBufferSize);
        memset(pcBuffer, 0, uBufferSize);
        gptJson->write(ptRoot, pcBuffer, &uBufferSize);
        
        gptVfs->register_file(pcName, false);
        plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_WRITE);
        gptVfs->write_file(tFileHandle, pcBuffer, uBufferSize);
        gptVfs->close_file(tFileHandle);

        PL_FREE(pcBuffer);
        gptJson->unload(&ptRoot);
    }
    return true;
}

static bool
pl__mesh_deserialize(const char* pcName, void* pMesh)
{
    plMesh* ptMesh = pMesh;

    if(!gptVfs->does_file_exist(pcName))
        return false;

    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plAssetFileHeader tAssetHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(plAssetFileHeader), 1, &tAssetHeader);

    if(tAssetHeader.uMagic == PL_ASSET_MAGIC)
    {
        // read mess file header
        plMeshFileHeader tHeader = {0};
        gptVfs->read_file_stream(tFileHandle, sizeof(tHeader), 1, &tHeader);

        // relevant sizes
        const size_t szMaterialTableSize = sizeof(plSubmeshHeader) * tHeader.uSubmeshCount;
        const size_t szRawDataSize = tHeader.uFileSize - sizeof(plAssetFileHeader) - sizeof(plMeshFileHeader) - szMaterialTableSize;

        // jump to actual mesh data
        gptVfs->set_file_stream_position(tFileHandle, sizeof(plAssetFileHeader) + sizeof(plMeshFileHeader) + szMaterialTableSize);

        // allocate working buffer
        void* pBuffer = PL_ALLOC(tHeader.uFileSize);
        memset(pBuffer, 0, tHeader.uFileSize);

        // read in raw mesh data
        gptVfs->read_file_stream(tFileHandle, szRawDataSize, 1, pBuffer);

        // use dummy mesh to retrieve enough info for mesh alloc
        plMesh tDummyMesh = {0};
        tDummyMesh.uSubmeshCount = tHeader.uSubmeshCount;
        tDummyMesh.atSubmeshes = (plSubmesh*)pBuffer;

        plSubmeshAllocationDesc* sbtSubAllocs = NULL;
        pl_sb_resize(sbtSubAllocs, tDummyMesh.uSubmeshCount);
        for(uint32_t i = 0; i < tDummyMesh.uSubmeshCount; i++)
        {
            sbtSubAllocs[i].uVertexStreamMask = tDummyMesh.atSubmeshes[i].uVertexStreamMask;
            sbtSubAllocs[i].szVertexCount = tDummyMesh.atSubmeshes[i].szVertexCount;
            sbtSubAllocs[i].szIndexCount = tDummyMesh.atSubmeshes[i].szIndexCount;
        }

        // allocate memory for actual mesh
        pl_mesh_allocate(ptMesh, sbtSubAllocs, tDummyMesh.uSubmeshCount);

        pl_sb_free(sbtSubAllocs);

        ptMesh->tAABB = tHeader.tAABB;
        size_t szRawDataOffset = sizeof(plSubmesh) * (size_t)tHeader.uSubmeshCount;
        uint8_t* pcFilingBuffer = pBuffer;
        memcpy(&ptMesh->puRawData[szRawDataOffset], &pcFilingBuffer[szRawDataOffset], ptMesh->szRawDataSize - szRawDataOffset);

        // retrieve material names
        for(uint32_t i = 0; i < tHeader.uSubmeshCount; i++)
        {
            gptVfs->set_file_stream_position(tFileHandle, sizeof(plAssetFileHeader) + sizeof(plMeshFileHeader) + sizeof(plSubmeshHeader) * i);

            plSubmeshHeader tSubmesh = {0};
            gptVfs->read_file_stream(tFileHandle, sizeof(plSubmeshHeader), 1, &tSubmesh);
            ptMesh->atSubmeshes[i].tMaterial = gptAsset->load(tSubmesh.acMaterialName);
            ptMesh->atSubmeshes[i].tAABB = tDummyMesh.atSubmeshes[i].tAABB;
        }
        gptVfs->close_file(tFileHandle);
        PL_FREE(pBuffer);
    }
    else // json
    {
        char acTempBuffer[256] = {0};

        gptVfs->set_file_stream_position(tFileHandle, 0);
        size_t szJsonFileSize = gptVfs->get_file_size_str(pcName);
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);
        
        gptVfs->read_file(tFileHandle, puFileBuffer, &szJsonFileSize);
        gptVfs->close_file(tFileHandle);

        plJsonObject* ptRoot = NULL;
        gptJson->load((const char*)puFileBuffer, &ptRoot);

        float afAABB[6] = {0};
        gptJson->float_array_member(ptRoot, "aabb", afAABB, NULL);

        uint32_t uSubmeshCount = 0;
        plJsonObject* ptJsonSubmeshes = gptJson->array_member(ptRoot, "submeshes", &uSubmeshCount);

        plSubmeshAllocationDesc* sbtAllocDesc = NULL;
        pl_sb_resize(sbtAllocDesc, uSubmeshCount);

        for(uint32_t i = 0; i < uSubmeshCount; i++)
        {
            plJsonObject* ptJsonSubmesh = gptJson->member_by_index(ptJsonSubmeshes, i);

            uint32_t uFloatCount = 0;
            gptJson->float_array_member(ptJsonSubmesh, "positions", NULL, &uFloatCount);

            sbtAllocDesc[i].szVertexCount = uFloatCount / 3;

            plJsonObject* ptJsonPositions = gptJson->member(ptJsonSubmesh, "positions");
            
            if(gptJson->member_exist(ptJsonSubmesh, "normals"))  sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL;
            if(gptJson->member_exist(ptJsonSubmesh, "tangents")) sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT;
            if(gptJson->member_exist(ptJsonSubmesh, "colors")) sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR;
            if(gptJson->member_exist(ptJsonSubmesh, "joints_0")) sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0;
            if(gptJson->member_exist(ptJsonSubmesh, "joints_1")) sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1;
            if(gptJson->member_exist(ptJsonSubmesh, "weights_0"))sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0;
            if(gptJson->member_exist(ptJsonSubmesh, "weights_1"))sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1;
            if(gptJson->member_exist(ptJsonSubmesh, "uv_0"))     sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;
            if(gptJson->member_exist(ptJsonSubmesh, "uv_1"))     sbtAllocDesc[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;

            if(gptJson->member_exist(ptJsonSubmesh, "indices"))
            {
                uint32_t uIndexCount = 0;
                gptJson->uint32_array_member(ptJsonSubmesh, "indices", NULL, &uIndexCount);
                sbtAllocDesc[i].szIndexCount = uIndexCount;
            }
        }
        pl_mesh_allocate(ptMesh, sbtAllocDesc, uSubmeshCount);
        pl_sb_free(sbtAllocDesc);
        ptMesh->tAABB.tMin.x = afAABB[0];
        ptMesh->tAABB.tMin.y = afAABB[1];
        ptMesh->tAABB.tMin.z = afAABB[2];
        ptMesh->tAABB.tMax.x = afAABB[3];
        ptMesh->tAABB.tMax.y = afAABB[4];
        ptMesh->tAABB.tMax.z = afAABB[5];

        for(uint32_t i = 0; i < uSubmeshCount; i++)
        {
            plJsonObject* ptJsonSubmesh = gptJson->member_by_index(ptJsonSubmeshes, i);

            gptJson->string_member(ptJsonSubmesh, "material", acTempBuffer, 256);
            ptMesh->atSubmeshes[i].tMaterial = gptAsset->load(acTempBuffer);

            gptJson->float_array_member(ptJsonSubmesh, "aabb", afAABB, NULL);
            ptMesh->atSubmeshes[i].tAABB.tMin.x = afAABB[0];
            ptMesh->atSubmeshes[i].tAABB.tMin.y = afAABB[1];
            ptMesh->atSubmeshes[i].tAABB.tMin.z = afAABB[2];
            ptMesh->atSubmeshes[i].tAABB.tMax.x = afAABB[3];
            ptMesh->atSubmeshes[i].tAABB.tMax.y = afAABB[4];
            ptMesh->atSubmeshes[i].tAABB.tMax.z = afAABB[5];

            gptJson->float_array_member(ptJsonSubmesh, "positions", (float*)ptMesh->atSubmeshes[i].ptVertexPositions, NULL);

            plJsonObject* ptJsonPositions = gptJson->member(ptJsonSubmesh, "positions");
            
            if(gptJson->member_exist(ptJsonSubmesh, "normals"))   gptJson->float_array_member(ptJsonSubmesh, "normals", (float*)ptMesh->atSubmeshes[i].ptVertexNormals, NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "tangents"))  gptJson->float_array_member(ptJsonSubmesh, "tangents", (float*)ptMesh->atSubmeshes[i].ptVertexTangents, NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "colors"))    gptJson->float_array_member(ptJsonSubmesh, "colors", (float*)ptMesh->atSubmeshes[i].ptVertexColors, NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "joints_0"))  gptJson->float_array_member(ptJsonSubmesh, "joints_0", (float*)ptMesh->atSubmeshes[i].ptVertexJoints[0], NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "joints_1"))  gptJson->float_array_member(ptJsonSubmesh, "joints_1", (float*)ptMesh->atSubmeshes[i].ptVertexJoints[1], NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "weights_0")) gptJson->float_array_member(ptJsonSubmesh, "weights_0", (float*)ptMesh->atSubmeshes[i].ptVertexWeights[0], NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "weights_1")) gptJson->float_array_member(ptJsonSubmesh, "weights_1", (float*)ptMesh->atSubmeshes[i].ptVertexWeights[1], NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "uv_0"))      gptJson->float_array_member(ptJsonSubmesh, "uv_0", (float*)ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[0], NULL);
            if(gptJson->member_exist(ptJsonSubmesh, "uv_1"))      gptJson->float_array_member(ptJsonSubmesh, "uv_1", (float*)ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[1], NULL);

            if(gptJson->member_exist(ptJsonSubmesh, "indices"))
                gptJson->uint32_array_member(ptJsonSubmesh, "indices", ptMesh->atSubmeshes[i].puIndices, NULL);
        }

        PL_FREE(puFileBuffer);
        gptJson->unload(&ptRoot);
    }
    return true;
}

void
pl__mesh_cleanup(void* pMesh)
{
    plMesh* ptMesh = pMesh;
    if(ptMesh->puRawData)
    {
        PL_FREE(ptMesh->puRawData);
        ptMesh->puRawData = NULL;
    }
    ptMesh->atSubmeshes = NULL;
    ptMesh->uSubmeshCount = 0;
    ptMesh->szRawDataSize = 0;
    ptMesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
}

bool
pl_mesh_generate_uvs(plMesh* ptMesh)
{

    // check if uvs are needed
    bool bUVsNeeded = false;
    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        if(ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[0] == NULL)
        {
            bUVsNeeded = true;
            break;
        }
    }

    if(!bUVsNeeded)
        return false;

    plSubmeshAllocationDesc* atSubmeshAllocDescs = PL_ALLOC(sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);
    memset(atSubmeshAllocDescs, 0, sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);

    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        atSubmeshAllocDescs[i].uVertexStreamMask = ptMesh->atSubmeshes[i].uVertexStreamMask;
        atSubmeshAllocDescs[i].szVertexCount = ptMesh->atSubmeshes[i].szVertexCount;
        atSubmeshAllocDescs[i].szIndexCount = ptMesh->atSubmeshes[i].szIndexCount;
        if(ptMesh->atSubmeshes[i].ptVertexTextureCoordinates[0] == NULL)
        {
            atSubmeshAllocDescs[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;
        }
    }

    plMesh tNewMesh = {0};
    pl_mesh_allocate(&tNewMesh, atSubmeshAllocDescs, ptMesh->uSubmeshCount);
    PL_FREE(atSubmeshAllocDescs);
    atSubmeshAllocDescs = NULL;
    tNewMesh.tAABB = ptMesh->tAABB;

    for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
    {

        plSubmesh* ptNewSubmesh = &tNewMesh.atSubmeshes[uSubmeshIndex];
        plSubmesh* ptOldSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];

        ptNewSubmesh->tMaterial = ptOldSubmesh->tMaterial;
        ptNewSubmesh->tAABB = ptOldSubmesh->tAABB;

        if(ptNewSubmesh->ptVertexPositions)             memcpy(ptNewSubmesh->ptVertexPositions, ptOldSubmesh->ptVertexPositions, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTangents)              memcpy(ptNewSubmesh->ptVertexTangents, ptOldSubmesh->ptVertexTangents, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexColors)                memcpy(ptNewSubmesh->ptVertexColors, ptOldSubmesh->ptVertexColors, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[0])            memcpy(ptNewSubmesh->ptVertexWeights[0], ptOldSubmesh->ptVertexWeights[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[1])            memcpy(ptNewSubmesh->ptVertexWeights[1], ptOldSubmesh->ptVertexWeights[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[0])             memcpy(ptNewSubmesh->ptVertexJoints[0], ptOldSubmesh->ptVertexJoints[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[1])             memcpy(ptNewSubmesh->ptVertexJoints[1], ptOldSubmesh->ptVertexJoints[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptOldSubmesh->puIndices)                     memcpy(ptNewSubmesh->puIndices, ptOldSubmesh->puIndices, sizeof(uint32_t) * ptOldSubmesh->szIndexCount);
        if(ptOldSubmesh->ptVertexNormals)               memcpy(ptNewSubmesh->ptVertexNormals, ptOldSubmesh->ptVertexNormals, sizeof(plVec3) * ptOldSubmesh->szVertexCount);


        if(ptOldSubmesh->ptVertexTextureCoordinates[0])
            memcpy(ptNewSubmesh->ptVertexTextureCoordinates[0], ptOldSubmesh->ptVertexTextureCoordinates[0], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        else
            memset(ptNewSubmesh->ptVertexTextureCoordinates[0], 0, sizeof(plVec2) * ptNewSubmesh->szVertexCount);

        if(ptOldSubmesh->ptVertexTextureCoordinates[1])
            memcpy(ptNewSubmesh->ptVertexTextureCoordinates[1], ptOldSubmesh->ptVertexTextureCoordinates[1], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        else
            memset(ptNewSubmesh->ptVertexTextureCoordinates[1], 0, sizeof(plVec2) * ptNewSubmesh->szVertexCount);
    }

    pl__mesh_cleanup(ptMesh);
    *ptMesh = tNewMesh;
    return true;
}

bool
pl_mesh_generate_colors(plMesh* ptMesh)
{

    // check if uvs are needed
    bool bColorsNeeded = false;
    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        if(ptMesh->atSubmeshes[i].ptVertexColors == NULL)
        {
            bColorsNeeded = true;
            break;
        }
    }

    if(!bColorsNeeded)
        return false;

    plSubmeshAllocationDesc* atSubmeshAllocDescs = PL_ALLOC(sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);
    memset(atSubmeshAllocDescs, 0, sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);

    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        atSubmeshAllocDescs[i].uVertexStreamMask = ptMesh->atSubmeshes[i].uVertexStreamMask;
        atSubmeshAllocDescs[i].szVertexCount = ptMesh->atSubmeshes[i].szVertexCount;
        atSubmeshAllocDescs[i].szIndexCount = ptMesh->atSubmeshes[i].szIndexCount;
        if(ptMesh->atSubmeshes[i].ptVertexColors == NULL)
        {
            atSubmeshAllocDescs[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR;
        }
    }

    plMesh tNewMesh = {0};
    pl_mesh_allocate(&tNewMesh, atSubmeshAllocDescs, ptMesh->uSubmeshCount);
    PL_FREE(atSubmeshAllocDescs);
    atSubmeshAllocDescs = NULL;
    tNewMesh.tAABB = ptMesh->tAABB;

    for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
    {

        plSubmesh* ptNewSubmesh = &tNewMesh.atSubmeshes[uSubmeshIndex];
        plSubmesh* ptOldSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];

        ptNewSubmesh->tMaterial = ptOldSubmesh->tMaterial;
        ptNewSubmesh->tAABB = ptOldSubmesh->tAABB;

        if(ptNewSubmesh->ptVertexPositions)             memcpy(ptNewSubmesh->ptVertexPositions, ptOldSubmesh->ptVertexPositions, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTangents)              memcpy(ptNewSubmesh->ptVertexTangents, ptOldSubmesh->ptVertexTangents, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[0])            memcpy(ptNewSubmesh->ptVertexWeights[0], ptOldSubmesh->ptVertexWeights[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[1])            memcpy(ptNewSubmesh->ptVertexWeights[1], ptOldSubmesh->ptVertexWeights[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[0])             memcpy(ptNewSubmesh->ptVertexJoints[0], ptOldSubmesh->ptVertexJoints[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[1])             memcpy(ptNewSubmesh->ptVertexJoints[1], ptOldSubmesh->ptVertexJoints[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->puIndices)                     memcpy(ptNewSubmesh->puIndices, ptOldSubmesh->puIndices, sizeof(uint32_t) * ptOldSubmesh->szIndexCount);
        if(ptNewSubmesh->ptVertexNormals)               memcpy(ptNewSubmesh->ptVertexNormals, ptOldSubmesh->ptVertexNormals, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[0]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[0], ptOldSubmesh->ptVertexTextureCoordinates[0], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[1]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[1], ptOldSubmesh->ptVertexTextureCoordinates[1], sizeof(plVec2) * ptOldSubmesh->szVertexCount);

        if(ptOldSubmesh->ptVertexColors)
            memcpy(ptNewSubmesh->ptVertexColors, ptOldSubmesh->ptVertexColors, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        else
        {
            for(uint32_t i = 0; i < ptNewSubmesh->szVertexCount; i++)
            {
                ptNewSubmesh->ptVertexColors[i] = (plVec4){1.0f, 1.0f, 1.0f, 1.0f};
            }
        }

    }

    pl__mesh_cleanup(ptMesh);
    *ptMesh = tNewMesh;
    return true;
}

bool
pl_mesh_generate_normals(plMesh* ptMesh)
{

    // check if normals are needed
    bool bNormalsNeeded = false;
    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        if(ptMesh->atSubmeshes[i].ptVertexNormals == NULL)
        {
            bNormalsNeeded = true;
            break;
        }
    }

    if(!bNormalsNeeded)
        return false;

    plSubmeshAllocationDesc* atSubmeshAllocDescs = PL_ALLOC(sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);
    memset(atSubmeshAllocDescs, 0, sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);

    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        atSubmeshAllocDescs[i].uVertexStreamMask = ptMesh->atSubmeshes[i].uVertexStreamMask;
        atSubmeshAllocDescs[i].szVertexCount = ptMesh->atSubmeshes[i].szVertexCount;
        atSubmeshAllocDescs[i].szIndexCount = ptMesh->atSubmeshes[i].szIndexCount;
        if(ptMesh->atSubmeshes[i].ptVertexNormals == NULL)
        {
            atSubmeshAllocDescs[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL;
        }
    }

    plMesh tNewMesh = {0};
    pl_mesh_allocate(&tNewMesh, atSubmeshAllocDescs, ptMesh->uSubmeshCount);
    PL_FREE(atSubmeshAllocDescs);
    atSubmeshAllocDescs = NULL;
    tNewMesh.tAABB = ptMesh->tAABB;

    for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
    {

        plSubmesh* ptNewSubmesh = &tNewMesh.atSubmeshes[uSubmeshIndex];
        plSubmesh* ptOldSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];

        ptNewSubmesh->tMaterial = ptOldSubmesh->tMaterial;
        ptNewSubmesh->tAABB = ptOldSubmesh->tAABB;

        if(ptNewSubmesh->ptVertexPositions)             memcpy(ptNewSubmesh->ptVertexPositions, ptOldSubmesh->ptVertexPositions, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTangents)              memcpy(ptNewSubmesh->ptVertexTangents, ptOldSubmesh->ptVertexTangents, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexColors)                memcpy(ptNewSubmesh->ptVertexColors, ptOldSubmesh->ptVertexColors, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[0])            memcpy(ptNewSubmesh->ptVertexWeights[0], ptOldSubmesh->ptVertexWeights[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[1])            memcpy(ptNewSubmesh->ptVertexWeights[1], ptOldSubmesh->ptVertexWeights[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[0])             memcpy(ptNewSubmesh->ptVertexJoints[0], ptOldSubmesh->ptVertexJoints[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[1])             memcpy(ptNewSubmesh->ptVertexJoints[1], ptOldSubmesh->ptVertexJoints[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[0]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[0], ptOldSubmesh->ptVertexTextureCoordinates[0], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[1]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[1], ptOldSubmesh->ptVertexTextureCoordinates[1], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        if(ptOldSubmesh->puIndices)                     memcpy(ptNewSubmesh->puIndices, ptOldSubmesh->puIndices, sizeof(uint32_t) * ptOldSubmesh->szIndexCount);

        if(ptOldSubmesh->ptVertexNormals)
            memcpy(ptNewSubmesh->ptVertexNormals, ptOldSubmesh->ptVertexNormals, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        else
        {
            memset(ptNewSubmesh->ptVertexNormals, 0, sizeof(plVec3) * ptNewSubmesh->szVertexCount);

            for(uint32_t i = 0; i < ptNewSubmesh->szIndexCount - 2; i += 3)
            {
                const uint32_t uIndex0 = ptNewSubmesh->puIndices[i + 0];
                const uint32_t uIndex1 = ptNewSubmesh->puIndices[i + 1];
                const uint32_t uIndex2 = ptNewSubmesh->puIndices[i + 2];

                const plVec3 tP0 = ptNewSubmesh->ptVertexPositions[uIndex0];
                const plVec3 tP1 = ptNewSubmesh->ptVertexPositions[uIndex1];
                const plVec3 tP2 = ptNewSubmesh->ptVertexPositions[uIndex2];

                const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                const plVec3 tNorm = pl_cross_vec3(tEdge1, tEdge2);

                ptNewSubmesh->ptVertexNormals[uIndex0] = pl_add_vec3(ptNewSubmesh->ptVertexNormals[uIndex0], tNorm);
                ptNewSubmesh->ptVertexNormals[uIndex1] = pl_add_vec3(ptNewSubmesh->ptVertexNormals[uIndex1], tNorm);
                ptNewSubmesh->ptVertexNormals[uIndex2] = pl_add_vec3(ptNewSubmesh->ptVertexNormals[uIndex2], tNorm);
            }

            for(uint32_t i = 0; i < ptNewSubmesh->szVertexCount; i++)
            {
                ptNewSubmesh->ptVertexNormals[i] = pl_norm_vec3(ptNewSubmesh->ptVertexNormals[i]);
            }
        }
    }

    pl__mesh_cleanup(ptMesh);
    *ptMesh = tNewMesh;
    return true;
}

bool
pl_mesh_generate_tangents(plMesh* ptMesh)
{

    // check if normals are needed
    bool bTangentsNeeded = false;
    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        if(ptMesh->atSubmeshes[i].ptVertexTangents == NULL)
        {
            bTangentsNeeded = true;
            break;
        }
    }

    if(!bTangentsNeeded)
        return false;

    pl_mesh_generate_normals(ptMesh);
    
    plSubmeshAllocationDesc* atSubmeshAllocDescs = PL_ALLOC(sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);
    memset(atSubmeshAllocDescs, 0, sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);

    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        atSubmeshAllocDescs[i].uVertexStreamMask = ptMesh->atSubmeshes[i].uVertexStreamMask;
        atSubmeshAllocDescs[i].szVertexCount = ptMesh->atSubmeshes[i].szVertexCount;
        atSubmeshAllocDescs[i].szIndexCount = ptMesh->atSubmeshes[i].szIndexCount;
        if(ptMesh->atSubmeshes[i].ptVertexTangents == NULL)
        {
            atSubmeshAllocDescs[i].uVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT;
        }
    }

    plMesh tNewMesh = {0};
    pl_mesh_allocate(&tNewMesh, atSubmeshAllocDescs, ptMesh->uSubmeshCount);
    PL_FREE(atSubmeshAllocDescs);
    atSubmeshAllocDescs = NULL;
    tNewMesh.tAABB = ptMesh->tAABB;

    for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
    {

        plSubmesh* ptNewSubmesh = &tNewMesh.atSubmeshes[uSubmeshIndex];
        plSubmesh* ptOldSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];

        ptNewSubmesh->tMaterial = ptOldSubmesh->tMaterial;
        ptNewSubmesh->tAABB = ptOldSubmesh->tAABB;

        if(ptNewSubmesh->ptVertexPositions)             memcpy(ptNewSubmesh->ptVertexPositions, ptOldSubmesh->ptVertexPositions, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexColors)                memcpy(ptNewSubmesh->ptVertexColors, ptOldSubmesh->ptVertexColors, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[0])            memcpy(ptNewSubmesh->ptVertexWeights[0], ptOldSubmesh->ptVertexWeights[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[1])            memcpy(ptNewSubmesh->ptVertexWeights[1], ptOldSubmesh->ptVertexWeights[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[0])             memcpy(ptNewSubmesh->ptVertexJoints[0], ptOldSubmesh->ptVertexJoints[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[1])             memcpy(ptNewSubmesh->ptVertexJoints[1], ptOldSubmesh->ptVertexJoints[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[0]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[0], ptOldSubmesh->ptVertexTextureCoordinates[0], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[1]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[1], ptOldSubmesh->ptVertexTextureCoordinates[1], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->puIndices)                     memcpy(ptNewSubmesh->puIndices, ptOldSubmesh->puIndices, sizeof(uint32_t) * ptOldSubmesh->szIndexCount);
        if(ptNewSubmesh->ptVertexNormals)               memcpy(ptNewSubmesh->ptVertexNormals, ptOldSubmesh->ptVertexNormals, sizeof(plVec3) * ptOldSubmesh->szVertexCount);


        if(ptOldSubmesh->ptVertexTangents)
        {
            memcpy(ptNewSubmesh->ptVertexTangents, ptOldSubmesh->ptVertexTangents, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        }
        else
        {

            // no UVs -> arbitrary tangent basis
            if(!ptNewSubmesh->ptVertexTextureCoordinates[0])
            {
                for(uint32_t i = 0; i < ptNewSubmesh->szVertexCount; i++)
                    ptNewSubmesh->ptVertexTangents[i] = pl__mesh_fallback_tangent(ptNewSubmesh->ptVertexNormals[i]);
                
                continue;
            }

            memset(ptNewSubmesh->ptVertexTangents, 0, sizeof(plVec4) * ptNewSubmesh->szVertexCount);

            for(uint32_t i = 0; i < ptNewSubmesh->szIndexCount - 2; i += 3)
            {
                const uint32_t uIndex0 = ptNewSubmesh->puIndices[i + 0];
                const uint32_t uIndex1 = ptNewSubmesh->puIndices[i + 1];
                const uint32_t uIndex2 = ptNewSubmesh->puIndices[i + 2];

                const plVec3 tP0 = ptNewSubmesh->ptVertexPositions[uIndex0];
                const plVec3 tP1 = ptNewSubmesh->ptVertexPositions[uIndex1];
                const plVec3 tP2 = ptNewSubmesh->ptVertexPositions[uIndex2];

                const plVec2 tTex0 = ptNewSubmesh->ptVertexTextureCoordinates[0][uIndex0];
                const plVec2 tTex1 = ptNewSubmesh->ptVertexTextureCoordinates[0][uIndex1];
                const plVec2 tTex2 = ptNewSubmesh->ptVertexTextureCoordinates[0][uIndex2];

                const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                const float fDeltaU1 = tTex1.x - tTex0.x;
                const float fDeltaV1 = tTex1.y - tTex0.y;
                const float fDeltaU2 = tTex2.x - tTex0.x;
                const float fDeltaV2 = tTex2.y - tTex0.y;

                const float fDet = fDeltaU1 * fDeltaV2 - fDeltaV1 * fDeltaU2;

                // degenerate UV triangle
                if(fabsf(fDet) < 1e-8f)
                    continue;

                const float fR = 1.0f / fDet;

                const plVec3 tTangent = {
                    fR * (fDeltaV2 * tEdge1.x - fDeltaV1 * tEdge2.x),
                    fR * (fDeltaV2 * tEdge1.y - fDeltaV1 * tEdge2.y),
                    fR * (fDeltaV2 * tEdge1.z - fDeltaV1 * tEdge2.z)
                };

                ptNewSubmesh->ptVertexTangents[uIndex0].xyz = pl_add_vec3(ptNewSubmesh->ptVertexTangents[uIndex0].xyz, tTangent);
                ptNewSubmesh->ptVertexTangents[uIndex1].xyz = pl_add_vec3(ptNewSubmesh->ptVertexTangents[uIndex1].xyz, tTangent);
                ptNewSubmesh->ptVertexTangents[uIndex2].xyz = pl_add_vec3(ptNewSubmesh->ptVertexTangents[uIndex2].xyz, tTangent);

                // This is adequate if mirrored UV regions have split vertices.
                const float fHandedness = fDet < 0.0f ? -1.0f : 1.0f;

                ptNewSubmesh->ptVertexTangents[uIndex0].w += fHandedness;
                ptNewSubmesh->ptVertexTangents[uIndex1].w += fHandedness;
                ptNewSubmesh->ptVertexTangents[uIndex2].w += fHandedness;

                const plVec3 atNormals[3] = { 
                    ptNewSubmesh->ptVertexNormals[uIndex0],
                    ptNewSubmesh->ptVertexNormals[uIndex1],
                    ptNewSubmesh->ptVertexNormals[uIndex2],
                };

                plVec4 atFinalTangents[3] = {0};
                for(uint32_t j = 0; j < 3; j++)
                {
                    const float fD = pl_dot_vec3(atNormals[j], tTangent);
                    const plVec3 tProjected = pl_mul_vec3_scalarf(atNormals[j], fD);
                    atFinalTangents[j].xyz = pl_norm_vec3(pl_sub_vec3(tTangent, tProjected));
                    atFinalTangents[j].w = fHandedness;
                }

                ptNewSubmesh->ptVertexTangents[uIndex0] = pl_add_vec4(ptNewSubmesh->ptVertexTangents[uIndex0], atFinalTangents[0]);
                ptNewSubmesh->ptVertexTangents[uIndex1] = pl_add_vec4(ptNewSubmesh->ptVertexTangents[uIndex1], atFinalTangents[1]);
                ptNewSubmesh->ptVertexTangents[uIndex2] = pl_add_vec4(ptNewSubmesh->ptVertexTangents[uIndex2], atFinalTangents[2]);
            }

            for(uint32_t i = 0; i < ptNewSubmesh->szVertexCount; i++)
            {
                const plVec3 tNormal = ptNewSubmesh->ptVertexNormals[i];
                plVec3 tTangent = ptNewSubmesh->ptVertexTangents[i].xyz;

                if(pl_dot_vec3(tTangent, tTangent) < 1e-12f)
                {
                    ptNewSubmesh->ptVertexTangents[i] = pl__mesh_fallback_tangent(tNormal);
                    continue;
                }

                // Gram-Schmidt
                tTangent = pl_sub_vec3(tTangent, pl_mul_vec3_scalarf(tNormal, pl_dot_vec3(tNormal, tTangent)));

                tTangent = pl_norm_vec3(tTangent);

                ptNewSubmesh->ptVertexTangents[i].xyz = tTangent;
                ptNewSubmesh->ptVertexTangents[i].w = ptNewSubmesh->ptVertexTangents[i].w < 0.0f ? -1.0f : 1.0f;
            }
        }
    }

    pl__mesh_cleanup(ptMesh);
    *ptMesh = tNewMesh;
    return true;
}

void
pl_mesh_calculate_bounds(plMesh* ptMesh)
{
    ptMesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};

    for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
    {
        plSubmesh* ptSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];
        ptSubmesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
        ptSubmesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};

        for(uint32_t i = 0; i < ptSubmesh->szVertexCount; i++)
        {
            if(ptSubmesh->ptVertexPositions[i].x > ptSubmesh->tAABB.tMax.x) ptSubmesh->tAABB.tMax.x = ptSubmesh->ptVertexPositions[i].x;
            if(ptSubmesh->ptVertexPositions[i].y > ptSubmesh->tAABB.tMax.y) ptSubmesh->tAABB.tMax.y = ptSubmesh->ptVertexPositions[i].y;
            if(ptSubmesh->ptVertexPositions[i].z > ptSubmesh->tAABB.tMax.z) ptSubmesh->tAABB.tMax.z = ptSubmesh->ptVertexPositions[i].z;
            if(ptSubmesh->ptVertexPositions[i].x < ptSubmesh->tAABB.tMin.x) ptSubmesh->tAABB.tMin.x = ptSubmesh->ptVertexPositions[i].x;
            if(ptSubmesh->ptVertexPositions[i].y < ptSubmesh->tAABB.tMin.y) ptSubmesh->tAABB.tMin.y = ptSubmesh->ptVertexPositions[i].y;
            if(ptSubmesh->ptVertexPositions[i].z < ptSubmesh->tAABB.tMin.z) ptSubmesh->tAABB.tMin.z = ptSubmesh->ptVertexPositions[i].z;
        }
    }

    for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
    {
        plSubmesh* ptSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];
        if(ptSubmesh->tAABB.tMin.x < ptMesh->tAABB.tMin.x) ptMesh->tAABB.tMin.x = ptSubmesh->tAABB.tMin.x;
        if(ptSubmesh->tAABB.tMin.y < ptMesh->tAABB.tMin.y) ptMesh->tAABB.tMin.y = ptSubmesh->tAABB.tMin.y;
        if(ptSubmesh->tAABB.tMax.x > ptMesh->tAABB.tMax.x) ptMesh->tAABB.tMax.x = ptSubmesh->tAABB.tMax.x;
        if(ptSubmesh->tAABB.tMax.y > ptMesh->tAABB.tMax.y) ptMesh->tAABB.tMax.y = ptSubmesh->tAABB.tMax.y;
    }
}

bool
pl_mesh_generate_indices(plMesh* ptMesh)
{
    // check if indices are needed
    bool bIndicesNeeded = false;
    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        if(ptMesh->atSubmeshes[i].szIndexCount == 0)
        {
            bIndicesNeeded = true;
            break;
        }
    }

    if(!bIndicesNeeded)
        return false;

    plSubmeshAllocationDesc* atSubmeshAllocDescs = PL_ALLOC(sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);
    memset(atSubmeshAllocDescs, 0, sizeof(plSubmeshAllocationDesc) * ptMesh->uSubmeshCount);

    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        atSubmeshAllocDescs[i].uVertexStreamMask = ptMesh->atSubmeshes[i].uVertexStreamMask;
        atSubmeshAllocDescs[i].szVertexCount = ptMesh->atSubmeshes[i].szVertexCount;
        atSubmeshAllocDescs[i].szIndexCount = ptMesh->atSubmeshes[i].szIndexCount;
        if(ptMesh->atSubmeshes[i].szIndexCount == 0)
        {
            atSubmeshAllocDescs[i].szIndexCount = ptMesh->atSubmeshes[i].szVertexCount;
        }
    }

    plMesh tNewMesh = {0};
    pl_mesh_allocate(&tNewMesh, atSubmeshAllocDescs, ptMesh->uSubmeshCount);
    PL_FREE(atSubmeshAllocDescs);
    atSubmeshAllocDescs = NULL;
    tNewMesh.tAABB = ptMesh->tAABB;

    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        plSubmesh* ptNewSubmesh = &tNewMesh.atSubmeshes[i];
        plSubmesh* ptOldSubmesh = &ptMesh->atSubmeshes[i];
        
        ptNewSubmesh->tMaterial = ptOldSubmesh->tMaterial;
        ptNewSubmesh->tAABB = ptOldSubmesh->tAABB;

        if(ptNewSubmesh->ptVertexPositions)             memcpy(ptNewSubmesh->ptVertexPositions, ptOldSubmesh->ptVertexPositions, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexNormals)               memcpy(ptNewSubmesh->ptVertexNormals, ptOldSubmesh->ptVertexNormals, sizeof(plVec3) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTangents)              memcpy(ptNewSubmesh->ptVertexTangents, ptOldSubmesh->ptVertexTangents, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexColors)                memcpy(ptNewSubmesh->ptVertexColors, ptOldSubmesh->ptVertexColors, sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[0])            memcpy(ptNewSubmesh->ptVertexWeights[0], ptOldSubmesh->ptVertexWeights[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexWeights[1])            memcpy(ptNewSubmesh->ptVertexWeights[1], ptOldSubmesh->ptVertexWeights[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[0])             memcpy(ptNewSubmesh->ptVertexJoints[0], ptOldSubmesh->ptVertexJoints[0], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexJoints[1])             memcpy(ptNewSubmesh->ptVertexJoints[1], ptOldSubmesh->ptVertexJoints[1], sizeof(plVec4) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[0]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[0], ptOldSubmesh->ptVertexTextureCoordinates[0], sizeof(plVec2) * ptOldSubmesh->szVertexCount);
        if(ptNewSubmesh->ptVertexTextureCoordinates[1]) memcpy(ptNewSubmesh->ptVertexTextureCoordinates[1], ptOldSubmesh->ptVertexTextureCoordinates[1], sizeof(plVec2) * ptOldSubmesh->szVertexCount);

        if(ptOldSubmesh->puIndices)
            memcpy(ptNewSubmesh->puIndices, ptOldSubmesh->puIndices, sizeof(uint32_t) * ptOldSubmesh->szIndexCount);
        else
        {
            for(uint32_t j = 0; j < ptOldSubmesh->szVertexCount; j++)
            {
                ptNewSubmesh->puIndices[j] = j;
            }
        }
    }

    pl__mesh_cleanup(ptMesh);
    *ptMesh = tNewMesh;
    return true;
}

void
pl_mesh_allocate(plMesh* ptMesh, const plSubmeshAllocationDesc* atAllocDesc, uint32_t uCount)
{

    ptMesh->szRawDataSize = sizeof(plSubmesh) * (size_t)uCount;
    ptMesh->uSubmeshCount = uCount;

    for(uint32_t i = 0; i < uCount; i++)
    {
        const plSubmeshAllocationDesc* ptDesc = &atAllocDesc[i];

        size_t szBytesPerVertex = sizeof(plVec3);

        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)     szBytesPerVertex += sizeof(plVec3);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT)    szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR)      szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0)   szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1)   szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)  szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)  szBytesPerVertex += sizeof(plVec4);
        
        ptMesh->szRawDataSize += szBytesPerVertex * ptDesc->szVertexCount + ptDesc->szIndexCount * sizeof(uint32_t);
    }

    ptMesh->puRawData = PL_ALLOC(ptMesh->szRawDataSize);
    memset(ptMesh->puRawData, 0, ptMesh->szRawDataSize);

    ptMesh->atSubmeshes = (plSubmesh*)ptMesh->puRawData;

    size_t szBufferOffset = sizeof(plSubmesh) * (size_t)uCount;

    for(uint32_t i = 0; i < uCount; i++)
    {
        const plSubmeshAllocationDesc* ptDesc = &atAllocDesc[i];
        plSubmesh* ptSubmesh = &ptMesh->atSubmeshes[i];
        ptSubmesh->uVertexStreamMask = ptDesc->uVertexStreamMask;
        ptSubmesh->szIndexCount = ptDesc->szIndexCount;
        ptSubmesh->szVertexCount = ptDesc->szVertexCount;
        ptSubmesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
        ptSubmesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};

        ptSubmesh->ptVertexPositions = (plVec3*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec3);
        
        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
        {
            ptSubmesh->ptVertexNormals = (plVec3*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec3);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT)
        {
            ptSubmesh->ptVertexTangents = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)
        {
            ptSubmesh->ptVertexTextureCoordinates[0] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec2);

            ptSubmesh->ptVertexTextureCoordinates[1] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec2);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR)
        {
            ptSubmesh->ptVertexColors = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0)
        {
            ptSubmesh->ptVertexJoints[0] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1)
        {
            ptSubmesh->ptVertexJoints[1] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)
        {
            ptSubmesh->ptVertexWeights[0] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)
        {
            ptSubmesh->ptVertexWeights[1] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->szIndexCount > 0)
        {
            ptSubmesh->puIndices = (uint32_t*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szIndexCount * sizeof(uint32_t);
        }
    }

    ptMesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
}

void
pl_mesh_cleanup(plMesh* ptMesh)
{
    pl__mesh_cleanup(ptMesh);
}

void
pl_mesh_create_sphere(float fRadius, uint32_t uLatitudeBands, uint32_t uLongitudeBands, plMesh* ptMesh)
{
    if(uLatitudeBands == 0)
        uLatitudeBands = 64;

    if(uLongitudeBands == 0)
    uLongitudeBands = 64;

    plSubmeshAllocationDesc tAllocDesc = {
        .uVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .szVertexCount = (uLatitudeBands + 1) * (uLongitudeBands + 1),
        .szIndexCount = uLatitudeBands * uLongitudeBands * 6
    };
    pl_mesh_allocate(ptMesh, &tAllocDesc, 1);

    uint32_t uCurrentPoint = 0;

    for(uint32_t uLatNumber = 0; uLatNumber <= uLatitudeBands; uLatNumber++)
    {
        const float fTheta = (float)uLatNumber * PL_PI / (float)uLatitudeBands;
        const float fSinTheta = sinf(fTheta);
        const float fCosTheta = cosf(fTheta);
        for(uint32_t uLongNumber = 0; uLongNumber <= uLongitudeBands; uLongNumber++)
        {
            const float fPhi = (float)uLongNumber * 2 * PL_PI / (float)uLongitudeBands;
            const float fSinPhi = sinf(fPhi);
            const float fCosPhi = cosf(fPhi);
            ptMesh->atSubmeshes[0].ptVertexPositions[uCurrentPoint] = (plVec3){
                fCosPhi * fSinTheta * fRadius,
                fCosTheta * fRadius,
                fSinPhi * fSinTheta * fRadius
            };
            ptMesh->atSubmeshes[0].ptVertexNormals[uCurrentPoint] = pl_norm_vec3(ptMesh->atSubmeshes[0].ptVertexPositions[uCurrentPoint]);
            uCurrentPoint++;
        }
    }

    uCurrentPoint = 0;
    for(uint32_t uLatNumber = 0; uLatNumber < uLatitudeBands; uLatNumber++)
    {

        for(uint32_t uLongNumber = 0; uLongNumber < uLongitudeBands; uLongNumber++)
        {
            const uint32_t uFirst = (uLatNumber * (uLongitudeBands + 1)) + uLongNumber;
            const uint32_t uSecond = uFirst + uLongitudeBands + 1;

            ptMesh->atSubmeshes[0].puIndices[uCurrentPoint + 0] = uFirst + 1;
            ptMesh->atSubmeshes[0].puIndices[uCurrentPoint + 1] = uSecond;
            ptMesh->atSubmeshes[0].puIndices[uCurrentPoint + 2] = uFirst;

            ptMesh->atSubmeshes[0].puIndices[uCurrentPoint + 3] = uFirst + 1;
            ptMesh->atSubmeshes[0].puIndices[uCurrentPoint + 4] = uSecond + 1;
            ptMesh->atSubmeshes[0].puIndices[uCurrentPoint + 5] = uSecond;

            uCurrentPoint += 6;
        }
    }
    ptMesh->tAABB.tMin = (plVec3){-fRadius, -fRadius, -fRadius};
    ptMesh->tAABB.tMax = (plVec3){fRadius, fRadius, fRadius};
}

void
pl_mesh_create_cube(plMesh* ptMesh)
{
    plSubmeshAllocationDesc tAllocDesc = {
        .uVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .szVertexCount = 4 * 6,
        .szIndexCount = 6 * 6
    };
    pl_mesh_allocate(ptMesh, &tAllocDesc, 1);

    // front (+z)
    ptMesh->atSubmeshes[0].ptVertexPositions[0] = (plVec3){  0.5f, -0.5f, 0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[1] = (plVec3){  0.5f,  0.5f, 0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[2] = (plVec3){ -0.5f,  0.5f, 0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[3] = (plVec3){ -0.5f, -0.5f, 0.5f };

    ptMesh->atSubmeshes[0].ptVertexNormals[0] = (plVec3){ 0.0f, 0.0f, 1.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[1] = (plVec3){ 0.0f, 0.0f, 1.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[2] = (plVec3){ 0.0f, 0.0f, 1.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[3] = (plVec3){ 0.0f, 0.0f, 1.0f};

    ptMesh->atSubmeshes[0].puIndices[0] = 0;
    ptMesh->atSubmeshes[0].puIndices[1] = 1;
    ptMesh->atSubmeshes[0].puIndices[2] = 2;
    ptMesh->atSubmeshes[0].puIndices[3] = 0;
    ptMesh->atSubmeshes[0].puIndices[4] = 2;
    ptMesh->atSubmeshes[0].puIndices[5] = 3;

    // back (-z)
    ptMesh->atSubmeshes[0].ptVertexPositions[4] = (plVec3){  0.5f, -0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[5] = (plVec3){  0.5f,  0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[6] = (plVec3){ -0.5f,  0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[7] = (plVec3){ -0.5f, -0.5f, -0.5f };

    ptMesh->atSubmeshes[0].ptVertexNormals[4] = (plVec3){ 0.0f, 0.0f, -1.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[5] = (plVec3){ 0.0f, 0.0f, -1.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[6] = (plVec3){ 0.0f, 0.0f, -1.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[7] = (plVec3){ 0.0f, 0.0f, -1.0f};

    ptMesh->atSubmeshes[0].puIndices[6] = 6;
    ptMesh->atSubmeshes[0].puIndices[7] = 5;
    ptMesh->atSubmeshes[0].puIndices[8] = 4;
    ptMesh->atSubmeshes[0].puIndices[9] = 7;
    ptMesh->atSubmeshes[0].puIndices[10] = 6;
    ptMesh->atSubmeshes[0].puIndices[11] = 4;

    // right (+x)
    ptMesh->atSubmeshes[0].ptVertexPositions[8]  = (plVec3){ 0.5f, -0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[9]  = (plVec3){ 0.5f,  0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[10] = (plVec3){ 0.5f,  0.5f,  0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[11] = (plVec3){ 0.5f, -0.5f,  0.5f };

    ptMesh->atSubmeshes[0].ptVertexNormals[8]  = (plVec3){ 1.0f, 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[9]  = (plVec3){ 1.0f, 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[10] = (plVec3){ 1.0f, 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[11] = (plVec3){ 1.0f, 0.0f, 0.0f};

    ptMesh->atSubmeshes[0].puIndices[12] = 8;
    ptMesh->atSubmeshes[0].puIndices[13] = 9;
    ptMesh->atSubmeshes[0].puIndices[14] = 10;
    ptMesh->atSubmeshes[0].puIndices[15] = 8;
    ptMesh->atSubmeshes[0].puIndices[16] = 10;
    ptMesh->atSubmeshes[0].puIndices[17] = 11;

    // left (-x)
    ptMesh->atSubmeshes[0].ptVertexPositions[12] = (plVec3){ -0.5f, -0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[13] = (plVec3){ -0.5f,  0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[14] = (plVec3){ -0.5f,  0.5f,  0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[15] = (plVec3){ -0.5f, -0.5f,  0.5f };

    ptMesh->atSubmeshes[0].ptVertexNormals[12] = (plVec3){ -1.0f, 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[13] = (plVec3){ -1.0f, 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[14] = (plVec3){ -1.0f, 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[15] = (plVec3){ -1.0f, 0.0f, 0.0f};

    ptMesh->atSubmeshes[0].puIndices[18] = 14;
    ptMesh->atSubmeshes[0].puIndices[19] = 13;
    ptMesh->atSubmeshes[0].puIndices[20] = 12;
    ptMesh->atSubmeshes[0].puIndices[21] = 15;
    ptMesh->atSubmeshes[0].puIndices[22] = 14;
    ptMesh->atSubmeshes[0].puIndices[23] = 12;

    // top (+y)
    ptMesh->atSubmeshes[0].ptVertexPositions[16] = (plVec3){  0.5f,  0.5f,  0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[17] = (plVec3){  0.5f,  0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[18] = (plVec3){ -0.5f,  0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[19] = (plVec3){ -0.5f,  0.5f,  0.5f };

    ptMesh->atSubmeshes[0].ptVertexNormals[16] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[17] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[18] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[19] = (plVec3){ 0.0f, 1.0f, 0.0f};

    ptMesh->atSubmeshes[0].puIndices[24] = 16;
    ptMesh->atSubmeshes[0].puIndices[25] = 17;
    ptMesh->atSubmeshes[0].puIndices[26] = 18;
    ptMesh->atSubmeshes[0].puIndices[27] = 16;
    ptMesh->atSubmeshes[0].puIndices[28] = 18;
    ptMesh->atSubmeshes[0].puIndices[29] = 19;

    // bottom (-y)
    ptMesh->atSubmeshes[0].ptVertexPositions[20] = (plVec3){  0.5f, -0.5f,  0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[21] = (plVec3){  0.5f, -0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[22] = (plVec3){ -0.5f, -0.5f, -0.5f };
    ptMesh->atSubmeshes[0].ptVertexPositions[23] = (plVec3){ -0.5f, -0.5f,  0.5f };

    ptMesh->atSubmeshes[0].ptVertexNormals[20] = (plVec3){ 0.0f, -1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[21] = (plVec3){ 0.0f, -1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[22] = (plVec3){ 0.0f, -1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[23] = (plVec3){ 0.0f, -1.0f, 0.0f};

    ptMesh->atSubmeshes[0].puIndices[30] = 22;
    ptMesh->atSubmeshes[0].puIndices[31] = 21;
    ptMesh->atSubmeshes[0].puIndices[32] = 20;
    ptMesh->atSubmeshes[0].puIndices[33] = 23;
    ptMesh->atSubmeshes[0].puIndices[34] = 22;
    ptMesh->atSubmeshes[0].puIndices[35] = 20;

    ptMesh->tAABB.tMin = (plVec3){-0.5f, -0.5f, -0.5f};
    ptMesh->tAABB.tMax = (plVec3){0.5f, 0.5f, 0.5f};
}

void
pl_mesh_create_plane(plMesh* ptMesh)
{
    plSubmeshAllocationDesc tAllocDesc = {
        .uVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .szVertexCount = 4,
        .szIndexCount = 6
    };
    pl_mesh_allocate(ptMesh, &tAllocDesc, 1);

    ptMesh->atSubmeshes[0].ptVertexPositions[0] = (plVec3){-0.5f, 0.0f, -0.5f};
    ptMesh->atSubmeshes[0].ptVertexPositions[1] = (plVec3){-0.5f, 0.0f,  0.5f};
    ptMesh->atSubmeshes[0].ptVertexPositions[2] = (plVec3){ 0.5f, 0.0f,  0.5f};
    ptMesh->atSubmeshes[0].ptVertexPositions[3] = (plVec3){ 0.5f, 0.0f, -0.5f};
    
    ptMesh->atSubmeshes[0].ptVertexNormals[0] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[1] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[2] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexNormals[3] = (plVec3){ 0.0f, 1.0f, 0.0f};

    ptMesh->atSubmeshes[0].ptVertexTextureCoordinates[0][0] = (plVec2){ 0.0f, 0.0f};
    ptMesh->atSubmeshes[0].ptVertexTextureCoordinates[0][1] = (plVec2){ 0.0f, 1.0f};
    ptMesh->atSubmeshes[0].ptVertexTextureCoordinates[0][2] = (plVec2){ 1.0f, 1.0f};
    ptMesh->atSubmeshes[0].ptVertexTextureCoordinates[0][3] = (plVec2){ 1.0f, 0.0f};

    ptMesh->atSubmeshes[0].puIndices[0] = 0;
    ptMesh->atSubmeshes[0].puIndices[1] = 1;
    ptMesh->atSubmeshes[0].puIndices[2] = 2;
    ptMesh->atSubmeshes[0].puIndices[3] = 0;
    ptMesh->atSubmeshes[0].puIndices[4] = 2;
    ptMesh->atSubmeshes[0].puIndices[5] = 3;
    
    ptMesh->tAABB.tMin = (plVec3){-0.5f, -0.05f, -0.5f};
    ptMesh->tAABB.tMax = (plVec3){0.5f, 0.05f, 0.5f};
}

plMeshBuilder*
pl_mesh_builder_create(plMeshBuilderOptions tOptions)
{
    if(tOptions.fWeldRadius == 0.0f)
        tOptions.fWeldRadius = 0.001f;

    plMeshBuilder* ptBuilder = PL_ALLOC(sizeof(plMeshBuilder));
    memset(ptBuilder, 0, sizeof(plMeshBuilder));
    ptBuilder->tOptions = tOptions;
    return ptBuilder;
}

void
pl_mesh_builder_cleanup(plMeshBuilder* ptBuilder)
{
    pl_sb_free(ptBuilder->sbtTriangles);
    pl_sb_free(ptBuilder->sbtVertices);
    PL_FREE(ptBuilder);
}

void
pl_mesh_builder_add_triangle(plMeshBuilder* ptBuilder, plVec3 tA, plVec3 tB, plVec3 tC)
{
    plMeshBuilderTriangle tTriangle;
    tTriangle.uIndex0 = UINT32_MAX;
    tTriangle.uIndex1 = UINT32_MAX;
    tTriangle.uIndex2 = UINT32_MAX;

    const float fWeldRadiusSqr = ptBuilder->tOptions.fWeldRadius * ptBuilder->tOptions.fWeldRadius;

    const uint32_t uVertexCount = pl_sb_size(ptBuilder->sbtVertices);

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        const plVec3* ptVertex = &ptBuilder->sbtVertices[i];

        if(tTriangle.uIndex0 == UINT32_MAX)
        {
            float fDist = pl_length_sqr_vec3(pl_sub_vec3(*ptVertex, tA));

            if(fDist < fWeldRadiusSqr)
                tTriangle.uIndex0 = i;
        }

        if(tTriangle.uIndex1 == UINT32_MAX)
        {
            float fDist = pl_length_sqr_vec3(pl_sub_vec3(*ptVertex, tB));

            if(fDist < fWeldRadiusSqr)
                tTriangle.uIndex1 = i;
        }

        if(tTriangle.uIndex2 == UINT32_MAX)
        {
            float fDist = pl_length_sqr_vec3(pl_sub_vec3(*ptVertex, tC));

            if(fDist < fWeldRadiusSqr)
                tTriangle.uIndex2 = i;
        }

        if(tTriangle.uIndex0 != UINT32_MAX &&
           tTriangle.uIndex1 != UINT32_MAX &&
           tTriangle.uIndex2 != UINT32_MAX)
            break;
    }

    if(tTriangle.uIndex0 == UINT32_MAX)
    {
        tTriangle.uIndex0 = pl_sb_size(ptBuilder->sbtVertices);
        pl_sb_push(ptBuilder->sbtVertices, tA);
    }

    if(tTriangle.uIndex1 == UINT32_MAX)
    {
        tTriangle.uIndex1 = pl_sb_size(ptBuilder->sbtVertices);
        pl_sb_push(ptBuilder->sbtVertices, tB);
    }

    if(tTriangle.uIndex2 == UINT32_MAX)
    {
        tTriangle.uIndex2 = pl_sb_size(ptBuilder->sbtVertices);
        pl_sb_push(ptBuilder->sbtVertices, tC);
    }

    pl_sb_push(ptBuilder->sbtTriangles, tTriangle);
}

void
pl_mesh_builder_commit(plMeshBuilder* ptBuilder, uint32_t* puIndexBuffer, plVec3* ptVertexBuffer, uint32_t* puIndexBufferCountOut, uint32_t* puVertexBufferCountOut)
{
    const uint32_t uVertexCount = pl_sb_size(ptBuilder->sbtVertices);
    const uint32_t uTriangleCount = pl_sb_size(ptBuilder->sbtTriangles);
    
    if(puVertexBufferCountOut)
        *puVertexBufferCountOut = uVertexCount;

    if(puIndexBufferCountOut)
        *puIndexBufferCountOut = uTriangleCount * 3;

    if(puIndexBuffer && ptVertexBuffer)
    {
        memcpy(puIndexBuffer, ptBuilder->sbtTriangles, uTriangleCount * 3 * sizeof(uint32_t));
        memcpy(ptVertexBuffer, ptBuilder->sbtVertices, uVertexCount * sizeof(plVec3));
        pl_sb_reset(ptBuilder->sbtTriangles);
        pl_sb_reset(ptBuilder->sbtVertices);
    }
}

void
pl_mesh_builder_add_triangle_double(plMeshBuilder* ptBuilder, plVec3d tA, plVec3d tB, plVec3d tC)
{
    plMeshBuilderTriangle tTriangle;
    tTriangle.uIndex0 = UINT32_MAX;
    tTriangle.uIndex1 = UINT32_MAX;
    tTriangle.uIndex2 = UINT32_MAX;

    const double fWeldRadiusSqr = (double)(ptBuilder->tOptions.fWeldRadius * ptBuilder->tOptions.fWeldRadius);

    const uint32_t uVertexCount = pl_sb_size(ptBuilder->sbtVerticesD);

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        const plVec3d* ptVertex = &ptBuilder->sbtVerticesD[i];

        double fDist = pl_length_sqr_vec3_d(pl_sub_vec3_d(*ptVertex, tA));

        if(fDist < fWeldRadiusSqr)
        {
            tTriangle.uIndex0 = i;
            break;
        }
    }

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        const plVec3d* ptVertex = &ptBuilder->sbtVerticesD[i];

        double fDist = pl_length_sqr_vec3_d(pl_sub_vec3_d(*ptVertex, tB));

        if(fDist < fWeldRadiusSqr)
        {
            tTriangle.uIndex1 = i;
            break;
        }
    }

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        const plVec3d* ptVertex = &ptBuilder->sbtVerticesD[i];

        double fDist = pl_length_sqr_vec3_d(pl_sub_vec3_d(*ptVertex, tC));

        if(fDist < fWeldRadiusSqr)
        {
            tTriangle.uIndex2 = i;
            break;
        }
    }

    if(tTriangle.uIndex0 == UINT32_MAX)
    {
        tTriangle.uIndex0 = pl_sb_size(ptBuilder->sbtVerticesD);
        pl_sb_push(ptBuilder->sbtVerticesD, tA);
    }

    if(tTriangle.uIndex1 == UINT32_MAX)
    {
        tTriangle.uIndex1 = pl_sb_size(ptBuilder->sbtVerticesD);
        pl_sb_push(ptBuilder->sbtVerticesD, tB);
    }

    if(tTriangle.uIndex2 == UINT32_MAX)
    {
        tTriangle.uIndex2 = pl_sb_size(ptBuilder->sbtVerticesD);
        pl_sb_push(ptBuilder->sbtVerticesD, tC);
    }

    pl_sb_push(ptBuilder->sbtTriangles, tTriangle);
}

void
pl_mesh_builder_commit_double(plMeshBuilder* ptBuilder, uint32_t* puIndexBuffer, plVec3d* ptVertexBuffer, uint32_t* puIndexBufferCountOut, uint32_t* puVertexBufferCountOut)
{
    const uint32_t uVertexCount = pl_sb_size(ptBuilder->sbtVerticesD);
    const uint32_t uTriangleCount = pl_sb_size(ptBuilder->sbtTriangles);
    
    if(puVertexBufferCountOut)
        *puVertexBufferCountOut = uVertexCount;

    if(puIndexBufferCountOut)
        *puIndexBufferCountOut = uTriangleCount * 3;

    if(puIndexBuffer && ptVertexBuffer)
    {
        memcpy(puIndexBuffer, ptBuilder->sbtTriangles, uTriangleCount * 3 * sizeof(uint32_t));
        memcpy(ptVertexBuffer, ptBuilder->sbtVerticesD, uVertexCount * sizeof(plVec3d));
        pl_sb_reset(ptBuilder->sbtTriangles);
        pl_sb_reset(ptBuilder->sbtVerticesD);
    }
}

void
pl_mesh_register_asset_types(void)
{
    static const plAssetTypeDesc tDesc = {
        .pcName           = "Mesh",
        .pcFileExtension  = "plmesh",
        .eDefaultEncoding = PL_ASSET_ENCODING_BINARY,
        .szSize           = sizeof(plMesh),
        .serialize        = pl__mesh_serialize,
        .deserialize      = pl__mesh_deserialize,
        .cleanup          = pl__mesh_cleanup
    };
    gptMeshCtx->tAssetTypeKey = gptAsset->register_type(tDesc);
}

plAssetTypeKey
pl_mesh_get_asset_type_key(void)
{
    return gptMeshCtx->tAssetTypeKey;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_mesh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMeshI ptApi0 = {
        .register_asset_types  = pl_mesh_register_asset_types,
        .get_asset_type_key    = pl_mesh_get_asset_type_key,
        .create_sphere         = pl_mesh_create_sphere,
        .create_cube           = pl_mesh_create_cube,
        .create_plane          = pl_mesh_create_plane,
        .generate_normals      = pl_mesh_generate_normals,
        .generate_tangents     = pl_mesh_generate_tangents,
        .generate_colors       = pl_mesh_generate_colors,
        .generate_uvs          = pl_mesh_generate_uvs,
        .calculate_bounds      = pl_mesh_calculate_bounds,
        .allocate              = pl_mesh_allocate,
        .cleanup               = pl_mesh_cleanup,
        .generate_indices      = pl_mesh_generate_indices,
    };
    pl_set_api(ptApiRegistry, plMeshI, &ptApi0);

    const plMeshBuilderI ptApi1 = {
        .create              = pl_mesh_builder_create,
        .cleanup             = pl_mesh_builder_cleanup,
        .add_triangle        = pl_mesh_builder_add_triangle,
        .add_triangle_double = pl_mesh_builder_add_triangle_double,
        .commit              = pl_mesh_builder_commit,
        .commit_double       = pl_mesh_builder_commit_double,
    };
    pl_set_api(ptApiRegistry, plMeshBuilderI, &ptApi1);

    #ifndef PL_UNITY_BUILD
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptLog    = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptAsset  = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptJson  = pl_get_api_latest(ptApiRegistry, plJsonI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptMeshCtx = ptDataRegistry->get_data("plMeshContext");
    }
    else // first load
    {
        static plMeshContext tCtx = {0};
        gptMeshCtx = &tCtx;
        ptDataRegistry->set_data("plMeshContext", gptMeshCtx);
    }
}

void
pl_unload_mesh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plMeshI* ptApi0 = pl_get_api_latest(ptApiRegistry, plMeshI);
    ptApiRegistry->remove_api(ptApi0);

    const plMeshBuilderI* ptApi1 = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    ptApiRegistry->remove_api(ptApi1);
}