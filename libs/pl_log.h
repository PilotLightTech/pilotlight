/*
   pl_log.h
     * simple logging library
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

// library version (format XYYZZ)
#define PL_LOG_VERSION    "1.0.1"
#define PL_LOG_VERSION_NUM 10001

/*
Index of this file:
// [SECTION] documentation
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] enums
// [SECTION] internal api
// [SECTION] c file start
*/

//-----------------------------------------------------------------------------
// [SECTION] documentation
//-----------------------------------------------------------------------------

/*

SETUP

    pl_create_log_context:
        plLogContext* pl_create_log_context();
            Creates the global context used by the logging system. Store the
            pointer returned if you want to use the logger across DLL boundaries.
            See "pl_set_log_context". 

    pl_cleanup_log_context:
        void pl_cleanup_log_context();
            Frees memory associated with the logging system. Do not call functions
            after this.

    pl_set_log_context:
        void pl_set_log_context(plLogContext*);
            Sets the current log context. Mostly used to allow logging across
            DLL boundaries.

CHANNELS

    pl_add_log_channel:
        uint64_t pl_add_log_channel(const char* pcName, plLogChannelInit tInfo);
            Creates a new log channel and returns the ID.

    pl_set_log_level:
        void pl_set_log_level(uID, uLevel);
            Sets the runtime logging level of the uID channel

    pl_clear_log_channel:
        void pl_clear_log_channel(uID);
            Frees the memory associated with log entries (for buffer channel types).

    pl_reset_log_channel:
        void pl_reset_log_channel(uID);
            Resets the log channel but does not free the memory.

    pl_get_log_channel_id:
        uint64_t pl_get_log_channel_id(const char* pcName);
            Returns the ID of the channel with the pcName (or UINT32_MAX if not found)

    pl_get_log_channel_info
        bool pl_get_log_channel_info(uint64_t uID, plLogChannelInfo*);
            Returns information on a channel (used for visualizing).

    pl_get_log_channel_count:
        uint64_t pl_get_log_channel_count(void);
            Returns the number of channels (used with the above function to iterate through channel info mostly).

SIMPLE LOGGING

    pl_log_trace
    pl_log_debug
    pl_log_info
    pl_log_warn
    pl_log_error
    pl_log_fatal;
        void pl_log_*(uID, pcMessage);
            Logs at the specified level. No color information. Faster if in a tight loop.

LOGGING WITH FORMAT SPECIFIERS

    pl_log_trace_f
    pl_log_debug_f
    pl_log_info_f
    pl_log_warn_f
    pl_log_error_f
    pl_log_fatal_f;
        void pl_log_*_f(uID, pcFormatString, ...);
            Logs at the specified level. Includes color when console.

CUSTOM LOGGING

    pl_log;
        void pl_log(pcPrefix, iPrefixSize, uLevel, uID, pcMessage);
            Logs at the specified level. No color information. Faster if in a tight loop.

CUSTOM LOGGING WITH FORMAT SPECIFIERS

    pl_log_f;
        void pl_log_f(cPrefix, iPrefixSize, uLevel, uID, pcFormatString, ...);
            Logs at the specified level. Includes color when console.

LOG LEVELS
    PL_LOG_LEVEL_ALL  
    PL_LOG_LEVEL_TRACE
    PL_LOG_LEVEL_DEBUG
    PL_LOG_LEVEL_INFO 
    PL_LOG_LEVEL_WARN 
    PL_LOG_LEVEL_ERROR
    PL_LOG_LEVEL_FATAL
    PL_LOG_LEVEL_OFF  

COMPILE TIME OPTIONS

    * Change maximum number of channels, define PL_LOG_MAX_CHANNEL_COUNT. (default is 16)
    * Change maximum lenght of lines, define PL_LOG_MAX_LINE_SIZE. (default is 1024)
    * Change the global log level, define PL_GLOBAL_LOG_LEVEL. (default is PL_LOG_LEVEL_ALL)
    * Change background colors by defining the following:
        PL_LOG_TRACE_BG_COLOR  <BACKGROUND COLOR OPTION>
        PL_LOG_DEBUG_BG_COLOR  <BACKGROUND COLOR OPTION>
        PL_LOG_INFO_BG_COLOR   <BACKGROUND COLOR OPTION>
        PL_LOG_WARN_BG_COLOR   <BACKGROUND COLOR OPTION>
        PL_LOG_ERROR_BG_COLOR  <BACKGROUND COLOR OPTION>
        PL_LOG_FATAL_BG_COLOR  <BACKGROUND COLOR OPTION>
        PL_LOG_CUSTOM_BG_COLOR <BACKGROUND COLOR OPTION>
    * Change foreground colors by defining the following:
        PL_LOG_TRACE_FG_COLOR  <FOREGROUND COLOR OPTION>
        PL_LOG_DEBUG_FG_COLOR  <FOREGROUND COLOR OPTION>
        PL_LOG_INFO_FG_COLOR   <FOREGROUND COLOR OPTION>
        PL_LOG_WARN_FG_COLOR   <FOREGROUND COLOR OPTION>
        PL_LOG_ERROR_FG_COLOR  <FOREGROUND COLOR OPTION>
        PL_LOG_FATAL_FG_COLOR  <FOREGROUND COLOR OPTION>
        PL_LOG_CUSTOM_FG_COLOR <FOREGROUND COLOR OPTION>
    * Use bold by defining the following:
        PL_LOG_TRACE_BOLD
        PL_LOG_DEBUG_BOLD
        PL_LOG_INFO_BOLD 
        PL_LOG_WARN_BOLD 
        PL_LOG_ERROR_BOLD
        PL_LOG_FATAL_BOLD
        PL_LOG_CUSTOM_BOLD
    * Use underline by defining the following:
        PL_LOG_TRACE_UNDERLINE
        PL_LOG_DEBUG_UNDERLINE
        PL_LOG_INFO_UNDERLINE 
        PL_LOG_WARN_UNDERLINE 
        PL_LOG_ERROR_UNDERLINE
        PL_LOG_FATAL_UNDERLINE
        PL_LOG_CUSTOM_UNDERLINE
    * Change allocators by defining both:
        PL_LOG_ALLOC(x)
        PL_LOG_FREE(x)

FOREGROUND COLOR OPTIONS

    PL_LOG_FG_COLOR_CODE_BLACK         
    PL_LOG_FG_COLOR_CODE_RED           
    PL_LOG_FG_COLOR_CODE_GREEN         
    PL_LOG_FG_COLOR_CODE_YELLOW        
    PL_LOG_FG_COLOR_CODE_BLUE          
    PL_LOG_FG_COLOR_CODE_MAGENTA       
    PL_LOG_FG_COLOR_CODE_CYAN          
    PL_LOG_FG_COLOR_CODE_WHITE         
    PL_LOG_FG_COLOR_CODE_STRONG_BLACK  
    PL_LOG_FG_COLOR_CODE_STRONG_RED    
    PL_LOG_FG_COLOR_CODE_STRONG_GREEN  
    PL_LOG_FG_COLOR_CODE_STRONG_YELLOW 
    PL_LOG_FG_COLOR_CODE_STRONG_BLUE   
    PL_LOG_FG_COLOR_CODE_STRONG_MAGENTA
    PL_LOG_FG_COLOR_CODE_STRONG_CYAN   
    PL_LOG_FG_COLOR_CODE_STRONG_WHITE  

BACKGROUND COLOR OPTIONS

    PL_LOG_BG_COLOR_CODE_BLACK        
    PL_LOG_BG_COLOR_CODE_RED          
    PL_LOG_BG_COLOR_CODE_GREEN        
    PL_LOG_BG_COLOR_CODE_YELLOW       
    PL_LOG_BG_COLOR_CODE_BLUE         
    PL_LOG_BG_COLOR_CODE_MAGENTA      
    PL_LOG_BG_COLOR_CODE_CYAN         
    PL_LOG_BG_COLOR_CODE_WHITE        
    PL_LOG_BG_COLOR_CODE_STRONG_BLACK  
    PL_LOG_BG_COLOR_CODE_STRONG_RED    
    PL_LOG_BG_COLOR_CODE_STRONG_GREEN  
    PL_LOG_BG_COLOR_CODE_STRONG_YELLOW 
    PL_LOG_BG_COLOR_CODE_STRONG_BLUE   
    PL_LOG_BG_COLOR_CODE_STRONG_MAGENTA
    PL_LOG_BG_COLOR_CODE_STRONG_CYAN   
    PL_LOG_BG_COLOR_CODE_STRONG_WHITE  

*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_LOG_H
#define PL_LOG_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <stdarg.h>  // var args
#include <stdbool.h> // bool

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
typedef struct _plLogContext     plLogContext;     // opaque struct
typedef struct _plLogChannelInit plLogChannelInit; // information to initialize a channel
typedef struct _plLogEntry       plLogEntry;       // represents a single entry for "buffer" channel types
typedef struct _plLogChannelInfo plLogChannelInfo; // information about a channel used mostly for visualization

// enums
typedef int plChannelType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

#ifndef PL_LOG_REMOVE_ALL_MACROS 

#ifdef PL_LOG_ON

    // setup/shutdown
    #define pl_create_log_context()       pl__create_log_context()
    #define pl_cleanup_log_context()      pl__cleanup_log_context()
    #define pl_set_log_context(tPContext) pl__set_log_context((tPContext))

    // channels
    #define pl_add_log_channel(pcName, tInfo)       pl__add_log_channel((pcName), (tInfo))
    #define pl_set_log_level(uID, uLevel)           pl__set_log_level((uID), (uLevel))
    #define pl_clear_log_channel(uID)               pl__clear_log_channel((uID))
    #define pl_reset_log_channel(uID)               pl__reset_log_channel((uID))
    #define pl_get_log_channel_count()              pl__get_log_channel_count()
    #define pl_get_log_channel_id(pcName)           pl__get_log_channel_id((pcName))
    #define pl_get_log_channel_info(uID, ptInfoOut) pl__get_log_channel_info((uID), (ptInfoOut))

    // custom levels
    #define pl_log(pcPrefix, iPrefixSize, uLevel, uID, pcMessage) pl__log(pcPrefix, iPrefixSize, uLevel, uID, pcMessage)
    #define pl_log_f(...) pl__log_p(__VA_ARGS__)

#endif // PL_LOG_ON

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_TRACE + 1 && defined(PL_LOG_ON)
    #define pl_log_trace(uID, pcMessage) pl__log_trace((uID), (pcMessage))
    #define pl_log_trace_f(...) pl__log_trace_p(__VA_ARGS__)
#else
    #define pl_log_trace(uID, pcMessage) //
    #define pl_log_trace_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_DEBUG + 1 && defined(PL_LOG_ON)
    #define pl_log_debug(uID, pcMessage) pl__log_debug((uID), (pcMessage))
    #define pl_log_debug_f(...) pl__log_debug_p(__VA_ARGS__)
#else
    #define pl_log_debug(uID, pcMessage) //
    #define pl_log_debug_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_INFO + 1 && defined(PL_LOG_ON)
    #define pl_log_info(uID, pcMessage) pl__log_info((uID), (pcMessage))
    #define pl_log_info_f(...) pl__log_info_p(__VA_ARGS__)
#else
    #define pl_log_info(uID, pcMessage) //
    #define pl_log_info_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_WARN + 1 && defined(PL_LOG_ON)
    #define pl_log_warn(uID, pcMessage) pl__log_warn((uID), (pcMessage))
    #define pl_log_warn_f(...) pl__log_warn_p(__VA_ARGS__)
#else
    #define pl_log_warn(tPContext, uID) //
    #define pl_log_warn_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_ERROR + 1 && defined(PL_LOG_ON)
    #define pl_log_error(uID, pcMessage) pl__log_error((uID), (pcMessage))
    #define pl_log_error_f(...) pl__log_error_p(__VA_ARGS__)
#else
    #define pl_log_error(tPContext, uID) //
    #define pl_log_error_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_FATAL + 1 && defined(PL_LOG_ON)
    #define pl_log_fatal(uID, pcMessage) pl__log_fatal((uID), (pcMessage))
    #define pl_log_fatal_f(...) pl__log_fatal_p(__VA_ARGS__)
#else
    #define pl_log_fatal(uID, pcMessage) //
    #define pl_log_fatal_f(...) //
#endif

#endif // PL_LOG_REMOVE_ALL_MACROS

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plLogChannelInit
{
    plChannelType tType;
    uint64_t      uEntryCount; // default: 1024
} plLogChannelInit;

typedef struct _plLogEntry
{
    uint64_t uLevel;
    uint64_t uOffset;
} plLogEntry;

typedef struct _plLogChannelInfo
{
    uint64_t      uID;
    plChannelType tType;
    const char*   pcName;
    size_t        szBufferSize;
    char*         pcBuffer;
    uint64_t      uEntryCount;
    uint64_t      uEntryCapacity;
    plLogEntry*   ptEntries;
} plLogChannelInfo;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plChannelType_
{
    PL_CHANNEL_TYPE_DEFAULT       = 0,
    PL_CHANNEL_TYPE_CONSOLE       = 1 << 0,
    PL_CHANNEL_TYPE_BUFFER        = 1 << 1,
    PL_CHANNEL_TYPE_CYCLIC_BUFFER = 1 << 2
};

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// setup/shutdown
plLogContext* pl__create_log_context (void);
void          pl__cleanup_log_context(void);
void          pl__set_log_context    (plLogContext*);

// channels
uint64_t pl__add_log_channel      (const char* pcName, plLogChannelInit);
void     pl__set_log_level        (uint64_t uID, uint64_t uLevel);
void     pl__clear_log_channel    (uint64_t uID);
void     pl__reset_log_channel    (uint64_t uID);
uint64_t pl__get_log_channel_id   (const char* pcName);
bool     pl__get_log_channel_info (uint64_t uID, plLogChannelInfo*);
uint64_t pl__get_log_channel_count(void);

// logging
void pl__log      (const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uID, const char* pcMessage);
void pl__log_trace(uint64_t uID, const char* pcMessage);
void pl__log_debug(uint64_t uID, const char* pcMessage);
void pl__log_info (uint64_t uID, const char* pcMessage);
void pl__log_warn (uint64_t uID, const char* pcMessage);
void pl__log_error(uint64_t uID, const char* pcMessage);
void pl__log_fatal(uint64_t uID, const char* pcMessage);

void pl__log_p      (const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uID, const char* cPFormat, ...);
void pl__log_trace_p(uint64_t uID, const char* cPFormat, ...);
void pl__log_debug_p(uint64_t uID, const char* cPFormat, ...);
void pl__log_info_p (uint64_t uID, const char* cPFormat, ...);
void pl__log_warn_p (uint64_t uID, const char* cPFormat, ...);
void pl__log_error_p(uint64_t uID, const char* cPFormat, ...);
void pl__log_fatal_p(uint64_t uID, const char* cPFormat, ...);

void pl__log_va      (const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uID, const char* cPFormat, va_list args);
void pl__log_trace_va(uint64_t uID, const char* cPFormat, va_list args);
void pl__log_debug_va(uint64_t uID, const char* cPFormat, va_list args);
void pl__log_info_va (uint64_t uID, const char* cPFormat, va_list args);
void pl__log_warn_va (uint64_t uID, const char* cPFormat, va_list args);
void pl__log_error_va(uint64_t uID, const char* cPFormat, va_list args);
void pl__log_fatal_va(uint64_t uID, const char* cPFormat, va_list args);

#ifndef PL_LOG_REMOVE_ALL_MACROS
#ifndef PL_LOG_ON
    #define pl_create_log_context() NULL
    #define pl_cleanup_log_context() //
    #define pl_set_log_context(ctx) //
    #define pl_add_log_channel(pcName, tInfo) 0u
    #define pl_set_log_level(uID, uLevel) //
    #define pl_clear_log_channel(uID) //
    #define pl_reset_log_channel(uID) //
    #define pl_get_log_channel_count() 0
    #define pl_get_log_channel_id(pcName) 0
    #define pl_get_log_channel_info(uID, ptInfo) false
    #define pl_log(pcPrefix, iPrefixSize, uLevel, uID, pcMessage) //
    #define pl_log_f(...) //
#endif
#endif // PL_LOG_REMOVE_ALL_MACROS

#endif // PL_LOG_H

//-----------------------------------------------------------------------------
// [SECTION] c file start
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global context
// [SECTION] internal api
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef PL_LOG_IMPLEMENTATION

#ifndef PL_LOG_ALLOC
    #include <stdlib.h>
    #define PL_LOG_ALLOC(x) malloc((x))
    #define PL_LOG_FREE(x)  free((x))
#endif

#ifndef PL_LOG_MAX_CHANNEL_COUNT
    #define PL_LOG_MAX_CHANNEL_COUNT 16
#endif

#ifndef PL_LOG_MAX_LINE_SIZE
    #define PL_LOG_MAX_LINE_SIZE 1024
#endif

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

#ifndef PL_LOG_FATAL_BG_COLOR
    #define PL_LOG_FATAL_BG_COLOR PL_LOG_BG_COLOR_CODE_RED
#endif
#ifndef PL_LOG_TRACE_FG_COLOR
    #define PL_LOG_TRACE_FG_COLOR PL_LOG_FG_COLOR_CODE_GREEN
#endif
#ifndef PL_LOG_DEBUG_FG_COLOR
    #define PL_LOG_DEBUG_FG_COLOR PL_LOG_FG_COLOR_CODE_CYAN
#endif
#ifndef PL_LOG_INFO_FG_COLOR
    #define PL_LOG_INFO_FG_COLOR PL_LOG_FG_COLOR_CODE_WHITE
#endif
#ifndef PL_LOG_WARN_FG_COLOR
    #define PL_LOG_WARN_FG_COLOR PL_LOG_FG_COLOR_CODE_YELLOW
#endif
#ifndef PL_LOG_ERROR_FG_COLOR
    #define PL_LOG_ERROR_FG_COLOR PL_LOG_FG_COLOR_CODE_RED
#endif
#ifndef PL_LOG_FATAL_FG_COLOR
    #define PL_LOG_FATAL_FG_COLOR PL_LOG_FG_COLOR_CODE_WHITE
#endif
#ifndef PL_LOG_CUSTOM_FG_COLOR
    #define PL_LOG_CUSTOM_FG_COLOR PL_LOG_FG_COLOR_CODE_WHITE
#endif
#ifndef PL_LOG_CUSTOM_BG_COLOR
    #define PL_LOG_CUSTOM_BG_COLOR PL_LOG_BG_COLOR_CODE_CYAN
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <stdbool.h>

#ifndef pl_snprintf
    #include <stdio.h>
    #define pl_snprintf snprintf
    #define pl_vsnprintf vsnprintf
#endif

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plLogChannel
{
    const char*   pcName;
    char*         pcBuffer;
    uint64_t      szGeneration;
    size_t        szBufferSize;
    size_t        szBufferCapacity; // real capacity is 2x this
    plLogEntry*   ptEntries;
    uint64_t      uEntryCount;
    uint64_t      uEntryCapacity;
    uint64_t      uNextEntry;
    uint64_t      uLevel;
    plChannelType tType;
    uint64_t      uID;
} plLogChannel;

typedef struct _plLogContext
{
    plLogChannel atChannels[PL_LOG_MAX_CHANNEL_COUNT];
    uint64_t     uChannelCount;
} plLogContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plLogContext* gptLogContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plLogEntry*
pl__get_new_log_entry(uint64_t uID)
{
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

    plLogEntry* ptEntry = NULL;

    if(tPChannel->tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER)
    {
        ptEntry = &tPChannel->ptEntries[tPChannel->uNextEntry];
        tPChannel->uNextEntry++;
        tPChannel->uNextEntry = tPChannel->uNextEntry % tPChannel->uEntryCapacity;
        tPChannel->uEntryCount++;
        if(tPChannel->uNextEntry == 0)
        {
            tPChannel->szBufferSize = 0;
            tPChannel->szGeneration++;
        }
    }
    else if(tPChannel->tType & PL_CHANNEL_TYPE_BUFFER)
    {

        // check if overflow reallocation is needed
        if(tPChannel->uEntryCount == tPChannel->uEntryCapacity)
        {
            uint64_t uNewCapacity = tPChannel->uEntryCapacity * 2;
            if(uNewCapacity == 0)
                uNewCapacity = 512;
            plLogEntry* sbtOldEntries = tPChannel->ptEntries;
            tPChannel->ptEntries = (plLogEntry*)PL_LOG_ALLOC(sizeof(plLogEntry) * uNewCapacity);
            memset(tPChannel->ptEntries, 0, sizeof(plLogEntry) * uNewCapacity);
            
            // copy old values
            if(tPChannel->uEntryCapacity > 0)
                memcpy(tPChannel->ptEntries, sbtOldEntries, sizeof(plLogEntry) * tPChannel->uEntryCapacity);
            
            tPChannel->uEntryCapacity  = uNewCapacity;

            PL_LOG_FREE(sbtOldEntries);
        }
        ptEntry = &tPChannel->ptEntries[tPChannel->uEntryCount];
        tPChannel->uEntryCount++;
    }
    return ptEntry;
}

static inline void
pl__log_buffer_may_grow(plLogChannel* ptChannel, int iAdditionalSize)
{
    if(ptChannel->szBufferSize + iAdditionalSize > ptChannel->szBufferCapacity) // grow
    {
        char* pcOldBuffer = ptChannel->pcBuffer;
        size_t uNewCapacity = ptChannel->szBufferCapacity * 2;
        if(uNewCapacity < ptChannel->szBufferSize + iAdditionalSize)
            uNewCapacity = (ptChannel->szBufferSize + (size_t)iAdditionalSize) * 2;
        ptChannel->pcBuffer = (char*)PL_LOG_ALLOC(uNewCapacity * 2);
        memset(ptChannel->pcBuffer, 0, uNewCapacity * 2);
        memcpy(ptChannel->pcBuffer, pcOldBuffer, ptChannel->szBufferCapacity);
        ptChannel->szBufferCapacity = uNewCapacity;
        PL_LOG_FREE(pcOldBuffer);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plLogContext*
pl__create_log_context(void)
{
    static plLogContext gtContext = {0};
    gptLogContext = &gtContext;

    // setup log channels
    plLogChannelInit tLogInit = {
        .tType       = PL_CHANNEL_TYPE_CONSOLE | PL_CHANNEL_TYPE_BUFFER,
        .uEntryCount = 1024
    };
    pl__add_log_channel("Default", tLogInit);

    #if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_TRACE + 1 && defined(PL_LOG_ON)
        pl__log_trace_p(0, "<- global enabled");
    #endif
    #if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_DEBUG + 1 && defined(PL_LOG_ON)
        pl__log_debug_p(0, "<- global enabled");
    #endif
    #if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_INFO + 1 && defined(PL_LOG_ON)
        pl__log_info_p(0, "<- global enabled");
    #endif
    #if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_WARN + 1 && defined(PL_LOG_ON)
        pl__log_warn_p(0, "<- global enabled");
    #endif
    #if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_ERROR + 1 && defined(PL_LOG_ON)
        pl__log_error_p(0, "<- global enabled");
    #endif
    #if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_FATAL + 1 && defined(PL_LOG_ON)
        pl__log_fatal_p(0, "<- global enabled");
    #endif
    
    return gptLogContext;
}

void
pl__cleanup_log_context(void)
{
    PL_ASSERT(gptLogContext && "no global log context set");
    if(gptLogContext)
    {
        for(uint64_t i = 0; i < gptLogContext->uChannelCount; i++)
        {
            plLogChannel* ptChannel = &gptLogContext->atChannels[i];
            if(ptChannel->pcBuffer)
                PL_LOG_FREE(ptChannel->pcBuffer);
            if(ptChannel->ptEntries)
            {
                PL_LOG_FREE(ptChannel->ptEntries);
            }
            ptChannel->ptEntries       = NULL;
            ptChannel->pcBuffer        = NULL;
            ptChannel->szBufferCapacity = 0;
            ptChannel->szBufferSize     = 0;
            ptChannel->uEntryCapacity   = 0;
            ptChannel->uEntryCount      = 0;
            ptChannel->uLevel           = 0;
            ptChannel->tType            = 0;
            ptChannel->uID              = 0;
        }
        memset(gptLogContext->atChannels, 0, sizeof(plLogChannel) * PL_LOG_MAX_CHANNEL_COUNT);
        gptLogContext->uChannelCount = 0;
    }
    gptLogContext = NULL;
}

void
pl__set_log_context(plLogContext* tPContext)
{
    PL_ASSERT(tPContext && "log context is NULL");
    gptLogContext = tPContext;
}

uint64_t
pl__add_log_channel(const char* pcName, plLogChannelInit tInit)
{
    uint64_t uID = gptLogContext->uChannelCount;

    if(tInit.uEntryCount == 0)
        tInit.uEntryCount = 1024;

    if((tInit.tType & PL_CHANNEL_TYPE_BUFFER) && (tInit.tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER))
    {
        PL_ASSERT(false && "Can't have PL_CHANNEL_TYPE_BUFFER and PL_CHANNEL_TYPE_CYCLIC_BUFFER together");
    }
    
    plLogChannel* ptChannel = &gptLogContext->atChannels[uID];
    ptChannel->pcName = pcName;
    if(tInit.tType & PL_CHANNEL_TYPE_BUFFER)
    {
        ptChannel->ptEntries       = (plLogEntry*)PL_LOG_ALLOC(tInit.uEntryCount * sizeof(plLogEntry));
        ptChannel->uEntryCapacity = tInit.uEntryCount;
        memset(ptChannel->ptEntries, 0, tInit.uEntryCount * sizeof(plLogEntry));
    }
    else if(tInit.tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER)
    {
        ptChannel->ptEntries       = (plLogEntry*)PL_LOG_ALLOC(tInit.uEntryCount * sizeof(plLogEntry));
        ptChannel->uEntryCapacity = tInit.uEntryCount;
    }
    else
    {
        ptChannel->ptEntries       = NULL;
        ptChannel->uEntryCapacity = 0;
        pl__log_buffer_may_grow(ptChannel, PL_LOG_MAX_LINE_SIZE);
    }

    ptChannel->uEntryCount    = 0;
    ptChannel->uNextEntry     = 0;
    ptChannel->uLevel         = 0;
    ptChannel->tType          = tInit.tType;
    ptChannel->uID            = uID;

    gptLogContext->uChannelCount++;
    return uID;
}

void
pl__set_log_level(uint64_t uID, uint64_t uLevel)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->atChannels[uID].uLevel = uLevel;
}

void
pl__clear_log_channel(uint64_t uID)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->atChannels[uID].uEntryCount = 0u;
    gptLogContext->atChannels[uID].uNextEntry = 0u;
    if(gptLogContext->atChannels[uID].tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER || gptLogContext->atChannels[uID].tType & PL_CHANNEL_TYPE_BUFFER)
    {
        PL_LOG_FREE(gptLogContext->atChannels[uID].ptEntries);
        gptLogContext->atChannels[uID].ptEntries = NULL;
        gptLogContext->atChannels[uID].uEntryCapacity = 0;
    }
}

void
pl__reset_log_channel(uint64_t uID)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->atChannels[uID].uEntryCount = 0u;
    gptLogContext->atChannels[uID].uNextEntry = 0u;

    if(gptLogContext->atChannels[uID].tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER || gptLogContext->atChannels[uID].tType & PL_CHANNEL_TYPE_BUFFER)
        memset(gptLogContext->atChannels[uID].ptEntries, 0, sizeof(plLogEntry) * gptLogContext->atChannels[uID].uEntryCapacity);
}

bool
pl__get_log_channel_info(uint64_t uID, plLogChannelInfo* ptOut)
{
    if(uID >= gptLogContext->uChannelCount)
        return false;

    ptOut->uID            = uID;
    ptOut->pcName         = gptLogContext->atChannels[uID].pcName;
    ptOut->tType          = gptLogContext->atChannels[uID].tType;
    ptOut->pcBuffer       = gptLogContext->atChannels[uID].pcBuffer;
    ptOut->uEntryCount   = gptLogContext->atChannels[uID].uEntryCount;
    ptOut->ptEntries      = gptLogContext->atChannels[uID].ptEntries;
    ptOut->uEntryCapacity = gptLogContext->atChannels[uID].uEntryCapacity;
    return true;
}

uint64_t
pl__get_log_channel_count(void)
{
    return gptLogContext->uChannelCount;
}

uint64_t
pl__get_log_channel_id(const char* pcName)
{
    for(uint64_t i = 0; i < gptLogContext->uChannelCount; i++)
    {
        if(strcmp(gptLogContext->atChannels[i].pcName, pcName) == 0)
            return i;
    }
    return SIZE_MAX;
}

#define PL__LOG_LEVEL_MACRO(level, prefix, prefixSize) \
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID]; \
    if(tPChannel->uLevel < level + 1) \
    { \
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE) \
            printf("%s (%s) %s\n", prefix, tPChannel->pcName, pcMessage); \
        if((tPChannel->tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER) || (tPChannel->tType & PL_CHANNEL_TYPE_BUFFER)) \
        { \
            plLogEntry* ptEntry = pl__get_new_log_entry(uID); \
            const size_t szNewSize = strlen(pcMessage) + prefixSize + 2; \
            pl__log_buffer_may_grow(tPChannel, (int)szNewSize); \
            char* cPDest = &tPChannel->pcBuffer[tPChannel->szBufferSize + tPChannel->szBufferCapacity * (tPChannel->szGeneration % 2)]; \
            ptEntry->uOffset = tPChannel->szBufferSize + tPChannel->szBufferCapacity * (tPChannel->szGeneration % 2); \
            ptEntry->uLevel = level; \
            tPChannel->szBufferSize += szNewSize; \
            strcpy(cPDest, prefix); \
            cPDest[prefixSize] = ' '; \
            cPDest += prefixSize + 1; \
            strcpy(cPDest, pcMessage); \
        } \
    }

void
pl__log(const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(uLevel, pcPrefix, iPrefixSize)
}

void
pl__log_trace(uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_TRACE, "[TRACE]", 7)
}

void
pl__log_debug(uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_DEBUG, "[DEBUG]", 7)
}

void
pl__log_info(uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_INFO, "[INFO ]", 7)
}

void
pl__log_warn(uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_WARN, "[WARN ]", 7)
}

void
pl__log_error(uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_ERROR, "[ERROR]", 7)
}

void
pl__log_fatal(uint64_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_FATAL, "[FATAL]", 7)
}

void
pl__log_p(const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_va(pcPrefix, iPrefixSize, uLevel, uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_trace_p(uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_trace_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_debug_p(uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_debug_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_info_p(uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_info_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_warn_p(uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_warn_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_error_p(uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_error_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_fatal_p(uint64_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_fatal_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

#define PL__LOG_LEVEL_VA_BUFFER_MACRO(level, prefix, prefixSize) \
        if((tPChannel->tType & PL_CHANNEL_TYPE_CYCLIC_BUFFER) || (tPChannel->tType & PL_CHANNEL_TYPE_BUFFER)) \
        { \
            va_list parm_copy; \
            va_copy(parm_copy, args); \
            plLogEntry* ptEntry = pl__get_new_log_entry(uID); \
            const int iNewSize = pl_vsnprintf(NULL, 0, cPFormat, parm_copy) + prefixSize + 2; \
            va_end(parm_copy); \
            pl__log_buffer_may_grow(tPChannel, iNewSize); \
            char* cPDest = &tPChannel->pcBuffer[tPChannel->szBufferSize + tPChannel->szBufferCapacity * (tPChannel->szGeneration % 2)]; \
            ptEntry->uOffset = tPChannel->szBufferSize + tPChannel->szBufferCapacity * (tPChannel->szGeneration % 2); \
            tPChannel->szBufferSize += iNewSize; \
            ptEntry->uLevel = level; \
            cPDest += pl_snprintf(cPDest, prefixSize + 2, "%s ", prefix); \
            va_list parm_copy2; \
            va_copy(parm_copy2, args); \
            pl_vsnprintf(cPDest, iNewSize, cPFormat, parm_copy2); \
            va_end(parm_copy2); \
        }

void
pl__log_va(const char* pcPrefix, int iPrefixSize, uint64_t uLevel, uint64_t uID, const char* cPFormat, va_list args)
{
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

    if(tPChannel->uLevel < uLevel + 1)
    {
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE)
        {
            #ifdef PL_LOG_CUSTOM_BOLD
            printf(PL_LOG_BOLD_CODE);
            #endif

            #ifdef PL_LOG_CUSTOM_UNDERLINE
            printf(PL_LOG_UNDERLINE_CODE);
            #endif

            #ifdef PL_LOG_CUSTOM_FG_COLOR
            printf(PL_LOG_CUSTOM_FG_COLOR);
            #endif

            #ifdef PL_LOG_CUSTOM_BG_COLOR
            printf(PL_LOG_CUSTOM_BG_COLOR);
            #endif

            printf("%s (%s) ", pcPrefix, tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(uLevel, pcPrefix, iPrefixSize) 
    }   
}

void
pl__log_trace_va(uint64_t uID, const char* cPFormat, va_list args)
{

    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

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

            printf("[TRACE] (%s) ", tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_TRACE, "[TRACE]", 7) 
    }   
}

void
pl__log_debug_va(uint64_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

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

            printf("[DEBUG] (%s) ", tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_DEBUG, "[DEBUG]", 7)
    }
}

void
pl__log_info_va(uint64_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

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

            printf("[INFO ] (%s) ", tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_INFO, "[INFO ]", 7)
    }
}

void
pl__log_warn_va(uint64_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

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

            printf("[WARN ] (%s) ", tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy); 
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_WARN, "[WARN ]", 7)
    }
}

void
pl__log_error_va(uint64_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

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

            printf("[ERROR] (%s) ", tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_ERROR, "[ERROR]", 7)
    }
}

void
pl__log_fatal_va(uint64_t uID, const char* cPFormat, va_list args)
{
   
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

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

            printf("[FATAL] (%s) ", tPChannel->pcName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_FATAL, "[FATAL]", 7)
    }
}

#endif // PL_LOG_IMPLEMENTATION