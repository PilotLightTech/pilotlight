/*
   pl_ecs_tools_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_ecs_tools_ext.h"
#include "pl_ecs_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ui_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

static const plMemoryI*  gptMemory = NULL;
static const plEcsI*      gptECS      = NULL;
static const plUiI*       gptUI       = NULL;
static const plRendererI* gptRenderer = NULL;

#ifndef PL_DS_ALLOC
    
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"
#endif

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static bool
pl_show_ecs_window(plEntity* ptSelectedEntity, uint32_t uSceneHandle, bool* pbShowWindow)
{
    plComponentLibrary* ptLibrary = gptRenderer->get_component_library(uSceneHandle);
    bool bResult = false;

    if(gptUI->begin_window("Entities", pbShowWindow, false))
    {
        const plVec2 tWindowSize = gptUI->get_window_size();
        const float pfRatios[] = {0.5f, 0.5f};
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        gptUI->text("Entities");
        gptUI->text("Components");
        gptUI->layout_dynamic(0.0f, 1);
        gptUI->separator();
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 75.0f, 2, pfRatios);


        if(gptUI->begin_child("Entities", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            const uint32_t uEntityCount = pl_sb_size(ptLibrary->tTagComponentManager.sbtEntities);
            plTagComponent* sbtTags = ptLibrary->tTagComponentManager.pComponents;

            plUiClipper tClipper = {(uint32_t)uEntityCount};
            while(gptUI->step_clipper(&tClipper))
            {
                for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                {
                    bool bSelected = ptSelectedEntity->ulData == ptLibrary->tTagComponentManager.sbtEntities[i].ulData;
                    char atBuffer[1024] = {0};
                    pl_sprintf(atBuffer, "%s, %u", sbtTags[i].acName, ptLibrary->tTagComponentManager.sbtEntities[i].uIndex);
                    if(gptUI->selectable(atBuffer, &bSelected, 0))
                    {
                        if(bSelected)
                        {
                            *ptSelectedEntity = ptLibrary->tTagComponentManager.sbtEntities[i];
                            if(ptSelectedEntity->uIndex != UINT32_MAX)
                                bResult = true;
                        }
                        else
                        {
                            ptSelectedEntity->uIndex = UINT32_MAX;
                            ptSelectedEntity->uGeneration = UINT32_MAX;
                            bResult = true;
                        }
                    }
                }
            }
            
            gptUI->end_child();
        }

        if(gptUI->begin_child("Components", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            if(ptSelectedEntity->ulData != UINT64_MAX)
            {
                gptUI->push_id_uint(ptSelectedEntity->uIndex);
                plTagComponent*               ptTagComp           = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, *ptSelectedEntity);
                plTransformComponent*         ptTransformComp     = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, *ptSelectedEntity);
                plMeshComponent*              ptMeshComp          = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_MESH, *ptSelectedEntity);
                plObjectComponent*            ptObjectComp        = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, *ptSelectedEntity);
                plHierarchyComponent*         ptHierarchyComp     = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_HIERARCHY, *ptSelectedEntity);
                plMaterialComponent*          ptMaterialComp      = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_MATERIAL, *ptSelectedEntity);
                plSkinComponent*              ptSkinComp          = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, *ptSelectedEntity);
                plCameraComponent*            ptCameraComp        = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, *ptSelectedEntity);
                plAnimationComponent*         ptAnimationComp     = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION, *ptSelectedEntity);
                plInverseKinematicsComponent* ptIKComp            = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_INVERSE_KINEMATICS, *ptSelectedEntity);
                plLightComponent*             ptLightComp         = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_LIGHT, *ptSelectedEntity);
                plEnvironmentProbeComponent*  ptProbeComp         = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, *ptSelectedEntity);
                plHumanoidComponent*          ptHumanComp         = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_HUMANOID, *ptSelectedEntity);

                if(ptObjectComp)
                {
                    gptUI->layout_dynamic(0.0f, 2);
                    gptUI->text("Entity: %u, %u", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);
                    if(gptUI->button("Delete"))
                    {
                        gptRenderer->select_entities(uSceneHandle, 0, NULL);
                        gptRenderer->remove_objects_from_scene(uSceneHandle, 1, ptSelectedEntity);
                        ptSelectedEntity->ulData = UINT64_MAX;
                        
                    }
                    gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);
                }
                else
                {
                    gptUI->text("Entity: %u, %u", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);
                }

                

                if(ptTagComp && gptUI->begin_collapsing_header("Tag", 0))
                {
                    gptUI->text("Name: %s", ptTagComp->acName);
                    gptUI->end_collapsing_header();
                }

                if(ptHumanComp && gptUI->begin_collapsing_header("Humanoid", 0))
                {
                    gptUI->end_collapsing_header();
                }

                if(ptProbeComp && gptUI->begin_collapsing_header("Environment Probe", 0))
                {
                    gptUI->input_float3("Position", ptProbeComp->tPosition.d, NULL, 0);

                    bool bRealTime = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                    if(gptUI->checkbox("Real Time", &bRealTime))
                    {
                        if(bRealTime)
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                        else
                            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                    }

                    bool bIncludeSky = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                    if(gptUI->checkbox("Include Sky", &bIncludeSky))
                    {
                        if(bIncludeSky)
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                        else
                            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                    }

                    bool bParallaxCorrection = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                    if(gptUI->checkbox("Box Parallax Correction", &bParallaxCorrection))
                    {
                        if(bParallaxCorrection)
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                        else
                            ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                    }

                    if(gptUI->button("Update"))
                    {
                        ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
                    }
                    gptUI->input_float("Range:", &ptProbeComp->fRange, NULL, 0);

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
                    gptUI->separator_text("Samples");
                    gptUI->radio_button("32", &iSelection, 0);
                    gptUI->radio_button("64", &iSelection, 1);
                    gptUI->radio_button("128", &iSelection, 2);
                    gptUI->radio_button("256", &iSelection, 3);
                    gptUI->radio_button("512", &iSelection, 4);
                    gptUI->radio_button("1024", &iSelection, 5);
                    ptProbeComp->uSamples = auSamples[iSelection];

                    gptUI->separator_text("Intervals");

                    int iSelection0 = (int)ptProbeComp->uInterval;
                    gptUI->radio_button("1", &iSelection0, 1);
                    gptUI->radio_button("2", &iSelection0, 2);
                    gptUI->radio_button("3", &iSelection0, 3);
                    gptUI->radio_button("4", &iSelection0, 4);
                    gptUI->radio_button("5", &iSelection0, 5);
                    gptUI->radio_button("6", &iSelection0, 6);

                    ptProbeComp->uInterval = (uint32_t)iSelection0;

                    gptUI->end_collapsing_header();
                }

                if(ptTransformComp && gptUI->begin_collapsing_header("Transform", 0))
                {
                    gptUI->text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                    gptUI->text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                    gptUI->text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                    gptUI->vertical_spacing();
                    gptUI->text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                    gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                    gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                    gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
                    gptUI->end_collapsing_header();
                }

                if(ptMeshComp && gptUI->begin_collapsing_header("Mesh", 0))
                {

                    plTagComponent* ptMaterialTagComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tMaterial);
                    plTagComponent* ptSkinTagComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptMeshComp->tSkinComponent);
                    gptUI->text("Material: %s, %u", ptMaterialTagComp->acName, ptMeshComp->tMaterial.uIndex);
                    gptUI->text("Skin:     %s", ptSkinTagComp ? ptSkinTagComp->acName : " ");

                    gptUI->vertical_spacing();
                    gptUI->text("Vertex Data (%u verts, %u idx)", pl_sb_size(ptMeshComp->sbtVertexPositions), pl_sb_size(ptMeshComp->sbuIndices));
                    gptUI->indent(15.0f);
                    gptUI->text("%s Positions", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION ? "ACTIVE" : "     ");
                    gptUI->text("%s Normals", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL ? "ACTIVE" : "     ");
                    gptUI->text("%s Tangents", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT ? "ACTIVE" : "     ");
                    gptUI->text("%s Texture Coordinates 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 ? "ACTIVE" : "     ");
                    gptUI->text("%s Texture Coordinates 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 ? "ACTIVE" : "     ");
                    gptUI->text("%s Colors 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0 ? "ACTIVE" : "     ");
                    gptUI->text("%s Colors 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1 ? "ACTIVE" : "     ");
                    gptUI->text("%s Joints 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0 ? "ACTIVE" : "     ");
                    gptUI->text("%s Joints 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1 ? "ACTIVE" : "     ");
                    gptUI->text("%s Weights 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0 ? "ACTIVE" : "     ");
                    gptUI->text("%s Weights 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1 ? "ACTIVE" : "     ");
                    gptUI->unindent(15.0f);
                    gptUI->end_collapsing_header();
                }

                if(ptObjectComp && gptUI->begin_collapsing_header("Object", 0))
                {
                    plTagComponent* ptMeshTagComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tMesh);
                    plTagComponent* ptTransformTagComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptObjectComp->tTransform);

                    gptUI->text("Mesh Entity:      %s, %u", ptMeshTagComp->acName, ptObjectComp->tMesh.uIndex);
                    gptUI->text("Transform Entity: %s, %u", ptTransformTagComp->acName, ptObjectComp->tTransform.uIndex);

                    bool bObjectRenderable = ptObjectComp->tFlags & PL_OBJECT_FLAGS_RENDERABLE;
                    bool bObjectCastShadow = ptObjectComp->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW;
                    bool bObjectDynamic = ptObjectComp->tFlags & PL_OBJECT_FLAGS_DYNAMIC;
                    bool bObjectForeground = ptObjectComp->tFlags & PL_OBJECT_FLAGS_FOREGROUND;
                    bool bObjectUpdateRequired = false;

                    if(gptUI->checkbox("Renderable", &bObjectRenderable))
                    {
                        bObjectUpdateRequired = true;
                        if(bObjectRenderable)
                            ptObjectComp->tFlags |= PL_OBJECT_FLAGS_RENDERABLE;
                        else
                            ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_RENDERABLE;
                    }

                    if(gptUI->checkbox("Cast Shadow", &bObjectCastShadow))
                    {
                        bObjectUpdateRequired = true;
                        if(bObjectCastShadow)
                            ptObjectComp->tFlags |= PL_OBJECT_FLAGS_CAST_SHADOW;
                        else
                            ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_CAST_SHADOW;
                    }

                    if(gptUI->checkbox("Dynamic", &bObjectDynamic))
                    {
                        bObjectUpdateRequired = true;
                        if(bObjectDynamic)
                            ptObjectComp->tFlags |= PL_OBJECT_FLAGS_DYNAMIC;
                        else
                            ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_DYNAMIC;
                    }

                    if(gptUI->checkbox("Foreground", &bObjectForeground))
                    {
                        bObjectUpdateRequired = true;
                        if(bObjectForeground)
                            ptObjectComp->tFlags |= PL_OBJECT_FLAGS_FOREGROUND;
                        else
                            ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_FOREGROUND;
                    }
                    if(bObjectUpdateRequired)
                        gptRenderer->update_scene_objects(uSceneHandle, 1, ptSelectedEntity);
                    gptUI->end_collapsing_header();
                }

                if(ptHierarchyComp && gptUI->begin_collapsing_header("Hierarchy", 0))
                {
                    plTagComponent* ptParentTagComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptHierarchyComp->tParent);
                    gptUI->text("Parent Entity: %s , %u", ptParentTagComp->acName, ptHierarchyComp->tParent.uIndex);
                    gptUI->end_collapsing_header();
                }

                if(ptLightComp && gptUI->begin_collapsing_header("Light", 0))
                {
                    static const char* apcLightTypes[] = {
                        "PL_LIGHT_TYPE_DIRECTIONAL",
                        "PL_LIGHT_TYPE_POINT",
                        "PL_LIGHT_TYPE_SPOT",
                    };
                    gptUI->text("Type: %s", apcLightTypes[ptLightComp->tType]);

                    bool bShowVisualizer = ptLightComp->tFlags & PL_LIGHT_FLAG_VISUALIZER;
                    if(gptUI->checkbox("Visualizer:", &bShowVisualizer))
                    {
                        if(bShowVisualizer)
                            ptLightComp->tFlags |= PL_LIGHT_FLAG_VISUALIZER;
                        else
                            ptLightComp->tFlags &= ~PL_LIGHT_FLAG_VISUALIZER;
                    }

                    gptUI->input_float3("Position:", ptLightComp->tPosition.d, NULL, 0);

                    gptUI->separator_text("Color");
                    gptUI->slider_float("r", &ptLightComp->tColor.x, 0.0f, 1.0f, 0);
                    gptUI->slider_float("g", &ptLightComp->tColor.y, 0.0f, 1.0f, 0);
                    gptUI->slider_float("b", &ptLightComp->tColor.z, 0.0f, 1.0f, 0);

                    gptUI->slider_float("Intensity", &ptLightComp->fIntensity, 0.0f, 20.0f, 0);

                    if(ptLightComp->tType != PL_LIGHT_TYPE_DIRECTIONAL)
                    {
                        gptUI->input_float("Radius:", &ptLightComp->fRadius, NULL, 0);
                        gptUI->input_float("Range", &ptLightComp->fRange, NULL, 0);
                    }

                    if(ptLightComp->tType == PL_LIGHT_TYPE_SPOT)
                    {
                        gptUI->slider_float("Inner Cone Angle:", &ptLightComp->fInnerConeAngle, 0.0f, PL_PI_2, 0);
                        gptUI->slider_float("Outer Cone Angle", &ptLightComp->fOuterConeAngle, 0.0f, PL_PI_2, 0);
                    }


                    if(ptLightComp->tType != PL_LIGHT_TYPE_POINT)
                    {
                        gptUI->separator_text("Direction");
                        gptUI->slider_float("x", &ptLightComp->tDirection.x, -1.0f, 1.0f, 0);
                        gptUI->slider_float("y", &ptLightComp->tDirection.y, -1.0f, 1.0f, 0);
                        gptUI->slider_float("z", &ptLightComp->tDirection.z, -1.0f, 1.0f, 0);
                    }

                    gptUI->separator_text("Shadows");

                    bool bCastShadow = ptLightComp->tFlags & PL_LIGHT_FLAG_CAST_SHADOW;
                    if(gptUI->checkbox("Cast Shadow:", &bCastShadow))
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
                        gptUI->radio_button("Resolution: 128", &iSelection, 0);
                        gptUI->radio_button("Resolution: 256", &iSelection, 1);
                        gptUI->radio_button("Resolution: 512", &iSelection, 2);
                        gptUI->radio_button("Resolution: 1024", &iSelection, 3);
                        gptUI->radio_button("Resolution: 2048", &iSelection, 4);
                        ptLightComp->uShadowResolution = auResolutions[iSelection];
                    }

                    if(ptLightComp->tType == PL_LIGHT_TYPE_DIRECTIONAL)
                    {
                        int iCascadeCount  = (int)ptLightComp->uCascadeCount;
                        if(gptUI->slider_int("Cascades", &iCascadeCount, 1, 4, 0))
                        {
                            ptLightComp->uCascadeCount = (uint32_t)iCascadeCount;
                        }

                        // int iResolution = (int)ptLight->uShadowResolution;
                        gptUI->input_float4("Cascade Splits", ptLightComp->afCascadeSplits, NULL, 0);
                    }
                    gptUI->end_collapsing_header();
                }

                if(ptMaterialComp && gptUI->begin_collapsing_header("Material", 0))
                {
                    bool bMaterialModified = false;
                    if(gptUI->input_float("Roughness", &ptMaterialComp->fRoughness, NULL, 0)) bMaterialModified = true;
                    if(gptUI->input_float("Metalness", &ptMaterialComp->fMetalness, NULL, 0)) bMaterialModified = true;
                    if(gptUI->input_float("Alpha Cutoff", &ptMaterialComp->fAlphaCutoff, NULL, 0)) bMaterialModified = true;
                    if(gptUI->input_float("Normal Map Strength", &ptMaterialComp->fNormalMapStrength, NULL, 0)) bMaterialModified = true;
                    if(gptUI->input_float("Occulusion Map Strength", &ptMaterialComp->fOcclusionMapStrength, NULL, 0)) bMaterialModified = true;
                    if(gptUI->input_float4("Base Factor", ptMaterialComp->tBaseColor.d, NULL, 0)) bMaterialModified = true;
                    if(gptUI->input_float4("Emmissive Factor", ptMaterialComp->tEmissiveColor.d, NULL, 0)) bMaterialModified = true;

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
                    gptUI->text("Blend Mode:                      %s", apcBlendModeNames[ptMaterialComp->tBlendMode]);

                    static const char* apcShaderNames[] = 
                    {
                        "PL_SHADER_TYPE_PBR",
                        "PL_SHADER_TYPE_UNLIT",
                        "PL_SHADER_TYPE_CUSTOM"
                    };
                    gptUI->text("Shader Type:                     %s", apcShaderNames[ptMaterialComp->tShaderType]);
                    gptUI->text("Double Sided:                    %s", ptMaterialComp->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED ? "true" : "false");
  
                    gptUI->vertical_spacing();
                    gptUI->text("Texture Maps");
                    gptUI->indent(15.0f);

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
                        gptUI->text("%s: %s", apcTextureSlotNames[i], ptMaterialComp->atTextureMaps[i].acName[0] == 0 ? " " : "present");
                    }
                    gptUI->unindent(15.0f);
                    gptUI->end_collapsing_header();
                }

                if(ptSkinComp && gptUI->begin_collapsing_header("Skin", 0))
                {
                    if(gptUI->tree_node("Joints", 0))
                    {
                        for(uint32_t i = 0; i < pl_sb_size(ptSkinComp->sbtJoints); i++)
                        {
                            plTagComponent* ptJointTagComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptSkinComp->sbtJoints[i]);
                            gptUI->text("%s", ptJointTagComp->acName);  
                        }
                        gptUI->tree_pop();
                    }
                    gptUI->end_collapsing_header();
                }

                if(ptCameraComp && gptUI->begin_collapsing_header("Camera", 0))
                { 
                    gptUI->text("Near Z:                  %+0.3f", ptCameraComp->fNearZ);
                    gptUI->text("Far Z:                   %+0.3f", ptCameraComp->fFarZ);
                    gptUI->text("Vertical Field of View:  %+0.3f", ptCameraComp->fFieldOfView);
                    gptUI->text("Aspect Ratio:            %+0.3f", ptCameraComp->fAspectRatio);
                    gptUI->text("Pitch:                   %+0.3f", ptCameraComp->fPitch);
                    gptUI->text("Yaw:                     %+0.3f", ptCameraComp->fYaw);
                    gptUI->text("Roll:                    %+0.3f", ptCameraComp->fRoll);
                    gptUI->text("Position: (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPos.x, ptCameraComp->tPos.y, ptCameraComp->tPos.z);
                    gptUI->text("Up:       (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tUpVec.x, ptCameraComp->_tUpVec.y, ptCameraComp->_tUpVec.z);
                    gptUI->text("Forward:  (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tForwardVec.x, ptCameraComp->_tForwardVec.y, ptCameraComp->_tForwardVec.z);
                    gptUI->text("Right:    (%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tRightVec.x, ptCameraComp->_tRightVec.y, ptCameraComp->_tRightVec.z);
                    gptUI->input_float3("Position", ptCameraComp->tPos.d, NULL, 0);
                    gptUI->input_float("Near Z Plane", &ptCameraComp->fNearZ, NULL, 0);
                    gptUI->input_float("Far Z Plane", &ptCameraComp->fFarZ, NULL, 0);
                    gptUI->end_collapsing_header();
                }

                if(ptAnimationComp && gptUI->begin_collapsing_header("Animation", 0))
                { 
                    bool bPlaying = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_PLAYING;
                    bool bLooped = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_LOOPED;
                    if(bLooped && bPlaying)
                        gptUI->text("Status: playing & looped");
                    else if(bPlaying)
                        gptUI->text("Status: playing");
                    else if(bLooped)
                        gptUI->text("Status: looped");
                    else
                        gptUI->text("Status: not playing");
                    if(gptUI->checkbox("Playing", &bPlaying))
                    {
                        if(bPlaying)
                            ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_PLAYING;
                        else
                            ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
                    }
                    if(gptUI->checkbox("Looped", &bLooped))
                    {
                        if(bLooped)
                            ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_LOOPED;
                        else
                            ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_LOOPED;
                    }
                    gptUI->text("Start: %0.3f s", ptAnimationComp->fStart);
                    gptUI->text("End:   %0.3f s", ptAnimationComp->fEnd);
                    gptUI->progress_bar(ptAnimationComp->fTimer / (ptAnimationComp->fEnd - ptAnimationComp->fStart), (plVec2){-1.0f, 0.0f}, NULL);
                    gptUI->text("Speed:   %0.3f s", ptAnimationComp->fSpeed);
                    gptUI->end_collapsing_header();
                }

                if(ptIKComp && gptUI->begin_collapsing_header("Inverse Kinematics", 0))
                { 
                    plTagComponent* ptTargetComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TAG, ptIKComp->tTarget);
                    gptUI->text("Target Entity: %s , %u", ptTargetComp->acName, ptIKComp->tTarget.uIndex);

                    int iChainLength = (int)ptIKComp->uChainLength;
                    gptUI->slider_int("Chain Length", &iChainLength, 1, 5, 0);

                    ptIKComp->uChainLength = (uint32_t)iChainLength;

                    // gptUI->text("Chain Length: %u", ptIKComp->uChainLength);
                    gptUI->text("Iterations: %u", ptIKComp->uIterationCount);

                    gptUI->checkbox("Enabled", &ptIKComp->bEnabled);
                    gptUI->end_collapsing_header();
                }

                gptUI->pop_id();
            }
            
            gptUI->end_child();
        }

        gptUI->end_window();
    }

    return bResult;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ecs_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plEcsToolsI tApi = {
        .show_ecs_window = pl_show_ecs_window
    };
    pl_set_api(ptApiRegistry, plEcsToolsI, &tApi);

    gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptRenderer = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptUI       = pl_get_api_latest(ptApiRegistry, plUiI);
    gptECS      = pl_get_api_latest(ptApiRegistry, plEcsI);
}

PL_EXPORT void
pl_unload_ecs_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plEcsToolsI* ptApi = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD
    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif
#endif