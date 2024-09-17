/*
   example_0.c
     - demonstrates minimal app
*/

/*
Index of this file:
// [SECTION] includes
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

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    // retrieve the IO API required to use plIO for "talking" with runtime)
    gptIO = ptApiRegistry->first(PL_API_IO);

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
pl_app_resize(void* pAppData)
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
