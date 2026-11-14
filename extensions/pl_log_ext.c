/*
   pl_log_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global context
// [SECTION] internal api
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

typedef struct _plLogFileSink
{
    FILE*          ptFile;
    plLogFileFlags tFlags;
    bool           bError;
} plLogFileSink;

static plLogFileSink* gptLogFiles = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline plLogFileSink*
pl__log_file_sink(uint64_t uChannelId)
{
    if(!gptLogFiles || (uChannelId != PL_LOG_ALL_CHANNELS && uChannelId >= gptLogCtx->uChannelCount))
        return NULL;
    return &gptLogFiles[uChannelId == PL_LOG_ALL_CHANNELS ? PL_LOG_MAX_CHANNEL_COUNT : uChannelId];
}

static void
pl__log_file_va(const char* pcPrefix, uint64_t uLevel, uint64_t uChannelId, const char* pcFormat, va_list tArgs)
{
    if(!gptLogFiles || uChannelId >= gptLogCtx->uChannelCount || gptLogCtx->atChannels[uChannelId].uLevel > uLevel)
        return;

    plLogFileSink* aptSinks[] = {&gptLogFiles[PL_LOG_MAX_CHANNEL_COUNT], &gptLogFiles[uChannelId]};
    for(uint32_t i = 0; i < 2; i++)
    {
        plLogFileSink* ptSink = aptSinks[i];
        if(!ptSink->ptFile)
            continue;
        va_list tCopy;
        va_copy(tCopy, tArgs);
        if(fprintf(ptSink->ptFile, "[%s] [%s] ", gptLogCtx->atChannels[uChannelId].pcName, pcPrefix) < 0)
            ptSink->bError = true;
        if(vfprintf(ptSink->ptFile, pcFormat, tCopy) < 0 || fputc('\n', ptSink->ptFile) == EOF)
            ptSink->bError = true;
        va_end(tCopy);
        if((ptSink->tFlags & PL_LOG_FILE_FLAGS_FLUSH_EACH_ENTRY) || uLevel >= PL_LOG_LEVEL_FATAL)
        {
            if(fflush(ptSink->ptFile) != 0)
                ptSink->bError = true;
        }
    }
}

static void
pl__log_file(const char* pcPrefix, uint64_t uLevel, uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list tArgs;
    va_start(tArgs, pcFormat);
    pl__log_file_va(pcPrefix, uLevel, uChannelId, pcFormat, tArgs);
    va_end(tArgs);
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

bool
pl_log_flush_file(uint64_t uChannelId)
{
    plLogFileSink* ptSink = pl__log_file_sink(uChannelId);
    if(!ptSink)
        return false;
    if(ptSink->ptFile && fflush(ptSink->ptFile) != 0)
        ptSink->bError = true;
    return !ptSink->bError;
}

bool
pl_log_close_file(uint64_t uChannelId)
{
    plLogFileSink* ptSink = pl__log_file_sink(uChannelId);
    if(!ptSink)
        return false;
    if(ptSink->ptFile && fclose(ptSink->ptFile) != 0)
        ptSink->bError = true;
    ptSink->ptFile = NULL;
    return !ptSink->bError;
}

bool
pl_log_has_file_error(uint64_t uChannelId)
{
    plLogFileSink* ptSink = pl__log_file_sink(uChannelId);
    return !ptSink || ptSink->bError;
}

bool
pl_log_set_file(uint64_t uChannelId, const char* pcPath, plLogFileFlags tFlags)
{
    plLogFileSink* ptSink = pl__log_file_sink(uChannelId);
    if(!ptSink || !pcPath || !pcPath[0] || (tFlags & ~(PL_LOG_FILE_FLAGS_APPEND | PL_LOG_FILE_FLAGS_FLUSH_EACH_ENTRY)))
        return false;
    // Flush before opening: the new path may refer to the same file in truncate mode.
    if(ptSink->ptFile && fflush(ptSink->ptFile) != 0)
    {
        ptSink->bError = true;
        return false;
    }
    FILE* ptFile = fopen(pcPath, (tFlags & PL_LOG_FILE_FLAGS_APPEND) ? "ab" : "wb");
    if(!ptFile)
        return false;
    if(ptSink->ptFile && fclose(ptSink->ptFile) != 0)
    {
        fclose(ptFile);
        ptSink->ptFile = NULL;
        ptSink->bError = true;
        return false;
    }
    *ptSink = (plLogFileSink){.ptFile = ptFile, .tFlags = tFlags};
    return true;
}

uint64_t
pl_log_add_channel(const char* pcName, plLogExtChannelInit tInit)
{
    plLogChannelInit tInitAdapt = {
        .tType = tInit.tType,
        .uEntryCount = tInit.uEntryCount
    };
    return pl__add_log_channel(pcName, tInitAdapt);
}

void
pl_log_set_level(uint64_t uChannelId, uint64_t uLevel)
{
    pl__set_log_level(uChannelId, uLevel);
}

void
pl_log_clear_channel(uint64_t uChannelId)
{
    pl__clear_log_channel(uChannelId);
}

void
pl_log_reset_channel(uint64_t uChannelId)
{
    pl__reset_log_channel(uChannelId);
}

uint64_t
pl_log_get_channel_id(const char* pcName)
{
    return pl__get_log_channel_id(pcName);
}

bool
pl_log_get_channel_info(uint64_t uChannelId, plLogExtChannelInfo* ptInfoOut)
{
    return pl__get_log_channel_info(uChannelId, (plLogChannelInfo*)ptInfoOut);
}

uint64_t
pl_log_get_channel_count(void)
{
    return pl__get_log_channel_count();
}


void
pl_log_custom(const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file(pcPrefix, uLevel, uChannelId, "%s", pcMessage);
    pl__log(pcPrefix, iPrefixSize, uLevel, uChannelId, pcMessage);
}

void
pl_log_trace(uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file("TRACE", PL_LOG_LEVEL_TRACE, uChannelId, "%s", pcMessage);
    pl__log_trace(uChannelId, pcMessage);
}

void
pl_log_debug(uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file("DEBUG", PL_LOG_LEVEL_DEBUG, uChannelId, "%s", pcMessage);
    pl__log_debug(uChannelId, pcMessage);
}

void
pl_log_info(uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file("INFO", PL_LOG_LEVEL_INFO, uChannelId, "%s", pcMessage);
    pl__log_info(uChannelId, pcMessage);
}

void
pl_log_warn(uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file("WARN", PL_LOG_LEVEL_WARN, uChannelId, "%s", pcMessage);
    pl__log_warn(uChannelId, pcMessage);
}

void
pl_log_error(uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file("ERROR", PL_LOG_LEVEL_ERROR, uChannelId, "%s", pcMessage);
    pl__log_error(uChannelId, pcMessage);
}

void
pl_log_fatal(uint64_t uChannelId, const char* pcMessage)
{
    pl__log_file("FATAL", PL_LOG_LEVEL_FATAL, uChannelId, "%s", pcMessage);
    pl__log_fatal(uChannelId, pcMessage);
}

void
pl_log_custom_p(const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va(pcPrefix, uLevel, uChannelId, pcFormat, argptr);
    pl__log_va(pcPrefix, iPrefixSize, uLevel, uChannelId, pcFormat, argptr);
    va_end(argptr);     
}

void
pl_log_trace_p(uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va("TRACE", PL_LOG_LEVEL_TRACE, uChannelId, pcFormat, argptr);
    pl__log_trace_va(uChannelId, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_log_debug_p(uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va("DEBUG", PL_LOG_LEVEL_DEBUG, uChannelId, pcFormat, argptr);
    pl__log_debug_va(uChannelId, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_log_info_p(uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va("INFO", PL_LOG_LEVEL_INFO, uChannelId, pcFormat, argptr);
    pl__log_info_va(uChannelId, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_log_warn_p(uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va("WARN", PL_LOG_LEVEL_WARN, uChannelId, pcFormat, argptr);
    pl__log_warn_va(uChannelId, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_log_error_p(uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va("ERROR", PL_LOG_LEVEL_ERROR, uChannelId, pcFormat, argptr);
    pl__log_error_va(uChannelId, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_log_fatal_p(uint64_t uChannelId, const char* pcFormat, ...)
{
    va_list argptr;
    va_start(argptr, pcFormat);
    pl__log_file_va("FATAL", PL_LOG_LEVEL_FATAL, uChannelId, pcFormat, argptr);
    pl__log_fatal_va(uChannelId, pcFormat, argptr);
    va_end(argptr);  
}

void
pl_log_custom_va(const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va(pcPrefix, uLevel, uChannelId, pcFormat, args);
    pl__log_va(pcPrefix, iPrefixSize, uLevel, uChannelId, pcFormat, args);
}

void
pl_log_trace_va(uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va("TRACE", PL_LOG_LEVEL_TRACE, uChannelId, pcFormat, args);
    pl__log_trace_va(uChannelId, pcFormat, args);
}

void
pl_log_debug_va(uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va("DEBUG", PL_LOG_LEVEL_DEBUG, uChannelId, pcFormat, args);
    pl__log_debug_va(uChannelId, pcFormat, args);
}

void
pl_log_info_va(uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va("INFO", PL_LOG_LEVEL_INFO, uChannelId, pcFormat, args);
    pl__log_info_va(uChannelId, pcFormat, args);
}

void
pl_log_warn_va(uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va("WARN", PL_LOG_LEVEL_WARN, uChannelId, pcFormat, args);
    pl__log_warn_va(uChannelId, pcFormat, args);
}

void
pl_log_error_va(uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va("ERROR", PL_LOG_LEVEL_ERROR, uChannelId, pcFormat, args);
    pl__log_error_va(uChannelId, pcFormat, args);
}

void
pl_log_fatal_va(uint64_t uChannelId, const char* pcFormat, va_list args)
{
    pl__log_file_va("FATAL", PL_LOG_LEVEL_FATAL, uChannelId, pcFormat, args);
    pl__log_fatal_va(uChannelId, pcFormat, args);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_log_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plLogI tApi = {
        .add_channel       = pl_log_add_channel,
        .set_level         = pl_log_set_level,
        .clear_channel     = pl_log_clear_channel,
        .reset_channel     = pl_log_reset_channel,
        .get_channel_id    = pl_log_get_channel_id,
        .get_channel_info  = pl_log_get_channel_info,
        .get_channel_count = pl_log_get_channel_count,
        .custom            = pl_log_custom,
        .trace             = pl_log_trace,
        .debug             = pl_log_debug,
        .info              = pl_log_info,
        .warn              = pl_log_warn,
        .error             = pl_log_error,
        .fatal             = pl_log_fatal,
        .custom_p          = pl_log_custom_p,
        .trace_p           = pl_log_trace_p,
        .debug_p           = pl_log_debug_p,
        .info_p            = pl_log_info_p,
        .warn_p            = pl_log_warn_p,
        .error_p           = pl_log_error_p,
        .fatal_p           = pl_log_fatal_p,
        .custom_va         = pl_log_custom_va,
        .trace_va          = pl_log_trace_va,
        .debug_va          = pl_log_debug_va,
        .info_va           = pl_log_info_va,
        .warn_va           = pl_log_warn_va,
        .error_va          = pl_log_error_va,
        .fatal_va          = pl_log_fatal_va,
        .set_file          = pl_log_set_file,
        .flush_file        = pl_log_flush_file,
        .close_file        = pl_log_close_file,
        .has_file_error    = pl_log_has_file_error
    };
    pl_set_api(ptApiRegistry, plLogI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptLogCtx = ptDataRegistry->get_data("plLogContext");
        gptLogFiles = ptDataRegistry->get_data("plLogFiles");
        pl__set_log_context(gptLogCtx);
    }
    else
    {
        gptLogCtx = pl__create_log_context();
        ptDataRegistry->set_data("plLogContext", gptLogCtx);
        gptLogFiles = PL_ALLOC(sizeof(plLogFileSink) * (PL_LOG_MAX_CHANNEL_COUNT + 1));
        memset(gptLogFiles, 0, sizeof(plLogFileSink) * (PL_LOG_MAX_CHANNEL_COUNT + 1));
        ptDataRegistry->set_data("plLogFiles", gptLogFiles);
    }
}

void
pl_unload_log_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plLogI* ptApi = pl_get_api_latest(ptApiRegistry, plLogI);
    ptApiRegistry->remove_api(ptApi);

    pl_log_close_file(PL_LOG_ALL_CHANNELS);
    for(uint64_t i = 0; i < gptLogCtx->uChannelCount; i++)
        pl_log_close_file(i);
    PL_FREE(gptLogFiles);
    gptLogFiles = NULL;
    pl__cleanup_log_context();
}

#ifndef PL_UNITY_BUILD
    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif
#endif
