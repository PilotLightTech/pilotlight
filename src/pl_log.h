/*
   pl_log, v0.1 (WIP)
   Do this:
        #define PL_LOG_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_LOG_IMPLEMENTATION
   #include "pl_log.h"
*/

/*
Index of this file:
// [SECTION] include
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
// [SECTION] internal api
// [SECTION] implementation
*/

#ifndef PL_LOG_H
#define PL_LOG_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdarg.h>

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct name ##_t name
#endif

#ifndef PL_LOG_MAX_LINE_SIZE
#define PL_LOG_MAX_LINE_SIZE 1024
#endif

#define PL_LOG_LEVEL_ALL       0
#define PL_LOG_LEVEL_TRACE  5000
#define PL_LOG_LEVEL_DEBUG  6000
#define PL_LOG_LEVEL_INFO   7000
#define PL_LOG_LEVEL_WARN   8000
#define PL_LOG_LEVEL_ERROR  9000
#define PL_LOG_LEVEL_FATAL 10000
#define PL_LOG_LEVEL_OFF   11000

#ifdef PL_LOG_ON
    #ifndef PL_GLOBAL_LOG_LEVEL
        #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
    #endif
#else
    #undef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_OFF
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
PL_DECLARE_STRUCT(plLogContext);
PL_DECLARE_STRUCT(plLogChannel);
PL_DECLARE_STRUCT(plLogEntry);

// enums
typedef int plChannelType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

#ifdef PL_LOG_ON

    // setup/shutdown
    #define pl_create_log_context(tPContext) pl__create_log_context((tPContext))
    #define pl_cleanup_log_context(tPContext) pl__cleanup_log_context((tPContext))

    // channels
    #define pl_add_log_channel(tPContext, cPName, uMaxEntries, tType) pl__add_log_channel((tPContext), (cPName), (uMaxEntries), (tType))
    #define pl_set_log_level(tPContext, uID, uLevel) pl__set_log_level((tPContext), (uID), (uLevel))
    #define pl_clear_log_channel(tPContext, uID) pl__clear_log_channel((tPContext), (uID))

#else

    #define pl_create_log_context(tPContext) //
    #define pl_cleanup_log_context(tPContext) //
    #define pl_add_log_channel(tPContext, cPName, uMaxEntries, tType) 0u
    #define pl_set_log_level(tPContext, uID, uLevel) //
    #define pl_clear_log_channel(tPContext, uID) //

#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_TRACE+1 && defined(PL_LOG_ON)
    #define pl_log_trace(tPContext, uID, cPMessage) pl__log_trace(tPContext, uID, cPMessage)
    #define pl_log_trace_f(...) pl__log_trace_p(__VA_ARGS__)
#else
    #define pl_log_trace(tPContext, uID, cPMessage) //
    #define pl_log_trace_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_DEBUG+1 && defined(PL_LOG_ON)
    #define pl_log_debug(tPContext, uID, cPMessage) pl__log_debug(tPContext, uID, cPMessage)
    #define pl_log_debug_f(...) pl__log_debug_p(__VA_ARGS__)
#else
    #define pl_log_debug(tPContext, uID, cPMessage) //
    #define pl_log_debug_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_INFO+1 && defined(PL_LOG_ON)
    #define pl_log_info(tPContext, uID, cPMessage) pl__log_info(tPContext, uID, cPMessage)
    #define pl_log_info_f(...) pl__log_info_p(__VA_ARGS__)
#else
    #define pl_log_info(tPContext, uID, cPMessage) //
    #define pl_log_info_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_WARN+1 && defined(PL_LOG_ON)
    #define pl_log_warn(tPContext, uID, cPMessage) pl__log_warn(tPContext, uID, cPMessage)
    #define pl_log_warn_f(...) pl__log_warn_p(__VA_ARGS__)
#else
    #define pl_log_warn(tPContext, uID, cPMessage) //
    #define pl_log_warn_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_ERROR+1 && defined(PL_LOG_ON)
    #define pl_log_error(tPContext, uID, cPMessage) pl__log_error(tPContext, uID, cPMessage)
    #define pl_log_error_f(...) pl__log_error_p(__VA_ARGS__)
#else
    #define pl_log_error(tPContext, uID, cPMessage) //
    #define pl_log_error_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_FATAL+1 && defined(PL_LOG_ON)
    #define pl_log_fatal(tPContext, uID, cPMessage) pl__log_fatal(tPContext, uID, cPMessage)
    #define pl_log_fatal_f(...) pl__log_fatal_p(__VA_ARGS__)
