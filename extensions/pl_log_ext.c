/*
   pl_log_ext.c
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
#include "pl_log_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#define PL_LOG_ALLOC(x) PL_ALLOC(x)
#define PL_LOG_FREE(x) PL_FREE(x)
#define PL_LOG_REMOVE_ALL_MACROS
#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plLogContext* gptLogCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

uint64_t
pl__add_log_ext_channel(const char* pcName, plLogExtChannelInit tInit)
{
    plLogChannelInit tInitAdapt = {
        .tType = tInit.tType,
        .uEntryCount = tInit.uEntryCount
    };
    return pl__add_log_channel(pcName, tInitAdapt);
}

bool
pl__get_log_ext_channel_info(uint64_t uID, plLogExtChannelInfo* ptOut)
{
    return pl__get_log_channel_info(uID, (plLogChannelInfo*)ptOut);
}


//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_log_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plLogI tApi = {
        .add_channel       = pl__add_log_ext_channel,
        .set_level         = pl__set_log_level,
        .clear_channel     = pl__clear_log_channel,
        .reset_channel     = pl__reset_log_channel,
        .get_channel_id    = pl__get_log_channel_id,
        .get_channel_info  = pl__get_log_ext_channel_info,
        .get_channel_count = pl__get_log_channel_count,
        .custom            = pl__log,
        .trace             = pl__log_trace,
        .debug             = pl__log_debug,
        .info              = pl__log_info,
        .warn              = pl__log_warn,
        .error             = pl__log_error,
        .fatal             = pl__log_fatal,
        .custom_p          = pl__log_p,
        .trace_p           = pl__log_trace_p,
        .debug_p           = pl__log_debug_p,
        .info_p            = pl__log_info_p,
        .warn_p            = pl__log_warn_p,
        .error_p           = pl__log_error_p,
        .fatal_p           = pl__log_fatal_p,
        .custom_va         = pl__log_va,
        .trace_va          = pl__log_trace_va,
        .debug_va          = pl__log_debug_va,
        .info_va           = pl__log_info_va,
        .warn_va           = pl__log_warn_va,
        .error_va          = pl__log_error_va,
        .fatal_va          = pl__log_fatal_va,
    };
    pl_set_api(ptApiRegistry, plLogI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptLogCtx = ptDataRegistry->get_data("plLogContext");
        pl__set_log_context(gptLogCtx);
    }
    else
    {
        gptLogCtx = pl__create_log_context();
        ptDataRegistry->set_data("plLogContext", gptLogCtx);
    }
}

PL_EXPORT void
pl_unload_log_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plLogI* ptApi = pl_get_api_latest(ptApiRegistry, plLogI);
    ptApiRegistry->remove_api(ptApi);

    pl__cleanup_log_context();
}