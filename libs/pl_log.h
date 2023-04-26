/*
   pl_log
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

// library version
#define PL_LOG_VERSION    "0.3.2"
#define PL_LOG_VERSION_NUM 00302

/*
Index of this file:
// [SECTION] documentation
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
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

    pl_get_log_context:
        plLogContext* pl_get_log_context();
            Returns the current log context.

CHANNELS

    pl_add_log_channel:
        uint32_t pl_add_log_channel(const char* pcName, plChannelType tType);
            Creates a new log channel and returns the ID.

    pl_set_log_level:
        void pl_set_log_level(uID, uLevel);
            Sets the runtime logging level of the uID channel

    pl_set_current_log_channel:
        void pl_set_current_log_channel(uID);
            Sets the current logging channel

    pl_get_current_log_channel:
        uint32_t pl_get_current_log_channel();
            Gets the current logging channel

    pl_clear_log_level:
        void pl_clear_log_level(uID);
            Frees the memory associated with log entries (for buffer channel types).

    pl_reset_log_level:
        void pl_reset_log_level(uID);
            Resets the log channel but does not free the memory.

    pl_get_log_entries:
        plLogEntry* pl_get_log_entries(uID, uint32_t* puEntryCount);
            Returns a pointer to the log entries (or NULL if empty). Fills out puEntryCount
            with the count.

SIMPLE LOGGING TO CURRENT CHANNEL

    pl_log_trace
    pl_log_debug
    pl_log_info
    pl_log_warn
    pl_log_error
    pl_log_fatal:
        void pl_log_*(pcMessage);
            Logs at the specified level. No color information. Faster if in a tight loop.

LOGGING TO CURRENT CHANNEL WITH FORMAT SPECIFIERS

    pl_log_trace_f
    pl_log_debug_f
    pl_log_info_f
    pl_log_warn_f
    pl_log_error_f
    pl_log_fatal_f:
        void pl_log_*_f(pcFormatString, ...);
            Logs at the specified level. Includes color when console.

SIMPLE LOGGING TO SPECIFIED CHANNEL

    pl_log_trace_to
    pl_log_debug_to
    pl_log_info_to
    pl_log_warn_to
    pl_log_error_to
    pl_log_fatal_to;
        void pl_log_*_to(uID, pcMessage);
            Logs at the specified level. No color information. Faster if in a tight loop.

LOGGING TO SPECIFIED CHANNEL WITH FORMAT SPECIFIERS

    pl_log_trace_to_f
    pl_log_debug_to_f
    pl_log_info_to_f
    pl_log_warn_to_f
    pl_log_error_to_f
    pl_log_fatal_to_f;
        void pl_log_*_to_f(uID, pcFormatString, ...);
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
    * Change the global log level, define PL_GLOBAL_LOG_LEVEL. (default is PL_LOG_LEVEL_ALL)
    * Change background colors by defining the following:
        PL_LOG_TRACE_BG_COLOR <BACKGROUND COLOR OPTION>
        PL_LOG_DEBUG_BG_COLOR <BACKGROUND COLOR OPTION>
        PL_LOG_INFO_BG_COLOR  <BACKGROUND COLOR OPTION>
        PL_LOG_WARN_BG_COLOR  <BACKGROUND COLOR OPTION>
        PL_LOG_ERROR_BG_COLOR <BACKGROUND COLOR OPTION>
        PL_LOG_FATAL_BG_COLOR <BACKGROUND COLOR OPTION>
    * Change foreground colors by defining the following:
        PL_LOG_TRACE_FG_COLOR <FOREGROUND COLOR OPTION>
        PL_LOG_DEBUG_FG_COLOR <FOREGROUND COLOR OPTION>
        PL_LOG_INFO_FG_COLOR  <FOREGROUND COLOR OPTION>
        PL_LOG_WARN_FG_COLOR  <FOREGROUND COLOR OPTION>
        PL_LOG_ERROR_FG_COLOR <FOREGROUND COLOR OPTION>
        PL_LOG_FATAL_FG_COLOR <FOREGROUND COLOR OPTION>
    * Use bold by defining the following:
        PL_LOG_TRACE_BOLD
        PL_LOG_DEBUG_BOLD
        PL_LOG_INFO_BOLD 
        PL_LOG_WARN_BOLD 
        PL_LOG_ERROR_BOLD
        PL_LOG_FATAL_BOLD
    * Use underline by defining the following:
        PL_LOG_TRACE_UNDERLINE
        PL_LOG_DEBUG_UNDERLINE
        PL_LOG_INFO_UNDERLINE 
        PL_LOG_WARN_UNDERLINE 
        PL_LOG_ERROR_UNDERLINE
        PL_LOG_FATAL_UNDERLINE
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

#ifndef PL_LOG_ALLOC
    #include <stdlib.h>
    #define PL_LOG_ALLOC(x) malloc((x))
    #define PL_LOG_FREE(x)  free((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdarg.h>

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

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
typedef struct _plLogContext plLogContext; // opaque struct
typedef struct _plLogEntry   plLogEntry;   // represents a single entry for "buffer" channel types

// enums
typedef int plChannelType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

#ifdef PL_LOG_ON

    // setup/shutdown
    #define pl_create_log_context() pl__create_log_context()
    #define pl_cleanup_log_context() pl__cleanup_log_context()
    #define pl_set_log_context(tPContext) pl__set_log_context((tPContext))
    #define pl_get_log_context() pl__get_log_context()

    // channels
    #define pl_add_log_channel(pcName, tType) pl__add_log_channel((pcName), (tType))
    #define pl_set_log_level(uID, uLevel) pl__set_log_level((uID), (uLevel))
    #define pl_set_current_log_channel(uID) pl__set_current_log_channel((uID))
    #define pl_get_current_log_channel() pl__get_current_log_channel()
    #define pl_clear_log_channel(uID) pl__clear_log_channel((uID))
    #define pl_reset_log_channel(uID) pl__reset_log_channel((uID))
    #define pl_get_log_entries(uID, puEntryCount) pl__get_log_entries((uID), (puEntryCount))

#endif // PL_LOG_ON

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_TRACE + 1 && defined(PL_LOG_ON)
    #define pl_log_trace(pcMessage) pl__log_trace(pl__get_current_log_channel(), (pcMessage))
    #define pl_log_trace_f(...) pl__log_trace_p(pl__get_current_log_channel(), __VA_ARGS__)
    #define pl_log_trace_to(uID, pcMessage) pl__log_trace((uID), (pcMessage))
    #define pl_log_trace_to_f(...) pl__log_trace_p(__VA_ARGS__)
#else
    #define pl_log_trace(pcMessage) //
    #define pl_log_trace_f(...) //
    #define pl_log_trace_to(tPContext, uID, pcMessage) //
    #define pl_log_trace_to_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_DEBUG + 1 && defined(PL_LOG_ON)
    #define pl_log_debug(pcMessage) pl__log_debug(pl__get_current_log_channel(), (pcMessage))
    #define pl_log_debug_f(...) pl__log_debug_p(pl__get_current_log_channel(), __VA_ARGS__)
    #define pl_log_debug_to(uID, pcMessage) pl__log_debug((uID), (pcMessage))
    #define pl_log_debug_to_f(...) pl__log_debug_p(__VA_ARGS__)
#else
    #define pl_log_debug(pcMessage) //
    #define pl_log_debug_f(...) //
    #define pl_log_debug_to(tPContext, uID, pcMessage) //
    #define pl_log_debug_to_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_INFO + 1 && defined(PL_LOG_ON)
    #define pl_log_info(pcMessage) pl__log_info(pl__get_current_log_channel(), (pcMessage))
    #define pl_log_info_f(...) pl__log_info_p(pl__get_current_log_channel(), __VA_ARGS__)
    #define pl_log_info_to(uID, pcMessage) pl__log_info((uID), (pcMessage))
    #define pl_log_info_to_f(...) pl__log_info_p(__VA_ARGS__)
#else
    #define pl_log_info(pcMessage) //
    #define pl_log_info_f(...) //
    #define pl_log_info_to(tPContext, uID, pcMessage) //
    #define pl_log_info_to_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_WARN + 1 && defined(PL_LOG_ON)
    #define pl_log_warn(pcMessage) pl__log_warn(pl__get_current_log_channel(), (pcMessage))
    #define pl_log_warn_f(...) pl__log_warn_p(pl__get_current_log_channel(), __VA_ARGS__)
    #define pl_log_warn_to(uID, pcMessage) pl__log_warn((uID), (pcMessage))
    #define pl_log_warn_to_f(...) pl__log_warn_p(__VA_ARGS__)
#else
    #define pl_log_warn(pcMessage) //
    #define pl_log_warn_f(...) //
    #define pl_log_warn_to(tPContext, uID, pcMessage) //
    #define pl_log_warn_to_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_ERROR + 1 && defined(PL_LOG_ON)
    #define pl_log_error(pcMessage) pl__log_error(pl__get_current_log_channel(), (pcMessage))
    #define pl_log_error_f(...) pl__log_error_p(pl__get_current_log_channel(), __VA_ARGS__)
    #define pl_log_error_to(uID, pcMessage) pl__log_error((uID), (pcMessage))
    #define pl_log_error_to_f(...) pl__log_error_p(__VA_ARGS__)
#else
    #define pl_log_error(pcMessage) //
    #define pl_log_error_f(...) //
    #define pl_log_error_to(tPContext, uID, pcMessage) //
    #define pl_log_error_to_f(...) //
#endif

#if PL_GLOBAL_LOG_LEVEL < PL_LOG_LEVEL_FATAL + 1 && defined(PL_LOG_ON)
    #define pl_log_fatal(pcMessage) pl__log_fatal(pl__get_current_log_channel(), (pcMessage))
    #define pl_log_fatal_f(...) pl__log_fatal_p(pl__get_current_log_channel(), __VA_ARGS__)
    #define pl_log_fatal_to(uID, pcMessage) pl__log_fatal((uID), (pcMessage))
    #define pl_log_fatal_to_f(...) pl__log_fatal_p(__VA_ARGS__)
#else
    #define pl_log_fatal(pcMessage) //
    #define pl_log_fatal_f(...) //
    #define pl_log_fatal_to(uID, pcMessage) //
    #define pl_log_fatal_to_f(...) //
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

typedef struct _plLogEntry
{
    uint32_t uLevel;
    char     cPBuffer[PL_LOG_MAX_LINE_SIZE];
} plLogEntry;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// setup/shutdown
plLogContext* pl__create_log_context (void);
void          pl__cleanup_log_context(void);
void          pl__set_log_context    (plLogContext* tPContext);
plLogContext* pl__get_log_context    (plLogContext* tPContext);

// channels
uint32_t    pl__add_log_channel        (const char* pcName, plChannelType tType);
uint32_t    pl__get_current_log_channel(void);
void        pl__set_current_log_channel(uint32_t uID);
void        pl__set_log_level          (uint32_t uID, uint32_t uLevel);
void        pl__clear_log_channel      (uint32_t uID);
void        pl__reset_log_channel      (uint32_t uID);
plLogEntry* pl__get_log_entries        (uint32_t uID, uint32_t* puEntryCount);

// logging
void pl__log_trace(uint32_t uID, const char* pcMessage);
void pl__log_debug(uint32_t uID, const char* pcMessage);
void pl__log_info (uint32_t uID, const char* pcMessage);
void pl__log_warn (uint32_t uID, const char* pcMessage);
void pl__log_error(uint32_t uID, const char* pcMessage);
void pl__log_fatal(uint32_t uID, const char* pcMessage);

void pl__log_trace_p(uint32_t uID, const char* cPFormat, ...);
void pl__log_debug_p(uint32_t uID, const char* cPFormat, ...);
void pl__log_info_p (uint32_t uID, const char* cPFormat, ...);
void pl__log_warn_p (uint32_t uID, const char* cPFormat, ...);
void pl__log_error_p(uint32_t uID, const char* cPFormat, ...);
void pl__log_fatal_p(uint32_t uID, const char* cPFormat, ...);

void pl__log_trace_va(uint32_t uID, const char* cPFormat, va_list args);
void pl__log_debug_va(uint32_t uID, const char* cPFormat, va_list args);
void pl__log_info_va (uint32_t uID, const char* cPFormat, va_list args);
void pl__log_warn_va (uint32_t uID, const char* cPFormat, va_list args);
void pl__log_error_va(uint32_t uID, const char* cPFormat, va_list args);
void pl__log_fatal_va(uint32_t uID, const char* cPFormat, va_list args);

#ifndef PL_LOG_ON
    #define pl_create_log_context() NULL
    #define pl_cleanup_log_context() //
    #define pl_set_log_context() //
    #define pl_get_log_context() NULL
    #define pl_add_log_channel(pcName, tType) 0u
    #define pl_set_log_level(uID, uLevel) //
    #define pl_clear_log_channel(uID) //
    #define pl_reset_log_channel(uID) //
    #define pl_get_log_entries(uID, puEntryCount) NULL
    #define pl_set_current_log_channel(uID) //
    #define pl_get_current_log_channel(uID) 0
#endif

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
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef PL_LOG_IMPLEMENTATION

#ifndef PL_LOG_MAX_CHANNEL_COUNT
    #define PL_LOG_MAX_CHANNEL_COUNT 16
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
    #define PL_LOG_INFO_FG_COLOR  PL_LOG_FG_COLOR_CODE_WHITE
#endif
#ifndef PL_LOG_WARN_FG_COLOR
    #define PL_LOG_WARN_FG_COLOR  PL_LOG_FG_COLOR_CODE_YELLOW
#endif
#ifndef PL_LOG_ERROR_FG_COLOR
    #define PL_LOG_ERROR_FG_COLOR PL_LOG_FG_COLOR_CODE_RED
#endif
#ifndef PL_LOG_FATAL_FG_COLOR
    #define PL_LOG_FATAL_FG_COLOR PL_LOG_FG_COLOR_CODE_WHITE
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
    char          cName[PL_LOG_MAX_LINE_SIZE];
    bool          bOverflowInUse;
    plLogEntry    atEntries[256];
    plLogEntry*   pEntries;
    uint32_t      uEntryCount;
    uint32_t      uEntryCapacity;
    uint32_t      uOverflowEntryCapacity;
    uint32_t      uLevel;
    plChannelType tType;
    uint32_t      uID;
} plLogChannel;

typedef struct _plLogContext
{
    plLogChannel atChannels[PL_LOG_MAX_CHANNEL_COUNT];
    uint32_t     uChannelCount;
    uint32_t     uCurrentChannel;
} plLogContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plLogContext* gptLogContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plLogEntry* pl__get_new_log_entry(uint32_t uID);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plLogContext*
pl__create_log_context(void)
{
    plLogContext* tPContext = (plLogContext*)PL_LOG_ALLOC(sizeof(plLogContext));
    memset(tPContext, 0, sizeof(plLogContext));
    tPContext->uChannelCount = 0;
    gptLogContext = tPContext;
    return tPContext;
}

void
pl__cleanup_log_context(void)
{
    PL_ASSERT(gptLogContext && "no global log context set");
    if(gptLogContext)
    {
        for(uint32_t i = 0; i < gptLogContext->uChannelCount; i++)
        {
            plLogChannel* ptChannel = &gptLogContext->atChannels[i];
            memset(ptChannel->cName, 0, sizeof(char) * PL_LOG_MAX_LINE_SIZE);
            if(ptChannel->bOverflowInUse)
                PL_LOG_FREE(ptChannel->pEntries);
            ptChannel->pEntries       = NULL;
            ptChannel->uEntryCapacity = 0;
            ptChannel->uEntryCount    = 0;
            ptChannel->uLevel         = 0;
            ptChannel->tType          = 0;
            ptChannel->uID            = 0;
        }
        memset(gptLogContext->atChannels, 0, sizeof(plLogChannel) * PL_LOG_MAX_CHANNEL_COUNT);
        gptLogContext->uChannelCount = 0;
        PL_LOG_FREE(gptLogContext);
    }
    gptLogContext = NULL;
}

void
pl__set_log_context(plLogContext* tPContext)
{
    PL_ASSERT(tPContext && "log context is NULL");
    gptLogContext = tPContext;
}

plLogContext*
pl__get_log_context(plLogContext* tPContext)
{
    PL_ASSERT(gptLogContext && "no global log context set");
    return gptLogContext;
}

uint32_t
pl__add_log_channel(const char* pcName, plChannelType tType)
{
    uint32_t uID = gptLogContext->uChannelCount;
    
    plLogChannel* ptChannel = &gptLogContext->atChannels[uID];
    strncpy(ptChannel->cName, pcName, PL_LOG_MAX_LINE_SIZE);
    ptChannel->pEntries       = ptChannel->atEntries;
    ptChannel->uEntryCapacity = 256;
    ptChannel->uEntryCount    = 0;
    ptChannel->uLevel         = 0;
    ptChannel->tType          = tType;
    ptChannel->uID            = uID;

    gptLogContext->uChannelCount++;
    return uID;
}

uint32_t
pl__get_current_log_channel(void)
{
    return gptLogContext->uCurrentChannel;
}

void
pl__set_current_log_channel(uint32_t uID)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->uCurrentChannel = uID;
}

void
pl__set_log_level(uint32_t uID, uint32_t uLevel)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->atChannels[uID].uLevel = uLevel;
}

void
pl__clear_log_channel(uint32_t uID)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->atChannels[uID].uEntryCount = 0u;
    gptLogContext->atChannels[uID].uEntryCapacity = 0u;
    PL_LOG_FREE(gptLogContext->atChannels[uID].pEntries);
    gptLogContext->atChannels[uID].pEntries = NULL;
    memset(gptLogContext->atChannels[uID].pEntries, 0, sizeof(plLogEntry) * gptLogContext->atChannels[uID].uEntryCapacity);
}

void
pl__reset_log_channel(uint32_t uID)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    gptLogContext->atChannels[uID].uEntryCount = 0u;
    memset(gptLogContext->atChannels[uID].pEntries, 0, sizeof(plLogEntry) * gptLogContext->atChannels[uID].uEntryCapacity);
}

plLogEntry*
pl__get_log_entries(uint32_t uID, uint32_t* puEntryCount)
{
    PL_ASSERT(uID < gptLogContext->uChannelCount && "channel ID is not valid");
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];
    if(puEntryCount)
        *puEntryCount = tPChannel->uEntryCount;
    return tPChannel->pEntries;
}

#define PL__LOG_LEVEL_MACRO(level, prefix, prefixSize) \
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID]; \
    plLogEntry* ptEntry = pl__get_new_log_entry(uID); \
    if(tPChannel->uLevel < level + 1) \
    { \
        if(tPChannel->tType & PL_CHANNEL_TYPE_CONSOLE) \
            printf(prefix " (%s) %s\n", tPChannel->cName, pcMessage); \
        if(tPChannel->tType & PL_CHANNEL_TYPE_BUFFER) \
        { \
            char* cPDest = ptEntry->cPBuffer; \
            ptEntry->uLevel = level; \
            strcpy(cPDest, prefix); \
            cPDest += prefixSize; \
            strcpy(cPDest, pcMessage); \
        } \
    }

void
pl__log_trace(uint32_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_TRACE, "[TRACE] ", 8)
}

void
pl__log_debug(uint32_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_DEBUG, "[DEBUG] ", 8)
}

void
pl__log_info(uint32_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_INFO, "[INFO ] ", 8)
}

void
pl__log_warn(uint32_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_WARN, "[WARN ] ", 8)
}

void
pl__log_error(uint32_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_ERROR, "[ERROR] ", 8)
}

void
pl__log_fatal(uint32_t uID, const char* pcMessage)
{
    PL__LOG_LEVEL_MACRO(PL_LOG_LEVEL_FATAL, "[FATAL] ", 8)
}

void
pl__log_trace_p(uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_trace_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_debug_p(uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_debug_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_info_p(uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_info_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_warn_p(uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_warn_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_error_p(uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_error_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

void
pl__log_fatal_p(uint32_t uID, const char* cPFormat, ...)
{
    va_list argptr;
    va_start(argptr, cPFormat);
    pl__log_fatal_va(uID, cPFormat, argptr);
    va_end(argptr);     
}

#define PL__LOG_LEVEL_VA_BUFFER_MACRO(level, prefix) \
        if(tPChannel->tType & PL_CHANNEL_TYPE_BUFFER) \
        { \
            plLogEntry* ptEntry = pl__get_new_log_entry(uID); \
            char* cPDest = ptEntry->cPBuffer; \
            ptEntry->uLevel = level; \
            cPDest += pl_snprintf(cPDest, PL_LOG_MAX_LINE_SIZE, prefix); \
            va_list parm_copy; \
            va_copy(parm_copy, args); \
            pl_vsnprintf(cPDest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); \
            va_end(parm_copy); \
        }

void
pl__log_trace_va(uint32_t uID, const char* cPFormat, va_list args)
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

            printf("[TRACE] (%s) ", tPChannel->cName);
            char dest[PL_LOG_MAX_LINE_SIZE];

            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_TRACE, "[TRACE] ")
    }

    
}

void
pl__log_debug_va(uint32_t uID, const char* cPFormat, va_list args)
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

            printf("[DEBUG] (%s) ", tPChannel->cName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_DEBUG, "[DEBUG] ")
    }
}

void
pl__log_info_va(uint32_t uID, const char* cPFormat, va_list args)
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

            printf("[INFO ] (%s) ", tPChannel->cName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_INFO, "[INFO ] ")
    }
}

void
pl__log_warn_va(uint32_t uID, const char* cPFormat, va_list args)
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

            printf("[WARN ] (%s) ", tPChannel->cName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy); 
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_WARN, "[WARN ] ")
    }
}

void
pl__log_error_va(uint32_t uID, const char* cPFormat, va_list args)
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

            printf("[ERROR] (%s) ", tPChannel->cName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_ERROR, "[ERROR] ")
    }
}

void
pl__log_fatal_va(uint32_t uID, const char* cPFormat, va_list args)
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

            printf("[FATAL] (%s) ", tPChannel->cName);
            char dest[PL_LOG_MAX_LINE_SIZE];
            va_list parm_copy;
            va_copy(parm_copy, args);
            pl_vsnprintf(dest, PL_LOG_MAX_LINE_SIZE, cPFormat, parm_copy); 
            printf("%s%s\n", dest, PL_LOG_POP_CODE);
            va_end(parm_copy);
        }
        PL__LOG_LEVEL_VA_BUFFER_MACRO(PL_LOG_LEVEL_FATAL, "[FATAL] ")
    }
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plLogEntry*
pl__get_new_log_entry(uint32_t uID)
{
    plLogChannel* tPChannel = &gptLogContext->atChannels[uID];

    plLogEntry* ptEntry = NULL;

    // check if new overflow
    if(!tPChannel->bOverflowInUse && tPChannel->uEntryCount == tPChannel->uEntryCapacity)
    {
        tPChannel->pEntries = (plLogEntry*)PL_LOG_ALLOC(sizeof(plLogEntry) * tPChannel->uEntryCapacity * 2);
        memset(tPChannel->pEntries, 0, sizeof(plLogEntry) * tPChannel->uEntryCapacity * 2);
        tPChannel->uOverflowEntryCapacity = tPChannel->uEntryCapacity * 2;

        // copy stack samples
        memcpy(tPChannel->pEntries, tPChannel->atEntries, sizeof(plLogEntry) * tPChannel->uEntryCapacity);
        tPChannel->bOverflowInUse = true;
    }
    // check if overflow reallocation is needed
    else if(tPChannel->bOverflowInUse && tPChannel->uEntryCount == tPChannel->uOverflowEntryCapacity)
    {
        plLogEntry* sbtOldInputEvents = tPChannel->pEntries;
        tPChannel->pEntries = (plLogEntry*)PL_LOG_ALLOC(sizeof(plLogEntry) * tPChannel->uOverflowEntryCapacity * 2);
        memset(tPChannel->pEntries, 0, sizeof(plLogEntry) * tPChannel->uOverflowEntryCapacity * 2);
        
        // copy old values
        memcpy(tPChannel->pEntries, sbtOldInputEvents, sizeof(plLogEntry) * tPChannel->uOverflowEntryCapacity);
        tPChannel->uOverflowEntryCapacity *= 2;

        PL_LOG_FREE(sbtOldInputEvents);
    }

    ptEntry = &tPChannel->pEntries[tPChannel->uEntryCount];
    tPChannel->uEntryCount++;

    return ptEntry;
}

#endif // PL_LOG_IMPLEMENTATION