#else
    #define pl_log_fatal(tPContext, uID, cPMessage) //
    #define pl_log_fatal_f(...) //
#endif

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plChannelType_
{
    PL_CHANNEL_TYPE_DEFAULT = 0,
    PL_CHANNEL_TYPE_CONSOLE = 1 << 0,
    PL_CHANNEL_TYPE_BUFFER  = 1 << 1
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plLogEntry_t
{
    uint32_t uLevel;
    char     cPBuffer[PL_LOG_MAX_LINE_SIZE];
} plLogEntry;


typedef struct plLogChannel_t
{
    plLogContext* tPContext;
    char          cName[PL_LOG_MAX_LINE_SIZE];
    plLogEntry*   pEntries;
    uint32_t      uLineIndex;
    uint32_t      uLinesActive;
    uint32_t      uLineCount;
    uint32_t      uLevel;
    plChannelType tType;
    uint32_t      uID;
} plLogChannel;

typedef struct plLogContext_t
{
    plLogChannel* sbChannels;
} plLogContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// setup/shutdown
void pl__create_log_context(plLogContext* tPContext);
void pl__cleanup_log_context(plLogContext* tPContext);

// channels
uint32_t pl__add_log_channel  (plLogContext* tPContext, const char* cPName, uint32_t uMaxEntries, plChannelType tType);
void     pl__set_log_level    (plLogContext* tPContext, uint32_t uID, uint32_t uLevel);
void     pl__clear_log_channel(plLogContext* tPContext, uint32_t uID);

// logging
void    pl__log_trace(plLogContext* tPContext, uint32_t uID, const char* cPMessage);
void    pl__log_debug(plLogContext* tPContext, uint32_t uID, const char* cPMessage);
void    pl__log_info (plLogContext* tPContext, uint32_t uID, const char* cPMessage);
void    pl__log_warn (plLogContext* tPContext, uint32_t uID, const char* cPMessage);
void    pl__log_error(plLogContext* tPContext, uint32_t uID, const char* cPMessage);
void    pl__log_fatal(plLogContext* tPContext, uint32_t uID, const char* cPMessage);

void    pl__log_trace_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...);
void    pl__log_debug_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...);
void    pl__log_info_p (plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...);
void    pl__log_warn_p (plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...);
void    pl__log_error_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...);
void    pl__log_fatal_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...);

void    pl__log_trace_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args);
void    pl__log_debug_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args);
void    pl__log_info_va (plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args);
void    pl__log_warn_va (plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args);
void    pl__log_error_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args);
void    pl__log_fatal_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args);

#endif // PL_LOG_H

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

#ifdef PL_LOG_IMPLEMENTATION

#ifdef _WIN32
#define PL_LOG_BOLD_CODE      "[1m"
#define PL_LOG_UNDERLINE_CODE "[4m"
#define PL_LOG_POP_CODE       "[0m"

#define PL_LOG_FG_COLOR_CODE_BLACK          "[30m"
#define PL_LOG_FG_COLOR_CODE_RED            "[31m"
#define PL_LOG_FG_COLOR_CODE_GREEN          "[32m"
#define PL_LOG_FG_COLOR_CODE_YELLOW         "[33m"
#define PL_LOG_FG_COLOR_CODE_BLUE           "[34m"
#define PL_LOG_FG_COLOR_CODE_MAGENTA        "[35m"
#define PL_LOG_FG_COLOR_CODE_CYAN           "[36m"
#define PL_LOG_FG_COLOR_CODE_WHITE          "[37m"
#define PL_LOG_FG_COLOR_CODE_STRONG_BLACK   "[90m"
#define PL_LOG_FG_COLOR_CODE_STRONG_RED     "[91m"
#define PL_LOG_FG_COLOR_CODE_STRONG_GREEN   "[92m"
#define PL_LOG_FG_COLOR_CODE_STRONG_YELLOW  "[93m"
#define PL_LOG_FG_COLOR_CODE_STRONG_BLUE    "[94m"
#define PL_LOG_FG_COLOR_CODE_STRONG_MAGENTA "[95m"
#define PL_LOG_FG_COLOR_CODE_STRONG_CYAN    "[96m"
#define PL_LOG_FG_COLOR_CODE_STRONG_WHITE   "[97m"

