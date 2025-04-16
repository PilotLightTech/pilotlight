/*
   example_basic_4.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates ui extension
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] full demo
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    This example introduces the UI extension. The UI extension is an immediate
    mode UI library. It is built on top of the drawing extension. You will
    again see the starter extension used but in a similar manner to the previous
    example we will disable it from handling the UI extension so we can do that.
    We will allow it to handle the drawing extension again.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>  // printf
#include <string.h> // memset
#include "pl.h"

// extensions
#include "pl_ui_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"

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

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plUiI*          gptUi          = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plStarterI*     gptStarter     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    // load extensions
    //   * first argument is the shared library name WITHOUT the extension
    //   * second & third argument is the load/unload functions names (use NULL for the default of "pl_load_ext" &
    //     "pl_unload_ext")
    //   * fourth argument indicates if the extension is reloadable (should we check for changes and reload if changed)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false); // provides the file API used by the drawing ext
    
    // load required apis
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example Basic 4",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 500,
        .uHeight = 500,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we will remove this flag so we can handle
    // management of the UI extension
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_UI_EXT;

    gptStarter->initialize(tStarterInit);

    // the UI extension only needs to be initialized
    // and given a default font to use
    gptUi->initialize();
    gptUi->set_default_font(gptStarter->get_default_font());

    // wraps up (i.e. builds font atlas)
    gptStarter->finalize();

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // we must now cleanup the UI
    // extension ourselves
    gptUi->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    if(!gptStarter->begin_frame())
        return;

    // you must call this once per frame before
    // calling any other UI extension calls
    gptUi->new_frame();

    // NOTE: UI code can be placed anywhere between the UI "new_frame" & "render".
    //       see pl_ui_ext.h for more information on the UI extension

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UI~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        if(gptUi->begin_collapsing_header("Information", 0))
        {
            
            gptUi->text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            gptUi->end_collapsing_header();
        }

        if(gptUi->button("Print"))
        {
            printf("Hello!\n");
        }

        gptUi->end_window();
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit drawlists~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // the starter extension will call begin/end main_pass if you don't but in this
    // case we are managing the draw backend so we must submit the drawlists
    // ourself. The starter extension still handles alot of the process of dealing
    // with command buffers, syncronization, submission, etc. but that is outside
    // the scope of the draw & draw backend extensions.

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    // this must be called which handles several things but
    // most importantly it orders the various draw layers in
    // the proper order
    gptUi->end_frame();

    // now we must submit both the drawlist & debug drawlist provided by
    // the UI extension.
    plVec2 tViewportSize = gptIO->get_io()->tMainViewportSize;
    gptDrawBackend->submit_2d_drawlist(gptUi->get_draw_list(), ptEncoder, tViewportSize.x, tViewportSize.y, 1);
    gptDrawBackend->submit_2d_drawlist(gptUi->get_debug_draw_list(), ptEncoder, tViewportSize.x, tViewportSize.y, 1);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}
