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
#include "pl_ecs_ext.h"
#include "pl_mesh_ext.h"
#include "pl_math.h"

// extensions
#include "pl_log_ext.h"

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

    static const plLogI* gptLog = NULL;
    static const plEcsI* gptECS = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMeshContext
{
    plEcsTypeKey tMeshComponentType;
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
    plMeshBuilderTriangle* sbtTriangles;
} plMeshBuilder;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plMeshContext* gptMeshCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

plEcsTypeKey
pl_mesh_get_ecs_type_key_mesh(void)
{
    return gptMeshCtx->tMeshComponentType;
}

void
pl_calculate_normals(plMeshComponent* atMeshes, uint32_t uComponentCount)
{

    for(uint32_t uMeshIndex = 0; uMeshIndex < uComponentCount; uMeshIndex++)
    {
        plMeshComponent* ptMesh = &atMeshes[uMeshIndex];

        PL_ASSERT(ptMesh->ptVertexNormals);

        if(ptMesh->ptVertexNormals)
        {
            for(uint32_t i = 0; i < ptMesh->szIndexCount - 2; i += 3)
            {
                const uint32_t uIndex0 = ptMesh->puIndices[i + 0];
                const uint32_t uIndex1 = ptMesh->puIndices[i + 1];
                const uint32_t uIndex2 = ptMesh->puIndices[i + 2];

                const plVec3 tP0 = ptMesh->ptVertexPositions[uIndex0];
                const plVec3 tP1 = ptMesh->ptVertexPositions[uIndex1];
                const plVec3 tP2 = ptMesh->ptVertexPositions[uIndex2];

                const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                const plVec3 tNorm = pl_cross_vec3(tEdge1, tEdge2);

                ptMesh->ptVertexNormals[uIndex0] = tNorm;
                ptMesh->ptVertexNormals[uIndex1] = tNorm;
                ptMesh->ptVertexNormals[uIndex2] = tNorm;
            }
        }
    }
}

void
pl_calculate_tangents(plMeshComponent* atMeshes, uint32_t uComponentCount)
{

    for(uint32_t uMeshIndex = 0; uMeshIndex < uComponentCount; uMeshIndex++)
    {
        plMeshComponent* ptMesh = &atMeshes[uMeshIndex];

        PL_ASSERT(ptMesh->ptVertexTangents);

        if(ptMesh->ptVertexTangents && ptMesh->ptVertexTextureCoordinates[0])
        {
            for(uint32_t i = 0; i < ptMesh->szIndexCount - 2; i += 3)
            {
                const uint32_t uIndex0 = ptMesh->puIndices[i + 0];
                const uint32_t uIndex1 = ptMesh->puIndices[i + 1];
                const uint32_t uIndex2 = ptMesh->puIndices[i + 2];

                const plVec3 tP0 = ptMesh->ptVertexPositions[uIndex0];
                const plVec3 tP1 = ptMesh->ptVertexPositions[uIndex1];
                const plVec3 tP2 = ptMesh->ptVertexPositions[uIndex2];

                const plVec2 tTex0 = ptMesh->ptVertexTextureCoordinates[0][uIndex0];
                const plVec2 tTex1 = ptMesh->ptVertexTextureCoordinates[0][uIndex1];
                const plVec2 tTex2 = ptMesh->ptVertexTextureCoordinates[0][uIndex2];

                const plVec3 atNormals[3] = { 
                    ptMesh->ptVertexNormals[uIndex0],
                    ptMesh->ptVertexNormals[uIndex1],
                    ptMesh->ptVertexNormals[uIndex2],
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

                ptMesh->ptVertexTangents[uIndex0] = atFinalTangents[0];
                ptMesh->ptVertexTangents[uIndex1] = atFinalTangents[1];
                ptMesh->ptVertexTangents[uIndex2] = atFinalTangents[2];
            } 
        }
    }
}

void
pl_allocate_vertex_data(plMeshComponent* ptMesh, size_t szVertexCount, uint64_t uVertexStreamMask, size_t szIndexCount)
{
    ptMesh->ulVertexStreamMask = uVertexStreamMask;
    ptMesh->szVertexCount = szVertexCount;
    ptMesh->szIndexCount = szIndexCount;

    size_t szBytesPerVertex = sizeof(plVec3);

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)  szBytesPerVertex += sizeof(plVec3);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0) szBytesPerVertex += sizeof(plVec4);
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1) szBytesPerVertex += sizeof(plVec4);

    ptMesh->puRawData = PL_ALLOC(szBytesPerVertex * szVertexCount + szIndexCount * sizeof(uint32_t));
    memset(ptMesh->puRawData, 0, szBytesPerVertex * szVertexCount + szIndexCount * sizeof(uint32_t));

    size_t szBufferOffset = 0;
    ptMesh->ptVertexPositions = (plVec3*)ptMesh->puRawData;
    szBufferOffset += szVertexCount * sizeof(plVec3);
    
    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
    {
        ptMesh->ptVertexNormals = (plVec3*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec3);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT)
    {
        ptMesh->ptVertexTangents = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)
    {
        ptMesh->ptVertexTextureCoordinates[0] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);

        ptMesh->ptVertexTextureCoordinates[1] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)
    {
        ptMesh->ptVertexTextureCoordinates[2] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);

        ptMesh->ptVertexTextureCoordinates[3] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2)
    {
        ptMesh->ptVertexTextureCoordinates[4] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);

        ptMesh->ptVertexTextureCoordinates[5] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3)
    {
        ptMesh->ptVertexTextureCoordinates[6] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);

        ptMesh->ptVertexTextureCoordinates[7] = (plVec2*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec2);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)
    {
        ptMesh->ptVertexColors[0] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1)
    {
        ptMesh->ptVertexColors[1] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0)
    {
        ptMesh->ptVertexJoints[0] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1)
    {
        ptMesh->ptVertexJoints[1] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)
    {
        ptMesh->ptVertexWeights[0] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)
    {
        ptMesh->ptVertexWeights[1] = (plVec4*)&ptMesh->puRawData[szBufferOffset];
        szBufferOffset += szVertexCount * sizeof(plVec4);
    }

    if(szIndexCount > 0)
    {
        ptMesh->puIndices = (uint32_t*)&ptMesh->puRawData[szBufferOffset];
    }
}

