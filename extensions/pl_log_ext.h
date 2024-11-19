/*
   pl_log_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
// [SECTION] macros
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_LOG_EXT_H
#define PL_LOG_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <stdarg.h>  // var args
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plLogI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_LOG_LEVEL_ALL       0
#define PL_LOG_LEVEL_TRACE  5000
#define PL_LOG_LEVEL_DEBUG  6000
#define PL_LOG_LEVEL_INFO   7000
#define PL_LOG_LEVEL_WARN   8000
#define PL_LOG_LEVEL_ERROR  9000
#define PL_LOG_LEVEL_FATAL 10000
#define PL_LOG_LEVEL_OFF   11000

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plLogExtContext     plLogExtContext;     // opaque struct
typedef struct _plLogExtChannelInit plLogExtChannelInit; // information to initialize a channel
typedef struct _plLogExtEntry       plLogExtEntry;       // represents a single entry for "buffer" channel types
typedef struct _plLogExtChannelInfo plLogExtChannelInfo; // information about a channel used mostly for visualization

// enums
typedef int plLogChannelType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plLogI
{

    uint64_t (*add_channel)      (const char* name, plLogExtChannelInit);
    void     (*set_level)        (uint64_t channelId, uint64_t level);
    void     (*clear_channel)    (uint64_t channelId);
    void     (*reset_channel)    (uint64_t channelId);
    uint64_t (*get_channel_id)   (const char* name);
    bool     (*get_channel_info) (uint64_t channelId, plLogExtChannelInfo*);
    uint64_t (*get_channel_count)(void);

    void (*custom)(const char* pcPrefix, int iPrefixSize, uint64_t level, uint64_t channelId, const char* pcMessage);
    void (*trace) (uint64_t channelId, const char* pcMessage);
    void (*debug) (uint64_t channelId, const char* pcMessage);
    void (*info)  (uint64_t channelId, const char* pcMessage);
    void (*warn)  (uint64_t channelId, const char* pcMessage);
    void (*error) (uint64_t channelId, const char* pcMessage);
    void (*fatal) (uint64_t channelId, const char* pcMessage);

    void (*custom_p)(const char* prefix, int prefixSize, uint64_t level, uint64_t channelId, const char* format, ...);
    void (*trace_p) (uint64_t channelId, const char* format, ...);
    void (*debug_p) (uint64_t channelId, const char* format, ...);
    void (*info_p)  (uint64_t channelId, const char* format, ...);
    void (*warn_p)  (uint64_t channelId, const char* format, ...);
    void (*error_p) (uint64_t channelId, const char* format, ...);
    void (*fatal_p) (uint64_t channelId, const char* format, ...);

    void (*custom_va)(const char* prefix, int prefixSize, uint64_t level, uint64_t channelId, const char* format, va_list args);
    void (*trace_va) (uint64_t channelId, const char* format, va_list args);
    void (*debug_va) (uint64_t channelId, const char* format, va_list args);
    void (*info_va)  (uint64_t channelId, const char* format, va_list args);
    void (*warn_va)  (uint64_t channelId, const char* format, va_list args);
    void (*error_va) (uint64_t channelId, const char* format, va_list args);
    void (*fatal_va) (uint64_t channelId, const char* format, va_list args);
} plLogI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plLogChannelType
{
    PL_LOG_CHANNEL_TYPE_DEFAULT       = 0,
    PL_LOG_CHANNEL_TYPE_CONSOLE       = 1 << 0,
    PL_LOG_CHANNEL_TYPE_BUFFER        = 1 << 1,
    PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER = 1 << 2
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plLogExtChannelInit
{
    plLogChannelType tType;
    uint64_t         uEntryCount; // default: 1024
} plLogExtChannelInit;

typedef struct _plLogExtEntry
{
    uint64_t uLevel;
    uint64_t uOffset;
} plLogExtEntry;

typedef struct _plLogExtChannelInfo
{
    uint64_t         uID;
    plLogChannelType tType;
    const char*      pcName;
    size_t           szBufferSize;
    char*            pcBuffer;
    uint64_t         uEntryCount;
    uint64_t         uEntryCapacity;
    plLogExtEntry*   ptEntries;
} plLogExtChannelInfo;

//-----------------------------------------------------------------------------
// [SECTION] macros
//-----------------------------------------------------------------------------

#ifdef PL_LOG_ON
    #ifndef PL_GLOBAL_LOG_LEVEL
        #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
    #endif

    #define pl_log(api, pcPrefix, iPrefixSize, uLevel, uID, pcMessage) (api)->log((pcPrefix), (iPrefixSize), (uLevel), (uID), (pcMessage))
    #define pl_log_f(api, ...) (api)->log_p(__VA_ARGS__)
#else
    #undef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_OFF
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_TRACE + 1 && defined(PL_LOG_ON)
    #define pl_log_trace(api, uID, pcMessage) (api)->trace((uID), (pcMessage))
    #define pl_log_trace_f(api, ...) (api)->trace_p(__VA_ARGS__)
#else
    #define pl_log_trace(api, uID, pcMessage) //
    #define pl_log_trace_f(api, ...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_DEBUG + 1 && defined(PL_LOG_ON)
    #define pl_log_debug(api, uID, pcMessage) (api)->debug((uID), (pcMessage))
    #define pl_log_debug_f(api, ...) (api)->debug_p(__VA_ARGS__)
#else
    #define pl_log_debug(api, uID, pcMessage) //
    #define pl_log_debug_f(api, ...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_INFO + 1 && defined(PL_LOG_ON)
    #define pl_log_info(api, uID, pcMessage) (api)->info((uID), (pcMessage))
    #define pl_log_info_f(api, ...) (api)->info_p(__VA_ARGS__)
#else
    #define pl_log_info(api, uID, pcMessage) //
    #define pl_log_info_f(api, ...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_WARN + 1 && defined(PL_LOG_ON)
    #define pl_log_warn(api, uID, pcMessage) (api)->warn((uID), (pcMessage))
    #define pl_log_warn_f(api, ...) (api)->warn_p(__VA_ARGS__)
#else
    #define pl_log_warn(api, uID, pcMessage) //
    #define pl_log_warn_f(api, ...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_ERROR + 1 && defined(PL_LOG_ON)
    #define pl_log_error(api, uID, pcMessage) (api)->error((uID), (pcMessage))
    #define pl_log_error_f(api, ...) (api)->error_p(__VA_ARGS__)
#else
    #define pl_log_error(api, uID, pcMessage) //
    #define pl_log_error_f(api, ...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_FATAL + 1 && defined(PL_LOG_ON)
    #define pl_log_fatal(api, uID, pcMessage) (api)->fatal((uID), (pcMessage))
    #define pl_log_fatal_f(api, ...) (api)->fatal_p(__VA_ARGS__)
#else
    #define pl_log_fatal(api, uID, pcMessage) //
    #define pl_log_fatal_f(api, ...) //
#endif

#endif // PL_LOG_EXT_H