/*
   pl_ecs_tools_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
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
#include "pl_animation_ext.h"
#include "pl_camera_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ui_ext.h"
#include "pl_physics_ext.h"
#include "pl_mesh_ext.h"
#include "pl_shader_interop_renderer.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

static const plMemoryI*   gptMemory = NULL;
static const plEcsI*      gptECS      = NULL;
static const plAnimationI*  gptAnimation = NULL;
static const plUiI*       gptUI       = NULL;
static const plRendererI* gptRenderer = NULL;
static const plPhysicsI*  gptPhysics = NULL;
static const plCameraI*   gptCamera = NULL;
static const plMeshI*     gptMesh = NULL;

#ifndef PL_DS_ALLOC
    
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"
#endif

#define PL_ICON_FA_MAGNIFYING_GLASS "\xef\x80\x82"	// U+f002
#define PL_ICON_FA_FILTER "\xef\x82\xb0"	// U+f0b0
#define PL_ICON_FA_SITEMAP "\xef\x83\xa8"	// U+f0e8
#define PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT "\xef\x81\x87"	// U+f047
#define PL_ICON_FA_CUBE "\xef\x86\xb2"	// U+f1b2
#define PL_ICON_FA_GHOST "\xef\x9b\xa2"	// U+f6e2
#define PL_ICON_FA_PALETTE "\xef\x94\xbf"	// U+f53f
#define PL_ICON_FA_MAP "\xef\x89\xb9"	// U+f279
#define PL_ICON_FA_CAMERA "\xef\x80\xb0"	// U+f030
#define PL_ICON_FA_PLAY "\xef\x81\x8b"	// U+f04b
#define PL_ICON_FA_DRAW_POLYGON "\xef\x97\xae"	// U+f5ee
#define PL_ICON_FA_LIGHTBULB "\xef\x83\xab"	// U+f0eb
#define PL_ICON_FA_MAP_PIN "\xef\x89\xb6"	// U+f276
#define PL_ICON_FA_PERSON "\xef\x86\x83"	// U+f183
#define PL_ICON_FA_CODE "\xef\x84\xa1"	// U+f121
#define PL_ICON_FA_BOXES_STACKED "\xef\x91\xa8"	// U+f468
#define PL_ICON_FA_WIND "\xef\x9c\xae"	// U+f72e

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plEcsToolsContext
{
    plUiTextFilter tFilter;
} plEcsToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plEcsToolsContext* gptEcsToolsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static bool
pl_show_ecs_window(plComponentLibrary* ptLibrary, plEntity* ptSelectedEntity, plScene* ptScene, bool* pbShowWindow)
{
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
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        if(gptUI->input_text_hint(PL_ICON_FA_MAGNIFYING_GLASS, "Filter (inc,-exc)", gptEcsToolsCtx->tFilter.acInputBuffer, 256, 0))
        {
            gptUI->text_filter_build(&gptEcsToolsCtx->tFilter);
        }

        static uint32_t uComponentFilter = 0;
        static const char* apcComponentNames[] = {
            "None",
            PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform",
            PL_ICON_FA_CUBE " Mesh",
            PL_ICON_FA_GHOST " Object",
            PL_ICON_FA_SITEMAP " Hierarchy",
            PL_ICON_FA_PALETTE " Material",
            PL_ICON_FA_MAP " Skin",
            PL_ICON_FA_CAMERA " Camera",
            PL_ICON_FA_PLAY " Animation",
            PL_ICON_FA_DRAW_POLYGON " Inverse Kinematics",
            PL_ICON_FA_LIGHTBULB " Light",
            PL_ICON_FA_MAP_PIN " Environment Probe",
            PL_ICON_FA_PERSON " Humanoid",
            PL_ICON_FA_CODE " Script",
            PL_ICON_FA_BOXES_STACKED " Rigid Body Physics",
            PL_ICON_FA_WIND " Force Field",
        };

        // const plEcsTypeKey tTransformComponentType = gptAnimation->get_ecs_type_key_transform();

        const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();
        const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
        const plEcsTypeKey tObjectComponentType = gptRenderer->get_ecs_type_key_object();
        const plEcsTypeKey tHierarchyComponentType = gptECS->get_ecs_type_key_hierarchy();
        const plEcsTypeKey tMaterialComponentType = gptRenderer->get_ecs_type_key_material();
        const plEcsTypeKey tSkinComponentType = gptRenderer->get_ecs_type_key_skin();
        const plEcsTypeKey tCameraComponentType = gptCamera->get_ecs_type_key();
        const plEcsTypeKey tAnimationComponentType = gptAnimation->get_ecs_type_key_animation();
        const plEcsTypeKey tInverseKinematicsComponentType = gptAnimation->get_ecs_type_key_inverse_kinematics();
        const plEcsTypeKey tLightComponentType = gptRenderer->get_ecs_type_key_light();
        const plEcsTypeKey tEnvironmentProbeComponentType = gptRenderer->get_ecs_type_key_environment_probe();
        const plEcsTypeKey tHumanoidComponentType = gptAnimation->get_ecs_type_key_humanoid();
        const plEcsTypeKey tScriptComponentType = gptECS->get_ecs_type_key_script();
        const plEcsTypeKey tRigidBodyComponentType = gptPhysics->get_ecs_type_key_rigid_body_physics();
        const plEcsTypeKey tForceFieldComponentType = gptPhysics->get_ecs_type_key_force_field();

        plEcsTypeKey atComponentTypes[] = {
            INT32_MAX,
            tTransformComponentType,
            tMeshComponentType,
            tObjectComponentType,
            tHierarchyComponentType,
            tMaterialComponentType,
            tSkinComponentType,
            tCameraComponentType,
            tAnimationComponentType,
            tInverseKinematicsComponentType,
            tLightComponentType,
            tEnvironmentProbeComponentType,
            tHumanoidComponentType,
            tScriptComponentType,
            tRigidBodyComponentType,
            tForceFieldComponentType
        };

        bool abCombo[16] = {0};
        abCombo[uComponentFilter] = true;
        if(gptUI->begin_combo(PL_ICON_FA_FILTER, apcComponentNames[uComponentFilter], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
        {
            for(uint32_t i = 0; i < 16; i++)
            {
                if(gptUI->selectable(apcComponentNames[i], &abCombo[i], 0))
                {
                    uComponentFilter = i;
                    gptUI->close_current_popup();
                } 
            }
            gptUI->end_combo();
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 105.0f, 2, pfRatios);

        if(gptUI->begin_child("Entities", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            plTagComponent* ptTags = NULL;
            const plEntity* ptEntities = NULL;
            const uint32_t uEntityCount = gptECS->get_components(ptLibrary, gptECS->get_ecs_type_key_tag(), (void**)&ptTags, &ptEntities);

            if(uComponentFilter != 0 || gptUI->text_filter_active(&gptEcsToolsCtx->tFilter))
            {
                for(uint32_t i = 0; i < uEntityCount; i++)
                {
                    if(gptUI->text_filter_pass(&gptEcsToolsCtx->tFilter, ptTags[i].acName, NULL))
                    {
                        bool bSelected = ptSelectedEntity->uData == ptEntities[i].uData;

                        plTagComponent*               ptTagComp           = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptEntities[i]);
                        plTransformComponent*         ptTransformComp     = gptECS->get_component(ptLibrary, tTransformComponentType, ptEntities[i]);
                        plMeshComponent*              ptMeshComp          = gptECS->get_component(ptLibrary, tMeshComponentType, ptEntities[i]);
                        plObjectComponent*            ptObjectComp        = gptECS->get_component(ptLibrary, tObjectComponentType, ptEntities[i]);
                        plHierarchyComponent*         ptHierarchyComp     = gptECS->get_component(ptLibrary, tHierarchyComponentType, ptEntities[i]);
                        plMaterialComponent*          ptMaterialComp      = gptECS->get_component(ptLibrary, tMaterialComponentType, ptEntities[i]);
                        plSkinComponent*              ptSkinComp          = gptECS->get_component(ptLibrary, tSkinComponentType, ptEntities[i]);
                        plCamera*            ptCameraComp        = gptECS->get_component(ptLibrary, tCameraComponentType, ptEntities[i]);
                        plAnimationComponent*         ptAnimationComp     = gptECS->get_component(ptLibrary, tAnimationComponentType, ptEntities[i]);
                        plInverseKinematicsComponent* ptIKComp            = gptECS->get_component(ptLibrary, tInverseKinematicsComponentType, ptEntities[i]);
                        plLightComponent*             ptLightComp         = gptECS->get_component(ptLibrary, tLightComponentType, ptEntities[i]);
                        plEnvironmentProbeComponent*  ptProbeComp         = gptECS->get_component(ptLibrary, tEnvironmentProbeComponentType, ptEntities[i]);
                        plHumanoidComponent*          ptHumanComp         = gptECS->get_component(ptLibrary, tHumanoidComponentType, ptEntities[i]);
                        plScriptComponent*            ptScriptComp        = gptECS->get_component(ptLibrary, tScriptComponentType, ptEntities[i]);
                        plRigidBodyPhysicsComponent*  ptRigidComp         = gptECS->get_component(ptLibrary, tRigidBodyComponentType, ptEntities[i]);
                        plForceFieldComponent*        ptForceField        = gptECS->get_component(ptLibrary, tForceFieldComponentType, ptEntities[i]);

                        if(uComponentFilter != 0)
                        {
                            void* pComponent = gptECS->get_component(ptLibrary, atComponentTypes[uComponentFilter], ptEntities[i]);
                            if(pComponent == NULL)
                                continue;
                        }

                        char atBuffer[1024] = {0};
                        pl_sprintf(atBuffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s %s, %u",
                            ptHierarchyComp ? PL_ICON_FA_SITEMAP : "",
                            ptTransformComp ? PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT : "",
                            ptMeshComp ? PL_ICON_FA_CUBE : "",
                            ptObjectComp ? PL_ICON_FA_GHOST : "",
                            ptMaterialComp ? PL_ICON_FA_PALETTE : "",
                            ptSkinComp ? PL_ICON_FA_MAP : "",
                            ptCameraComp ? PL_ICON_FA_CAMERA : "",
                            ptAnimationComp ? PL_ICON_FA_PLAY : "",
                            ptIKComp ? PL_ICON_FA_DRAW_POLYGON : "",
                            ptLightComp ? PL_ICON_FA_LIGHTBULB : "",
                            ptProbeComp ? PL_ICON_FA_MAP_PIN : "",
                            ptHumanComp ? PL_ICON_FA_PERSON : "",
                            ptScriptComp ? PL_ICON_FA_CODE : "",
                            ptRigidComp ? PL_ICON_FA_BOXES_STACKED : "",
                            ptForceField ? PL_ICON_FA_WIND : "",
                            ptTags[i].acName,
                            ptEntities[i].uIndex);
                        if(gptUI->selectable(atBuffer, &bSelected, 0))
                        {
                            if(bSelected)
                            {
                                *ptSelectedEntity = ptEntities[i];
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
            }
            else
            {
                plUiClipper tClipper = {(uint32_t)uEntityCount};
                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        bool bSelected = ptSelectedEntity->uData == ptEntities[i].uData;

                        plTagComponent*               ptTagComp           = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptEntities[i]);
                        plTransformComponent*         ptTransformComp     = gptECS->get_component(ptLibrary, tTransformComponentType, ptEntities[i]);
                        plMeshComponent*              ptMeshComp          = gptECS->get_component(ptLibrary, tMeshComponentType, ptEntities[i]);
                        plObjectComponent*            ptObjectComp        = gptECS->get_component(ptLibrary, tObjectComponentType, ptEntities[i]);
                        plHierarchyComponent*         ptHierarchyComp     = gptECS->get_component(ptLibrary, tHierarchyComponentType, ptEntities[i]);
                        plMaterialComponent*          ptMaterialComp      = gptECS->get_component(ptLibrary, tMaterialComponentType, ptEntities[i]);
                        plSkinComponent*              ptSkinComp          = gptECS->get_component(ptLibrary, tSkinComponentType, ptEntities[i]);
                        plCamera*            ptCameraComp        = gptECS->get_component(ptLibrary, tCameraComponentType, ptEntities[i]);
                        plAnimationComponent*         ptAnimationComp     = gptECS->get_component(ptLibrary, tAnimationComponentType, ptEntities[i]);
                        plInverseKinematicsComponent* ptIKComp            = gptECS->get_component(ptLibrary, tInverseKinematicsComponentType, ptEntities[i]);
                        plLightComponent*             ptLightComp         = gptECS->get_component(ptLibrary, tLightComponentType, ptEntities[i]);
                        plEnvironmentProbeComponent*  ptProbeComp         = gptECS->get_component(ptLibrary, tEnvironmentProbeComponentType, ptEntities[i]);
                        plHumanoidComponent*          ptHumanComp         = gptECS->get_component(ptLibrary, tHumanoidComponentType, ptEntities[i]);
                        plScriptComponent*            ptScriptComp        = gptECS->get_component(ptLibrary, tScriptComponentType, ptEntities[i]);
                        plRigidBodyPhysicsComponent*  ptRigidComp         = gptECS->get_component(ptLibrary, tRigidBodyComponentType, ptEntities[i]);
                        plForceFieldComponent*        ptForceField        = gptECS->get_component(ptLibrary, tForceFieldComponentType, ptEntities[i]);

                        char atBuffer[1024] = {0};
                        pl_sprintf(atBuffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s %s, %u",
                            ptHierarchyComp ? PL_ICON_FA_SITEMAP : "",
                            ptTransformComp ? PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT : "",
                            ptMeshComp ? PL_ICON_FA_CUBE : "",
                            ptObjectComp ? PL_ICON_FA_GHOST : "",
                            ptMaterialComp ? PL_ICON_FA_PALETTE : "",
                            ptSkinComp ? PL_ICON_FA_MAP : "",
                            ptCameraComp ? PL_ICON_FA_CAMERA : "",
                            ptAnimationComp ? PL_ICON_FA_PLAY : "",
                            ptIKComp ? PL_ICON_FA_DRAW_POLYGON : "",
                            ptLightComp ? PL_ICON_FA_LIGHTBULB : "",
                            ptProbeComp ? PL_ICON_FA_MAP_PIN : "",
                            ptHumanComp ? PL_ICON_FA_PERSON : "",
                            ptScriptComp ? PL_ICON_FA_CODE : "",
                            ptRigidComp ? PL_ICON_FA_BOXES_STACKED : "",
                            ptForceField ? PL_ICON_FA_WIND : "",
                            ptTags[i].acName,
                            ptEntities[i].uIndex);
                        if(gptUI->selectable(atBuffer, &bSelected, 0))
                        {
                            if(bSelected)
                            {
                                *ptSelectedEntity = ptEntities[i];
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
            }

            gptUI->end_child();
        }

        if(gptUI->begin_child("Components", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            if(ptSelectedEntity->uData != UINT64_MAX)
            {
                gptUI->push_id_uint(ptSelectedEntity->uIndex);

                plTagComponent*               ptTagComp           = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), *ptSelectedEntity);
                plTransformComponent*         ptTransformComp     = gptECS->get_component(ptLibrary, tTransformComponentType, *ptSelectedEntity);
                plMeshComponent*              ptMeshComp          = gptECS->get_component(ptLibrary, tMeshComponentType, *ptSelectedEntity);
                plObjectComponent*            ptObjectComp        = gptECS->get_component(ptLibrary, tObjectComponentType, *ptSelectedEntity);
                plHierarchyComponent*         ptHierarchyComp     = gptECS->get_component(ptLibrary, tHierarchyComponentType, *ptSelectedEntity);
                plMaterialComponent*          ptMaterialComp      = gptECS->get_component(ptLibrary, tMaterialComponentType, *ptSelectedEntity);
                plSkinComponent*              ptSkinComp          = gptECS->get_component(ptLibrary, tSkinComponentType, *ptSelectedEntity);
                plCamera*            ptCameraComp        = gptECS->get_component(ptLibrary, tCameraComponentType, *ptSelectedEntity);
                plAnimationComponent*         ptAnimationComp     = gptECS->get_component(ptLibrary, tAnimationComponentType, *ptSelectedEntity);
                plInverseKinematicsComponent* ptIKComp            = gptECS->get_component(ptLibrary, tInverseKinematicsComponentType, *ptSelectedEntity);
                plLightComponent*             ptLightComp         = gptECS->get_component(ptLibrary, tLightComponentType, *ptSelectedEntity);
                plEnvironmentProbeComponent*  ptProbeComp         = gptECS->get_component(ptLibrary, tEnvironmentProbeComponentType, *ptSelectedEntity);
                plHumanoidComponent*          ptHumanComp         = gptECS->get_component(ptLibrary, tHumanoidComponentType, *ptSelectedEntity);
                plScriptComponent*            ptScriptComp        = gptECS->get_component(ptLibrary, tScriptComponentType, *ptSelectedEntity);
                plRigidBodyPhysicsComponent*  ptRigidComp         = gptECS->get_component(ptLibrary, tRigidBodyComponentType, *ptSelectedEntity);
                plForceFieldComponent*        ptForceField        = gptECS->get_component(ptLibrary, tForceFieldComponentType, *ptSelectedEntity);

                if(ptObjectComp)
                {
                    gptUI->layout_dynamic(0.0f, 2);
                    gptUI->text("Entity: %u, %u", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);
                    if(gptUI->button("Delete"))
                    {
                        gptRenderer->outline_entities(ptScene, 0, NULL);
                        gptRenderer->remove_objects_from_scene(ptScene, 1, ptSelectedEntity);
                        ptSelectedEntity->uData = UINT64_MAX;
                    }
                    
                }
                else
                {
                    gptUI->text("Entity: %u, %u", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);
                }

                if(ptTransformComp && ptRigidComp == NULL)
                {
                    gptUI->layout_dynamic(0.0f, 1);
                    if(gptUI->button("Add Rigid Body Component"))
                    {
                        plRigidBodyPhysicsComponent* ptRigid = gptECS->add_component(ptLibrary, tRigidBodyComponentType, *ptSelectedEntity);
                        ptRigid->tFlags |= PL_RIGID_BODY_PHYSICS_FLAG_START_SLEEPING;
                        ptRigid->fMass = 10.0f;
                        ptRigid->tShape = PL_COLLISION_SHAPE_SPHERE;
                        
                        ptRigid->tLocalOffset.x = 0.0f;
                        ptRigid->tLocalOffset.y = 0.0f;
                        ptRigid->tLocalOffset.z = 0.0f;
                        ptRigid->tExtents.x = 1.0f;
                        ptRigid->tExtents.y = 1.0f;
                        ptRigid->tExtents.z = 1.0f;
                    }
                }

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

                if(ptTagComp && gptUI->begin_collapsing_header("Tag", 0))
                {
                    gptUI->text("Name: %s", ptTagComp->acName);
                    gptUI->end_collapsing_header();
                }

                if(ptScriptComp && gptUI->begin_collapsing_header("Script", 0))
                {
                    gptUI->text("File: %s", ptScriptComp->acFile);

                    bool bPlaying = ptScriptComp->tFlags & PL_SCRIPT_FLAG_PLAYING;
                    if(gptUI->checkbox("Playing", &bPlaying))
                    {
                        if(bPlaying)
                        ptScriptComp->tFlags |= PL_SCRIPT_FLAG_PLAYING;
                        else
                        ptScriptComp->tFlags &= ~PL_SCRIPT_FLAG_PLAYING;
                    }

                    bool bPlayOnce = ptScriptComp->tFlags & PL_SCRIPT_FLAG_PLAY_ONCE;
                    if(gptUI->checkbox("Play Once", &bPlayOnce))
                    {
                        if(bPlayOnce)
                            ptScriptComp->tFlags |= PL_SCRIPT_FLAG_PLAY_ONCE;
                        else
                            ptScriptComp->tFlags &= ~PL_SCRIPT_FLAG_PLAY_ONCE;
                    }

                    bool bReloadable = ptScriptComp->tFlags & PL_SCRIPT_FLAG_RELOADABLE;
                    if(gptUI->checkbox("Reloadable", &bReloadable))
                    {
                        if(bReloadable)
                            ptScriptComp->tFlags |= PL_SCRIPT_FLAG_RELOADABLE;
                        else
                            ptScriptComp->tFlags &= ~PL_SCRIPT_FLAG_RELOADABLE;
                    }
                    gptUI->end_collapsing_header();
                }

                if(ptHumanComp && gptUI->begin_collapsing_header("Humanoid", 0))
                {
                    gptUI->end_collapsing_header();
                }

                if(ptRigidComp && gptUI->begin_collapsing_header("Rigid Body Physics", 0))
                {
                    bool bNoSleeping = ptRigidComp->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING;
                    if(gptUI->checkbox("No Sleeping", &bNoSleeping))
                    {
                        if(bNoSleeping)
                        {
                            ptRigidComp->tFlags |= PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING;
                            gptPhysics->wake_up_body(ptLibrary, *ptSelectedEntity);
                        }
                        else
                            ptRigidComp->tFlags &= ~PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING;
                    }

                    bool bKinematic = ptRigidComp->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC;
                    if(gptUI->checkbox("Kinematic", &bKinematic))
                    {
                        if(bKinematic)
                        {
                            ptRigidComp->tFlags |= PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC;
                            gptPhysics->wake_up_body(ptLibrary, *ptSelectedEntity);
                        }
                        else
                            ptRigidComp->tFlags &= ~PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC;
                    }
                    gptUI->input_float("Mass", &ptRigidComp->fMass, "%g", 0);
                    gptUI->slider_float("Friction", &ptRigidComp->fFriction, 0.0f, 1.0f, 0);
                    gptUI->slider_float("Restitution", &ptRigidComp->fRestitution, 0.0f, 1.0f, 0);
                    gptUI->slider_float("Linear Damping", &ptRigidComp->fLinearDamping, 0.0f, 1.0f, 0);
                    gptUI->slider_float("Angluar Damping", &ptRigidComp->fAngularDamping, 0.0f, 1.0f, 0);
                    gptUI->input_float3("Gravity", ptRigidComp->tGravity.d, NULL, 0);
                    gptUI->input_float3("Local Offset", ptRigidComp->tLocalOffset.d, "%g", 0);

                    gptUI->vertical_spacing();

                    gptUI->separator_text("Collision Shape");
                    gptUI->radio_button("Box", &ptRigidComp->tShape, PL_COLLISION_SHAPE_BOX);
                    gptUI->radio_button("Sphere", &ptRigidComp->tShape, PL_COLLISION_SHAPE_SPHERE);

                    gptUI->vertical_spacing();

                    if(ptRigidComp->tShape == PL_COLLISION_SHAPE_BOX)
                    {
                        gptUI->input_float3("Extents", ptRigidComp->tExtents.d, "%g", 0);
                    }
                    else if(ptRigidComp->tShape == PL_COLLISION_SHAPE_SPHERE)
                    {
                        gptUI->input_float("Radius", &ptRigidComp->fRadius, "%g", 0);
                    }

                    static plVec3 tPoint = {0};
                    static plVec3 tForce = {1000.0f};
                    static plVec3 tTorque = {0.0f, 100.0f, 0.0f};
                    static plVec3 tLinearVelocity = {0.0f, 0.0f, 0.0f};
                    static plVec3 tAngularVelocity = {0.0f, 0.0f, 0.0f};
                    gptUI->input_float3("Point", tPoint.d, NULL, 0);
                    gptUI->input_float3("Force", tForce.d, NULL, 0);
                    gptUI->input_float3("Torque", tTorque.d, NULL, 0);
                    gptUI->input_float3("Velocity", tLinearVelocity.d, NULL, 0);
                    gptUI->input_float3("Angular Velocity", tAngularVelocity.d, NULL, 0);

                    gptUI->push_theme_color(PL_UI_COLOR_BUTTON, (plVec4){0.02f, 0.51f, 0.10f, 1.00f});
                    gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, (plVec4){ 0.02f, 0.61f, 0.10f, 1.00f});
                    gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, (plVec4){0.02f, 0.87f, 0.10f, 1.00f});

                    gptUI->layout_dynamic(0.0f, 2);
                    if(gptUI->button("Stop"))
                    {
                        gptPhysics->set_linear_velocity(ptLibrary, *ptSelectedEntity, (plVec3){0});
                        gptPhysics->set_angular_velocity(ptLibrary, *ptSelectedEntity, (plVec3){0});
                    }
                    
                    if(gptUI->button("Set Velocity"))          gptPhysics->set_linear_velocity(ptLibrary, *ptSelectedEntity, tLinearVelocity);
                    if(gptUI->button("Set A. Velocity"))       gptPhysics->set_angular_velocity(ptLibrary, *ptSelectedEntity, tAngularVelocity);
                    if(gptUI->button("torque"))                gptPhysics->apply_torque(ptLibrary, *ptSelectedEntity, tTorque);
                    if(gptUI->button("impulse torque"))        gptPhysics->apply_impulse_torque(ptLibrary, *ptSelectedEntity, tTorque);
                    if(gptUI->button("force"))                 gptPhysics->apply_force(ptLibrary, *ptSelectedEntity, tForce);
                    if(gptUI->button("force at point"))        gptPhysics->apply_force_at_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                    if(gptUI->button("force at body point"))   gptPhysics->apply_force_at_body_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                    if(gptUI->button("impulse"))               gptPhysics->apply_impulse(ptLibrary, *ptSelectedEntity, tForce);
                    if(gptUI->button("impulse at point"))      gptPhysics->apply_impulse_at_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                    if(gptUI->button("impulse at body point")) gptPhysics->apply_impulse_at_body_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                    if(gptUI->button("wake up"))               gptPhysics->wake_up_body(ptLibrary, *ptSelectedEntity);
                    if(gptUI->button("sleep"))                 gptPhysics->sleep_body(ptLibrary, *ptSelectedEntity);
                    gptUI->invisible_button("not_used", (plVec2){1.0f, 1.0f});

                    gptUI->pop_theme_color(3);

                    gptUI->end_collapsing_header();
                }

                if(ptProbeComp && gptUI->begin_collapsing_header("Environment Probe", 0))
                {
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
                    gptUI->input_float("Range", &ptProbeComp->fRange, NULL, 0);

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


                if(ptForceField && gptUI->begin_collapsing_header("Force Field", 0))
                {
                    gptUI->radio_button("Type: PL_FORCE_FIELD_TYPE_POINT", &ptForceField->tType, PL_FORCE_FIELD_TYPE_POINT);
                    gptUI->radio_button("Type: PL_FORCE_FIELD_TYPE_PLANE", &ptForceField->tType, PL_FORCE_FIELD_TYPE_PLANE);
                    gptUI->input_float("Gravity", &ptForceField->fGravity, NULL, 0);
                    gptUI->input_float("Range", &ptForceField->fRange, NULL, 0);
                    gptUI->end_collapsing_header();
                }

                if(ptMeshComp && gptUI->begin_collapsing_header("Mesh", 0))
                {

                    plTagComponent* ptMaterialTagComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptMeshComp->tMaterial);
                    plTagComponent* ptSkinTagComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptMeshComp->tSkinComponent);
                    gptUI->text("Material: %s, %u", ptMaterialTagComp->acName, ptMeshComp->tMaterial.uIndex);
                    gptUI->text("Skin:     %s, %u", ptSkinTagComp ? ptSkinTagComp->acName : " ", ptSkinTagComp ? ptMeshComp->tSkinComponent.uIndex : 0);

                    gptUI->vertical_spacing();
                    gptUI->text("Vertex Data (%u verts, %u idx)", (uint32_t)ptMeshComp->szVertexCount, (uint32_t)ptMeshComp->szIndexCount);
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
                    plTagComponent* ptMeshTagComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptObjectComp->tMesh);
                    plTagComponent* ptTransformTagComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptObjectComp->tTransform);

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
                        gptRenderer->update_scene_objects(ptScene, 1, ptSelectedEntity);
                    gptUI->end_collapsing_header();
                }

                if(ptHierarchyComp && gptUI->begin_collapsing_header("Hierarchy", 0))
                {
                    plTagComponent* ptParentTagComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptHierarchyComp->tParent);
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
                    gptUI->labeled_text("Type", "%s", apcLightTypes[ptLightComp->tType]);

                    bool bShowVisualizer = ptLightComp->tFlags & PL_LIGHT_FLAG_VISUALIZER;
                    if(gptUI->checkbox("Visualizer", &bShowVisualizer))
                    {
                        if(bShowVisualizer)
                            ptLightComp->tFlags |= PL_LIGHT_FLAG_VISUALIZER;
                        else
                            ptLightComp->tFlags &= ~PL_LIGHT_FLAG_VISUALIZER;
                    }

                    gptUI->input_float3("Position", ptLightComp->tPosition.d, NULL, 0);

                    gptUI->separator_text("Color");
                    gptUI->slider_float("r", &ptLightComp->tColor.x, 0.0f, 1.0f, 0);
                    gptUI->slider_float("g", &ptLightComp->tColor.y, 0.0f, 1.0f, 0);
                    gptUI->slider_float("b", &ptLightComp->tColor.z, 0.0f, 1.0f, 0);

                    gptUI->slider_float("Intensity", &ptLightComp->fIntensity, 0.0f, 20.0f, 0);

                    if(ptLightComp->tType != PL_LIGHT_TYPE_DIRECTIONAL)
                    {
                        gptUI->input_float("Radius", &ptLightComp->fRadius, NULL, 0);
                        gptUI->input_float("Range", &ptLightComp->fRange, NULL, 0);
                    }

                    if(ptLightComp->tType == PL_LIGHT_TYPE_SPOT)
                    {
                        gptUI->slider_float("Inner Cone Angle", &ptLightComp->fInnerConeAngle, 0.0f, PL_PI_2, 0);
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
                    if(gptUI->checkbox("Cast Shadow", &bCastShadow))
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
                            2048,
                            4096,
                        };
                        int iSelection = 0;
                        if(ptLightComp->uShadowResolution == 128)       iSelection = 0;
                        else if(ptLightComp->uShadowResolution == 256)  iSelection = 1;
                        else if(ptLightComp->uShadowResolution == 512)  iSelection = 2;
                        else if(ptLightComp->uShadowResolution == 1024) iSelection = 3;
                        else if(ptLightComp->uShadowResolution == 2048) iSelection = 4;
                        else if(ptLightComp->uShadowResolution == 4096) iSelection = 5;
                        gptUI->radio_button("Resolution: 128", &iSelection, 0);
                        gptUI->radio_button("Resolution: 256", &iSelection, 1);
                        gptUI->radio_button("Resolution: 512", &iSelection, 2);
                        gptUI->radio_button("Resolution: 1024", &iSelection, 3);
                        gptUI->radio_button("Resolution: 2048", &iSelection, 4);
                        gptUI->radio_button("Resolution: 4096", &iSelection, 5);
                        ptLightComp->uShadowResolution = auResolutions[iSelection];
                    }

                    if(ptLightComp->tType == PL_LIGHT_TYPE_DIRECTIONAL)
                    {
                        gptUI->slider_uint("Cascades", &ptLightComp->uCascadeCount, 1, 4, 0);
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
                        gptRenderer->update_scene_materials(ptScene, 1, ptSelectedEntity);

                    static const char* apcBlendModeNames[] = 
                    {
                        "PL_MATERIAL_BLEND_MODE_OPAQUE",
                        "PL_MATERIAL_BLEND_MODE_ALPHA",
                        "PL_MATERIAL_BLEND_MODE_PREMULTIPLIED",
                        "PL_MATERIAL_BLEND_MODE_ADDITIVE",
                        "PL_MATERIAL_BLEND_MODE_MULTIPLY",
                        "PL_MATERIAL_BLEND_MODE_CLIP_MASK"
                    };
                    gptUI->labeled_text("Blend Mode", "%s", apcBlendModeNames[ptMaterialComp->tBlendMode]);

                    static const char* apcShaderNames[] = 
                    {
                        "PL_SHADER_TYPE_PBR",
                        "PL_SHADER_TYPE_UNLIT",
                        "PL_SHADER_TYPE_CUSTOM"
                    };
                    gptUI->labeled_text("Shader Type", "%s", apcShaderNames[ptMaterialComp->tShaderType]);
                    gptUI->labeled_text("Double Sided", "%s", ptMaterialComp->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED ? "true" : "false");
  
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
                            plTagComponent* ptJointTagComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptSkinComp->sbtJoints[i]);
                            gptUI->text("%s", ptJointTagComp->acName);  
                        }
                        gptUI->tree_pop();
                    }
                    gptUI->end_collapsing_header();
                }

                if(ptCameraComp && gptUI->begin_collapsing_header("Camera", 0))
                { 
                    gptUI->labeled_text("Near Z", "%+0.3f", ptCameraComp->fNearZ);
                    gptUI->labeled_text("Far Z", "%+0.3f", ptCameraComp->fFarZ);
                    gptUI->labeled_text("Vertical Field of View", "%+0.3f", ptCameraComp->fFieldOfView);
                    gptUI->labeled_text("Aspect Ratio", "%+0.3f", ptCameraComp->fAspectRatio);
                    gptUI->labeled_text("Pitch", "%+0.3f", ptCameraComp->fPitch);
                    gptUI->labeled_text("Yaw", "%+0.3f", ptCameraComp->fYaw);
                    gptUI->labeled_text("Roll", "%+0.3f", ptCameraComp->fRoll);
                    gptUI->labeled_text("Position", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPos.x, ptCameraComp->tPos.y, ptCameraComp->tPos.z);
                    gptUI->labeled_text("Up", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tUpVec.x, ptCameraComp->_tUpVec.y, ptCameraComp->_tUpVec.z);
                    gptUI->labeled_text("Forward", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tForwardVec.x, ptCameraComp->_tForwardVec.y, ptCameraComp->_tForwardVec.z);
                    gptUI->labeled_text("Right", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tRightVec.x, ptCameraComp->_tRightVec.y, ptCameraComp->_tRightVec.z);
                    gptUI->input_float3("Position", ptCameraComp->tPos.d, NULL, 0);
                    gptUI->input_float("Near Z Plane", &ptCameraComp->fNearZ, NULL, 0);
                    gptUI->input_float("Far Z Plane", &ptCameraComp->fFarZ, NULL, 0);
                    gptUI->end_collapsing_header();
                }

                if(ptAnimationComp && gptUI->begin_collapsing_header("Animation", 0))
                { 
                    bool bPlaying = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_PLAYING;
                    bool bLooped = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_LOOPED;
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
                    gptUI->labeled_text("Start", "%0.3f s", ptAnimationComp->fStart);
                    gptUI->labeled_text("End", "%0.3f s", ptAnimationComp->fEnd);
                    // gptUI->labeled_text("Speed", "%0.3f s", ptAnimationComp->fSpeed);
                    gptUI->slider_float("Speed", &ptAnimationComp->fSpeed, 0.0f, 2.0f, 0);
                    gptUI->slider_float("Time", &ptAnimationComp->fTimer, ptAnimationComp->fStart, ptAnimationComp->fEnd, 0);
                    gptUI->progress_bar(ptAnimationComp->fTimer / (ptAnimationComp->fEnd - ptAnimationComp->fStart), (plVec2){-1.0f, 0.0f}, NULL);
                    gptUI->end_collapsing_header();
                }

                if(ptIKComp && gptUI->begin_collapsing_header("Inverse Kinematics", 0))
                { 
                    plTagComponent* ptTargetComp = gptECS->get_component(ptLibrary, gptECS->get_ecs_type_key_tag(), ptIKComp->tTarget);
                    gptUI->text("Target Entity: %s , %u", ptTargetComp->acName, ptIKComp->tTarget.uIndex);
                    gptUI->slider_uint("Chain Length", &ptIKComp->uChainLength, 1, 5, 0);
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

void
pl_ecs_tools_initialize(void)
{

}

void
pl_ecs_tools_cleanup(void)
{
    gptUI->text_filter_cleanup(&gptEcsToolsCtx->tFilter);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ecs_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plEcsToolsI tApi = {
        .show_ecs_window = pl_show_ecs_window,
        .initialize      = pl_ecs_tools_initialize,
        .cleanup         = pl_ecs_tools_cleanup
    };
    pl_set_api(ptApiRegistry, plEcsToolsI, &tApi);

    gptMemory   = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptRenderer = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptUI       = pl_get_api_latest(ptApiRegistry, plUiI);
    gptECS      = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptPhysics  = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCamera   = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptAnimation  = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh     = pl_get_api_latest(ptApiRegistry, plMeshI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptEcsToolsCtx = ptDataRegistry->get_data("plEcsToolsContext");
    }
    else
    {
        static plEcsToolsContext gtEcsToolsCtx = {0};
        gptEcsToolsCtx = &gtEcsToolsCtx;
        ptDataRegistry->set_data("plEcsToolsContext", gptEcsToolsCtx);
    }
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