/*
   example_basic_1.c
     - demonstrates loading APIs
     - demonstrates creating a window
     - demonstrates hot reloading
     - demonstrates keyboard input
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
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    This example building on example 0 but introduces hot reloading and the
    window extension. To utilize hot reload, simply run the build script again!

    IMPORTANT:

    It is absolutely necessary that you understand concepts related to dll 
    boundaries to fully utilize hot reloading. Any static/global variables need
    to be set again between reloads since you are now "in" another dll. For
    example, its typical to see the APIs used as globals within an app or
    extension and thus the re-setting of those variables during a reload. It is
    also important that you do not change the memory layout of data stored in
    the data registry between reloads (i.e. don't add a new struct member).
    Reloads should primarily be used for changes in logic.

*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>  // printf
#include <string.h> // memset
#include "pl.h"

// extension
#include "pl_window_ext.h"

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

const plIOI*     gptIO      = NULL;
const plWindowI* gptWindows = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);
    
    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        printf("Hot reload!\n");

        // return the same memory again
        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example Basic 1",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    // return app memory, which will be returned to us as an argument in the other functions
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
    plIO* ptIO = gptIO->get_io();
    printf("resize to %d, %d\n", (int)ptIO->tMainViewportSize.x, (int)ptIO->tMainViewportSize.y);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    gptIO->new_frame(); // must be called once at the beginning of a frame

    // check for key press
    if(gptIO->is_key_pressed(PL_KEY_P, true))
    {
        printf("P key pressed!\n");
    }

}