/*
   example_basic_0.c
     - demonstrates minimal app
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
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
    The purpose of this example is to demonstrate a bare minimum app. This app
    is not actually meant to run though it might(?). The most important
    information to gather from this example is the 4 functions that most apps 
    should export:

    ~~~> void* pl_app_load    (plApiRegistryI*, void*)
    ~~~> void  pl_app_shutdown(void*)
    ~~~> void  pl_app_resize  (void*)
    ~~~> void  pl_app_update  (void*)

    The primary "components" of Pilot Light are:

    * runtime    ~~~> small executable (i.e. pilot_light.exe)
    * app        ~~~> shared library (i.e. this file)
    * extensions ~~~> shared libraries
    
    runtime:
        This is the small executable that manages & orchestrates the core
        systems: API, Extension, and Data registries (among other things).

    app:
        This is a shared library exporting the functions discussed above.

    extension:
        A shared library that exports load & unload functions for loading
        and registering APIs. This will be discussed in another example.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h> // printf
#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI* gptIO = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, void* pAppData)
{

    // NOTE: on first load, "pAppData" will be NULL

    // retrieve the IO API required to use plIO for "talking" with runtime)
    gptIO = pl_get_api_latest(ptApiRegistry, plIOI);

    // return optional application memory
    return NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    printf("shutting down\n");
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, void* pAppData)
{
    // NOTE: this function is not used here since this example doesn't have a window
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(void* pAppData)
{

    gptIO->new_frame(); // must be called once at the beginning of a frame

    static int iIteration = 0;

    printf("iteration: %d\n", iIteration++);

    // shutdown main event loop after 50 iterations
    if(iIteration == 50)
    {
        plIO* ptIO = gptIO->get_io();
        ptIO->bRunning = false;
    }
}