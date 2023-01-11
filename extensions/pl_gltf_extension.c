/*
   pl_gltf_extension.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_gltf_extension.h"
#include "pl_ds.h"
#include "pl_math.h"
#include "stb_image.h"
#include "pl_memory.h"
#include "pl_graphics_vulkan.h"
#include "pl_renderer.h"
#include "pl_string.h"
#include "pilotlight.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void pl__load_gltf_material(plResourceManager* ptResourceManager, const char* pcPath, const cgltf_material* ptMaterial, plMaterial* ptMaterialOut);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_ext_load_gltf(plRenderer* ptRenderer, const char* pcPath, plGltf* ptGltfOut)
{
    size_t szTotalOffset = pl_sb_size(ptRenderer->sbfStorageBuffer) / 4;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    char acFileName[1024] = {0};
    pl_str_get_file_name_only(pcPath, acFileName);

    cgltf_options tGltfOptions = {0};
    cgltf_data* ptGltfData = NULL;

    cgltf_result tGltfResult = cgltf_parse_file(&tGltfOptions, pcPath, &ptGltfData);

    if(tGltfResult != cgltf_result_success)
        return false;

    ptGltfOut->pcPath = pcPath;

    // reserve enough space for all meshes
    uint32_t uMeshCount = 0;
    for(size_t i = 0; i < ptGltfData->meshes_count; i++)
        uMeshCount += (uint32_t)ptGltfData->meshes[i].primitives_count;
    pl_sb_resize(ptGltfOut->sbtMeshes, uMeshCount);
    pl_sb_resize(ptGltfOut->sbuVertexOffsets, uMeshCount);
    

    cgltf_material** sbtMaterialBuffer = NULL;
    pl_sb_resize(ptGltfOut->sbtMaterials, (uint32_t)ptGltfData->materials_count);
    for(size_t i = 0; i < ptGltfData->materials_count; i++)
    {
        pl_sb_push(sbtMaterialBuffer, &ptGltfData->materials[i]);

        char acMaterialName[PL_MATERIAL_MAX_NAME_LENGTH] = {0};
        char acMaterialIndex[64] = {0};
        pl_sprintf(acMaterialIndex, "_mesh_%zu", i);
        if(ptGltfData->materials[i].name == NULL)
            pl_str_concatenate(acFileName, acMaterialIndex, acMaterialName, PL_MATERIAL_MAX_NAME_LENGTH);
        else
            strncpy(acMaterialName, ptGltfData->materials[i].name, PL_MATERIAL_MAX_NAME_LENGTH);
        pl_initialize_material(&ptGltfOut->sbtMaterials[i], acMaterialName);
        pl__load_gltf_material(&ptGraphics->tResourceManager, pcPath, &ptGltfData->materials[i], &ptGltfOut->sbtMaterials[i]);
    }

    // reserve enough space for all materials
    pl_sb_resize(ptGltfOut->sbuMaterialIndices, uMeshCount);

    float* sbfVertexBuffer = NULL;
    uint32_t* sbuIndexBuffer = NULL;
    pl_sb_reserve(sbfVertexBuffer, 1000);
    pl_sb_reserve(sbuIndexBuffer, 1000);

    tGltfResult = cgltf_load_buffers(&tGltfOptions, ptGltfData, pcPath);

    if(tGltfResult != cgltf_result_success)
    {
        pl_sb_free(sbfVertexBuffer);
        pl_sb_free(sbuIndexBuffer);
        pl_sb_free(ptGltfOut->sbtMaterials);
        pl_sb_free(ptGltfOut->sbtMeshes);
        return false;
    }

    uint32_t uCurrentMesh = 0;
    for(size_t szMeshIndex = 0; szMeshIndex < ptGltfData->meshes_count; szMeshIndex++)
    {
        const cgltf_mesh* ptMesh = &ptGltfData->meshes[szMeshIndex];
        for(size_t szPrimitiveIndex = 0; szPrimitiveIndex < ptMesh->primitives_count; szPrimitiveIndex++)
        {
            const cgltf_primitive* ptPrimitive = &ptMesh->primitives[szPrimitiveIndex];
            const size_t szVertexCount = ptPrimitive->attributes[0].data->count;
            uint32_t uAttributeComponents = 0u;
            uint32_t uExtraAttributeComponents = 0u;
            plMeshFormatFlags tVertexBufferFlags = 0;

            unsigned char* pucPosBufferStart     = NULL;
            unsigned char* pucNormalBufferStart  = NULL;
            unsigned char* pucTextBufferStart    = NULL;
            unsigned char* pucTangentBufferStart = NULL;
            unsigned char* pucColorBufferStart   = NULL;
            unsigned char* pucJointsBufferStart  = NULL;
            unsigned char* pucWeightsBufferStart = NULL;

            size_t szPosBufferStride     = 0;
            size_t szNormalBufferStride  = 0;
            size_t szTextBufferStride    = 0;
            size_t szTangentBufferStride = 0;
            size_t szColorBufferStride   = 0;
            size_t szJointsBufferStride  = 0;
            size_t szWeightsBufferStride = 0;

            for(size_t szAttributeIndex = 0; szAttributeIndex < ptPrimitive->attributes_count; szAttributeIndex++)
            {
                const cgltf_attribute* ptAttribute = &ptPrimitive->attributes[szAttributeIndex];
                const cgltf_buffer* ptBuffer = ptAttribute->data->buffer_view->buffer;
                const size_t szStride = ptAttribute->data->buffer_view->stride;

                unsigned char* pucBufferStart = &((unsigned char*)ptBuffer->data)[ptAttribute->data->buffer_view->offset + ptAttribute->data->offset];
                switch(ptAttribute->type)
                {
                    case cgltf_attribute_type_position: pucPosBufferStart     = pucBufferStart; uAttributeComponents      += 3; szPosBufferStride     = szStride == 0 ? sizeof(float) * 3 : szStride; break;
                    case cgltf_attribute_type_normal:   pucNormalBufferStart  = pucBufferStart; uExtraAttributeComponents += 4; szNormalBufferStride  = szStride == 0 ? sizeof(float) * 3 : szStride; break;
                    case cgltf_attribute_type_tangent:  pucTangentBufferStart = pucBufferStart; uExtraAttributeComponents += 4; szTangentBufferStride = szStride == 0 ? sizeof(float) * 4 : szStride; break;
                    case cgltf_attribute_type_texcoord: pucTextBufferStart    = pucBufferStart; uExtraAttributeComponents += 4; szTextBufferStride    = szStride == 0 ? sizeof(float) * 2 : szStride; break;
                    case cgltf_attribute_type_color:    pucColorBufferStart   = pucBufferStart; uExtraAttributeComponents += 4; szColorBufferStride   = szStride == 0 ? sizeof(float) * 4 : szStride; break;
                    case cgltf_attribute_type_joints:   pucJointsBufferStart  = pucBufferStart; uExtraAttributeComponents += 4; szJointsBufferStride  = szStride == 0 ? sizeof(float) * 4 : szStride; break;
                    case cgltf_attribute_type_weights:  pucWeightsBufferStart = pucBufferStart; uExtraAttributeComponents += 4; szWeightsBufferStride = szStride == 0 ? sizeof(float) * 4 : szStride; break;
                    default:
                        PL_ASSERT(false && "unknown gltf attribute type");
                        break;
                }
            }

            if(pucTangentBufferStart == NULL)
            {
                uExtraAttributeComponents += 4;
                szTangentBufferStride += sizeof(float) * 4;
            }

            if(pucNormalBufferStart == NULL)
            {
                uExtraAttributeComponents += 4;
                szNormalBufferStride += sizeof(float) * 3;
            }

            // allocate CPU buffers
            pl_sb_resize(sbfVertexBuffer, uAttributeComponents * (uint32_t)szVertexCount);
            pl_sb_resize(sbuIndexBuffer, (uint32_t)ptPrimitive->indices->count);

            const uint32_t uStartPoint = pl_sb_size(ptRenderer->sbfStorageBuffer);
            pl_sb_add_n(ptRenderer->sbfStorageBuffer, uExtraAttributeComponents * (uint32_t)szVertexCount);
            uint32_t uCurrentAttributeOffset = 0;

            // index buffer
            unsigned char* pucIdexBufferStart = &((unsigned char*)ptPrimitive->indices->buffer_view->buffer->data)[ptPrimitive->indices->buffer_view->offset + ptPrimitive->indices->offset];
            if(ptPrimitive->indices->component_type == cgltf_component_type_r_32u)
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        sbuIndexBuffer[i] = *(uint32_t*)&pucIdexBufferStart[i * sizeof(uint32_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        sbuIndexBuffer[i] = *(uint32_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
            }
            else if(ptPrimitive->indices->component_type == cgltf_component_type_r_16u)
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        sbuIndexBuffer[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * sizeof(unsigned short)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        sbuIndexBuffer[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
            }
            else if(ptPrimitive->indices->component_type == cgltf_component_type_r_8u)
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        sbuIndexBuffer[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * sizeof(uint8_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        sbuIndexBuffer[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
            }
            else
            {
                PL_ASSERT(false);
            }

            if(szPosBufferStride)
            {
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    const float x = *(float*)&pucPosBufferStart[i * szPosBufferStride];
                    const float y = ((float*)&pucPosBufferStart[i * szPosBufferStride])[1];
                    const float z = ((float*)&pucPosBufferStart[i * szPosBufferStride])[2];

                    sbfVertexBuffer[i * 3]   = x;
                    sbfVertexBuffer[i * 3 + 1] = y;
                    sbfVertexBuffer[i * 3 + 2] = z;
                }
            }

            // normals
            tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_NORMAL;
            if(pucNormalBufferStart)
            {  
                for(size_t i = 0; i < szVertexCount; i++)
                {

                    const float nx = *(float*)&pucNormalBufferStart[i * szNormalBufferStride];
                    const float ny = ((float*)&pucNormalBufferStart[i * szNormalBufferStride])[1];
                    const float nz = ((float*)&pucNormalBufferStart[i * szNormalBufferStride])[2];

                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset]     = nx;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = ny;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = nz;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = 0.0f;
                }
                uCurrentAttributeOffset += 4;
            }
            else // calculate normals
            {
                for(size_t i = 0; i < ptPrimitive->indices->count - 2; i+= 3)
                {
                    const uint32_t uIndex0 = sbuIndexBuffer[i + 0];
                    const uint32_t uIndex1 = sbuIndexBuffer[i + 1];
                    const uint32_t uIndex2 = sbuIndexBuffer[i + 2];

                    const plVec3 tP0 = {
                        sbfVertexBuffer[uIndex0 * 3],
                        sbfVertexBuffer[uIndex0 * 3 + 1],
                        sbfVertexBuffer[uIndex0 * 3 + 2],
                    };

                    const plVec3 tP1 = {
                        sbfVertexBuffer[uIndex1 * 3],
                        sbfVertexBuffer[uIndex1 * 3 + 1],
                        sbfVertexBuffer[uIndex1 * 3 + 2],
                    };

                    const plVec3 tP2 = {
                        sbfVertexBuffer[uIndex2 * 3],
                        sbfVertexBuffer[uIndex2 * 3 + 1],
                        sbfVertexBuffer[uIndex2 * 3 + 2],
                    };

                    const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                    const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                    const plVec3 tNorm = pl_norm_vec3(pl_cross_vec3(tEdge1, tEdge2));

                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 0] = tNorm.x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = tNorm.y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tNorm.z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = 0.0f;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 0] = tNorm.x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = tNorm.y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tNorm.z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = 0.0f;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 0] = tNorm.x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = tNorm.y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tNorm.z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = 0.0f;      
                } 
                uCurrentAttributeOffset += 4;
            }

            // tangents
            if(pucTangentBufferStart)
            {
                tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_TANGENT;
                for(size_t i = 0; i < szVertexCount; i++)
                {

                    const float tx = *(float*)&pucTangentBufferStart[i * szTangentBufferStride];
                    const float ty = ((float*)&pucTangentBufferStart[i * szTangentBufferStride])[1];
                    const float tz = ((float*)&pucTangentBufferStart[i * szTangentBufferStride])[2]; 
                    const float tw = ((float*)&pucTangentBufferStart[i * szTangentBufferStride])[3]; 

                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset]     = tx;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = ty;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tz;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = tw;
                }
                uCurrentAttributeOffset += 4;
            }
            else if(pucTextBufferStart) // calculate tangents
            {
                tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_TANGENT;
                for(size_t i = 0; i < ptPrimitive->indices->count - 2; i+= 3)
                {
                    const uint32_t uIndex0 = sbuIndexBuffer[i + 0];
                    const uint32_t uIndex1 = sbuIndexBuffer[i + 1];
                    const uint32_t uIndex2 = sbuIndexBuffer[i + 2];

                    const plVec3 tP0 = { sbfVertexBuffer[uIndex0 * 3], sbfVertexBuffer[uIndex0 * 3 + 1], sbfVertexBuffer[uIndex0 * 3 + 2]};
                    const plVec3 tP1 = { sbfVertexBuffer[uIndex1 * 3], sbfVertexBuffer[uIndex1 * 3 + 1], sbfVertexBuffer[uIndex1 * 3 + 2]};
                    const plVec3 tP2 = { sbfVertexBuffer[uIndex2 * 3], sbfVertexBuffer[uIndex2 * 3 + 1], sbfVertexBuffer[uIndex2 * 3 + 2]};

                    const plVec3 tN0 = { 
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 0],
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 1],
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 2]
                    };

                    const plVec3 tN1 = { 
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 0],
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 1],
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 2]
                    };

                    const plVec3 tN2 = { 
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 0],
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 1],
                        ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset - 4 + 2]
                    };

                    const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
                    const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

                    const plVec2 tTex0 = {
                        ((float*)&pucTextBufferStart[uIndex0 * szTextBufferStride])[0],
                        ((float*)&pucTextBufferStart[uIndex0 * szTextBufferStride])[1]
                    };

                    const plVec2 tTex1 = {
                        ((float*)&pucTextBufferStart[uIndex1 * szTextBufferStride])[0],
                        ((float*)&pucTextBufferStart[uIndex1 * szTextBufferStride])[1]
                    };

                    const plVec2 tTex2 = {
                        ((float*)&pucTextBufferStart[uIndex2 * szTextBufferStride])[0],
                        ((float*)&pucTextBufferStart[uIndex2 * szTextBufferStride])[1]
                    };
                    
                    const float fDeltaU1 = tTex1.x - tTex0.x;
                    const float fDeltaV1 = tTex1.y - tTex0.y;
                    const float fDeltaU2 = tTex2.x - tTex0.x;
                    const float fDeltaV2 = tTex2.y - tTex0.y;

                    const float fDividend = (fDeltaU1 * fDeltaV2 - fDeltaU2 * fDeltaV1);
                    const float fC = 1.0f / fDividend;

                    const float fSx = fDeltaU1;
                    const float fSy = fDeltaU2;
                    const float fTx = fDeltaV1;
                    const float fTy = fDeltaV2;
                    const float fHandedness = ((fTx * fSy - fTy * fSx) < 0.0f) ? -1.0f : 1.0f;

                    const plVec3 tTangent = 
                        pl_norm_vec3((plVec3){
                            fC * (fDeltaV2 * tEdge1.x - fDeltaV1 * tEdge2.x),
                            fC * (fDeltaV2 * tEdge1.y - fDeltaV1 * tEdge2.y),
                            fC * (fDeltaV2 * tEdge1.z - fDeltaV1 * tEdge2.z)
                    });

                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset]     = tTangent.x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = tTangent.y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tTangent.z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex0 * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = fHandedness;

                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset]     = tTangent.x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = tTangent.y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tTangent.z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex1 * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = fHandedness;

                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset]     = tTangent.x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = tTangent.y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = tTangent.z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + uIndex2 * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = fHandedness;
                }
                uCurrentAttributeOffset += 4;
            }

            // texture coordinates
            if(pucTextBufferStart)
            {
                tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0;
                for(size_t i = 0; i < szVertexCount; i++)
                {

                    const float u = *(float*)&pucTextBufferStart[i * szTextBufferStride];
                    const float v = ((float*)&pucTextBufferStart[i * szTextBufferStride])[1];

                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset]     = u;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = v;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = 0.0f;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = 0.0f;
                }
                uCurrentAttributeOffset += 4;
            }

            // colors
            if(pucColorBufferStart)
            {
                tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0;
                for(size_t i = 0; i < szVertexCount; i++)
                {

                    const float r = *(float*)&pucColorBufferStart[i * szColorBufferStride];
                    const float g = ((float*)&pucColorBufferStart[i * szColorBufferStride])[1];
                    const float b = ((float*)&pucColorBufferStart[i * szColorBufferStride])[2]; 
                    const float a = ((float*)&pucColorBufferStart[i * szColorBufferStride])[3]; 

                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset]     = r;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = g;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = b;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = a;
                }
                uCurrentAttributeOffset += 4;
            }

            // joints
            if(pucJointsBufferStart)
            {
                tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0;
                for(size_t i = 0; i < szVertexCount; i++)
                {

                    const float x = *(float*)&pucJointsBufferStart[i * szJointsBufferStride];
                    const float y = ((float*)&pucJointsBufferStart[i * szJointsBufferStride])[1];
                    const float z = ((float*)&pucJointsBufferStart[i * szJointsBufferStride])[2]; 
                    const float w = ((float*)&pucJointsBufferStart[i * szJointsBufferStride])[3]; 

                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset]     = x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = w;
                }
                uCurrentAttributeOffset += 4;
            }

            // weights
            if(pucWeightsBufferStart)
            {
                tVertexBufferFlags |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0;
                for(size_t i = 0; i < szVertexCount; i++)
                {

                    const float x = *(float*)&pucWeightsBufferStart[i * szJointsBufferStride];
                    const float y = ((float*)&pucWeightsBufferStart[i * szJointsBufferStride])[1];
                    const float z = ((float*)&pucWeightsBufferStart[i * szJointsBufferStride])[2]; 
                    const float w = ((float*)&pucWeightsBufferStart[i * szJointsBufferStride])[3]; 

                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset]     = x;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 1] = y;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 2] = z;
                    ptRenderer->sbfStorageBuffer[uStartPoint + i * uExtraAttributeComponents + uCurrentAttributeOffset + 3] = w;
                }
                uCurrentAttributeOffset += 4;
            }

            ptGltfOut->sbuVertexOffsets[uCurrentMesh] = (uint32_t)szTotalOffset;
            szTotalOffset += (size_t)uExtraAttributeComponents / 4 * szVertexCount;

            const plMesh tMesh = {
                .uIndexCount         = (uint32_t)ptPrimitive->indices->count,
                .uVertexCount        = (uint32_t)szVertexCount,
                .uIndexBuffer        = pl_create_index_buffer(&ptGraphics->tResourceManager, sizeof(uint32_t) * (uint32_t)ptPrimitive->indices->count, sbuIndexBuffer),
                .uVertexBuffer       = pl_create_vertex_buffer(&ptGraphics->tResourceManager, sizeof(float) * szVertexCount * uAttributeComponents, sizeof(float) * uAttributeComponents, sbfVertexBuffer),
                .ulVertexStreamMask0 = PL_MESH_FORMAT_FLAG_HAS_POSITION,
                .ulVertexStreamMask1 = tVertexBufferFlags
            };

            bool bMaterialFound = false;
            for(uint32_t i = 0; i < pl_sb_size(sbtMaterialBuffer); i++)
            {
                if(sbtMaterialBuffer[i] == ptPrimitive->material)
                {
                    ptGltfOut->sbuMaterialIndices[uCurrentMesh] = i;
                    bMaterialFound = true;
                    break;
                }
            }
            PL_ASSERT(bMaterialFound);

            pl_sb_reset(sbuIndexBuffer);
            pl_sb_reset(sbfVertexBuffer);
            ptGltfOut->sbtMeshes[uCurrentMesh] = tMesh;
            uCurrentMesh++;
        }
    } 
    pl_sb_free(sbuIndexBuffer);
    pl_sb_free(sbfVertexBuffer);
    pl_sb_free(sbtMaterialBuffer);
    return true;    
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__load_gltf_material(plResourceManager* ptResourceManager, const char* pcPath, const cgltf_material* ptMaterial, plMaterial* ptMaterialOut)
{
    ptMaterialOut->bDoubleSided = ptMaterial->double_sided;
    ptMaterialOut->fAlphaCutoff = ptMaterial->alpha_cutoff;

    if(ptMaterial->has_pbr_metallic_roughness)
    {
        ptMaterialOut->tAlbedo.x = ptMaterial->pbr_metallic_roughness.base_color_factor[0];
        ptMaterialOut->tAlbedo.y = ptMaterial->pbr_metallic_roughness.base_color_factor[1];
        ptMaterialOut->tAlbedo.z = ptMaterial->pbr_metallic_roughness.base_color_factor[2];
        ptMaterialOut->tAlbedo.w = ptMaterial->pbr_metallic_roughness.base_color_factor[3];

        if(ptMaterial->pbr_metallic_roughness.base_color_texture.texture)
        {

            char acFilepath[2048] = {0};
            pl_str_get_directory(pcPath, acFilepath);

            pl_str_concatenate(acFilepath, ptMaterial->pbr_metallic_roughness.base_color_texture.texture->image->uri, acFilepath, 2048);

            int texWidth, texHeight, texNumChannels;
            int texForceNumChannels = 4;
            unsigned char* rawBytes = stbi_load(acFilepath, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
            PL_ASSERT(rawBytes);

            const plTextureDesc tTextureDesc = {
                .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
                .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
                .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .uLayers     = 1,
                .uMips       = 0, // means all mips
                .tType       = VK_IMAGE_TYPE_2D,
                .tViewType   = VK_IMAGE_VIEW_TYPE_2D
            };
            ptMaterialOut->uAlbedoMap = pl_create_texture(ptResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * 4, rawBytes);
            ptMaterialOut->ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;

            stbi_image_free(rawBytes);
        }

        if(ptMaterial->normal_texture.texture)
        {

            char acFilepath[2048] = {0};
            pl_str_get_directory(pcPath, acFilepath);

            pl_str_concatenate(acFilepath, ptMaterial->normal_texture.texture->image->uri, acFilepath, 2048);

            int texWidth, texHeight, texNumChannels;
            int texForceNumChannels = 4;
            unsigned char* rawBytes = stbi_load(acFilepath, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
            PL_ASSERT(rawBytes);

            const plTextureDesc tTextureDesc = {
                .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
                .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
                .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .uLayers     = 1,
                .uMips       = 1,
                .tType       = VK_IMAGE_TYPE_2D,
                .tViewType   = VK_IMAGE_VIEW_TYPE_2D,
            };
            ptMaterialOut->uNormalMap = pl_create_texture(ptResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * 4, rawBytes);
            ptMaterialOut->ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
            stbi_image_free(rawBytes);
        }

        if(ptMaterial->emissive_texture.texture)
        {
            char acFilepath[2048] = {0};
            pl_str_get_directory(pcPath, acFilepath);
            pl_str_concatenate(acFilepath, ptMaterial->emissive_texture.texture->image->uri, acFilepath, 2048);

            int texWidth, texHeight, texNumChannels;
            int texForceNumChannels = 4;
            unsigned char* rawBytes = stbi_load(acFilepath, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
            PL_ASSERT(rawBytes);

            const plTextureDesc tTextureDesc = {
                .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
                .tFormat     = VK_FORMAT_R8G8B8A8_UNORM,
                .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .uLayers     = 1,
                .uMips       = 0, // means all mips
                .tType       = VK_IMAGE_TYPE_2D,
                .tViewType   = VK_IMAGE_VIEW_TYPE_2D
            };
            ptMaterialOut->uEmissiveMap = pl_create_texture(ptResourceManager, tTextureDesc, sizeof(unsigned char) * texHeight * texHeight * 4, rawBytes);
            ptMaterialOut->ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;
            stbi_image_free(rawBytes);
        }
    }
}