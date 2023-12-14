#pragma once

#include "pilotlight.h"
#include "pl_ui.h"
#include "pl_ecs_ext.h"
#include "pl_graphics_ext.h"
#include "pl_math.h"
#include "pl_ds.h"
#include "pl_string.h"

static void
pl_show_ecs_window(const plEcsI* ptECS, plComponentLibrary* ptLibrary, bool* pbShowWindow)
{
    if(pl_begin_window("Entities", pbShowWindow, false))
    {
        const plVec2 tWindowSize = pl_get_window_size();
        const float pfRatios[] = {0.5f, 0.5f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        pl_text("Entities");
        pl_text("Components");
        pl_layout_dynamic(0.0f, 1);
        pl_separator();
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 30.0f, 2, pfRatios);
        static int iSelectedEntity = -1;
        static plEntity tSelectedEntity = {0};

        if(pl_begin_child("Entities"))
        {
            const float pfRatiosInner[] = {1.0f};
            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            const uint32_t uEntityCount = pl_sb_size(ptLibrary->tTagComponentManager.sbtEntities);
            plTagComponent* sbtTags = ptLibrary->tTagComponentManager.pComponents;


            plUiClipper tClipper = {(uint32_t)uEntityCount};
            while(pl_step_clipper(&tClipper))
            {
                for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                {
                    bool bSelected = (int)i == iSelectedEntity;
                    char atBuffer[1024] = {0};
                    pl_sprintf(atBuffer, "%s ##%u", sbtTags[i].acName, i);
                    if(pl_selectable(atBuffer, &bSelected))
                    {
                        if(bSelected)
                        {
                            iSelectedEntity = (int)i;
                            tSelectedEntity = ptLibrary->tTagComponentManager.sbtEntities[i];
                        }
                        else
                            iSelectedEntity = -1;
                    }
                }
            }
            
            pl_end_child();
        }

        if(pl_begin_child("Components"))
        {
            const float pfRatiosInner[] = {1.0f};
            pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            if(iSelectedEntity != -1)
            {
                plTagComponent*       ptTagComp       = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, tSelectedEntity);
                plTransformComponent* ptTransformComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tSelectedEntity);
                plMeshComponent*      ptMeshComp      = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tSelectedEntity);
                plObjectComponent*    ptObjectComp    = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tSelectedEntity);
                plHierarchyComponent* ptHierarchyComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, tSelectedEntity);
                plMaterialComponent*  ptMaterialComp  = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, tSelectedEntity);
                plSkinComponent*      ptSkinComp      = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, tSelectedEntity);
                plCameraComponent*    ptCameraComp    = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, tSelectedEntity);

                pl_text("Entity: %u, %u", tSelectedEntity.uIndex, tSelectedEntity.uGeneration);

                if(ptTagComp && pl_collapsing_header("Tag"))
                {
                    pl_text("Name: %s", ptTagComp->acName);
                    pl_end_collapsing_header();
                }

                if(ptTransformComp && pl_collapsing_header("Transform"))
                {
                    pl_text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                    pl_text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                    pl_text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                    pl_vertical_spacing();
                    pl_text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                    pl_text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                    pl_text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                    pl_text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
                    pl_end_collapsing_header();
                }

                if(ptMeshComp && pl_collapsing_header("Mesh"))
                {

                    plTagComponent* ptMaterialTagComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tMaterial);
                    plTagComponent* ptSkinTagComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tSkinComponent);
                    pl_text("Material: %s", ptMaterialTagComp->acName);
                    pl_text("Skin:     %s", ptSkinTagComp ? ptSkinTagComp->acName : " ");

                    pl_vertical_spacing();
                    pl_text("Vertex Data (%u verts, %u idx)", pl_sb_size(ptMeshComp->sbtVertexPositions), pl_sb_size(ptMeshComp->sbuIndices));
                    pl_indent(15.0f);
                    pl_text("%s Positions", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION ? "ACTIVE" : "     ");
                    pl_text("%s Normals", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL ? "ACTIVE" : "     ");
                    pl_text("%s Tangents", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT ? "ACTIVE" : "     ");
                    pl_text("%s Texture Coordinates 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 ? "ACTIVE" : "     ");
                    pl_text("%s Texture Coordinates 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 ? "ACTIVE" : "     ");
                    pl_text("%s Colors 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0 ? "ACTIVE" : "     ");
                    pl_text("%s Colors 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1 ? "ACTIVE" : "     ");
                    pl_text("%s Joints 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0 ? "ACTIVE" : "     ");
                    pl_text("%s Joints 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1 ? "ACTIVE" : "     ");
                    pl_text("%s Weights 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0 ? "ACTIVE" : "     ");
                    pl_text("%s Weights 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1 ? "ACTIVE" : "     ");
                    pl_unindent(15.0f);
                    pl_end_collapsing_header();
                }

                if(ptObjectComp && pl_collapsing_header("Object"))
                {
                    plTagComponent* ptMeshTagComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tMesh);
                    plTagComponent* ptTransformTagComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tTransform);
                    pl_text("Mesh Entity:      %s", ptMeshTagComp->acName);
                    pl_text("Transform Entity: %s", ptTransformTagComp->acName);
                    pl_end_collapsing_header();
                }

                if(ptHierarchyComp && pl_collapsing_header("Hierarchy"))
                {
                    plTagComponent* ptParentTagComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptHierarchyComp->tParent);
                    pl_text("Parent Entity: %s", ptParentTagComp->acName);
                    pl_end_collapsing_header();
                }

                if(ptMaterialComp && pl_collapsing_header("Material"))
                {
                    pl_text("Base Color:            (%0.3f, %0.3f, %0.3f, %0.3f)", ptMaterialComp->tBaseColor.r, ptMaterialComp->tBaseColor.g, ptMaterialComp->tBaseColor.b, ptMaterialComp->tBaseColor.a);
                    pl_text("Specular Color:        (%0.3f, %0.3f, %0.3f, %0.3f)", ptMaterialComp->tSpecularColor.r, ptMaterialComp->tSpecularColor.g, ptMaterialComp->tSpecularColor.b, ptMaterialComp->tSpecularColor.a);
                    pl_text("Emissive Color:        (%0.3f, %0.3f, %0.3f, %0.3f)", ptMaterialComp->tEmissiveColor.r, ptMaterialComp->tEmissiveColor.g, ptMaterialComp->tEmissiveColor.b, ptMaterialComp->tEmissiveColor.a);
                    pl_text("Sheen Color:           (%0.3f, %0.3f, %0.3f, %0.3f)", ptMaterialComp->tSheenColor.r, ptMaterialComp->tSheenColor.g, ptMaterialComp->tSheenColor.b, ptMaterialComp->tSheenColor.a);
                    pl_text("Subsurface Scattering: (%0.3f, %0.3f, %0.3f, %0.3f)", ptMaterialComp->tSubsurfaceScattering.r, ptMaterialComp->tSubsurfaceScattering.g, ptMaterialComp->tSubsurfaceScattering.b, ptMaterialComp->tSubsurfaceScattering.a);
                    pl_text("Roughness:                       %0.3f", ptMaterialComp->fRoughness);
                    pl_text("Reflectance:                     %0.3f", ptMaterialComp->fReflectance);
                    pl_text("Metalness:                       %0.3f", ptMaterialComp->fMetalness);
                    pl_text("Normal Map Strength:             %0.3f", ptMaterialComp->fNormalMapStrength);
                    pl_text("Occlusion Map Strength:          %0.3f", ptMaterialComp->fOcclusionMapStrength);
                    pl_text("Parallax Occlusion Map Strength: %0.3f", ptMaterialComp->fParallaxOcclusionMapStrength);
                    pl_text("Displacement Map Strength:       %0.3f", ptMaterialComp->fDisplacementMapStrength);
                    pl_text("Refraction:                      %0.3f", ptMaterialComp->fRefraction);
                    pl_text("Transmission:                    %0.3f", ptMaterialComp->fTransmission);
                    pl_text("Anisotropy Strength:             %0.3f", ptMaterialComp->fAnisotropyStrength);
                    pl_text("Anisotropy Rotation:             %0.3f", ptMaterialComp->fAnisotropyRotation);
                    pl_text("Sheen Roughness:                 %0.3f", ptMaterialComp->fSheenRoughness);
                    pl_text("Clearcoat Factor:                %0.3f", ptMaterialComp->fClearcoatFactor);
                    pl_text("Clearcoat Roughness:             %0.3f", ptMaterialComp->fClearcoatRoughness);
                    pl_text("Thickness Factor:                %0.3f", ptMaterialComp->fThicknessFactor);
                    pl_text("Alpha Cutoff:                    %0.3f", ptMaterialComp->fAlphaCutoff);

                    static const char* apcBlendModeNames[] = 
                    {
                        "PL_MATERIAL_BLEND_MODE_OPAQUE",
                        "PL_MATERIAL_BLEND_MODE_ALPHA",
                        "PL_MATERIAL_BLEND_MODE_PREMULTIPLIED",
                        "PL_MATERIAL_BLEND_MODE_ADDITIVE",
                        "PL_MATERIAL_BLEND_MODE_MULTIPLY"
                    };
                    pl_text("Blend Mode:                      %s", apcBlendModeNames[ptMaterialComp->tBlendMode]);

                    static const char* apcShaderNames[] = 
                    {
                        "PL_SHADER_TYPE_PBR",
                        "PL_SHADER_TYPE_UNLIT",
                        "PL_SHADER_TYPE_CUSTOM"
                    };
                    pl_text("Shader Type:                     %s", apcShaderNames[ptMaterialComp->tShaderType]);
                    pl_text("Double Sided:                    %s", ptMaterialComp->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED ? "true" : "false");
  
                    pl_vertical_spacing();
                    pl_text("Texture Maps");
                    pl_indent(15.0f);

                    static const char* apcTextureSlotNames[] = 
                    {
                        "PL_TEXTURE_SLOT_BASE_COLOR_MAP",
                        "PL_TEXTURE_SLOT_NORMAL_MAP",
                        "PL_TEXTURE_SLOT_EMISSIVE_MAP",
                        "PL_TEXTURE_SLOT_OCCLUSSION_MAP",
                        "PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP",
                        "PL_TEXTURE_SLOT_SHEEN_COLOR_MAP",
                        "PL_TEXTURE_SLOT_SHEEN_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_TRANSMISSION_MAP",
                        "PL_TEXTURE_SLOT_DISPLACEMENT_MAP",
                        "PL_TEXTURE_SLOT_SPECULAR_MAP",
                        "PL_TEXTURE_SLOT_ANISOTROPY_MAP",
                        "PL_TEXTURE_SLOT_SURFACE_MAP"
                    };

                    for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
                    {
                        pl_text("%s: %s", apcTextureSlotNames[i], ptMaterialComp->atTextureMaps[i].acName[0] == 0 ? " " : ptMaterialComp->atTextureMaps[i].acName);
                    }
                    pl_unindent(15.0f);
                    pl_end_collapsing_header();
                }

                if(ptSkinComp && pl_collapsing_header("Skin"))
                {
                    if(pl_tree_node("Joints"))
                    {
                        for(uint32_t i = 0; i < pl_sb_size(ptSkinComp->sbtJoints); i++)
                        {
                            plTagComponent* ptJointTagComp = ptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptSkinComp->sbtJoints[i]);
                            pl_text("%s", ptJointTagComp->acName);  
                        }
                        pl_tree_pop();
                    }
                    pl_end_collapsing_header();
                }

                if(ptCameraComp && pl_collapsing_header("Camera"))
                { 
                    pl_text("Near Z:                  %+0.3f", ptCameraComp->fNearZ);
                    pl_text("Far Z:                   %+0.3f", ptCameraComp->fFarZ);
                    pl_text("Vertical Field of View:  %+0.3f", ptCameraComp->fFieldOfView);
                    pl_text("Aspect Ratio:            %+0.3f", ptCameraComp->fAspectRatio);
                    pl_text("Pitch:                   %+0.3f", ptCameraComp->fPitch);
                    pl_text("Yaw:                     %+0.3f", ptCameraComp->fYaw);
                    pl_text("Roll:                    %+0.3f", ptCameraComp->fRoll);
                    pl_text("Position: (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPos.x, ptCameraComp->tPos.y, ptCameraComp->tPos.z);
                    pl_text("Up:       (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tUpVec.x, ptCameraComp->_tUpVec.y, ptCameraComp->_tUpVec.z);
                    pl_text("Forward:  (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tForwardVec.x, ptCameraComp->_tForwardVec.y, ptCameraComp->_tForwardVec.z);
                    pl_text("Right:    (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tRightVec.x, ptCameraComp->_tRightVec.y, ptCameraComp->_tRightVec.z);
                    pl_end_collapsing_header();
                }
            }
            
            pl_end_child();
        }

        pl_end_window();
    }
}