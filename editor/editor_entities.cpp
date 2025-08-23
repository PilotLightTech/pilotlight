#include "editor.h"
#include "pl_shader_interop_renderer.h"

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

void
pl__show_entity_components(plAppData* ptAppData, plScene* ptScene, plEntity tEntity)
{
    plComponentLibrary* ptLibrary = ptAppData->ptCompLibrary;
    const plEcsTypeKey tTransformComponentType = gptEcs->get_ecs_type_key_transform();
    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    const plEcsTypeKey tObjectComponentType = gptRenderer->get_ecs_type_key_object();
    const plEcsTypeKey tHierarchyComponentType = gptEcs->get_ecs_type_key_hierarchy();
    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();
    const plEcsTypeKey tSkinComponentType = gptRenderer->get_ecs_type_key_skin();
    const plEcsTypeKey tCameraComponentType = gptCamera->get_ecs_type_key();
    const plEcsTypeKey tAnimationComponentType = gptAnimation->get_ecs_type_key_animation();
    const plEcsTypeKey tInverseKinematicsComponentType = gptAnimation->get_ecs_type_key_inverse_kinematics();
    const plEcsTypeKey tLightComponentType = gptRenderer->get_ecs_type_key_light();
    const plEcsTypeKey tEnvironmentProbeComponentType = gptRenderer->get_ecs_type_key_environment_probe();
    const plEcsTypeKey tHumanoidComponentType = gptAnimation->get_ecs_type_key_humanoid();
    const plEcsTypeKey tScriptComponentType = gptScript->get_ecs_type_key();
    const plEcsTypeKey tRigidBodyComponentType = gptPhysics->get_ecs_type_key_rigid_body_physics();
    const plEcsTypeKey tForceFieldComponentType = gptPhysics->get_ecs_type_key_force_field();

    if(ImGui::Begin("Entity Components"))
    {
        if(ptLibrary && gptEcs->is_entity_valid(ptLibrary, tEntity))
        {
            plTagComponent*               ptTagComp           = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tEntity);
            plTransformComponent*         ptTransformComp     = (plTransformComponent*)gptEcs->get_component(ptLibrary, tTransformComponentType, tEntity);
            plMeshComponent*              ptMeshComp          = (plMeshComponent*)gptEcs->get_component(ptLibrary, tMeshComponentType, tEntity);
            plObjectComponent*            ptObjectComp        = (plObjectComponent*)gptEcs->get_component(ptLibrary, tObjectComponentType, tEntity);
            plHierarchyComponent*         ptHierarchyComp     = (plHierarchyComponent*)gptEcs->get_component(ptLibrary, tHierarchyComponentType, tEntity);
            plMaterialComponent*          ptMaterialComp      = (plMaterialComponent*)gptEcs->get_component(ptLibrary, tMaterialComponentType, tEntity);
            plSkinComponent*              ptSkinComp          = (plSkinComponent*)gptEcs->get_component(ptLibrary, tSkinComponentType, tEntity);
            plCamera*            ptCameraComp        = (plCamera*)gptEcs->get_component(ptLibrary, tCameraComponentType, tEntity);
            plAnimationComponent*         ptAnimationComp     = (plAnimationComponent*)gptEcs->get_component(ptLibrary, tAnimationComponentType, tEntity);
            plInverseKinematicsComponent* ptIKComp            = (plInverseKinematicsComponent*)gptEcs->get_component(ptLibrary, tInverseKinematicsComponentType, tEntity);
            plLightComponent*             ptLightComp         = (plLightComponent*)gptEcs->get_component(ptLibrary, tLightComponentType, tEntity);
            plEnvironmentProbeComponent*  ptProbeComp         = (plEnvironmentProbeComponent*)gptEcs->get_component(ptLibrary, tEnvironmentProbeComponentType, tEntity);
            plHumanoidComponent*          ptHumanComp         = (plHumanoidComponent*)gptEcs->get_component(ptLibrary, tHumanoidComponentType, tEntity);
            plScriptComponent*            ptScriptComp        = (plScriptComponent*)gptEcs->get_component(ptLibrary, tScriptComponentType, tEntity);
            plRigidBodyPhysicsComponent*  ptRigidComp         = (plRigidBodyPhysicsComponent*)gptEcs->get_component(ptLibrary, tRigidBodyComponentType, tEntity);
            plForceFieldComponent*        ptForceField        = (plForceFieldComponent*)gptEcs->get_component(ptLibrary, tForceFieldComponentType, tEntity);
            
            if(ptTagComp && ImGui::CollapsingHeader("Tag"))
            {
                ImGui::Text("Name: %s", ptTagComp->acName);
            }

            if(ptScriptComp && ImGui::CollapsingHeader(PL_ICON_FA_CODE " Script"))
            {
                ImGui::Text("File: %s", ptScriptComp->acFile);

                bool bPlaying = ptScriptComp->tFlags & PL_SCRIPT_FLAG_PLAYING;
                if(ImGui::Checkbox("Playing", &bPlaying))
                {
                    if(bPlaying)
                        ptScriptComp->tFlags |= PL_SCRIPT_FLAG_PLAYING;
                    else
                        ptScriptComp->tFlags &= ~PL_SCRIPT_FLAG_PLAYING;
                }

                bool bPlayOnce = ptScriptComp->tFlags & PL_SCRIPT_FLAG_PLAY_ONCE;
                if(ImGui::Checkbox("Play Once", &bPlayOnce))
                {
                    if(bPlayOnce)
                        ptScriptComp->tFlags |= PL_SCRIPT_FLAG_PLAY_ONCE;
                    else
                        ptScriptComp->tFlags &= ~PL_SCRIPT_FLAG_PLAY_ONCE;
                }

                bool bReloadable = ptScriptComp->tFlags & PL_SCRIPT_FLAG_RELOADABLE;
                if(ImGui::Checkbox("Reloadable", &bReloadable))
                {
                    if(bReloadable)
                        ptScriptComp->tFlags |= PL_SCRIPT_FLAG_RELOADABLE;
                    else
                        ptScriptComp->tFlags &= ~PL_SCRIPT_FLAG_RELOADABLE;
                }
            }

            if(ptHumanComp && ImGui::CollapsingHeader(PL_ICON_FA_PERSON " Humanoid"))
            {
            }

            if(ptRigidComp && ImGui::CollapsingHeader(PL_ICON_FA_BOXES_STACKED " Rigid Body Physics"))
            {
                bool bNoSleeping = ptRigidComp->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING;
                if(ImGui::Checkbox("No Sleeping", &bNoSleeping))
                {
                    if(bNoSleeping)
                    {
                        ptRigidComp->tFlags |= PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING;
                        gptPhysics->wake_up_body(ptLibrary, tEntity);
                    }
                    else
                        ptRigidComp->tFlags &= ~PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING;
                }

                bool bKinematic = ptRigidComp->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC;
                if(ImGui::Checkbox("Kinematic", &bKinematic))
                {
                    if(bKinematic)
                    {
                        ptRigidComp->tFlags |= PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC;
                        gptPhysics->wake_up_body(ptLibrary, tEntity);
                    }
                    else
                        ptRigidComp->tFlags &= ~PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC;
                }
                ImGui::InputFloat("Mass", &ptRigidComp->fMass);
                ImGui::SliderFloat("Friction", &ptRigidComp->fFriction, 0.0f, 1.0f);
                ImGui::SliderFloat("Restitution", &ptRigidComp->fRestitution, 0.0f, 1.0f);
                ImGui::SliderFloat("Linear Damping", &ptRigidComp->fLinearDamping, 0.0f, 1.0f);
                ImGui::SliderFloat("Angluar Damping", &ptRigidComp->fAngularDamping, 0.0f, 1.0f);
                ImGui::InputFloat3("Gravity", ptRigidComp->tGravity.d);
                ImGui::InputFloat3("Local Offset", ptRigidComp->tLocalOffset.d);

                ImGui::Dummy({25.0f, 15.0f});

                ImGui::SeparatorText("Collision Shape");
                ImGui::RadioButton("Box", &ptRigidComp->tShape, PL_COLLISION_SHAPE_BOX);
                ImGui::RadioButton("Sphere", &ptRigidComp->tShape, PL_COLLISION_SHAPE_SPHERE);

                ImGui::Dummy({25.0f, 15.0f});

                if(ptRigidComp->tShape == PL_COLLISION_SHAPE_BOX)
                {
                    ImGui::InputFloat3("Extents", ptRigidComp->tExtents.d);
                }
                else if(ptRigidComp->tShape == PL_COLLISION_SHAPE_SPHERE)
                {
                    ImGui::InputFloat("Radius", &ptRigidComp->fRadius);
                }

                static plVec3 tPoint = {0};
                static plVec3 tForce = {1000.0f};
                static plVec3 tTorque = {0.0f, 100.0f, 0.0f};
                static plVec3 tLinearVelocity = {0.0f, 0.0f, 0.0f};
                static plVec3 tAngularVelocity = {0.0f, 0.0f, 0.0f};
                ImGui::InputFloat3("Point", tPoint.d);
                ImGui::InputFloat3("Force", tForce.d);
                ImGui::InputFloat3("Torque", tTorque.d);
                ImGui::InputFloat3("Velocity", tLinearVelocity.d);
                ImGui::InputFloat3("Angular Velocity", tAngularVelocity.d);

                // gptUI->push_theme_color(PL_UI_COLOR_BUTTON, (plVec4){0.02f, 0.51f, 0.10f, 1.00f});
                // gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, (plVec4){ 0.02f, 0.61f, 0.10f, 1.00f});
                // gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, (plVec4){0.02f, 0.87f, 0.10f, 1.00f});


                if(ImGui::Button("Stop"))
                {
                    gptPhysics->set_linear_velocity(ptLibrary, tEntity, {0});
                    gptPhysics->set_angular_velocity(ptLibrary, tEntity, {0});
                }
                
                if(ImGui::Button("Set Velocity"))          gptPhysics->set_linear_velocity(ptLibrary, tEntity, tLinearVelocity);
                if(ImGui::Button("Set A. Velocity"))       gptPhysics->set_angular_velocity(ptLibrary, tEntity, tAngularVelocity);
                if(ImGui::Button("torque"))                gptPhysics->apply_torque(ptLibrary, tEntity, tTorque);
                if(ImGui::Button("impulse torque"))        gptPhysics->apply_impulse_torque(ptLibrary, tEntity, tTorque);
                if(ImGui::Button("force"))                 gptPhysics->apply_force(ptLibrary, tEntity, tForce);
                if(ImGui::Button("force at point"))        gptPhysics->apply_force_at_point(ptLibrary, tEntity, tForce, tPoint);
                if(ImGui::Button("force at body point"))   gptPhysics->apply_force_at_body_point(ptLibrary, tEntity, tForce, tPoint);
                if(ImGui::Button("impulse"))               gptPhysics->apply_impulse(ptLibrary, tEntity, tForce);
                if(ImGui::Button("impulse at point"))      gptPhysics->apply_impulse_at_point(ptLibrary, tEntity, tForce, tPoint);
                if(ImGui::Button("impulse at body point")) gptPhysics->apply_impulse_at_body_point(ptLibrary, tEntity, tForce, tPoint);
                if(ImGui::Button("wake up"))               gptPhysics->wake_up_body(ptLibrary, tEntity);
                if(ImGui::Button("sleep"))                 gptPhysics->sleep_body(ptLibrary, tEntity);

                // gptUI->pop_theme_color(3);

            }

            if(ptProbeComp && ImGui::CollapsingHeader(PL_ICON_FA_MAP_PIN " Environment Probe", 0))
            {
                bool bRealTime = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                if(ImGui::Checkbox("Real Time", &bRealTime))
                {
                    if(bRealTime)
                        ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                    else
                        ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_REALTIME;
                }

                bool bIncludeSky = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                if(ImGui::Checkbox("Include Sky", &bIncludeSky))
                {
                    if(bIncludeSky)
                        ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                    else
                        ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;
                }

                bool bParallaxCorrection = ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                if(ImGui::Checkbox("Box Parallax Correction", &bParallaxCorrection))
                {
                    if(bParallaxCorrection)
                        ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                    else
                        ptProbeComp->tFlags &= ~PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX;
                }

                if(ImGui::Button("Update"))
                {
                    ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
                }
                ImGui::InputFloat("Range", &ptProbeComp->fRange);

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
                ImGui::SeparatorText("Samples");
                ImGui::RadioButton("32", &iSelection, 0);
                ImGui::RadioButton("64", &iSelection, 1);
                ImGui::RadioButton("128", &iSelection, 2);
                ImGui::RadioButton("256", &iSelection, 3);
                ImGui::RadioButton("512", &iSelection, 4);
                ImGui::RadioButton("1024", &iSelection, 5);
                ptProbeComp->uSamples = auSamples[iSelection];

                ImGui::SeparatorText("Intervals");

                int iSelection0 = (int)ptProbeComp->uInterval;
                ImGui::RadioButton("1", &iSelection0, 1);
                ImGui::RadioButton("2", &iSelection0, 2);
                ImGui::RadioButton("3", &iSelection0, 3);
                ImGui::RadioButton("4", &iSelection0, 4);
                ImGui::RadioButton("5", &iSelection0, 5);
                ImGui::RadioButton("6", &iSelection0, 6);

                ptProbeComp->uInterval = (uint32_t)iSelection0;
            }

            if(ptTransformComp && ImGui::CollapsingHeader(PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform"))
            {
                ImGui::Text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                ImGui::Text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                ImGui::Text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                ImGui::Dummy({25.0f, 15.0f});
                ImGui::Text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                ImGui::Text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                ImGui::Text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                ImGui::Text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
            }

            if(ptForceField && ImGui::CollapsingHeader(PL_ICON_FA_WIND " Force Field"))
            {
                ImGui::RadioButton("Type: PL_FORCE_FIELD_TYPE_POINT", &ptForceField->tType, PL_FORCE_FIELD_TYPE_POINT);
                ImGui::RadioButton("Type: PL_FORCE_FIELD_TYPE_PLANE", &ptForceField->tType, PL_FORCE_FIELD_TYPE_PLANE);
                ImGui::InputFloat("Gravity", &ptForceField->fGravity);
                ImGui::InputFloat("Range", &ptForceField->fRange);
            }

            if(ptMeshComp && ImGui::CollapsingHeader(PL_ICON_FA_CUBE " Mesh"))
            {

                plTagComponent* ptMaterialTagComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptMeshComp->tMaterial);
                plTagComponent* ptSkinTagComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptMeshComp->tSkinComponent);
                ImGui::Text("Material: %s, %u", ptMaterialTagComp->acName, ptMeshComp->tMaterial.uIndex);
                ImGui::Text("Skin:     %s, %u", ptSkinTagComp ? ptSkinTagComp->acName : " ", ptSkinTagComp ? ptMeshComp->tSkinComponent.uIndex : 0);

                ImGui::Dummy({25.0f, 15.0f});
                ImGui::Text("Vertex Data (%u verts, %u idx)", (uint32_t)ptMeshComp->szVertexCount, (uint32_t)ptMeshComp->szIndexCount);
                ImGui::Indent();
                ImGui::Text("%s Positions", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION ? "ACTIVE" : "     ");
                ImGui::Text("%s Normals", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL ? "ACTIVE" : "     ");
                ImGui::Text("%s Tangents", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT ? "ACTIVE" : "     ");
                ImGui::Text("%s Texture Coordinates 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 ? "ACTIVE" : "     ");
                ImGui::Text("%s Texture Coordinates 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 ? "ACTIVE" : "     ");
                ImGui::Text("%s Colors 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0 ? "ACTIVE" : "     ");
                ImGui::Text("%s Colors 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1 ? "ACTIVE" : "     ");
                ImGui::Text("%s Joints 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0 ? "ACTIVE" : "     ");
                ImGui::Text("%s Joints 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1 ? "ACTIVE" : "     ");
                ImGui::Text("%s Weights 0", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0 ? "ACTIVE" : "     ");
                ImGui::Text("%s Weights 1", ptMeshComp->ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1 ? "ACTIVE" : "     ");
                ImGui::Unindent();
            }

            if(ptObjectComp && ImGui::CollapsingHeader(PL_ICON_FA_GHOST " Object"))
            {
                plTagComponent* ptMeshTagComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptObjectComp->tMesh);
                plTagComponent* ptTransformTagComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptObjectComp->tTransform);

                ImGui::Text("Mesh Entity:      %s, %u", ptMeshTagComp->acName, ptObjectComp->tMesh.uIndex);
                ImGui::Text("Transform Entity: %s, %u", ptTransformTagComp->acName, ptObjectComp->tTransform.uIndex);

                bool bObjectRenderable = ptObjectComp->tFlags & PL_OBJECT_FLAGS_RENDERABLE;
                bool bObjectCastShadow = ptObjectComp->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW;
                bool bObjectDynamic = ptObjectComp->tFlags & PL_OBJECT_FLAGS_DYNAMIC;
                bool bObjectForeground = ptObjectComp->tFlags & PL_OBJECT_FLAGS_FOREGROUND;
                bool bObjectUpdateRequired = false;

                if(ImGui::Checkbox("Renderable", &bObjectRenderable))
                {
                    bObjectUpdateRequired = true;
                    if(bObjectRenderable)
                        ptObjectComp->tFlags |= PL_OBJECT_FLAGS_RENDERABLE;
                    else
                        ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_RENDERABLE;
                }

                if(ImGui::Checkbox("Cast Shadow", &bObjectCastShadow))
                {
                    bObjectUpdateRequired = true;
                    if(bObjectCastShadow)
                        ptObjectComp->tFlags |= PL_OBJECT_FLAGS_CAST_SHADOW;
                    else
                        ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_CAST_SHADOW;
                }

                if(ImGui::Checkbox("Dynamic", &bObjectDynamic))
                {
                    bObjectUpdateRequired = true;
                    if(bObjectDynamic)
                        ptObjectComp->tFlags |= PL_OBJECT_FLAGS_DYNAMIC;
                    else
                        ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_DYNAMIC;
                }

                if(ImGui::Checkbox("Foreground", &bObjectForeground))
                {
                    bObjectUpdateRequired = true;
                    if(bObjectForeground)
                        ptObjectComp->tFlags |= PL_OBJECT_FLAGS_FOREGROUND;
                    else
                        ptObjectComp->tFlags &= ~PL_OBJECT_FLAGS_FOREGROUND;
                }
                if(bObjectUpdateRequired)
                    gptRenderer->update_scene_objects(ptScene, 1, &tEntity);
            }

            if(ptHierarchyComp && ImGui::CollapsingHeader(PL_ICON_FA_SITEMAP " Hierarchy"))
            {
                plTagComponent* ptParentTagComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptHierarchyComp->tParent);
                ImGui::Text("Parent Entity: %s , %u", ptParentTagComp->acName, ptHierarchyComp->tParent.uIndex);
            }

            if(ptLightComp && ImGui::CollapsingHeader(PL_ICON_FA_LIGHTBULB " Light"))
            {
                static const char* apcLightTypes[] = {
                    "PL_LIGHT_TYPE_DIRECTIONAL",
                    "PL_LIGHT_TYPE_POINT",
                    "PL_LIGHT_TYPE_SPOT",
                };
                ImGui::LabelText("Type", "%s", apcLightTypes[ptLightComp->tType]);

                bool bShowVisualizer = ptLightComp->tFlags & PL_LIGHT_FLAG_VISUALIZER;
                if(ImGui::Checkbox("Visualizer", &bShowVisualizer))
                {
                    if(bShowVisualizer)
                        ptLightComp->tFlags |= PL_LIGHT_FLAG_VISUALIZER;
                    else
                        ptLightComp->tFlags &= ~PL_LIGHT_FLAG_VISUALIZER;
                }

                ImGui::InputFloat3("Position", ptLightComp->tPosition.d);

                ImGui::SeparatorText("Color");
                ImGui::SliderFloat("r", &ptLightComp->tColor.x, 0.0f, 1.0f);
                ImGui::SliderFloat("g", &ptLightComp->tColor.y, 0.0f, 1.0f);
                ImGui::SliderFloat("b", &ptLightComp->tColor.z, 0.0f, 1.0f);

                ImGui::SliderFloat("Intensity", &ptLightComp->fIntensity, 0.0f, 20.0f, 0);

                if(ptLightComp->tType != PL_LIGHT_TYPE_DIRECTIONAL)
                {
                    ImGui::InputFloat("Radius", &ptLightComp->fRadius);
                    ImGui::InputFloat("Range", &ptLightComp->fRange);
                }

                if(ptLightComp->tType == PL_LIGHT_TYPE_SPOT)
                {
                    ImGui::SliderFloat("Inner Cone Angle", &ptLightComp->fInnerConeAngle, 0.0f, PL_PI_2);
                    ImGui::SliderFloat("Outer Cone Angle", &ptLightComp->fOuterConeAngle, 0.0f, PL_PI_2);
                }


                if(ptLightComp->tType != PL_LIGHT_TYPE_POINT)
                {
                    ImGui::SeparatorText("Direction");
                    ImGui::SliderFloat("x", &ptLightComp->tDirection.x, -1.0f, 1.0f);
                    ImGui::SliderFloat("y", &ptLightComp->tDirection.y, -1.0f, 1.0f);
                    ImGui::SliderFloat("z", &ptLightComp->tDirection.z, -1.0f, 1.0f);
                }

                ImGui::SeparatorText("Shadows");

                bool bCastShadow = ptLightComp->tFlags & PL_LIGHT_FLAG_CAST_SHADOW;
                if(ImGui::Checkbox("Cast Shadow", &bCastShadow))
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
                    ImGui::RadioButton("Resolution: 128", &iSelection, 0);
                    ImGui::RadioButton("Resolution: 256", &iSelection, 1);
                    ImGui::RadioButton("Resolution: 512", &iSelection, 2);
                    ImGui::RadioButton("Resolution: 1024", &iSelection, 3);
                    ImGui::RadioButton("Resolution: 2048", &iSelection, 4);
                    ImGui::RadioButton("Resolution: 4096", &iSelection, 5);
                    ptLightComp->uShadowResolution = auResolutions[iSelection];
                }

                if(ptLightComp->tType == PL_LIGHT_TYPE_DIRECTIONAL)
                {
                    static const uint32_t uCascadeMin = 1;
                    static const uint32_t uCascadeMax = 4;
                    ImGui::SliderScalar("Cascades", ImGuiDataType_U32, &ptLightComp->uCascadeCount, &uCascadeMin, &uCascadeMax);
                    ImGui::InputFloat4("Cascade Splits", ptLightComp->afCascadeSplits);
                }
            }

            if(ptMaterialComp && ImGui::CollapsingHeader(PL_ICON_FA_PALETTE " Material"))
            {
                bool bMaterialModified = false;
                if(ImGui::InputFloat("Roughness", &ptMaterialComp->fRoughness)) bMaterialModified = true;
                if(ImGui::InputFloat("Metalness", &ptMaterialComp->fMetalness)) bMaterialModified = true;
                if(ImGui::InputFloat("Clearcoat", &ptMaterialComp->fClearcoat)) bMaterialModified = true;
                if(ImGui::InputFloat("Clearcoat Roughness", &ptMaterialComp->fClearcoatRoughness)) bMaterialModified = true;
                if(ImGui::InputFloat("Alpha Cutoff", &ptMaterialComp->fAlphaCutoff)) bMaterialModified = true;
                if(ImGui::InputFloat4("Base Factor", ptMaterialComp->tBaseColor.d)) bMaterialModified = true;
                if(ImGui::InputFloat4("Emmissive Factor", ptMaterialComp->tEmissiveColor.d)) bMaterialModified = true;

                if(bMaterialModified)
                    gptRenderer->update_scene_materials(ptScene, 1, &tEntity);

                static const char* apcBlendModeNames[] = 
                {
                    "PL_MATERIAL_BLEND_MODE_OPAQUE",
                    "PL_MATERIAL_BLEND_MODE_ALPHA",
                    "PL_MATERIAL_BLEND_MODE_PREMULTIPLIED",
                    "PL_MATERIAL_BLEND_MODE_ADDITIVE",
                    "PL_MATERIAL_BLEND_MODE_MULTIPLY",
                    "PL_MATERIAL_BLEND_MODE_CLIP_MASK"
                };
                ImGui::LabelText("Blend Mode", "%s", apcBlendModeNames[ptMaterialComp->tBlendMode]);

                static const char* apcShaderNames[] = 
                {
                    "PL_SHADER_TYPE_PBR",
                    "PL_SHADER_TYPE_PBR_CLEARCOAT",
                    "PL_SHADER_TYPE_PBR_SHEEN",
                    "PL_SHADER_TYPE_CUSTOM"
                };
                ImGui::LabelText("Shader Type", "%s", apcShaderNames[ptMaterialComp->tShaderType]);
                ImGui::LabelText("Double Sided", "%s", ptMaterialComp->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED ? "true" : "false");

                ImGui::Dummy({25.0f, 14.0f});
                ImGui::Text("Texture Maps");
                ImGui::Indent();

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
                    ImGui::Text("%s: %s", apcTextureSlotNames[i], ptMaterialComp->atTextureMaps[i].acName[0] == 0 ? " " : "present");
                }
                ImGui::Unindent();
            }

            if(ptSkinComp && ImGui::CollapsingHeader(PL_ICON_FA_MAP " Skin"))
            {
                if(ImGui::TreeNode("Joints"))
                {
                    for(uint32_t i = 0; i < pl_sb_size(ptSkinComp->sbtJoints); i++)
                    {
                        plTagComponent* ptJointTagComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptSkinComp->sbtJoints[i]);
                        ImGui::Text("%s", ptJointTagComp->acName);  
                    }
                    ImGui::TreePop();
                }
            }

            if(ptCameraComp && ImGui::CollapsingHeader(PL_ICON_FA_CAMERA " Camera"))
            { 
                ImGui::LabelText("Near Z", "%+0.3f", ptCameraComp->fNearZ);
                ImGui::LabelText("Far Z", "%+0.3f", ptCameraComp->fFarZ);
                ImGui::LabelText("Vertical Field of View", "%+0.3f", ptCameraComp->fFieldOfView);
                ImGui::LabelText("Aspect Ratio", "%+0.3f", ptCameraComp->fAspectRatio);
                ImGui::LabelText("Pitch", "%+0.3f", ptCameraComp->fPitch);
                ImGui::LabelText("Yaw", "%+0.3f", ptCameraComp->fYaw);
                ImGui::LabelText("Roll", "%+0.3f", ptCameraComp->fRoll);
                ImGui::LabelText("Position", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPos.x, ptCameraComp->tPos.y, ptCameraComp->tPos.z);
                ImGui::LabelText("Up", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tUpVec.x, ptCameraComp->_tUpVec.y, ptCameraComp->_tUpVec.z);
                ImGui::LabelText("Forward", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tForwardVec.x, ptCameraComp->_tForwardVec.y, ptCameraComp->_tForwardVec.z);
                ImGui::LabelText("Right", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->_tRightVec.x, ptCameraComp->_tRightVec.y, ptCameraComp->_tRightVec.z);
                ImGui::InputFloat3("Position", ptCameraComp->tPos.d);
                ImGui::InputFloat("Near Z Plane", &ptCameraComp->fNearZ);
                ImGui::InputFloat("Far Z Plane", &ptCameraComp->fFarZ);
            }

            if(ptAnimationComp && ImGui::CollapsingHeader(PL_ICON_FA_PLAY " Animation"))
            { 
                bool bPlaying = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_PLAYING;
                bool bLooped = ptAnimationComp->tFlags & PL_ANIMATION_FLAG_LOOPED;
                if(ImGui::Checkbox("Playing", &bPlaying))
                {
                    if(bPlaying)
                        ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_PLAYING;
                    else
                        ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_PLAYING;
                }
                if(ImGui::Checkbox("Looped", &bLooped))
                {
                    if(bLooped)
                        ptAnimationComp->tFlags |= PL_ANIMATION_FLAG_LOOPED;
                    else
                        ptAnimationComp->tFlags &= ~PL_ANIMATION_FLAG_LOOPED;
                }
                ImGui::LabelText("Start", "%0.3f s", ptAnimationComp->fStart);
                ImGui::LabelText("End", "%0.3f s", ptAnimationComp->fEnd);
                // ImGui::LabelText("Speed", "%0.3f s", ptAnimationComp->fSpeed);
                ImGui::SliderFloat("Speed", &ptAnimationComp->fSpeed, 0.0f, 2.0f);
                ImGui::SliderFloat("Time", &ptAnimationComp->fTimer, ptAnimationComp->fStart, ptAnimationComp->fEnd);
                ImGui::ProgressBar(ptAnimationComp->fTimer / (ptAnimationComp->fEnd - ptAnimationComp->fStart), {-1.0f, 0.0f});
            }

            if(ptIKComp && ImGui::CollapsingHeader(PL_ICON_FA_DRAW_POLYGON " Inverse Kinematics"))
            { 
                plTagComponent* ptTargetComp = (plTagComponent*)gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptIKComp->tTarget);
                ImGui::Text("Target Entity: %s , %u", ptTargetComp->acName, ptIKComp->tTarget.uIndex);
                static const uint32_t uChainMin = 1;
                static const uint32_t uChainMax = 5;
                ImGui::SliderScalar("Chain Length", ImGuiDataType_U32, &ptIKComp->uChainLength, &uChainMin, &uChainMax);
                ImGui::Text("Iterations: %u", ptIKComp->uIterationCount);

                ImGui::Checkbox("Enabled", &ptIKComp->bEnabled);
            }
        }
        else
            ImGui::Text("No Selected Entity");
    }
    ImGui::End();
}