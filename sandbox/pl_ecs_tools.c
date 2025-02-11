/*
   pl_ecs_tools.c
*/

/*
Index of this file:
// [SECTION] internal structs & enums
// [SECTION] internal api declarations
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

#include "pl.h"
#include "pl_ecs_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_show_ecs_window(plEntity* ptSelectedEntity, uint32_t uSceneHandle, bool* pbShowWindow)
{
    plComponentLibrary* ptLibrary = gptRenderer->get_component_library(uSceneHandle);
    bool bResult = false;

    if(gptUi->begin_window("Entities", pbShowWindow, false))
    {
        const plVec2 tWindowSize = gptUi->get_window_size();
        const float pfRatios[] = {0.5f, 0.5f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        gptUi->text("Entities");
        gptUi->text("Components");
        gptUi->layout_dynamic(0.0f, 1);
        gptUi->separator();
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 75.0f, 2, pfRatios);


        if(gptUi->begin_child("Entities", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            const uint32_t uEntityCount = pl_sb_size(ptLibrary->tTagComponentManager.sbtEntities);
            plTagComponent* sbtTags = ptLibrary->tTagComponentManager.pComponents;

            plUiClipper tClipper = {(uint32_t)uEntityCount};
            while(gptUi->step_clipper(&tClipper))
            {
                for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                {
                    bool bSelected = ptSelectedEntity->ulData == ptLibrary->tTagComponentManager.sbtEntities[i].ulData;
                    char atBuffer[1024] = {0};
                    pl_sprintf(atBuffer, "%s, %u", sbtTags[i].acName, ptLibrary->tTagComponentManager.sbtEntities[i].uIndex);
                    if(gptUi->selectable(atBuffer, &bSelected, 0))
                    {
                        if(bSelected)
                        {
                            *ptSelectedEntity = ptLibrary->tTagComponentManager.sbtEntities[i];
                            if(ptSelectedEntity->uIndex != UINT32_MAX)
                                bResult = true;
                                // gptRenderer->select_entities(ptAppData->uSceneHandle0, 1, &ptAppData->tSelectedEntity);
                        }
                        else
                        {
                            ptSelectedEntity->uIndex = UINT32_MAX;
                            ptSelectedEntity->uGeneration = UINT32_MAX;
                            bResult = true;
                            // gptRenderer->select_entities(ptAppData->uSceneHandle0, 0, NULL);
                        }
                    }
                }
            }
            
            gptUi->end_child();
        }

        if(gptUi->begin_child("Components", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            if(ptSelectedEntity->ulData != UINT64_MAX)
            {
                gptUi->push_id_uint(ptSelectedEntity->uIndex);
                plTagComponent*               ptTagComp           = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, *ptSelectedEntity);
                plTransformComponent*         ptTransformComp     = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, *ptSelectedEntity);
                plMeshComponent*              ptMeshComp          = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_MESH, *ptSelectedEntity);
                plObjectComponent*            ptObjectComp        = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, *ptSelectedEntity);
                plHierarchyComponent*         ptHierarchyComp     = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, *ptSelectedEntity);
                plMaterialComponent*          ptMaterialComp      = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, *ptSelectedEntity);
                plSkinComponent*              ptSkinComp          = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, *ptSelectedEntity);
                plCameraComponent*            ptCameraComp        = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, *ptSelectedEntity);
                plAnimationComponent*         ptAnimationComp     = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION, *ptSelectedEntity);
                plInverseKinematicsComponent* ptIKComp            = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_INVERSE_KINEMATICS, *ptSelectedEntity);
                plLightComponent*             ptLightComp         = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_LIGHT, *ptSelectedEntity);
                plEnvironmentProbeComponent*  ptProbeComp         = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, *ptSelectedEntity);
                plHumanoidComponent*          ptHumanComp         = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_HUMANOID, *ptSelectedEntity);


                gptUi->text("Entity: %u, %u", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);

                if(ptTagComp && gptUi->begin_collapsing_header("Tag", 0))
                {
                    gptUi->text("Name: %s", ptTagComp->acName);
                    gptUi->end_collapsing_header();
                }

                if(ptHumanComp && gptUi->begin_collapsing_header("Humanoid", 0))
                {
                    gptUi->end_collapsing_header();
                }

                if(ptProbeComp && gptUi->begin_collapsing_header("Environment Probe", 0))
                {
                    gptUi->input_float3("Position", ptProbeComp->tPosition.d, NULL, 0);

                    bool bRealTime = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                    if(gptUi->checkbox("Real Time", &bRealTime))
                    {
                        if(bRealTime)
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                        else
                            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                    }

                    bool bIncludeSky = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                    if(gptUi->checkbox("Include Sky", &bIncludeSky))
                    {
                        if(bIncludeSky)
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                        else
                            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                    }

                    bool bParallaxCorrection = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                    if(gptUi->checkbox("Box Parallax Correction", &bParallaxCorrection))
                    {
                        if(bParallaxCorrection)
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                        else
                            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                    }

                    if(gptUi->button("Update"))
                    {
                        ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
                    }
                    gptUi->input_float("Range:", &ptProbeComp->fRange, NULL, 0);

                    uint32_t auSamples[] = {
                        32,
                        64,
                        128,
                        256,
                        512,
                        1024
                    };
                    int iSelection = 0;
                    if(ptProbeComp->uSamples == 32)        iSelection = 0;
                    else if(ptProbeComp->uSamples == 64)   iSelection = 1;
                    else if(ptProbeComp->uSamples == 128)  iSelection = 2;
                    else if(ptProbeComp->uSamples == 256)  iSelection = 3;
                    else if(ptProbeComp->uSamples == 512)  iSelection = 4;
                    else if(ptProbeComp->uSamples == 1024) iSelection = 5;
                    gptUi->separator_text("Samples");
                    gptUi->radio_button("32", &iSelection, 0);
                    gptUi->radio_button("64", &iSelection, 1);
                    gptUi->radio_button("128", &iSelection, 2);
                    gptUi->radio_button("256", &iSelection, 3);
                    gptUi->radio_button("512", &iSelection, 4);
                    gptUi->radio_button("1024", &iSelection, 5);
                    ptProbeComp->uSamples = auSamples[iSelection];

                    gptUi->separator_text("Intervals");

                    int iSelection0 = (int)ptProbeComp->uInterval;
                    gptUi->radio_button("1", &iSelection0, 1);
                    gptUi->radio_button("2", &iSelection0, 2);
                    gptUi->radio_button("3", &iSelection0, 3);
                    gptUi->radio_button("4", &iSelection0, 4);
                    gptUi->radio_button("5", &iSelection0, 5);
                    gptUi->radio_button("6", &iSelection0, 6);

                    ptProbeComp->uInterval = (uint32_t)iSelection0;

                    gptUi->end_collapsing_header();
                }

                if(ptTransformComp && gptUi->begin_collapsing_header("Transform", 0))
                {
                    gptUi->text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                    gptUi->text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                    gptUi->text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                    gptUi->vertical_spacing();
                    gptUi->text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                    gptUi->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                    gptUi->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                    gptUi->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
                    gptUi->end_collapsing_header();
                }

                if(ptMeshComp && gptUi->begin_collapsing_header("Mesh", 0))
                {

                    plTagComponent* ptMaterialTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tMaterial);
                    plTagComponent* ptSkinTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tSkinComponent);
                    gptUi->text("Material: %s, %u", ptMaterialTagComp->acName, ptMeshComp->tMaterial.uIndex);
                    gptUi->text("Skin:     %s", ptSkinTagComp ? ptSkinTagComp->acName : " ");

                    gptUi->vertical_spacing();
                    gptUi->text("Vertex Data (%u verts, %u idx)", pl_sb_size(ptMeshComp->sbtVertexPositions), pl_sb_size(ptMeshComp->sbuIndices));
                    gptUi->indent(15.0f);
                    gptUi->text("%s Positions", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION ? "ACTIVE" : "     ");
                    gptUi->text("%s Normals", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL ? "ACTIVE" : "     ");
                    gptUi->text("%s Tangents", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT ? "ACTIVE" : "     ");
                    gptUi->text("%s Texture Coordinates 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Texture Coordinates 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 ? "ACTIVE" : "     ");
                    gptUi->text("%s Colors 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Colors 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1 ? "ACTIVE" : "     ");
                    gptUi->text("%s Joints 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Joints 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1 ? "ACTIVE" : "     ");
                    gptUi->text("%s Weights 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0 ? "ACTIVE" : "     ");
                    gptUi->text("%s Weights 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1 ? "ACTIVE" : "     ");
                    gptUi->unindent(15.0f);
                    gptUi->end_collapsing_header();
                }

                if(ptObjectComp && gptUi->begin_collapsing_header("Object", 0))
                {
                    plTagComponent* ptMeshTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tMesh);
                    plTagComponent* ptTransformTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tTransform);
                    gptUi->text("Mesh Entity:      %s, %u", ptMeshTagComp->acName, ptObjectComp->tMesh.uIndex);
                    gptUi->text("Transform Entity: %s, %u", ptTransformTagComp->acName, ptObjectComp->tTransform.uIndex);
                    gptUi->end_collapsing_header();
                }

                if(ptHierarchyComp && gptUi->begin_collapsing_header("Hierarchy", 0))
                {
                    plTagComponent* ptParentTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptHierarchyComp->tParent);
                    gptUi->text("Parent Entity: %s , %u", ptParentTagComp->acName, ptHierarchyComp->tParent.uIndex);
                    gptUi->end_collapsing_header();
                }

                if(ptLightComp && gptUi->begin_collapsing_header("Light", 0))
                {
                    static const char* apcLightTypes[] = {
                        "PL_LIGHT_TYPE_DIRECTIONAL",
                        "PL_LIGHT_TYPE_POINT",
                        "PL_LIGHT_TYPE_SPOT",
                    };
                    gptUi->text("Type: %s", apcLightTypes[ptLightComp->tType]);

                    bool bShowVisualizer = ptLightComp->tFlags & PL_LIGHT_FLAG_VISUALIZER;
                    if(gptUi->checkbox("Visualizer:", &bShowVisualizer))
                    {
                        if(bShowVisualizer)
                            ptLightComp->tFlags |= PL_LIGHT_FLAG_VISUALIZER;
                        else
                            ptLightComp->tFlags &= ~PL_LIGHT_FLAG_VISUALIZER;
                    }

                    gptUi->input_float3("Position:", ptLightComp->tPosition.d, NULL, 0);

                    gptUi->separator_text("Color");
                    gptUi->slider_float("r", &ptLightComp->tColor.x, 0.0f, 1.0f, 0);
                    gptUi->slider_float("g", &ptLightComp->tColor.y, 0.0f, 1.0f, 0);
                    gptUi->slider_float("b", &ptLightComp->tColor.z, 0.0f, 1.0f, 0);

                    gptUi->slider_float("Intensity", &ptLightComp->fIntensity, 0.0f, 20.0f, 0);

                    if(ptLightComp->tType != PL_LIGHT_TYPE_DIRECTIONAL)
                    {
                        gptUi->input_float("Radius:", &ptLightComp->fRadius, NULL, 0);
                        gptUi->input_float("Range", &ptLightComp->fRange, NULL, 0);
                    }

                    if(ptLightComp->tType == PL_LIGHT_TYPE_SPOT)
                    {
                        gptUi->slider_float("Inner Cone Angle:", &ptLightComp->fInnerConeAngle, 0.0f, PL_PI_2, 0);
                        gptUi->slider_float("Outer Cone Angle", &ptLightComp->fOuterConeAngle, 0.0f, PL_PI_2, 0);
                    }


                    if(ptLightComp->tType != PL_LIGHT_TYPE_POINT)
                    {
                        gptUi->separator_text("Direction");
                        gptUi->slider_float("x", &ptLightComp->tDirection.x, -1.0f, 1.0f, 0);
                        gptUi->slider_float("y", &ptLightComp->tDirection.y, -1.0f, 1.0f, 0);
                        gptUi->slider_float("z", &ptLightComp->tDirection.z, -1.0f, 1.0f, 0);
                    }

                    gptUi->separator_text("Shadows");

                    bool bCastShadow = ptLightComp->tFlags & PL_LIGHT_FLAG_CAST_SHADOW;
                    if(gptUi->checkbox("Cast Shadow:", &bCastShadow))
                    {
                        if(bCastShadow)
                            ptLightComp->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW;
                        else
                            ptLightComp->tFlags &= ~PL_LIGHT_FLAG_CAST_SHADOW;
                    }

                    if(bCastShadow)
                    {
                        uint32_t auResolutions[] = {
                            128,
                            256,
                            512,
                            1024,
                            2048
                        };
                        int iSelection = 0;
                        if(ptLightComp->uShadowResolution == 128)       iSelection = 0;
                        else if(ptLightComp->uShadowResolution == 256)  iSelection = 1;
                        else if(ptLightComp->uShadowResolution == 512)  iSelection = 2;
                        else if(ptLightComp->uShadowResolution == 1024) iSelection = 3;
                        else if(ptLightComp->uShadowResolution == 2048) iSelection = 4;
                        gptUi->radio_button("Resolution: 128", &iSelection, 0);
                        gptUi->radio_button("Resolution: 256", &iSelection, 1);
                        gptUi->radio_button("Resolution: 512", &iSelection, 2);
                        gptUi->radio_button("Resolution: 1024", &iSelection, 3);
                        gptUi->radio_button("Resolution: 2048", &iSelection, 4);
                        ptLightComp->uShadowResolution = auResolutions[iSelection];
                    }

                    if(ptLightComp->tType == PL_LIGHT_TYPE_DIRECTIONAL)
                    {
                        int iCascadeCount  = (int)ptLightComp->uCascadeCount;
                        if(gptUi->slider_int("Cascades", &iCascadeCount, 1, 4, 0))
                        {
                            ptLightComp->uCascadeCount = (uint32_t)iCascadeCount;
                        }

                        // int iResolution = (int)ptLight->uShadowResolution;
                        gptUi->input_float4("Cascade Splits", ptLightComp->afCascadeSplits, NULL, 0);
                    }
                    gptUi->end_collapsing_header();
                }

                if(ptMaterialComp && gptUi->begin_collapsing_header("Material", 0))
                {
                    bool bMaterialModified = false;
                    if(gptUi->input_float("Roughness", &ptMaterialComp->fRoughness, NULL, 0)) bMaterialModified = true;
                    if(gptUi->input_float("Metalness", &ptMaterialComp->fMetalness, NULL, 0)) bMaterialModified = true;
                    if(gptUi->input_float("Alpha Cutoff", &ptMaterialComp->fAlphaCutoff, NULL, 0)) bMaterialModified = true;
                    if(gptUi->input_float("Normal Map Strength", &ptMaterialComp->fNormalMapStrength, NULL, 0)) bMaterialModified = true;
                    if(gptUi->input_float("Occulusion Map Strength", &ptMaterialComp->fOcclusionMapStrength, NULL, 0)) bMaterialModified = true;
                    if(gptUi->input_float4("Base Factor", ptMaterialComp->tBaseColor.d, NULL, 0)) bMaterialModified = true;
                    if(gptUi->input_float4("Emmissive Factor", ptMaterialComp->tEmissiveColor.d, NULL, 0)) bMaterialModified = true;

                    if(bMaterialModified)
                        gptRenderer->update_scene_materials(uSceneHandle, 1, ptSelectedEntity);

                    static const char* apcBlendModeNames[] = 
                    {
                        "PL_MATERIAL_BLEND_MODE_OPAQUE",
                        "PL_MATERIAL_BLEND_MODE_ALPHA",
                        "PL_MATERIAL_BLEND_MODE_PREMULTIPLIED",
                        "PL_MATERIAL_BLEND_MODE_ADDITIVE",
                        "PL_MATERIAL_BLEND_MODE_MULTIPLY",
                        "PL_MATERIAL_BLEND_MODE_CLIP_MASK"
                    };
                    gptUi->text("Blend Mode:                      %s", apcBlendModeNames[ptMaterialComp->tBlendMode]);

                    static const char* apcShaderNames[] = 
                    {
                        "PL_SHADER_TYPE_PBR",
                        "PL_SHADER_TYPE_UNLIT",
                        "PL_SHADER_TYPE_CUSTOM"
                    };
                    gptUi->text("Shader Type:                     %s", apcShaderNames[ptMaterialComp->tShaderType]);
                    gptUi->text("Double Sided:                    %s", ptMaterialComp->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED ? "true" : "false");
  
                    gptUi->vertical_spacing();
                    gptUi->text("Texture Maps");
                    gptUi->indent(15.0f);

                    static const char* apcTextureSlotNames[] = 
                    {
                        "PL_TEXTURE_SLOT_BASE_COLOR_MAP",
                        "PL_TEXTURE_SLOT_NORMAL_MAP",
                        "PL_TEXTURE_SLOT_EMISSIVE_MAP",
                        "PL_TEXTURE_SLOT_OCCLUSION_MAP",
                        "PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP",
                        "PL_TEXTURE_SLOT_SHEEN_COLOR_MAP",
                        "PL_TEXTURE_SLOT_SHEEN_ROUGHNESS_MAP",
                        "PL_TEXTURE_SLOT_TRANSMISSION_MAP",
                        "PL_TEXTURE_SLOT_SPECULAR_MAP",
                        "PL_TEXTURE_SLOT_SPECULAR_COLOR_MAP",
                        "PL_TEXTURE_SLOT_ANISOTROPY_MAP",
                        "PL_TEXTURE_SLOT_SURFACE_MAP",
                        "PL_TEXTURE_SLOT_IRIDESCENCE_MAP",
                        "PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS_MAP"
                    };

                    for(uint32_t i = 0; i < PL_TEXTURE_SLOT_COUNT; i++)
                    {
                        gptUi->text("%s: %s", apcTextureSlotNames[i], ptMaterialComp->atTextureMaps[i].acName[0] == 0 ? " " : "present");
                    }
                    gptUi->unindent(15.0f);
                    gptUi->end_collapsing_header();
                }

                if(ptSkinComp && gptUi->begin_collapsing_header("Skin", 0))
                {
                    if(gptUi->tree_node("Joints", 0))
                    {
                        for(uint32_t i = 0; i < pl_sb_size(ptSkinComp->sbtJoints); i++)
                        {
                            plTagComponent* ptJointTagComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptSkinComp->sbtJoints[i]);
                            gptUi->text("%s", ptJointTagComp->acName);  
                        }
                        gptUi->tree_pop();
                    }
                    gptUi->end_collapsing_header();
                }

                if(ptCameraComp && gptUi->begin_collapsing_header("Camera", 0))
                { 
                    gptUi->text("Near Z:                  %+0.3f", ptCameraComp->fNearZ);
                    gptUi->text("Far Z:                   %+0.3f", ptCameraComp->fFarZ);
                    gptUi->text("Vertical Field of View:  %+0.3f", ptCameraComp->fFieldOfView);
                    gptUi->text("Aspect Ratio:            %+0.3f", ptCameraComp->fAspectRatio);
                    gptUi->text("Pitch:                   %+0.3f", ptCameraComp->fPitch);
                    gptUi->text("Yaw:                     %+0.3f", ptCameraComp->fYaw);
                    gptUi->text("Roll:                    %+0.3f", ptCameraComp->fRoll);
                    gptUi->text("Position: (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPos.x, ptCameraComp->tPos.y, ptCameraComp->tPos.z);
                    gptUi->text("Up:       (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tUpVec.x, ptCameraComp->_tUpVec.y, ptCameraComp->_tUpVec.z);
                    gptUi->text("Forward:  (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tForwardVec.x, ptCameraComp->_tForwardVec.y, ptCameraComp->_tForwardVec.z);
                    gptUi->text("Right:    (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tRightVec.x, ptCameraComp->_tRightVec.y, ptCameraComp->_tRightVec.z);
                    gptUi->input_float3("Position", ptCameraComp->tPos.d, NULL, 0);
                    gptUi->input_float("Near Z Plane", &ptCameraComp->fNearZ, NULL, 0);
                    gptUi->input_float("Far Z Plane", &ptCameraComp->fFarZ, NULL, 0);
                    gptUi->end_collapsing_header();
                }

                if(ptAnimationComp && gptUi->begin_collapsing_header("Animation", 0))
                { 
                    bool bPlaying = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_PLAYING;
                    bool bLooped = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_LOOPED;
                    if(bLooped && bPlaying)
                        gptUi->text("Status: playing & looped");
                    else if(bPlaying)
                        gptUi->text("Status: playing");
                    else if(bLooped)
                        gptUi->text("Status: looped");
                    else
                        gptUi->text("Status: not playing");
                    if(gptUi->checkbox("Playing", &bPlaying))
                    {
                        if(bPlaying)
                            ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_PLAYING;
                        else
                            ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
                    }
                    if(gptUi->checkbox("Looped", &bLooped))
                    {
                        if(bLooped)
                            ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_LOOPED;
                        else
                            ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_LOOPED;
                    }
                    gptUi->text("Start: %0.3f s", ptAnimationComp->fStart);
                    gptUi->text("End:   %0.3f s", ptAnimationComp->fEnd);
                    gptUi->progress_bar(ptAnimationComp->fTimer / (ptAnimationComp->fEnd - ptAnimationComp->fStart), (plVec2){-1.0f, 0.0f}, NULL);
                    gptUi->text("Speed:   %0.3f s", ptAnimationComp->fSpeed);
                    gptUi->end_collapsing_header();
                }

                if(ptIKComp && gptUi->begin_collapsing_header("Inverse Kinematics", 0))
                { 
                    plTagComponent* ptTargetComp = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptIKComp->tTarget);
                    gptUi->text("Target Entity: %s , %u", ptTargetComp->acName, ptIKComp->tTarget.uIndex);

                    int iChainLength = (int)ptIKComp->uChainLength;
                    gptUi->slider_int("Chain Length", &iChainLength, 1, 5, 0);

                    ptIKComp->uChainLength = (uint32_t)iChainLength;

                    // gptUi->text("Chain Length: %u", ptIKComp->uChainLength);
                    gptUi->text("Iterations: %u", ptIKComp->uIterationCount);

                    gptUi->checkbox("Enabled", &ptIKComp->bEnabled);
                    gptUi->end_collapsing_header();
                }

                gptUi->pop_id();
            }
            
            gptUi->end_child();
        }

        gptUi->end_window();
    }

    return bResult;
}