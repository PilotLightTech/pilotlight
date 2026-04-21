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
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
// [SECTION] macros
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_LOG_EXT_H
#define PL_LOG_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <stdarg.h>  // var args
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plLogI_version {2, 0, 0}

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

// extension loading
PL_API void pl_load_log_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_log_ext(plApiRegistryI*, bool reload);

PL_API uint64_t pl_log_add_channel      (const char* name, plLogExtChannelInit);
PL_API void     pl_log_set_level        (uint64_t channelId, uint64_t level);
PL_API void     pl_log_clear_channel    (uint64_t channelId);
PL_API void     pl_log_reset_channel    (uint64_t channelId);
PL_API uint64_t pl_log_get_channel_id   (const char* name);
PL_API bool     pl_log_get_channel_info (uint64_t channelId, plLogExtChannelInfo*);
PL_API uint64_t pl_log_get_channel_count(void);

PL_API void pl_log_custom(const char* pcPrefix, int iPrefixSize, uint64_t level, uint64_t channelId, const char* pcMessage);
PL_API void pl_log_trace (uint64_t channelId, const char* pcMessage);
PL_API void pl_log_debug (uint64_t channelId, const char* pcMessage);
PL_API void pl_log_info  (uint64_t channelId, const char* pcMessage);
PL_API void pl_log_warn  (uint64_t channelId, const char* pcMessage);
PL_API void pl_log_error (uint64_t channelId, const char* pcMessage);
PL_API void pl_log_fatal (uint64_t channelId, const char* pcMessage);

PL_API void pl_log_custom_p(const char* prefix, int prefixSize, uint64_t level, uint64_t channelId, const char* format, ...);
PL_API void pl_log_trace_p (uint64_t channelId, const char* format, ...);
PL_API void pl_log_debug_p (uint64_t channelId, const char* format, ...);
PL_API void pl_log_info_p  (uint64_t channelId, const char* format, ...);
PL_API void pl_log_warn_p  (uint64_t channelId, const char* format, ...);
PL_API void pl_log_error_p (uint64_t channelId, const char* format, ...);
PL_API void pl_log_fatal_p (uint64_t channelId, const char* format, ...);

PL_API void pl_log_custom_va(const char* prefix, int prefixSize, uint64_t level, uint64_t channelId, const char* format, va_list args);
PL_API void pl_log_trace_va (uint64_t channelId, const char* format, va_list args);
PL_API void pl_log_debug_va (uint64_t channelId, const char* format, va_list args);
PL_API void pl_log_info_va  (uint64_t channelId, const char* format, va_list args);
PL_API void pl_log_warn_va  (uint64_t channelId, const char* format, va_list args);
PL_API void pl_log_error_va (uint64_t channelId, const char* format, va_list args);
PL_API void pl_log_fatal_va (uint64_t channelId, const char* format, va_list args);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
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

    #define PL_LOG_API(api, pcPrefix, iPrefixSize, uLevel, uID, pcMessage) (api)->custom((pcPrefix), (iPrefixSize), (uLevel), (uID), (pcMessage))
    #define PL_LOG_API_F(api, ...) (api)->custom_p(__VA_ARGS__)
    #define PL_LOG(pcPrefix, iPrefixSize, uLevel, uID, pcMessage) pl_log_custom((pcPrefix), (iPrefixSize), (uLevel), (uID), (pcMessage))
    #define PL_LOG_F(...) pl_log_custom_p(__VA_ARGS__)
#else
    #undef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_OFF
    #define PL_LOG_API(api, uID, pcMessage) //
    #define PL_LOG_API_F(api, ...) //
    #define PL_LOG(uID, pcMessage) //
    #define PL_LOG_F(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_TRACE + 1 && defined(PL_LOG_ON)
    #define PL_LOG_TRACE_API(api, uID, pcMessage) (api)->trace((uID), (pcMessage))
    #define PL_LOG_TRACE_API_F(api, ...) (api)->trace_p(__VA_ARGS__)
    #define PL_LOG_TRACE(uID, pcMessage) pl_log_trace((uID), (pcMessage))
    #define PL_LOG_TRACE_F(...) pl_log_trace_p(__VA_ARGS__)