static void
pl__mesh_cleanup(plComponentLibrary* ptLibrary)
{
    plMeshComponent* ptComponents = NULL;
    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptMeshCtx->tMeshComponentType, (void**)&ptComponents, NULL);
    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        PL_FREE(ptComponents[i].puRawData);
        ptComponents[i].puRawData = NULL;
    }
}

plEntity
pl_ecs_create_mesh(plComponentLibrary* ptLibrary, const char* pcName, plMeshComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed mesh";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created mesh: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plMeshComponent* ptCompOut = gptECS->add_component(ptLibrary, gptMeshCtx->tMeshComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptCompOut;
    return tNewEntity;
}

plEntity
pl_ecs_create_sphere_mesh(plComponentLibrary* ptLibrary, const char* pcName, float fRadius, uint32_t uLatitudeBands, uint32_t uLongitudeBands, plMeshComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed sphere mesh";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created sphere mesh: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plMeshComponent* ptMesh = gptECS->add_component(ptLibrary, gptMeshCtx->tMeshComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptMesh;


    if(uLatitudeBands == 0)
        uLatitudeBands = 64;

    if(uLongitudeBands == 0)
    uLongitudeBands = 64;

    pl_allocate_vertex_data(ptMesh, (uLatitudeBands + 1) * (uLongitudeBands + 1), PL_MESH_FORMAT_FLAG_HAS_NORMAL, uLatitudeBands * uLongitudeBands * 6);

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
            ptMesh->ptVertexPositions[uCurrentPoint] = (plVec3){
                fCosPhi * fSinTheta * fRadius,
                fCosTheta * fRadius,
                fSinPhi * fSinTheta * fRadius
            };
            ptMesh->ptVertexNormals[uCurrentPoint] = pl_norm_vec3(ptMesh->ptVertexPositions[uCurrentPoint]);
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

            ptMesh->puIndices[uCurrentPoint + 0] = uFirst + 1;
            ptMesh->puIndices[uCurrentPoint + 1] = uSecond;
            ptMesh->puIndices[uCurrentPoint + 2] = uFirst;

            ptMesh->puIndices[uCurrentPoint + 3] = uFirst + 1;
            ptMesh->puIndices[uCurrentPoint + 4] = uSecond + 1;
            ptMesh->puIndices[uCurrentPoint + 5] = uSecond;

            uCurrentPoint += 6;
        }
    }
    ptMesh->tAABB.tMin = (plVec3){-fRadius, -fRadius, -fRadius};
    ptMesh->tAABB.tMax = (plVec3){fRadius, fRadius, fRadius};
    return tNewEntity;
}