#define PL_LOG_BG_COLOR_CODE_BLACK          "[40m"
#define PL_LOG_BG_COLOR_CODE_RED            "[41m"
#define PL_LOG_BG_COLOR_CODE_GREEN          "[42m"
#define PL_LOG_BG_COLOR_CODE_YELLOW         "[43m"
#define PL_LOG_BG_COLOR_CODE_BLUE           "[44m"
#define PL_LOG_BG_COLOR_CODE_MAGENTA        "[45m"
#define PL_LOG_BG_COLOR_CODE_CYAN           "[46m"
#define PL_LOG_BG_COLOR_CODE_WHITE          "[47m"
#define PL_LOG_BG_COLOR_CODE_STRONG_BLACK   "[100m"
#define PL_LOG_BG_COLOR_CODE_STRONG_RED     "[101m"
#define PL_LOG_BG_COLOR_CODE_STRONG_GREEN   "[102m"
#define PL_LOG_BG_COLOR_CODE_STRONG_YELLOW  "[103m"
#define PL_LOG_BG_COLOR_CODE_STRONG_BLUE    "[104m"
#define PL_LOG_BG_COLOR_CODE_STRONG_MAGENTA "[105m"
#define PL_LOG_BG_COLOR_CODE_STRONG_CYAN    "[106m"
#define PL_LOG_BG_COLOR_CODE_STRONG_WHITE   "[107m"

#else

#define PL_LOG_BOLD_CODE      "\033[1m"
#define PL_LOG_UNDERLINE_CODE "\033[4m"
#define PL_LOG_POP_CODE       "\033[0m"

#define PL_LOG_FG_COLOR_CODE_BLACK          "\033[30m"
#define PL_LOG_FG_COLOR_CODE_RED            "\033[31m"
#define PL_LOG_FG_COLOR_CODE_GREEN          "\033[32m"
#define PL_LOG_FG_COLOR_CODE_YELLOW         "\033[33m"
#define PL_LOG_FG_COLOR_CODE_BLUE           "\033[34m"
#define PL_LOG_FG_COLOR_CODE_MAGENTA        "\033[35m"
#define PL_LOG_FG_COLOR_CODE_CYAN           "\033[36m"
#define PL_LOG_FG_COLOR_CODE_WHITE          "\033[37m"
#define PL_LOG_FG_COLOR_CODE_STRONG_BLACK   "\033[90m"
#define PL_LOG_FG_COLOR_CODE_STRONG_RED     "\033[91m"
#define PL_LOG_FG_COLOR_CODE_STRONG_GREEN   "\033[92m"
#define PL_LOG_FG_COLOR_CODE_STRONG_YELLOW  "\033[93m"
#define PL_LOG_FG_COLOR_CODE_STRONG_BLUE    "\033[94m"
#define PL_LOG_FG_COLOR_CODE_STRONG_MAGENTA "\033[95m"
#define PL_LOG_FG_COLOR_CODE_STRONG_CYAN    "\033[96m"
#define PL_LOG_FG_COLOR_CODE_STRONG_WHITE   "\033[97m"

#define PL_LOG_BG_COLOR_CODE_BLACK          "\033[40m"
#define PL_LOG_BG_COLOR_CODE_RED            "\033[41m"
#define PL_LOG_BG_COLOR_CODE_GREEN          "\033[42m"
#define PL_LOG_BG_COLOR_CODE_YELLOW         "\033[43m"
#define PL_LOG_BG_COLOR_CODE_BLUE           "\033[44m"
#define PL_LOG_BG_COLOR_CODE_MAGENTA        "\033[45m"
#define PL_LOG_BG_COLOR_CODE_CYAN           "\033[46m"
#define PL_LOG_BG_COLOR_CODE_WHITE          "\033[47m"
#define PL_LOG_BG_COLOR_CODE_STRONG_BLACK   "\033[100m"
#define PL_LOG_BG_COLOR_CODE_STRONG_RED     "\033[101m"
#define PL_LOG_BG_COLOR_CODE_STRONG_GREEN   "\033[102m"
#define PL_LOG_BG_COLOR_CODE_STRONG_YELLOW  "\033[103m"
#define PL_LOG_BG_COLOR_CODE_STRONG_BLUE    "\033[104m"
#define PL_LOG_BG_COLOR_CODE_STRONG_MAGENTA "\033[105m"
#define PL_LOG_BG_COLOR_CODE_STRONG_CYAN    "\033[106m"
#define PL_LOG_BG_COLOR_CODE_STRONG_WHITE   "\033[107m"

#endif

#include "pl_ds.h"

#ifndef PL_ALLOC
#include <stdlib.h>
#define PL_ALLOC(x) malloc(x)
#endif

