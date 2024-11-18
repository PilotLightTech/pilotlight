/*
   pl_profile_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global context
// [SECTION] implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_profile_ext.h"
#include "pl_threads_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    const plThreadsI* gptThreads = NULL;
    const plDataRegistryI* gptDataRegistry = NULL;
#endif

#define PL_PROFILE_ALLOC(x) PL_ALLOC(x)
#define PL_PROFILE_FREE(x) PL_FREE(x)
#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plProfileContext* gptProfileCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

void
pl_initialize_profile_ext(void)
{
    plProfileInit tInit = {
        .uThreadCount = gptThreads->get_hardware_thread_count()
    };
    gptProfileCtx = pl_create_profile_context(tInit);
    gptDataRegistry->set_data("plProfileContext", gptProfileCtx);
}

plProfileCpuSample*
pl_get_last_frame_cpu_samples(uint32_t uThreadIndex, uint32_t* puSize)
{
    plProfileSample* ptSamples = pl_get_last_frame_samples(uThreadIndex, puSize);
    return (plProfileCpuSample*)ptSamples;
}

double
pl__get_profile_overhead(uint32_t uThreadIndex)
{
    plProfileFrame* ptFrame = gptProfileContext->ptThreadData[uThreadIndex].ptLastFrame;
    return ptFrame->dInternalDuration;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_profile_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plProfileI tApi = {
        .begin_frame             = pl__begin_profile_frame,
        .end_frame               = pl__end_profile_frame,
        .begin_sample            = pl__begin_profile_sample,
        .end_sample              = pl__end_profile_sample,
        .get_last_frame_samples  = pl_get_last_frame_cpu_samples,
        .get_last_frame_overhead = pl__get_profile_overhead
    };
    pl_set_api(ptApiRegistry, plProfileI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptThreads = pl_get_api_latest(ptApiRegistry, plThreadsI);

    if(bReload)
    {
        gptProfileCtx = gptDataRegistry->get_data("plProfileContext");
        pl_set_profile_context(gptProfileCtx);
    }
}

PL_EXPORT void
pl_unload_profile_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plProfileI* ptApi = pl_get_api_latest(ptApiRegistry, plProfileI);
    ptApiRegistry->remove_api(ptApi);

    pl_cleanup_profile_context();

}
