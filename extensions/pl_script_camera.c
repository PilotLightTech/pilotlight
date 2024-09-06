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

#include "pilot_light.h"
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

static const char*
pl_script_name(void)
{
    return "pl_script_camera";
}

//-----------------------------------------------------------------------------
// [SECTION] script loading
//-----------------------------------------------------------------------------

static const plScriptI*
pl_load_script_api(void)
{
    static const plScriptI tApi = {
        .setup = NULL,
        .run   = pl_script_run,
        .name  = pl_script_name
    };
    return &tApi;
}

PL_EXPORT void
pl_load_script(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    // load apis
    gptEcs    = ptApiRegistry->first(PL_API_ECS);
    gptCamera = ptApiRegistry->first(PL_API_CAMERA);
    gptIO     = ptApiRegistry->first(PL_API_IO);
    gptUi     = ptApiRegistry->first(PL_API_UI);

    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_SCRIPT), pl_load_script_api());
    else
        ptApiRegistry->add(PL_API_SCRIPT, pl_load_script_api());
}

PL_EXPORT void
pl_unload_script(plApiRegistryI* ptApiRegistry)
{

}