#ifndef pl_sprintf
#include <stdio.h>
#define pl_sprintf sprintf
#define pl_vsprintf vsprintf
#endif

void
pl__create_log_context(plLogContext* tPContext)
{
    tPContext->sbChannels = NULL;
}

void
pl__cleanup_log_context(plLogContext* tPContext)
{
    pl_sb_free(tPContext->sbChannels); 
}

uint32_t
pl__add_log_channel(plLogContext* tPContext, const char* cPName, uint32_t uMaxEntries, plChannelType tType)
{
    uint32_t uID = pl_sb_size(tPContext->sbChannels);
    
    plLogChannel tChannel = 
    {
        .tPContext = tPContext,
        .tType = tType,
        .uID = uID
    };

    pl_sb_push(tPContext->sbChannels, tChannel);
    return uID;
}

void
pl__set_log_level(plLogContext* tPContext, uint32_t uID, uint32_t uLevel)
{
    PL_ASSERT(uID < pl_sb_size(tPContext->sbChannels) && "channel ID is not valid");
    tPContext->sbChannels[uID].uLevel = uLevel;
}

void
pl__clear_log_channel(plLogContext* tPContext, uint32_t uID)
{
    PL_ASSERT(uID < pl_sb_size(tPContext->sbChannels) && "channel ID is not valid");
    tPContext->sbChannels[uID].uLineCount = 0u;
    tPContext->sbChannels[uID].uLineIndex = 0u;
    tPContext->sbChannels[uID].uLinesActive = 0u;
    pl_sb_reset(tPContext->sbChannels[uID].pEntries);
}

#define PL__LOG_LEVEL_MACRO(level, prefix, prefixSize) \
    plLogChannel* tPChannel = &tPContext->sbChannels[uID]; \
    if(tPChannel->uLevel < level + 1) \
    { \
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE) printf(prefix "%s\n", cPMessage); \
        if(tPChannel->tType & PL_CHANNEL_TYPE_BUFFER) \
        { \
            char* cPDest = tPChannel->pEntries[tPChannel->uLineIndex].cPBuffer; \
            tPChannel->pEntries[tPChannel->uLineIndex].uLevel = level; \
            strcpy(cPDest, prefix); \
            cPDest += prefixSize; \
            strcpy(cPDest, cPMessage); \
            tPChannel->uLineIndex++; \
            if(tPChannel->uLinesActive < tPChannel->uLineCount) tPChannel->uLinesActive++; \
            if(tPChannel->uLineIndex == tPChannel->uLineCount)  tPChannel->uLineIndex = 0u; \
        } \
    }

void
pl__log_trace(plLogContext* tPContext, uint32_t uID, const char* cPMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_TRACE, "[TRACE] ", 8)
}

void
pl__log_debug(plLogContext* tPContext, uint32_t uID, const char* cPMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_DEBUG, "[DEBUG] ", 8)
}

void
pl__log_info (plLogContext* tPContext, uint32_t uID, const char* cPMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_INFO, "[INFO ] ", 8)
}

void
pl__log_warn (plLogContext* tPContext, uint32_t uID, const char* cPMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_WARN, "[WARN ] ", 8)
}

void
pl__log_error(plLogContext* tPContext, uint32_t uID, const char* cPMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_ERROR, "[ERROR] ", 8)
}

void
pl__log_fatal(plLogContext* tPContext, uint32_t uID, const char* cPMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_FATAL, "[FATAL] ", 8)
}

