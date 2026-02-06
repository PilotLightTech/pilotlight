/*
   pl_camera_script.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data & APIs
// [SECTION] implementation
// [SECTION] script loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include "pl.h"
#include "pl_script_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_camera_ext.h"
#include "pl_animation_ext.h"
#include "pl_ui_ext.h"
#include "pl_gizmo_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data & APIs
//-----------------------------------------------------------------------------

// required APIs
const plIOI*     gptIO     = NULL;
const plEcsI*    gptEcs = NULL;
const plCameraI* gptCamera = NULL;
const plUiI*     gptUi     = NULL;
const plGizmoI*  gptGizmo  = NULL;

static float gfOriginalFOV = 0.0f;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl_script_setup(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plCamera* ptCamera = gptEcs->get_component(ptLibrary, gptCamera->get_ecs_type_key(), tEntity);
    gfOriginalFOV = ptCamera->fFieldOfView;
}

static void
pl_script_run(plComponentLibrary* ptLibrary, plEntity tEntity)
{

    if(gptGizmo->active())
        return;

    plCamera* ptCamera = gptEcs->get_component(ptLibrary, gptCamera->get_ecs_type_key(), tEntity);

    if(gfOriginalFOV == 0.0f)
    {
        gfOriginalFOV = ptCamera->fFieldOfView;
    }

    plIO* ptIO = gptIO->get_io();

    static const float gfCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    float fCameraTravelSpeed = gfCameraTravelSpeed;

    bool bOwnKeyboard = gptUi->wants_keyboard_capture();
    bool bOwnMouse = gptUi->wants_mouse_capture();

    if(!bOwnKeyboard && !bOwnMouse)
    {

        bool bRMB = gptIO->is_mouse_down(PL_MOUSE_BUTTON_RIGHT);
        bool bLMB = gptIO->is_mouse_down(PL_MOUSE_BUTTON_LEFT);

        if(gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
        {
            gfOriginalFOV = ptCamera->fFieldOfView;
        }
        else if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_RIGHT))
        {
            ptCamera->fFieldOfView = gfOriginalFOV;
        }

        if(gptIO->is_key_down(PL_KEY_MOD_SHIFT))
            fCameraTravelSpeed *= 3.0f;


        // camera space
        
        if(bRMB)
        {
            if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
            if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
            if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
            if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

            // world space
            if(gptIO->is_key_down(PL_KEY_Q)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
            if(gptIO->is_key_down(PL_KEY_E)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

            if(gptIO->is_key_down(PL_KEY_Z))
            {
                ptCamera->fFieldOfView += 0.25f * (PL_PI / 180.0f);
                ptCamera->fFieldOfView = pl_minf(ptCamera->fFieldOfView, 2.96706f);
            }
            if(gptIO->is_key_down(PL_KEY_C))
            {
                ptCamera->fFieldOfView -= 0.25f * (PL_PI / 180.0f);

                ptCamera->fFieldOfView = pl_maxf(ptCamera->fFieldOfView, 0.03f);
            }
        }

        if(bLMB && gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_RIGHT, 1.0f))
        {
            const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT, 1.0f);
            gptCamera->translate(ptCamera,  tMouseDelta.x * fCameraTravelSpeed * ptIO->fDeltaTime, -tMouseDelta.y * fCameraTravelSpeed * ptIO->fDeltaTime, 0.0f);
            gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT);
            gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }

        else if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_RIGHT, 1.0f))
        {
            const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT, 1.0f);
            gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
            gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_RIGHT);
        }

        else if(bLMB)
        {
            const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            gptCamera->rotate(ptCamera,  0.0f,  -tMouseDelta.x * fCameraRotationSpeed);
            ptCamera->tPosDouble.x += (double)(-tMouseDelta.y * fCameraTravelSpeed * ptIO->fDeltaTime * sinf(ptCamera->fYaw));
            ptCamera->tPosDouble.z += (double)(-tMouseDelta.y * fCameraTravelSpeed * ptIO->fDeltaTime * cosf(ptCamera->fYaw));
            gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
    }

    gptCamera->update(ptCamera);
}

//-----------------------------------------------------------------------------
// [SECTION] script loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_script(plApiRegistryI* ptApiRegistry, bool bReload)
{
    // load apis
    gptEcs    = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptIO     = pl_get_api_latest(ptApiRegistry, plIOI);
    gptUi     = pl_get_api_latest(ptApiRegistry, plUiI);
    gptGizmo  = pl_get_api_latest(ptApiRegistry, plGizmoI);

    const plScriptInterface tApi = {
        .setup = pl_script_setup,
        .run   = pl_script_run
    };

    ptApiRegistry->set_api("pl_script_camera", (plVersion)plScriptInterface_version, &tApi, sizeof(plScriptInterface));
}

PL_EXPORT void
pl_unload_script(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;
        
    const plScriptInterface* ptApi = ptApiRegistry->get_api("pl_script_camera", (plVersion)plScriptInterface_version);
    ptApiRegistry->remove_api(ptApi);
}