plEntity
pl_ecs_create_cube_mesh(plComponentLibrary* ptLibrary, const char* pcName, plMeshComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed cube mesh";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created cube mesh: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plMeshComponent* ptMesh = gptECS->add_component(ptLibrary, gptMeshCtx->tMeshComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptMesh;

    pl_allocate_vertex_data(ptMesh, 4 * 6, PL_MESH_FORMAT_FLAG_HAS_NORMAL, 6 * 6);

    // front (+z)
    ptMesh->ptVertexPositions[0] = (plVec3){  0.5f, -0.5f, 0.5f };
    ptMesh->ptVertexPositions[1] = (plVec3){  0.5f,  0.5f, 0.5f };
    ptMesh->ptVertexPositions[2] = (plVec3){ -0.5f,  0.5f, 0.5f };
    ptMesh->ptVertexPositions[3] = (plVec3){ -0.5f, -0.5f, 0.5f };

    ptMesh->ptVertexNormals[0] = (plVec3){ 0.0f, 0.0f, 1.0f};
    ptMesh->ptVertexNormals[1] = (plVec3){ 0.0f, 0.0f, 1.0f};
    ptMesh->ptVertexNormals[2] = (plVec3){ 0.0f, 0.0f, 1.0f};
    ptMesh->ptVertexNormals[3] = (plVec3){ 0.0f, 0.0f, 1.0f};

    ptMesh->puIndices[0] = 0;
    ptMesh->puIndices[1] = 1;
    ptMesh->puIndices[2] = 2;
    ptMesh->puIndices[3] = 0;
    ptMesh->puIndices[4] = 2;
    ptMesh->puIndices[5] = 3;

    // back (-z)
    ptMesh->ptVertexPositions[4] = (plVec3){  0.5f, -0.5f, -0.5f };
    ptMesh->ptVertexPositions[5] = (plVec3){  0.5f,  0.5f, -0.5f };
    ptMesh->ptVertexPositions[6] = (plVec3){ -0.5f,  0.5f, -0.5f };
    ptMesh->ptVertexPositions[7] = (plVec3){ -0.5f, -0.5f, -0.5f };

    ptMesh->ptVertexNormals[4] = (plVec3){ 0.0f, 0.0f, -1.0f};
    ptMesh->ptVertexNormals[5] = (plVec3){ 0.0f, 0.0f, -1.0f};
    ptMesh->ptVertexNormals[6] = (plVec3){ 0.0f, 0.0f, -1.0f};
    ptMesh->ptVertexNormals[7] = (plVec3){ 0.0f, 0.0f, -1.0f};

    ptMesh->puIndices[6] = 6;
    ptMesh->puIndices[7] = 5;
    ptMesh->puIndices[8] = 4;
    ptMesh->puIndices[9] = 7;
    ptMesh->puIndices[10] = 6;
    ptMesh->puIndices[11] = 4;

    // right (+x)
    ptMesh->ptVertexPositions[8]  = (plVec3){ 0.5f, -0.5f, -0.5f };
    ptMesh->ptVertexPositions[9]  = (plVec3){ 0.5f,  0.5f, -0.5f };
    ptMesh->ptVertexPositions[10] = (plVec3){ 0.5f,  0.5f,  0.5f };
    ptMesh->ptVertexPositions[11] = (plVec3){ 0.5f, -0.5f,  0.5f };

    ptMesh->ptVertexNormals[8]  = (plVec3){ 1.0f, 0.0f, 0.0f};
    ptMesh->ptVertexNormals[9]  = (plVec3){ 1.0f, 0.0f, 0.0f};
    ptMesh->ptVertexNormals[10] = (plVec3){ 1.0f, 0.0f, 0.0f};
    ptMesh->ptVertexNormals[11] = (plVec3){ 1.0f, 0.0f, 0.0f};

    ptMesh->puIndices[12] = 8;
    ptMesh->puIndices[13] = 9;
    ptMesh->puIndices[14] = 10;
    ptMesh->puIndices[15] = 8;
    ptMesh->puIndices[16] = 10;
    ptMesh->puIndices[17] = 11;

    // left (-x)
    ptMesh->ptVertexPositions[12] = (plVec3){ -0.5f, -0.5f, -0.5f };
    ptMesh->ptVertexPositions[13] = (plVec3){ -0.5f,  0.5f, -0.5f };
    ptMesh->ptVertexPositions[14] = (plVec3){ -0.5f,  0.5f,  0.5f };
    ptMesh->ptVertexPositions[15] = (plVec3){ -0.5f, -0.5f,  0.5f };

    ptMesh->ptVertexNormals[12] = (plVec3){ -1.0f, 0.0f, 0.0f};
    ptMesh->ptVertexNormals[13] = (plVec3){ -1.0f, 0.0f, 0.0f};
    ptMesh->ptVertexNormals[14] = (plVec3){ -1.0f, 0.0f, 0.0f};
    ptMesh->ptVertexNormals[15] = (plVec3){ -1.0f, 0.0f, 0.0f};

    ptMesh->puIndices[18] = 14;
    ptMesh->puIndices[19] = 13;
    ptMesh->puIndices[20] = 12;
    ptMesh->puIndices[21] = 15;
    ptMesh->puIndices[22] = 14;
    ptMesh->puIndices[23] = 12;

    // top (+y)
    ptMesh->ptVertexPositions[16] = (plVec3){  0.5f,  0.5f,  0.5f };
    ptMesh->ptVertexPositions[17] = (plVec3){  0.5f,  0.5f, -0.5f };
    ptMesh->ptVertexPositions[18] = (plVec3){ -0.5f,  0.5f, -0.5f };
    ptMesh->ptVertexPositions[19] = (plVec3){ -0.5f,  0.5f,  0.5f };

    ptMesh->ptVertexNormals[16] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->ptVertexNormals[17] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->ptVertexNormals[18] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->ptVertexNormals[19] = (plVec3){ 0.0f, 1.0f, 0.0f};

    ptMesh->puIndices[24] = 16;
    ptMesh->puIndices[25] = 17;
    ptMesh->puIndices[26] = 18;
    ptMesh->puIndices[27] = 16;
    ptMesh->puIndices[28] = 18;
    ptMesh->puIndices[29] = 19;

    // bottom (-y)
    ptMesh->ptVertexPositions[20] = (plVec3){  0.5f, -0.5f,  0.5f };
    ptMesh->ptVertexPositions[21] = (plVec3){  0.5f, -0.5f, -0.5f };
    ptMesh->ptVertexPositions[22] = (plVec3){ -0.5f, -0.5f, -0.5f };
    ptMesh->ptVertexPositions[23] = (plVec3){ -0.5f, -0.5f,  0.5f };

    ptMesh->ptVertexNormals[20] = (plVec3){ 0.0f, -1.0f, 0.0f};
    ptMesh->ptVertexNormals[21] = (plVec3){ 0.0f, -1.0f, 0.0f};
    ptMesh->ptVertexNormals[22] = (plVec3){ 0.0f, -1.0f, 0.0f};
    ptMesh->ptVertexNormals[23] = (plVec3){ 0.0f, -1.0f, 0.0f};

    ptMesh->puIndices[30] = 22;
    ptMesh->puIndices[31] = 21;
    ptMesh->puIndices[32] = 20;
    ptMesh->puIndices[33] = 23;
    ptMesh->puIndices[34] = 22;
    ptMesh->puIndices[35] = 20;

    ptMesh->tAABB.tMin = (plVec3){-0.5f, -0.5f, -0.5f};
    ptMesh->tAABB.tMax = (plVec3){0.5f, 0.5f, 0.5f};
    return tNewEntity;
}