#else
    #define PL_LOG_TRACE_API(api, uID, pcMessage) //
    #define PL_LOG_TRACE_API_F(api, ...) //
    #define PL_LOG_TRACE(uID, pcMessage) //
    #define PL_LOG_TRACE_F(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_DEBUG + 1 && defined(PL_LOG_ON)
    #define PL_LOG_DEBUG_API(api, uID, pcMessage) (api)->debug((uID), (pcMessage))
    #define PL_LOG_DEBUG_API_F(api, ...) (api)->debug_p(__VA_ARGS__)
    #define PL_LOG_DEBUG(uID, pcMessage) pl_log_debug((uID), (pcMessage))
    #define PL_LOG_DEBUG_F(...) pl_log_debug_p(__VA_ARGS__)
#else
    #define PL_LOG_DEBUG_API(api, uID, pcMessage) //
    #define PL_LOG_DEBUG_API_F(api, ...) //
    #define PL_LOG_DEBUG(uID, pcMessage) //
    #define PL_LOG_DEBUG_F(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_INFO + 1 && defined(PL_LOG_ON)
    #define PL_LOG_INFO_API(api, uID, pcMessage) (api)->info((uID), (pcMessage))
    #define PL_LOG_INFO_API_F(api, ...) (api)->info_p(__VA_ARGS__)
    #define PL_LOG_INFO(uID, pcMessage) pl_log_info((uID), (pcMessage))
    #define PL_LOG_INFO_F(...) pl_log_info_p(__VA_ARGS__)
#else
    #define PL_LOG_INFO_API(api, uID, pcMessage) //
    #define PL_LOG_INFO_API_F(api, ...) //
    #define PL_LOG_INFO(uID, pcMessage) //
    #define PL_LOG_INFO_F(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_WARN + 1 && defined(PL_LOG_ON)
    #define PL_LOG_WARN_API(api, uID, pcMessage) (api)->warn((uID), (pcMessage))
    #define PL_LOG_WARN_API_F(api, ...) (api)->warn_p(__VA_ARGS__)
    #define PL_LOG_WARN(uID, pcMessage) pl_log_warn((uID), (pcMessage))
    #define PL_LOG_WARN_F(...) pl_log_warn_p(__VA_ARGS__)
#else
    #define PL_LOG_WARN_API(api, uID, pcMessage) //
    #define PL_LOG_WARN_API_F(api, ...) //
    #define PL_LOG_WARN(uID, pcMessage) //
    #define PL_LOG_WARN_F(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_ERROR + 1 && defined(PL_LOG_ON)
    #define PL_LOG_ERROR_API(api, uID, pcMessage) (api)->error((uID), (pcMessage))
    #define PL_LOG_ERROR_API_F(api, ...) (api)->error_p(__VA_ARGS__)
    #define PL_LOG_ERROR(uID, pcMessage) pl_log_error((uID), (pcMessage))
    #define PL_LOG_ERROR_F(...) pl_log_error_p(__VA_ARGS__)
#else
    #define PL_LOG_ERROR_API(api, uID, pcMessage) //
    #define PL_LOG_ERROR_API_F(api, ...) //
    #define PL_LOG_ERROR(uID, pcMessage) //
    #define PL_LOG_ERROR_F(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_FATAL + 1 && defined(PL_LOG_ON)
    #define PL_LOG_FATAL_API(api, uID, pcMessage) (api)->fatal((uID), (pcMessage))
    #define PL_LOG_FATAL_API_F(api, ...) (api)->fatal_p(__VA_ARGS__)
    #define PL_LOG_FATAL(uID, pcMessage) pl_log_fatal((uID), (pcMessage))
    #define PL_LOG_FATAL_F(...) pl_log_fatal_p(__VA_ARGS__)
#else
    #define PL_LOG_FATAL_API(api, uID, pcMessage) //
    #define PL_LOG_FATAL_API_F(api, ...) //
    #define PL_LOG_FATAL(uID, pcMessage) //
    #define PL_LOG_FATAL_F(...) //
#endif

#ifdef __cplusplus
}
#endif

#endif // PL_LOG_EXT_H