void
pl__log_trace_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_trace_va(tPContext, uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_debug_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_debug_va(tPContext, uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_info_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_info_va(tPContext, uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_warn_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_warn_va(tPContext, uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_error_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_error_va(tPContext, uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_fatal_p(plLogContext* tPContext, uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_fatal_va(tPContext, uID, cPFormat, argptr);
    va_end(argptr);     
}

#define PL__LOG_LEVEL_VA_BUFFER_MACRO(level, prefix) \
        if(tPChannel->tType & PL_CHANNEL_TYPE_BUFFER) \
        { \
            char* cPDest = tPChannel->pEntries[tPChannel->uLineIndex].cPBuffer; \
            tPChannel->pEntries[tPChannel->uLineIndex].uLevel = level; \
            cPDest += pl_sprintf(cPDest, prefix); \
            pl_vsprintf(cPDest, cPFormat, args); \
            tPChannel->uLineIndex++; \
            if(tPChannel->uLinesActive < tPChannel->uLineCount) tPChannel->uLinesActive++; \
            if(tPChannel->uLineIndex == tPChannel->uLineCount) tPChannel->uLineIndex = 0u; \
        }

void
pl__log_trace_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &tPContext->sbChannels[uID];

    if(tPChannel->uLevel < PL_LOG_LEVEL_TRACE + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_TRACE_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_TRACE_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_TRACE_FG_COLOR
            printf(PL_LOG_TRACE_FG_COLOR);
            #endif

            #ifdef PL_LOG_TRACE_BG_COLOR
            printf(PL_LOG_TRACE_BG_COLOR);
            #endif

            printf("[TRACE] ");
            char dest[PL_LOG_MAX_LINE_SIZE];
            pl_vsprintf(dest, cPFormat, args);
            printf("%s\n", dest);
            printf(PL_LOG_POP_CODE); // pop color   
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_TRACE, "[TRACE] ")
    }
}

void
pl__log_debug_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &tPContext->sbChannels[uID];

    if(tPChannel->uLevel < PL_LOG_LEVEL_DEBUG + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_DEBUG_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_DEBUG_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_DEBUG_FG_COLOR
            printf(PL_LOG_DEBUG_FG_COLOR);
            #endif

            #ifdef PL_LOG_DEBUG_BG_COLOR
            printf(PL_LOG_DEBUG_BG_COLOR);
            #endif

            printf("[DEBUG] ");
            char dest[PL_LOG_MAX_LINE_SIZE];
            pl_vsprintf(dest, cPFormat, args);
            printf("%s\n", dest);
            printf(PL_LOG_POP_CODE); // pop color   
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_DEBUG, "[DEBUG] ")
    }
}

void
pl__log_info_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &tPContext->sbChannels[uID];

    if(tPChannel->uLevel < PL_LOG_LEVEL_INFO + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_INFO_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_INFO_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_INFO_FG_COLOR
            printf(PL_LOG_INFO_FG_COLOR);
            #endif

            #ifdef PL_LOG_INFO_BG_COLOR
            printf(PL_LOG_INFO_BG_COLOR);
            #endif

            printf("[INFO ] ");
            char dest[PL_LOG_MAX_LINE_SIZE];
            pl_vsprintf(dest, cPFormat, args);
            printf("%s\n", dest);
            printf(PL_LOG_POP_CODE); // pop color   
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_INFO, "[INFO ] ")
    }
}

void
pl__log_warn_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &tPContext->sbChannels[uID];

    if(tPChannel->uLevel < PL_LOG_LEVEL_WARN + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_WARN_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_WARN_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_WARN_FG_COLOR
            printf(PL_LOG_WARN_FG_COLOR);
            #endif

            #ifdef PL_LOG_WARN_BG_COLOR
            printf(PL_LOG_WARN_BG_COLOR);
            #endif

            printf("[WARN ] ");
            char dest[PL_LOG_MAX_LINE_SIZE];
            pl_vsprintf(dest, cPFormat, args);
            printf("%s\n", dest);
            printf(PL_LOG_POP_CODE); // pop color   
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_WARN, "[WARN ] ")
    }
}

void
pl__log_error_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &tPContext->sbChannels[uID];

    if(tPChannel->uLevel < PL_LOG_LEVEL_ERROR + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_ERROR_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_ERROR_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_ERROR_FG_COLOR
            printf(PL_LOG_ERROR_FG_COLOR);
            #endif

            #ifdef PL_LOG_ERROR_BG_COLOR
            printf(PL_LOG_ERROR_BG_COLOR);
            #endif

            printf("[ERROR] ");
            char dest[PL_LOG_MAX_LINE_SIZE];
            pl_vsprintf(dest, cPFormat, args);
            printf("%s\n", dest);
            printf(PL_LOG_POP_CODE); // pop color   
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_ERROR, "[ERROR] ")
    }
}

void
pl__log_fatal_va(plLogContext* tPContext, uint32_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &tPContext->sbChannels[uID];

    if(tPChannel->uLevel < PL_LOG_LEVEL_FATAL + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_FATAL_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_FATAL_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_FATAL_FG_COLOR
            printf(PL_LOG_FATAL_FG_COLOR);
            #endif

            #ifdef PL_LOG_FATAL_BG_COLOR
            printf(PL_LOG_FATAL_BG_COLOR);
            #endif

            printf("[FATAL] ");
            char dest[PL_LOG_MAX_LINE_SIZE];
            pl_vsprintf(dest, cPFormat, args);
            printf("%s\n", dest);
            printf(PL_LOG_POP_CODE); // pop color   
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_FATAL, "[FATAL] ")
    }
}

#endif // PL_LOG_IMPLEMENTATION