plEntity
pl_ecs_create_plane_mesh(plComponentLibrary* ptLibrary, const char* pcName, plMeshComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed plane mesh";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created plane mesh: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);
    plMeshComponent* ptMesh = gptECS->add_component(ptLibrary, gptMeshCtx->tMeshComponentType, tNewEntity);

    if(pptCompOut)
        *pptCompOut = ptMesh;

    pl_allocate_vertex_data(ptMesh, 4, PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0, 6);

    ptMesh->ptVertexPositions[0] = (plVec3){-0.5f, 0.0f, -0.5f};
    ptMesh->ptVertexPositions[1] = (plVec3){-0.5f, 0.0f,  0.5f};
    ptMesh->ptVertexPositions[2] = (plVec3){ 0.5f, 0.0f,  0.5f};
    ptMesh->ptVertexPositions[3] = (plVec3){ 0.5f, 0.0f, -0.5f};
    
    ptMesh->ptVertexNormals[0] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->ptVertexNormals[1] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->ptVertexNormals[2] = (plVec3){ 0.0f, 1.0f, 0.0f};
    ptMesh->ptVertexNormals[3] = (plVec3){ 0.0f, 1.0f, 0.0f};

    ptMesh->ptVertexTextureCoordinates[0][0] = (plVec2){ 0.0f, 0.0f};
    ptMesh->ptVertexTextureCoordinates[0][1] = (plVec2){ 0.0f, 1.0f};
    ptMesh->ptVertexTextureCoordinates[0][2] = (plVec2){ 1.0f, 1.0f};
    ptMesh->ptVertexTextureCoordinates[0][3] = (plVec2){ 1.0f, 0.0f};

    ptMesh->puIndices[0] = 0;
    ptMesh->puIndices[1] = 1;
    ptMesh->puIndices[2] = 2;
    ptMesh->puIndices[3] = 0;
    ptMesh->puIndices[4] = 2;
    ptMesh->puIndices[5] = 3;
    
    ptMesh->tAABB.tMin = (plVec3){-0.5f, -0.05f, -0.5f};
    ptMesh->tAABB.tMax = (plVec3){0.5f, 0.05f, 0.5f};
    return tNewEntity;
}

