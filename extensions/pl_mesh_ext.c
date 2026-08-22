/*
   pl_ecs_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
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

    static const plLogI*   gptLog = NULL;
    static const plVfsI*   gptVfs = NULL;
    static const plAssetI* gptAsset = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMeshContext
{
    int a;
} plMeshContext;

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

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plMeshContext* gptMeshCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

typedef struct _plSubmeshHeader
{
    char acMaterialName[256];
} plSubmeshHeader;

typedef struct _plMeshFileHeader
{
    uint32_t uMagic;
    uint32_t uVersion;
    uint32_t uSubmeshCount;
    uint32_t uFlags;
    plAABB   tAABB;
    uint64_t uFileSize;
} plMeshFileHeader;

void
pl_mesh_serialize(const char* pcName, const plMesh* ptMesh)
{
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ_WRITE);

    plMeshFileHeader tHeader = {
        .uMagic = 0x48534D50, // "PMSH"
        .uVersion = 1,
        .tAABB = ptMesh->tAABB,
        .uSubmeshCount = ptMesh->uSubmeshCount,
        .uFlags = 0,

    };
    tHeader.uFileSize = sizeof(tHeader) + ptMesh->szRawDataSize + sizeof(plSubmeshHeader) * ptMesh->uSubmeshCount;
    gptVfs->write_file_stream(tFileHandle, 1, sizeof(tHeader), &tHeader);
    for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
    {
        const char* pcMaterialName = gptAsset->get_name(ptMesh->atSubmeshes[i].tMaterial);
        plSubmeshHeader tSubHeader = {0};
        
        if(pcMaterialName)
            strncpy(tSubHeader.acMaterialName, pcMaterialName, 256);

        gptVfs->write_file_stream(tFileHandle, 1, 256, &tSubHeader);
    }
    gptVfs->write_file_stream(tFileHandle, 1, ptMesh->szRawDataSize, ptMesh->puRawData);
    gptVfs->close_file(tFileHandle);
}

void
pl_mesh_deserialize(const char* pcName, plMesh* ptMesh)
{
    plVfsFileHandle tFileHandle = gptVfs->open_file(pcName, PL_VFS_FILE_MODE_READ);

    plMeshFileHeader tHeader = {0};
    gptVfs->read_file_stream(tFileHandle, sizeof(tHeader), 1, &tHeader);
    plMesh tDummyMesh = {0};
    size_t szMaterialTableSize = sizeof(plSubmeshHeader) * tHeader.uSubmeshCount;
    gptVfs->set_file_stream_position(tFileHandle, sizeof(tHeader) + szMaterialTableSize);
    void* pBuffer = PL_ALLOC(tHeader.uFileSize);
    memset(pBuffer, 0, tHeader.uFileSize);
    gptVfs->read_file_stream(tFileHandle, tHeader.uFileSize - sizeof(tHeader) - szMaterialTableSize, 1, pBuffer);
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

    pl_mesh_allocate(ptMesh, sbtSubAllocs, tDummyMesh.uSubmeshCount);

    pl_sb_free(sbtSubAllocs);

    ptMesh->tAABB = tHeader.tAABB;
    size_t szRawDataOffset = sizeof(plSubmesh) * (size_t)tDummyMesh.uSubmeshCount;
    uint8_t* pcFilingBuffer = pBuffer;
    memcpy(&ptMesh->puRawData[szRawDataOffset], &pcFilingBuffer[szRawDataOffset], ptMesh->szRawDataSize - szRawDataOffset);

    for(uint32_t i = 0; i < tDummyMesh.uSubmeshCount; i++)
    {
        gptVfs->set_file_stream_position(tFileHandle, sizeof(tHeader) + sizeof(plSubmeshHeader) * i);

        plSubmeshHeader tSubmesh = {0};
        gptVfs->read_file_stream(tFileHandle, sizeof(plSubmeshHeader), 1, &tSubmesh);
        ptMesh->atSubmeshes[i].tMaterial = gptAsset->load(tSubmesh.acMaterialName);
    }
    gptVfs->close_file(tFileHandle);
    PL_FREE(pBuffer);
}

void
pl_mesh_calculate_normals(plMesh* atMeshes, uint32_t uComponentCount)
{

    for(uint32_t uMeshIndex = 0; uMeshIndex < uComponentCount; uMeshIndex++)
    {
        plMesh* ptMesh = &atMeshes[uMeshIndex];

        for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
        {
            plSubmesh* ptSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];
            PL_ASSERT(ptSubmesh->ptVertexNormals);

            if(ptSubmesh->ptVertexNormals)
            {
                for(uint32_t i = 0; i < ptSubmesh->szIndexCount - 2; i += 3)
                {
                    const uint32_t uIndex0 = ptSubmesh->puIndices[i + 0];
                    const uint32_t uIndex1 = ptSubmesh->puIndices[i + 1];
                    const uint32_t uIndex2 = ptSubmesh->puIndices[i + 2];

                    const plVec3 tP0 = ptSubmesh->ptVertexPositions[uIndex0];
                    const plVec3 tP1 = ptSubmesh->ptVertexPositions[uIndex1];
                    const plVec3 tP2 = ptSubmesh->ptVertexPositions[uIndex2];

                    const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                    const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                    const plVec3 tNorm = pl_cross_vec3(tEdge1, tEdge2);

                    ptSubmesh->ptVertexNormals[uIndex0] = tNorm;
                    ptSubmesh->ptVertexNormals[uIndex1] = tNorm;
                    ptSubmesh->ptVertexNormals[uIndex2] = tNorm;
                }
            }
        }
    }
}

void
pl_mesh_calculate_tangents(plMesh* atMeshes, uint32_t uComponentCount)
{

    for(uint32_t uMeshIndex = 0; uMeshIndex < uComponentCount; uMeshIndex++)
    {
        plMesh* ptMesh = &atMeshes[uMeshIndex];

        for(uint32_t uSubmeshIndex = 0; uSubmeshIndex < ptMesh->uSubmeshCount; uSubmeshIndex++)
        {
            plSubmesh* ptSubmesh = &ptMesh->atSubmeshes[uSubmeshIndex];

            PL_ASSERT(ptSubmesh->ptVertexTangents);

            if(ptSubmesh->ptVertexTangents && ptSubmesh->ptVertexTextureCoordinates[0])
            {
                for(uint32_t i = 0; i < ptSubmesh->szIndexCount - 2; i += 3)
                {
                    const uint32_t uIndex0 = ptSubmesh->puIndices[i + 0];
                    const uint32_t uIndex1 = ptSubmesh->puIndices[i + 1];
                    const uint32_t uIndex2 = ptSubmesh->puIndices[i + 2];

                    const plVec3 tP0 = ptSubmesh->ptVertexPositions[uIndex0];
                    const plVec3 tP1 = ptSubmesh->ptVertexPositions[uIndex1];
                    const plVec3 tP2 = ptSubmesh->ptVertexPositions[uIndex2];

                    const plVec2 tTex0 = ptSubmesh->ptVertexTextureCoordinates[0][uIndex0];
                    const plVec2 tTex1 = ptSubmesh->ptVertexTextureCoordinates[0][uIndex1];
                    const plVec2 tTex2 = ptSubmesh->ptVertexTextureCoordinates[0][uIndex2];

                    const plVec3 atNormals[3] = { 
                        ptSubmesh->ptVertexNormals[uIndex0],
                        ptSubmesh->ptVertexNormals[uIndex1],
                        ptSubmesh->ptVertexNormals[uIndex2],
                    };

                    const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                    const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                    const float fDeltaU1 = tTex1.x - tTex0.x;
                    const float fDeltaV1 = tTex1.y - tTex0.y;
                    const float fDeltaU2 = tTex2.x - tTex0.x;
                    const float fDeltaV2 = tTex2.y - tTex0.y;

                    const float fSx = fDeltaU1;
                    const float fSy = fDeltaU2;
                    const float fTx = fDeltaV1;
                    const float fTy = fDeltaV2;
                    const float fHandedness = ((fSx * fTy - fTx * fSy) < 0.0f) ? -1.0f : 1.0f;

                    const plVec3 tTangent = {
                            fHandedness * (fDeltaV2 * tEdge1.x - fDeltaV1 * tEdge2.x),
                            fHandedness * (fDeltaV2 * tEdge1.y - fDeltaV1 * tEdge2.y),
                            fHandedness * (fDeltaV2 * tEdge1.z - fDeltaV1 * tEdge2.z)
                    };

                    plVec4 atFinalTangents[3] = {0};
                    for(uint32_t j = 0; j < 3; j++)
                    {
                        atFinalTangents[j].xyz = pl_mul_vec3(tTangent, atNormals[j]);
                        atFinalTangents[j].xyz = pl_mul_vec3(atNormals[j], atFinalTangents[j].xyz);
                        atFinalTangents[j].xyz = pl_norm_vec3(pl_sub_vec3(tTangent, atFinalTangents[j].xyz));
                        atFinalTangents[j].w = fHandedness;
                    }

                    ptSubmesh->ptVertexTangents[uIndex0] = atFinalTangents[0];
                    ptSubmesh->ptVertexTangents[uIndex1] = atFinalTangents[1];
                    ptSubmesh->ptVertexTangents[uIndex2] = atFinalTangents[2];
                } 
            }
        }
    }
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

        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)  szBytesPerVertex += sizeof(plVec3);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0) szBytesPerVertex += sizeof(plVec4);
        if(ptDesc->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1) szBytesPerVertex += sizeof(plVec4);
        
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

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)
        {
            ptSubmesh->ptVertexColors[0] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
            szBufferOffset += ptSubmesh->szVertexCount * sizeof(plVec4);
        }

        if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1)
        {
            ptSubmesh->ptVertexColors[1] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
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
pl_mesh_create_sphere(const char* pcName, float fRadius, uint32_t uLatitudeBands, uint32_t uLongitudeBands, plMesh* ptMesh)
{
    pcName = pcName ? pcName : "unnamed sphere mesh";

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
pl_mesh_create_cube(const char* pcName, plMesh* ptMesh)
{
    pcName = pcName ? pcName : "unnamed cube mesh";

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
pl_mesh_create_plane(const char* pcName, plMesh* ptMesh)
{
    pcName = pcName ? pcName : "unnamed plane mesh";

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

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_mesh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMeshI tApi = {
        .serialize             = pl_mesh_serialize,
        .deserialize           = pl_mesh_deserialize,
        .create_sphere         = pl_mesh_create_sphere,
        .create_cube           = pl_mesh_create_cube,
        .create_plane          = pl_mesh_create_plane,
        .calculate_normals     = pl_mesh_calculate_normals,
        .calculate_tangents    = pl_mesh_calculate_tangents,
        .allocate              = pl_mesh_allocate,
    };
    pl_set_api(ptApiRegistry, plMeshI, &tApi);

    const plMeshBuilderI tApi2 = {
        .create              = pl_mesh_builder_create,
        .cleanup             = pl_mesh_builder_cleanup,
        .add_triangle        = pl_mesh_builder_add_triangle,
        .add_triangle_double = pl_mesh_builder_add_triangle_double,
        .commit              = pl_mesh_builder_commit,
        .commit_double       = pl_mesh_builder_commit_double,
    };
    pl_set_api(ptApiRegistry, plMeshBuilderI, &tApi2);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptLog    = pl_get_api_latest(ptApiRegistry, plLogI);
    gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptAsset  = pl_get_api_latest(ptApiRegistry, plAssetI);

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

    const plMeshI* ptApi = pl_get_api_latest(ptApiRegistry, plMeshI);
    ptApiRegistry->remove_api(ptApi);

    const plMeshBuilderI* ptApi2 = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    ptApiRegistry->remove_api(ptApi2);
}
