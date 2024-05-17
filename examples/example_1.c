/*
   example_1.c
     - demonstrates loading APIs
     - demonstrates hot reloading
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h> // memset
#include "pilotlight.h"
#include "pl_ui.h"
#include "pl_os.h" // window api

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plWindowI* gptWindows = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    // retrieve the UI context (provided by the runtime) and
    // set it (required to use plIO for "talking" with runtime & keyboard/mouse input)
    plUiContext* ptUIContext = ptDataRegistry->get_data("context");
    pl_set_context(ptUIContext);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        printf("Hot reload!\n");

        // re-retrieve the windows API since we are now in
        // a different dll/so
        gptWindows = ptApiRegistry->first(PL_API_WINDOW);

        // return the same memory again
        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // load required apis (NULL if not available)
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 1",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // perform any cleanup here
    gptWindows->destroy_window(ptAppData->ptWindow);
    
    // free app memory
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    plIO* ptIO = pl_get_io();
    printf("resize to %d, %d\n", (int)ptIO->afMainViewportSize[0], (int)ptIO->afMainViewportSize[1]);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    pl_new_frame(); // must be called once at the beginning of a frame

    // check for key press
    if(pl_is_key_pressed(PL_KEY_P, true))
    {
        printf("P key pressed!\n");
    }

}