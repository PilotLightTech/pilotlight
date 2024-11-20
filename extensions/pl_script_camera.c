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

#include "pl.h"
#include "pl_script_ext.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] global data & APIs
//-----------------------------------------------------------------------------

// required APIs
const plIOI*     gptIO     = NULL;
const plCameraI* gptCamera = NULL;
const plEcsI*    gptEcs    = NULL;
const plUiI*     gptUi     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

// static void
// pl_script_run(plComponentLibrary* ptLibrary, plEntity tEntity)
// {

// }

static void
pl_script_run(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plCameraComponent* ptCamera = gptEcs->get_component(ptLibrary, PL_COMPONENT_TYPE_CAMERA, tEntity);

    plIO* ptIO = gptIO->get_io();

    static const float fCameraTravelSpeed = 4.0f;
    static const float fCameraRotationSpeed = 0.005f;

    // camera space
    bool bOwnKeyboard = gptUi->wants_keyboard_capture();
    if(!bOwnKeyboard)
    {
        if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
        if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
        if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
        if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

        // world space
        if(gptIO->is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
        if(gptIO->is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_MIDDLE, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_MIDDLE, 1.0f);
        gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_MIDDLE);
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

    const plScriptI tApi = {
        .setup = NULL,
        .run   = pl_script_run
    };

    #ifdef PL_CONFIG_DEBUG
        ptApiRegistry->set_api("pl_script_camerad", plScriptI_version, &tApi, sizeof(plScriptI));
    #endif
    #ifdef PL_CONFIG_RELEASE
        ptApiRegistry->set_api("pl_script_camera", plScriptI_version, &tApi, sizeof(plScriptI));
    #endif
}

PL_EXPORT void
pl_unload_script(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;
        

    #ifdef PL_CONFIG_DEBUG
        const plScriptI* ptApi = ptApiRegistry->get_api("pl_script_camerad", plScriptI_version);
    #endif
    #ifdef PL_CONFIG_RELEASE
        const plScriptI* ptApi = ptApiRegistry->get_api("pl_script_camera", plScriptI_version);
    #endif
    ptApiRegistry->remove_api(ptApi);
}