void
pl_mesh_register_system(void)
{

    const plComponentDesc tMeshDesc = {
        .pcName = "Mesh",
        .szSize = sizeof(plMeshComponent),
        .cleanup = pl__mesh_cleanup,
        .reset = pl__mesh_cleanup,
    };

    static const plMeshComponent tMeshComponentDefault = {
        .tSkinComponent = {UINT32_MAX, UINT32_MAX}
    };
    gptMeshCtx->tMeshComponentType = gptECS->register_type(tMeshDesc, &tMeshComponentDefault);
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

        float fDist = pl_length_sqr_vec3(pl_sub_vec3(*ptVertex, tA));

        if(fDist < fWeldRadiusSqr)
        {
            tTriangle.uIndex0 = i;
            break;
        }
    }

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        const plVec3* ptVertex = &ptBuilder->sbtVertices[i];

        float fDist = pl_length_sqr_vec3(pl_sub_vec3(*ptVertex, tB));

        if(fDist < fWeldRadiusSqr)
        {
            tTriangle.uIndex1 = i;
            break;
        }
    }

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        const plVec3* ptVertex = &ptBuilder->sbtVertices[i];

        float fDist = pl_length_sqr_vec3(pl_sub_vec3(*ptVertex, tC));

        if(fDist < fWeldRadiusSqr)
        {
            tTriangle.uIndex2 = i;
            break;
        }
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


//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_mesh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plMeshI tApi = {
        .register_ecs_system   = pl_mesh_register_system,
        .create_mesh           = pl_ecs_create_mesh,
        .create_sphere_mesh    = pl_ecs_create_sphere_mesh,
        .create_cube_mesh      = pl_ecs_create_cube_mesh,
        .create_plane_mesh     = pl_ecs_create_plane_mesh,
        .calculate_normals     = pl_calculate_normals,
        .calculate_tangents    = pl_calculate_tangents,
        .allocate_vertex_data  = pl_allocate_vertex_data,
        .get_ecs_type_key_mesh = pl_mesh_get_ecs_type_key_mesh,
    };
    pl_set_api(ptApiRegistry, plMeshI, &tApi);

    const plMeshBuilderI tApi2 = {
        .create       = pl_mesh_builder_create,
        .cleanup      = pl_mesh_builder_cleanup,
        .add_triangle = pl_mesh_builder_add_triangle,
        .commit       = pl_mesh_builder_commit,
    };
    pl_set_api(ptApiRegistry, plMeshBuilderI, &tApi2);

    gptECS    = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptLog    = pl_get_api_latest(ptApiRegistry, plLogI);

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

static void
pl_unload_mesh_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plMeshI* ptApi = pl_get_api_latest(ptApiRegistry, plMeshI);
    ptApiRegistry->remove_api(ptApi);

    const plMeshBuilderI* ptApi2 = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    ptApiRegistry->remove_api(ptApi2);
}